// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_controller.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/url_constants.h"
#include "ash/public/cpp/ash_web_view_factory.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/do_not_disturb_notification_controller.h"
#include "ash/system/focus_mode/focus_mode_histogram_names.h"
#include "ash/system/focus_mode/focus_mode_metrics_recorder.h"
#include "ash/system/focus_mode/focus_mode_session.h"
#include "ash/system/focus_mode/focus_mode_tray.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/focus_mode/youtube_music/youtube_music_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/unified/unified_system_tray.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
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

void ShowEndingMomentNudge() {
  auto* tray = GetFocusModeTrayInActiveWindow();
  if (!tray) {
    return;
  }

  // NOTE: we anchor to `tray->image_view()` in order to center the nudge
  // properly because there is extra spacing on the actual `FocusModeTray` view.
  AnchoredNudgeData nudge_data(
      focus_mode_util::kFocusModeEndingMomentNudgeId,
      NudgeCatalogName::kFocusModeEndingMomentNudge,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE),
      tray->image_view());
  nudge_data.arrow = views::BubbleBorder::BOTTOM_CENTER;
  nudge_data.duration = NudgeDuration::kDefaultDuration;
  nudge_data.anchored_to_shelf = true;
  nudge_data.click_callback =
      base::BindRepeating(&FocusModeTray::ShowBubble, base::Unretained(tray));
  AnchoredNudgeManager::Get()->Show(nudge_data);

  auto current_session = FocusModeController::Get()->current_session();
  CHECK(current_session);
  const std::u16string duration_string =
      focus_mode_util::GetDurationString(current_session->session_duration(),
                                         /*digital_format=*/false);
  std::u16string title = l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_TITLE);
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_ENDING_MOMENT_NUDGE_ALERT, title,
          duration_string));
}

void HideEndingMomentNudge() {
  if (AnchoredNudgeManager* nudge_manager = AnchoredNudgeManager::Get()) {
    nudge_manager->Cancel(focus_mode_util::kFocusModeEndingMomentNudgeId);
  }
}

}  // namespace

FocusModeController::FocusModeController(
    std::unique_ptr<FocusModeDelegate> delegate)
    : session_duration_(kDefaultSessionDuration),
      delegate_(std::move(delegate)) {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  focus_mode_sounds_controller_ = std::make_unique<FocusModeSoundsController>();
  youtube_music_controller_ =
      std::make_unique<youtube_music::YouTubeMusicController>();

  focus_mode_sounds_controller_->AddObserver(this);
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
  UpdateFromUserPrefs();
}

void FocusModeController::OnSelectedPlaylistChanged() {
  if (!in_focus_session()) {
    return;
  }

  if (media_widget_) {
    CloseMediaWidget();
  }

  MaybeCreateMediaWidget();
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
    MaybeCreateMediaWidget();

    focus_mode_metrics_recorder_->RecordHistogramOnEndingMoment(
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
      observer.OnFocusModeChanged(/*in_focus_session=*/true);
    }
  }

  MaybeUpdateDndNotification();
}

void FocusModeController::ResetFocusSession() {
  if (focus_mode_metrics_recorder_) {
    focus_mode_metrics_recorder_->RecordHistogramsOnEnd();
    if (!in_focus_session()) {
      focus_mode_metrics_recorder_->RecordHistogramOnEndingMoment(
          current_session()->persistent_ending()
              ? focus_mode_histogram_names::EndingMomentBubbleClosedReason::
                    kOpended
              : focus_mode_histogram_names::EndingMomentBubbleClosedReason::
                    kIgnored);
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
      observer.OnFocusModeChanged(/*in_focus_session=*/false);
    }
  }
}

void FocusModeController::EnablePersistentEnding() {
  // This is only used right now for when we click the tray icon to open the
  // bubble during the ending moment. This prevents the bubble from being closed
  // automatically.
  if (!in_ending_moment()) {
    return;
  }

  if (timer_.IsRunning()) {
    timer_.Stop();
  }
  // Update the session to stay in the ending moment state.
  current_session_->set_persistent_ending();

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
                                  focus_mode_util::kEndingMomentDuration
                            : current_session_->end_time();
}

void FocusModeController::SetSelectedTask(const FocusModeTask& task) {
  const bool same_task = (selected_task_.task_id == task.task_id);

  selected_task_ = task;

  // Do not update metrics or user prefs if it is not a new task.
  if (same_task) {
    return;
  }

  if (in_focus_session() || in_ending_moment()) {
    SaveSelectedTaskSettingsToUserPrefs();
  }

  if (focus_mode_metrics_recorder_ && !selected_task_.empty()) {
    focus_mode_metrics_recorder_->IncrementTasksSelectedCount();
  }
}

bool FocusModeController::HasSelectedTask() const {
  return !selected_task_.task_id.empty();
}

void FocusModeController::CompleteTask(bool update) {
  if (update && !selected_task_.empty() && !selected_task_.title.empty()) {
    tasks_provider_.UpdateTask(selected_task_.task_list_id,
                               selected_task_.task_id, selected_task_.title,
                               /*completed=*/true, base::DoNothing());
  }
  SetSelectedTask({});

  if (focus_mode_metrics_recorder_) {
    focus_mode_metrics_recorder_->IncrementTasksCompletedCount();
  }
}

void FocusModeController::MaybeShowEndingMomentNudge() {
  // Do not show the nudge if there is a persistent tray bubble open during the
  // ending moment.
  if (!in_ending_moment() || current_session_->persistent_ending()) {
    return;
  }

  if (auto* anchored_nudge_manager = AnchoredNudgeManager::Get();
      anchored_nudge_manager->IsNudgeShown(
          focus_mode_util::kFocusModeEndingMomentNudgeId)) {
    return;
  }

  ShowEndingMomentNudge();
}

void FocusModeController::TriggerEndingMomentImmediately() {
  if (!in_focus_session()) {
    return;
  }
  current_session_->set_end_time(base::Time::Now());
  OnTimerTick();
}

void FocusModeController::StartFocusSession(
    focus_mode_histogram_names::ToggleSource source) {
  focus_mode_metrics_recorder_ =
      std::make_unique<FocusModeMetricsRecorder>(session_duration_);
  focus_mode_metrics_recorder_->RecordHistogramsOnStart(source,
                                                        selected_task_.task_id);
  if (HasSelectedTask()) {
    focus_mode_metrics_recorder_->IncrementTasksSelectedCount();
  }

  current_session_ = FocusModeSession(session_duration_,
                                      session_duration_ + base::Time::Now());

  SaveSettingsToUserPrefs();

  // Start timer for the specified `session_duration_`. Set `current_session_`
  // before `SetQuietMode` called, because we may indirectly call
  // `GetActualEndTime` to create a notification.
  timer_.Start(FROM_HERE, base::Seconds(1), this,
               &FocusModeController::OnTimerTick, base::TimeTicks::Now());

  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  if (turn_on_do_not_disturb_ && !message_center->IsQuietMode()) {
    // Only turn on DND if it is not enabled before starting a session and
    // `turn_on_do_not_disturb_` is true.
    message_center->SetQuietMode(
        true, message_center::QuietModeSourceType::kFocusMode);
  } else if (!turn_on_do_not_disturb_ && IsQuietModeOnSetByFocusMode()) {
    // This is the case where a user toggles off DND in the focus panel before
    // it has been switched off by the termination of the ending moment.
    message_center->SetQuietMode(
        false, message_center::QuietModeSourceType::kFocusMode);
  } else if (turn_on_do_not_disturb_ && IsQuietModeOnSetByFocusMode()) {
    // This can only happen if a new focus session is started during an ending
    // moment. If the DND state is preserved (i.e. `turn_on_do_not_disturb_` is
    // still true), then just update the notification.
    MaybeUpdateDndNotification();
  }

  CloseSystemTrayBubble();
  SetFocusTrayVisibility(true);
  HideEndingMomentNudge();
  MaybeCreateMediaWidget();

  for (auto& observer : observers_) {
    observer.OnFocusModeChanged(/*in_focus_session=*/true);
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

      if (media_widget_) {
        CloseMediaWidget();
      }

      // Set a timer to terminate the ending moment. If the focus tray bubble is
      // open, the ending moment will exist until the bubble is closed.
      if (!IsFocusTrayBubbleVisible()) {
        timer_.Start(FROM_HERE, focus_mode_util::kEndingMomentDuration, this,
                     &FocusModeController::ResetFocusSession,
                     base::TimeTicks::Now());
        MaybeUpdateDndNotification();
      } else {
        current_session_->set_persistent_ending();
      }

      for (auto& observer : observers_) {
        observer.OnFocusModeChanged(/*in_focus_session=*/false);
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

  // Get the selected task from the dict and also update `selected_task_` if
  // there is a task.
  const auto& selected_task_dict =
      active_user_prefs->GetDict(prefs::kFocusModeSelectedTask);
  selected_task_ = {};
  if (!selected_task_dict.empty()) {
    // TODO(b/339914681): call the API to populate the rest of the
    // `selected_task_` data. This will also verify if the task has already been
    // completed or not.
    selected_task_.task_list_id =
        *(selected_task_dict.FindString(focus_mode_util::kTaskListIdKey));
    selected_task_.task_id =
        *(selected_task_dict.FindString(focus_mode_util::kTaskIdKey));
  }
}

void FocusModeController::SaveSettingsToUserPrefs() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    active_user_prefs->SetTimeDelta(prefs::kFocusModeSessionDuration,
                                    session_duration_);
    active_user_prefs->SetBoolean(prefs::kFocusModeDoNotDisturb,
                                  turn_on_do_not_disturb_);
    SaveSelectedTaskSettingsToUserPrefs();
  }
}

void FocusModeController::SaveSelectedTaskSettingsToUserPrefs() {
  if (PrefService* active_user_prefs =
          Shell::Get()->session_controller()->GetActivePrefService()) {
    base::Value::Dict selected_task_dict;

    // If there is a `selected_task_`, we will save its `task_list_id` and
    // `task_id`; otherwise, we will store an empty dict.
    if (HasSelectedTask()) {
      selected_task_dict.Set(focus_mode_util::kTaskListIdKey,
                             selected_task_.task_list_id);
      selected_task_dict.Set(focus_mode_util::kTaskIdKey,
                             selected_task_.task_id);
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

void FocusModeController::MaybeCreateMediaWidget() {
  if (media_widget_ ||
      focus_mode_sounds_controller_->selected_playlist().empty()) {
    return;
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
  focus_mode_media_view_ = media_widget_->SetContentsView(
      AshWebViewFactory::Get()->Create(web_view_params));
  focus_mode_media_view_->Navigate(GURL(chrome::kChromeUIFocusModeMediaURL));
}

void FocusModeController::CloseMediaWidget() {
  CHECK(media_widget_);
  focus_mode_media_view_.ClearAndDelete();
  focus_mode_media_view_ = nullptr;
  media_widget_.reset();
}

}  // namespace ash
