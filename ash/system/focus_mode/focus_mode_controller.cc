// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/url_constants.h"
#include "ash/media/media_controller_impl.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_metrics_recorder.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "chromeos/ash/components/audio/system_sounds_delegate.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

FocusModeController* g_instance = nullptr;

// The default Focus Mode session duration.
constexpr base::TimeDelta kDefaultSessionDuration = base::Minutes(25);

constexpr base::TimeDelta kSessionEndSoundDelay = base::Milliseconds(200);

constexpr base::TimeDelta kEndingMomentBounceAnimationDelay = base::Minutes(1);

bool IsQuietModeOnSetByFocusMode() {
  auto* message_center = message_center::MessageCenter::Get();
  return message_center->IsQuietMode() &&
         message_center->GetLastQuietModeChangeSourceType() ==
             message_center::QuietModeSourceType::kFocusMode;
}

// Updates the notification if DND was turned on by the focus mode.
void MaybeUpdateDndNotification() {
  if (!IsQuietModeOnSetByFocusMode()) {
    return;
  }

  if (auto* notification_controller =
          DoNotDisturbNotificationController::Get()) {
    notification_controller->MaybeUpdateNotification();
  }
}

FocusModeTray* GetFocusModeTrayInActiveWindow() {
  auto* window = Shell::Get()->GetRootWindowForNewWindows();
  if (!window) {
    return nullptr;
  }

  auto* root_window_controller = RootWindowController::ForWindow(window);
  if (!root_window_controller) {
    return nullptr;
  }

  auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
  if (!status_area_widget) {
    return nullptr;
  }

  return status_area_widget->focus_mode_tray();
}

void ShowEndingMomentNudge(
    const size_t congratulatory_index,
    const std::optional<FocusModeSession>& current_session) {
  auto* tray = GetFocusModeTrayInActiveWindow();
  if (!tray) {
    return;
  }

  // NOTE: we anchor to `tray->image_view()` in order to center the nudge
  // properly because there is extra spacing on the actual `FocusModeTray` view.
  const auto& title_and_emoji =
      focus_mode_util::GetCongratulatoryTextAndEmoji(congratulatory_index);
  AnchoredNudgeData nudge_data(focus_mode_util::kFocusModeEndingMomentNudgeId,
                               NudgeCatalogName::kFocusModeEndingMomentNudge,
                               title_and_emoji, tray->image_view());
  nudge_data.arrow = views::BubbleBorder::BOTTOM_CENTER;
  nudge_data.duration = NudgeDuration::kDefaultDuration;
  nudge_data.anchored_to_shelf = true;
  nudge_data.announce_chromevox = false;
  nudge_data.click_callback =
      base::BindRepeating(&FocusModeTray::ShowBubble, base::Unretained(tray));
  AnchoredNudgeManager::Get()->Show(nudge_data);

  CHECK(current_session);
  const std::u16string duration_string =
      focus_mode_util::GetDurationString(current_session->session_duration(),
                                         /*digital_format=*/false);
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_NUDGE_ALERT,
          title_and_emoji, duration_string));
}

void HideEndingMomentNudge() {
  if (AnchoredNudgeManager* nudge_manager = AnchoredNudgeManager::Get()) {
    nudge_manager->Cancel(focus_mode_util::kFocusModeEndingMomentNudgeId);
  }
}

void OnTaskFetched(FocusModeTasksModel::Delegate::FetchTaskCallback callback,
                   const FocusModeTask& task) {
  if (task.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(task);
}

}  // namespace

FocusModeController::FocusModeController(
    std::unique_ptr<FocusModeDelegate> delegate)
    : session_duration_(kDefaultSessionDuration),
      delegate_(std::move(delegate)) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  focus_mode_sounds_controller_ =
      std::make_unique<FocusModeSoundsController>(delegate_->GetLocale());

  focus_mode_sounds_controller_->AddObserver(this);
  tasks_model_.SetDelegate(weak_factory_.GetWeakPtr());
  tasks_model_observation_.Observe(&tasks_model_);
  Shell::Get()->session_controller()->AddObserver(this);
}

FocusModeController::~FocusModeController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  focus_mode_sounds_controller_->RemoveObserver(this);

  // TODO(b/338694884): Move this to startup.
  if (IsQuietModeOnSetByFocusMode()) {
    message_center::MessageCenter::Get()->SetQuietMode(
        false, message_center::QuietModeSourceType::kFocusMode);
  }

  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FocusModeController* FocusModeController::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
bool FocusModeController::CanExtendSessionDuration(
    const FocusModeSession::Snapshot& snapshot) {
  return snapshot.session_duration < focus_mode_util::kMaximumDuration;
}

// static
void FocusModeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(
      prefs::kFocusModeSessionDuration,
      /*default_value=*/kDefaultSessionDuration,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kFocusModeDoNotDisturb,
      /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      prefs::kFocusModeSelectedTask,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterDictionaryPref(
      prefs::kFocusModeSoundSection,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  // Prefs for YouTube Music.
  registry->RegisterBooleanPref(
      prefs::kFocusModeYTMDisplayOAuthConsent, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kFocusModeYTMDisplayFreeTrial, /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);

  // Pref is device-local and should never be synced.
  registry->RegisterStringPref(prefs::kFocusModeDeviceId, "");

  // Pref only set via policy.
  registry->RegisterStringPref(prefs::kFocusModeSoundsEnabled,
                               focus_mode_util::kFocusModeSoundsEnabled);
}

void FocusModeController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FocusModeController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FocusModeController::ToggleFocusMode(
    focus_mode_histogram_names::ToggleSource source) {
  if (in_focus_session()) {
    base::UmaHistogramEnumeration(
        /*name=*/focus_mode_histogram_names::
            kToggleEndButtonDuringSessionHistogramName,
        /*sample=*/source);
    ResetFocusSession();
    return;
  }
  StartFocusSession(source);
}

void FocusModeController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  ResetFocusSession();
  tasks_model_.Reset();
  tasks_provider_.Reset();

  // Since we cannot guarantee that `TasksClientImpl::InvalidateCache()` has
  // been called before this when the active user session changes, we should
  // just call `FocusModeController::UpdateFromUserPrefs()` as a PostTask to
  // prevent the `TasksClientImpl::GetTasks()` callback from potentially being
  // failed.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FocusModeController::UpdateFromUserPrefs,
                                weak_factory_.GetWeakPtr()));
}

void FocusModeController::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Make the tray consistent with other pods for when it should be shown (i.e.
  // not in the login screen).
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    SetFocusTrayVisibility(false);
    return;
  }

  if (in_focus_session() || in_ending_moment()) {
    SetFocusTrayVisibility(true);
  }
}

void FocusModeController::OnSelectedPlaylistChanged() {
  // If a user swaps playlists or deselects the playlist, we should close the
  // previous media widget. The reason we don't just reuse the existing widget
  // with a new playlist is that we need to refresh the web view source title so
  // that it's populated correctly in the media controls.
  if (media_widget_) {
    CloseMediaWidget();
  }

  if (focus_mode_metrics_recorder_) {
    focus_mode_metrics_recorder_->SetHasSelectedSoundType(
        focus_mode_sounds_controller_->selected_playlist());
  }

  // Only attempt to create the media widget if we are in an active focus
  // session.
  if (in_focus_session()) {
    MaybeCreateMediaWidget();
  }
}

void FocusModeController::OnSelectedTaskChanged(
    const std::optional<FocusModeTask>& task) {
  if (in_focus_session() || in_ending_moment()) {
    SaveSelectedTaskSettingsToUserPrefs(task);
  }

  if (focus_mode_metrics_recorder_ && task) {
    focus_mode_metrics_recorder_->IncrementTasksSelectedCount();
  }
}

void FocusModeController::OnTasksUpdated(
    const std::vector<FocusModeTask>& tasks) {}

void FocusModeController::OnTaskCompleted(const FocusModeTask& completed_task) {
  if (focus_mode_metrics_recorder_) {
    focus_mode_metrics_recorder_->IncrementTasksCompletedCount();
  }
}

void FocusModeController::FetchTask(
    const TaskId& task_id,
    FocusModeTasksModel::Delegate::FetchTaskCallback callback) {
  tasks_provider_.GetTask(task_id.list_id, task_id.id,
                          base::BindOnce(OnTaskFetched, std::move(callback)));
}

void FocusModeController::FetchTasks() {
  tasks_provider_.GetSortedTaskList(base::BindOnce(
      &FocusModeController::OnTasksReceived, weak_factory_.GetWeakPtr()));
}

void FocusModeController::AddTask(
    const FocusModeTasksModel::TaskUpdate& update,
    FocusModeTasksModel::Delegate::FetchTaskCallback callback) {
  tasks_provider_.AddTask(*update.title,
                          base::BindOnce(OnTaskFetched, std::move(callback)));
}

void FocusModeController::UpdateTask(
    const FocusModeTasksModel::TaskUpdate& update) {
  const TaskId& task_id = *update.task_id;
  const std::string& title = update.title.has_value() ? *update.title : "";
  const bool completed =
      update.completed.has_value() ? update.completed.value() : false;
  tasks_provider_.UpdateTask(task_id.list_id, task_id.id, title, completed,
                             base::DoNothing());
}

void FocusModeController::ExtendSessionDuration() {
  CHECK(current_session_);

  const bool was_in_ending_moment = in_ending_moment();
  const base::Time now = base::Time::Now();
  // We call this with `now` to make sure that all the actions taken are synced
  // to the same time, since the state depends on `now`.
  current_session_->ExtendSession(now);

  std::string message;
  if (was_in_ending_moment) {
    PerformActionsForMusic();
    paused_by_ending_moment_ = false;

    MaybeEnableDND();

    focus_mode_metrics_recorder_->RecordEndingMomentBubbleHistogram(
        focus_mode_histogram_names::EndingMomentBubbleClosedReason::kExtended);

    message = l10n_util::GetStringUTF8(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_EXTEND_TEN_MINUTES_BUTTON_ALERT);
  } else {
    const std::u16string duration_string = focus_mode_util::GetDurationString(
        current_session_->GetTimeRemaining(now), /*digital_format=*/false);
    message = l10n_util::GetStringFUTF8(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_INCREASE_TEN_MINUTES_BUTTON_ALERT,
        duration_string);
  }
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(message);

  const auto session_snapshot = current_session_->GetSnapshot(now);
  for (auto& observer : observers_) {
    observer.OnActiveSessionDurationChanged(session_snapshot);
  }

  if (!timer_.IsRunning()) {
    // Start the `session_duration_` timer again.
    timer_.Start(FROM_HERE, base::Seconds(1), this,
                 &FocusModeController::OnTimerTick, base::TimeTicks::Now());

    for (auto& observer : observers_) {
      observer.OnFocusModeChanged(
          /*session_state=*/FocusModeSession::State::kOn);
    }
  }

  MaybeUpdateDndNotification();
}

void FocusModeController::ResetFocusSession() {
  if (focus_mode_metrics_recorder_) {
    focus_mode_metrics_recorder_->RecordHistogramsOnEnd();
    if (!in_focus_session()) {
      focus_mode_metrics_recorder_->RecordEndingMomentBubbleHistogram(
          focus_mode_histogram_names::EndingMomentBubbleClosedReason::kOpended);
    }

    focus_mode_metrics_recorder_.reset();
  }

  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  HideEndingMomentNudge();

  SetFocusTrayVisibility(false);
  if (media_widget_) {
    CloseMediaWidget();
  }

  if (IsQuietModeOnSetByFocusMode()) {
    message_center::MessageCenter::Get()->SetQuietMode(
        false, message_center::QuietModeSourceType::kFocusMode);
  }

  const bool was_in_focus_session = in_focus_session();
  current_session_.reset();

  if (was_in_focus_session) {
    for (auto& observer : observers_) {
      observer.OnFocusModeChanged(
          /*session_state=*/FocusModeSession::State::kOff);
    }
  }
}

void FocusModeController::OnEndingBubbleShown() {
  if (!in_ending_moment()) {
    return;
  }

  if (timer_.IsRunning()) {
    timer_.Stop();
  }

  HideEndingMomentNudge();
}

void FocusModeController::SetInactiveSessionDuration(
    const base::TimeDelta& new_session_duration) {
  CHECK(!in_focus_session());
  const base::TimeDelta valid_new_session_duration =
      std::clamp(new_session_duration, focus_mode_util::kMinimumDuration,
                 focus_mode_util::kMaximumDuration);

  if (session_duration_ == valid_new_session_duration) {
    return;
  }

  // We do not immediately commit the change directly to the user prefs because
  // the user has not yet indicated their preferred timer duration by starting
  // the timer.
  session_duration_ = valid_new_session_duration;

  for (auto& observer : observers_) {
    observer.OnInactiveSessionDurationChanged(session_duration_);
  }
}

bool FocusModeController::HasStartedSessionBefore() const {
  // Since `kFocusModeDoNotDisturb` is always set whenever a focus session is
  // started, we can use this as an indicator of if the user has ever started a
  // focus session before.
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    return active_user_prefs->HasPrefPath(prefs::kFocusModeDoNotDisturb);
  }
  return false;
}

FocusModeSession::Snapshot FocusModeController::GetSnapshot(
    const base::Time& now) const {
  return current_session_ ? current_session_->GetSnapshot(now)
                          : FocusModeSession::Snapshot{};
}

base::TimeDelta FocusModeController::GetSessionDuration() const {
  return in_focus_session() ? current_session_->session_duration()
                            : session_duration_;
}

base::Time FocusModeController::GetActualEndTime() const {
  if (!current_session_) {
    return base::Time();
  }

  return in_ending_moment() ? current_session_->end_time() +
                                  focus_mode_util::kInitialEndingMomentDuration
                            : current_session_->end_time();
}

void FocusModeController::SetSelectedTask(const FocusModeTask& task) {
  if (task.task_id.empty()) {
    tasks_model_.ClearSelectedTask();
    return;
  }

  tasks_model_.SetSelectedTask(task);
}

bool FocusModeController::HasSelectedTask() const {
  return !!tasks_model_.selected_task();
}

void FocusModeController::CompleteTask() {
  const FocusModeTask* selected_task = tasks_model_.selected_task();
  if (!selected_task) {
    return;
  }
  tasks_model_.UpdateTask(
      FocusModeTasksModel::TaskUpdate::CompletedUpdate(selected_task->task_id));
}

void FocusModeController::MaybeShowEndingMomentNudge() {
  // Do not show the nudge if there is a tray bubble open during the ending
  // moment.
  if (!in_ending_moment() || IsFocusTrayBubbleVisible()) {
    return;
  }

  if (auto* anchored_nudge_manager = AnchoredNudgeManager::Get();
      anchored_nudge_manager->IsNudgeShown(
          focus_mode_util::kFocusModeEndingMomentNudgeId)) {
    return;
  }

  ShowEndingMomentNudge(congratulatory_index_, current_session_);
}

void FocusModeController::TriggerEndingMomentImmediately() {
  if (!in_focus_session()) {
    return;
  }
  current_session_->set_end_time(base::Time::Now());
  OnTimerTick();
}

void FocusModeController::MaybeEnableDND() {
  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  if (turn_on_do_not_disturb_ && !message_center->IsQuietMode()) {
    // Only turn on DND if it is not enabled before starting a session and
    // `turn_on_do_not_disturb_` is true.
    message_center->SetQuietMode(
        true, message_center::QuietModeSourceType::kFocusMode);
  } else if (IsQuietModeOnSetByFocusMode()) {
    if (turn_on_do_not_disturb_) {
      // This can only happen if a new focus session is started during an ending
      // moment. If the DND state is preserved (i.e. `turn_on_do_not_disturb_`
      // is still true), then just update the notification.
      MaybeUpdateDndNotification();
    } else {
      // This is the case where a user toggles off DND in the focus panel before
      // it has been switched off by the termination of the ending moment.
      message_center->SetQuietMode(
          false, message_center::QuietModeSourceType::kFocusMode);
    }
  }
}

void FocusModeController::MaybeDisableDND() {
  // We need to make sure the histogram is recorded before we make any changes
  // to the DND state.
  if (focus_mode_metrics_recorder_) {
    focus_mode_metrics_recorder_->RecordDNDHistogram();
  }

  if (in_ending_moment() && IsQuietModeOnSetByFocusMode()) {
    message_center::MessageCenter::Get()->SetQuietMode(
        false, message_center::QuietModeSourceType::kFocusMode);
  }
}

void FocusModeController::BounceTrayIcon() {
  CHECK(in_ending_moment());

  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    return;
  }

  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (auto* status_area_widget =
            root_window_controller->GetStatusAreaWidget()) {
      auto* tray = status_area_widget->focus_mode_tray();
      tray->MaybePlayBounceInAnimation();
    }
  }
}

const base::UnguessableToken& FocusModeController::GetMediaSessionRequestId() {
  if (!test_media_request_id_.is_empty()) {
    CHECK_IS_TEST();
    return test_media_request_id_;
  }

  return focus_mode_media_view_
             ? focus_mode_media_view_->GetMediaSessionRequestId()
             : base::UnguessableToken::Null();
}

void FocusModeController::RequestTasksUpdateForTesting() {
  tasks_model_.RequestUpdate();
}

bool FocusModeController::TasksProviderHasCachedTasksForTesting() const {
  return !tasks_provider_.TasksForTesting().empty();  // IN-TEST
}

media_session::mojom::MediaSessionInfoPtr
FocusModeController::GetSystemMediaSessionInfo() {
  if (test_media_session_info_) {
    CHECK_IS_TEST();
    return std::move(test_media_session_info_);
  }
  return Shell::Get()->media_controller()->GetMediaSessionInfo();
}

void FocusModeController::StartFocusSession(
    focus_mode_histogram_names::ToggleSource source) {
  paused_by_ending_moment_ = false;
  focus_mode_sounds_controller_->reset_paused_event_count();
  focus_mode_metrics_recorder_ =
      std::make_unique<FocusModeMetricsRecorder>(session_duration_);
  const FocusModeTask* selected_task = tasks_model_.selected_task();
  focus_mode_metrics_recorder_->RecordHistogramsOnStart(
      source, selected_task ? selected_task->task_id : TaskId());
  if (selected_task) {
    focus_mode_metrics_recorder_->IncrementTasksSelectedCount();
  }

  const auto& selected_playlist =
      focus_mode_sounds_controller_->selected_playlist();
  focus_mode_metrics_recorder_->SetHasSelectedSoundType(selected_playlist);
  if (!selected_playlist.empty()) {
    focus_mode_sounds_controller_->SoundsStarted();
  }

  current_session_ = FocusModeSession(session_duration_,
                                      session_duration_ + base::Time::Now());

  SaveSettingsToUserPrefs();

  // Start timer for the specified `session_duration_`. Set `current_session_`
  // before `SetQuietMode` called, because we may indirectly call
  // `GetActualEndTime` to create a notification.
  timer_.Start(FROM_HERE, base::Seconds(1), this,
               &FocusModeController::OnTimerTick, base::TimeTicks::Now());

  MaybeEnableDND();
  CloseSystemTrayBubble();
  SetFocusTrayVisibility(true);
  HideEndingMomentNudge();
  MaybeCreateMediaWidget();

  for (auto& observer : observers_) {
    observer.OnFocusModeChanged(/*session_state=*/FocusModeSession::State::kOn);
  }
}

void FocusModeController::OnTimerTick() {
  CHECK(current_session_);
  auto session_snapshot = current_session_->GetSnapshot(base::Time::Now());
  switch (session_snapshot.state) {
    case FocusModeSession::State::kOn:
      for (auto& observer : observers_) {
        observer.OnTimerTick(session_snapshot);
      }
      return;
    case FocusModeSession::State::kEnding:
      timer_.Stop();
      congratulatory_index_ = base::RandInt(
          /*min=*/0, /*max=*/focus_mode_util::kCongratulatoryTitleNum - 1);

      if (media_widget_) {
        paused_by_ending_moment_ =
            focus_mode_sounds_controller_->selected_playlist().state ==
            focus_mode_util::SoundState::kPlaying;
        if (paused_by_ending_moment_) {
          focus_mode_sounds_controller_->PausePlayback();
        }
      }

      // Set a timer to nudge the user every `kEndingMomentBounceAnimationDelay`
      // that the session has ended. The ending moment will exist until the user
      // opens the bubble and takes an action.
      if (!IsFocusTrayBubbleVisible()) {
        timer_.Start(FROM_HERE, kEndingMomentBounceAnimationDelay, this,
                     &FocusModeController::BounceTrayIcon,
                     base::TimeTicks::Now());
        MaybeUpdateDndNotification();
      }
      current_session_->set_persistent_ending();

      // If Focus Mode enabled DND, we will turn it off after a short delay and
      // not allow it to persist with the ending moment.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FocusModeController::MaybeDisableDND,
                         weak_factory_.GetWeakPtr()),
          focus_mode_util::kInitialEndingMomentDuration);

      // Play sounds effect after 200ms delay.
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce([]() {
            if (Shell::HasInstance()) {
              Shell::Get()->system_sounds_delegate()->Play(
                  Sound::kFocusModeEndingMoment);
            }
          }),
          kSessionEndSoundDelay);

      for (auto& observer : observers_) {
        observer.OnFocusModeChanged(
            /*session_state=*/FocusModeSession::State::kEnding);
      }
      return;
    case FocusModeSession::State::kOff:
      ResetFocusSession();
      return;
  }
}

void FocusModeController::UpdateFromUserPrefs() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    // Can be null in tests.
    return;
  }

  session_duration_ =
      active_user_prefs->GetTimeDelta(prefs::kFocusModeSessionDuration);
  turn_on_do_not_disturb_ =
      active_user_prefs->GetBoolean(prefs::kFocusModeDoNotDisturb);

  if (session_duration_ <= base::TimeDelta()) {
    session_duration_ = kDefaultSessionDuration;
  }

  UpdateSelectedTaskFromUserPrefs();
  focus_mode_sounds_controller_->UpdateFromUserPrefs();
}

void FocusModeController::UpdateSelectedTaskFromUserPrefs() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    // Can be null in tests.
    return;
  }

  // Get the selected task from the dict and also update the selected task if
  // there is a task.
  const auto& selected_task_dict =
      active_user_prefs->GetDict(prefs::kFocusModeSelectedTask);
  if (selected_task_dict.empty()) {
    return;
  }

  TaskId pref_task = {
      .list_id =
          *(selected_task_dict.FindString(focus_mode_util::kTaskListIdKey)),
      .id = *(selected_task_dict.FindString(focus_mode_util::kTaskIdKey))};
  if (!pref_task.empty()) {
    tasks_model_.SetSelectedTaskFromPrefs(pref_task);
  }
}

void FocusModeController::SaveSettingsToUserPrefs() {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!active_user_prefs) {
    return;
  }

  active_user_prefs->SetTimeDelta(prefs::kFocusModeSessionDuration,
                                  session_duration_);
  active_user_prefs->SetBoolean(prefs::kFocusModeDoNotDisturb,
                                turn_on_do_not_disturb_);
  const auto* selected_task = tasks_model_.selected_task();
  SaveSelectedTaskSettingsToUserPrefs(
      selected_task ? std::make_optional<FocusModeTask>(*selected_task)
                    : std::nullopt);
}

void FocusModeController::SaveSelectedTaskSettingsToUserPrefs(
    const std::optional<FocusModeTask>& task) {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    base::Value::Dict selected_task_dict;

    // If there is a selected task, we will save its `task_id.list_id` and
    // `task_id.id`; otherwise, we will store an empty dict.
    if (task) {
      selected_task_dict.Set(focus_mode_util::kTaskListIdKey,
                             task->task_id.list_id);
      selected_task_dict.Set(focus_mode_util::kTaskIdKey, task->task_id.id);
    }
    active_user_prefs->SetDict(prefs::kFocusModeSelectedTask,
                               std::move(selected_task_dict));
  }
}

void FocusModeController::CloseSystemTrayBubble() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (root_window_controller->IsSystemTrayVisible()) {
      root_window_controller->GetStatusAreaWidget()
          ->unified_system_tray()
          ->CloseBubble();
    }
  }
}

void FocusModeController::SetFocusTrayVisibility(bool visible) {
  if (visible && Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    return;
  }

  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (auto* status_area_widget =
            root_window_controller->GetStatusAreaWidget()) {
      auto* tray = status_area_widget->focus_mode_tray();
      if (!visible) {
        tray->CloseBubble();
      }
      tray->SetVisiblePreferred(visible);
    }
  }
}

bool FocusModeController::IsFocusTrayBubbleVisible() const {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (root_window_controller->GetStatusAreaWidget()
            ->focus_mode_tray()
            ->GetBubbleView()) {
      return true;
    }
  }
  return false;
}

bool FocusModeController::MaybeCreateMediaWidget() {
  if (media_widget_ ||
      focus_mode_sounds_controller_->selected_playlist().empty()) {
    return false;
  }

  CHECK(in_focus_session());

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "FocusModeMediaWidget";
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  params.child = true;

  // The media window should be hidden.
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // The media window does not receive any events.
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;

  media_widget_ = std::make_unique<views::Widget>();
  media_widget_->Init(std::move(params));

  AshWebView::InitParams web_view_params;
  web_view_params.suppress_navigation = true;
  web_view_params.enable_wake_locks = false;
  web_view_params.source_title =
      focus_mode_util::GetSourceTitleForMediaControls(
          focus_mode_sounds_controller_->selected_playlist());
  focus_mode_media_view_ = media_widget_->SetContentsView(
      AshWebViewFactory::Get()->Create(web_view_params));
  focus_mode_media_view_->Navigate(GURL(chrome::kChromeUIFocusModeMediaURL));
  return true;
}

void FocusModeController::CloseMediaWidget() {
  CHECK(media_widget_);
  focus_mode_media_view_.ClearAndDelete();
  focus_mode_media_view_ = nullptr;
  media_widget_.reset();
}

void FocusModeController::PerformActionsForMusic() {
  const auto& selected_playlist =
      focus_mode_sounds_controller_->selected_playlist();
  // Do nothing if there is no selected playlist, or a new media widget was
  // created.
  if (selected_playlist.empty() || MaybeCreateMediaWidget()) {
    return;
  }

  // If the music was paused by the user before the ending moment, we should
  // keep it in paused state after extending the session; otherwise, we will
  // continue to play the existing music because it was paused by the ending
  // moment.
  if (paused_by_ending_moment_) {
    focus_mode_sounds_controller_->ResumePlayingPlayback();
  }
}

void FocusModeController::OnTasksReceived(
    const std::vector<FocusModeTask>& tasks) {
  std::vector<FocusModeTask> copy = tasks;
  tasks_model_.SetTaskList(std::move(copy));
}

}  // namespace ash
