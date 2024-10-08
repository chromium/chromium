// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/base_capture_mode_session.h"
#include "ash/capture_mode/capture_mode_ash_notification_view.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_camera_controller.h"
#include "ash/capture_mode/capture_mode_education_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_observer.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/null_capture_mode_session.h"
#include "ash/capture_mode/search_results_panel.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_type.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/aura/env.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

CaptureModeController* g_instance = nullptr;

// The amount of time that can elapse from the prior screenshot to be considered
// consecutive.
constexpr base::TimeDelta kConsecutiveScreenshotThreshold = base::Seconds(5);

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";
constexpr char kScreenCaptureStoppedNotificationId[] =
    "capture_mode_stopped_notification";
constexpr char kScreenCaptureNotifierId[] = "ash.capture_mode_controller";
constexpr char kScreenShotNotificationType[] = "screen_shot_notification_type";
constexpr char kScreenRecordingNotificationType[] =
    "screen_recording_notification_type";

// The format strings of the file names of captured images.
// TODO(afakhry): Discuss with UX localizing "Screenshot" and "Screen
// recording".
constexpr char kScreenshotFileNameFmtStr[] = "Screenshot %s %s";
constexpr char kVideoFileNameFmtStr[] = "Screen recording %s %s";

// Duration to clear the capture region selection from the previous session.
constexpr base::TimeDelta kResetCaptureRegionDuration = base::Minutes(8);

// The name of a file path pref for the user-selected custom path to which
// captured images and videos should be saved.
constexpr char kCustomCapturePathPrefName[] =
    "ash.capture_mode.custom_save_path";

// The name of a boolean pref that indicates whether the default downloads path
// is currently selected even if a custom capture path is set.
constexpr char kUsesDefaultCapturePathPrefName[] =
    "ash.capture_mode.uses_default_capture_path";

constexpr char kShareToYouTubeURL[] = "https://youtube.com/upload";

// The name of a boolean pref that determines whether we can show the demo tools
// user nudge. When this pref is false, it means that we showed the nudge at
// some point and the user interacted with the capture mode session UI in such a
// way that the nudge no longer needs to be displayed again.
constexpr char kCanShowDemoToolsNudge[] =
    "ash.capture_mode.can_show_demo_tools_nudge";

// An invalid IDS value used as a placeholder to not show a message in a
// notification.
constexpr int kNoMessage = -1;

// The screenshot notification button index.
enum ScreenshotNotificationButtonIndex {
  kButtonEdit = 0,
  kButtonDelete,
};

// The video notification button index.
enum GameDashboardVideoNotificationButtonIndex {
  kButtonShareToYoutube = 0,
  kButtonDeleteGameVideo,
};
enum VideoNotificationButtonIndex {
  kButtonDeleteVideo = 0,
};

// Returns the file extension for the given `recording_type` and the current
// capture `source`.
std::string GetVideoExtension(RecordingType recording_type,
                              CaptureModeSource source) {
  switch (recording_type) {
    case RecordingType::kGif:
      // Currently, we only support recording GIF for partial regions, so we
      // ignore the recording type if the source is fullscreen or window, and
      // force recording in webm.
      return source == CaptureModeSource::kRegion ? "gif" : "webm";
    case RecordingType::kWebM:
      return "webm";
  }
}

// Returns true if the given `video_file_path` is of a type that supports audio
// recording (e.g. ".webm" files).
bool SupportsAudioRecording(const base::FilePath& video_file_path) {
  return video_file_path.MatchesExtension(".webm");
}

bool IsVideoFileExtensionSupported(const base::FilePath& video_file_path) {
  for (const auto* const extension : {".webm", ".gif"}) {
    if (video_file_path.MatchesExtension(extension)) {
      return true;
    }
  }
  return false;
}

// Selects a file path for captured files (image/video) from `current_path` and
// `fallback_path`. If `current_path` is valid, use `current_path`, otherwise
// use `fallback_path`.
base::FilePath SelectFilePathForCapturedFile(
    const base::FilePath& current_path,
    const base::FilePath& fallback_path) {
  // TODO(b/323146997): Revisit the behavior if enforced by policy.
  if (base::PathExists(current_path.DirName()))
    return current_path;
  DCHECK(base::PathExists(fallback_path.DirName()));
  return fallback_path;
}

// Writes the given `data` in a file with `path`. Returns true if saving
// succeeded, or false otherwise.
base::FilePath DoSaveFile(scoped_refptr<base::RefCountedMemory> data,
                          const base::FilePath& path) {
  DCHECK(data);
  DCHECK(data->size());
  if (!base::WriteFile(path, *data)) {
    LOG(ERROR) << "Failed to save file: " << path;
    return base::FilePath();
  }
  return path;
}

// Attempts to write the given `data` with the file path returned from
// `SelectAFilePathForCapturedFile`.
base::FilePath SaveFile(scoped_refptr<base::RefCountedMemory> data,
                        const base::FilePath& current_path,
                        const base::FilePath& fallback_path) {
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!current_path.empty());
  DCHECK(!fallback_path.empty());

  return DoSaveFile(data,
                    SelectFilePathForCapturedFile(current_path, fallback_path));
}

void DeleteFileAsync(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     const base::FilePath& path,
                     OnFileDeletedCallback callback) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::DeleteFile, path),
      callback ? base::BindOnce(std::move(callback), path)
               : base::BindOnce(
                     [](const base::FilePath& path, bool success) {
                       // TODO(afakhry): Show toast?
                       if (!success)
                         LOG(ERROR) << "Failed to delete the file: " << path;
                     },
                     path));
}

// Called when the "Share to YouTube" button is pressed to
// open the YouTube share video page.
void OnShareToYouTubeButtonPressed() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kShareToYouTubeURL),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

// Adds the given `notification` to the message center after it removes any
// existing notification that has the same ID.
void AddNotificationToMessageCenter(
    std::unique_ptr<message_center::Notification> notification) {
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

// Shows a Capture Mode related notification with the given parameters.
// |for_video_thumbnail| will be considered only if |optional_fields| contain
// an image to show in the notification as a thumbnail for what was captured.
void ShowNotification(
    const std::string& notification_id,
    int title_id,
    int message_id,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    message_center::SystemNotificationWarningLevel warning_level =
        message_center::SystemNotificationWarningLevel::NORMAL,
    const gfx::VectorIcon& notification_icon = kCaptureModeIcon,
    bool for_video_thumbnail = false) {
  const auto type = optional_fields.image.IsEmpty()
                        ? message_center::NOTIFICATION_TYPE_SIMPLE
                        : message_center::NOTIFICATION_TYPE_CUSTOM;
  const std::u16string message = message_id == kNoMessage
                                     ? std::u16string()
                                     : l10n_util::GetStringUTF16(message_id);
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          type, notification_id, l10n_util::GetStringUTF16(title_id), message,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kScreenCaptureNotifierId,
              NotificationCatalogName::kScreenCapture),
          optional_fields, delegate, notification_icon, warning_level);
  if (type == message_center::NOTIFICATION_TYPE_CUSTOM) {
    notification->set_custom_view_type(for_video_thumbnail
                                           ? kScreenRecordingNotificationType
                                           : kScreenShotNotificationType);
  }

  AddNotificationToMessageCenter(std::move(notification));
}

// Shows a notification informing the user that a Capture Mode operation has
// failed.
void ShowFailureNotification() {
  ShowNotification(kScreenCaptureStoppedNotificationId,
                   IDS_ASH_SCREEN_CAPTURE_FAILURE_TITLE,
                   IDS_ASH_SCREEN_CAPTURE_FAILURE_MESSAGE,
                   /*optional_fields=*/{}, /*delegate=*/nullptr);
}

// Shows a notification that indicates to the user that the GIF file is being
// processed and will be ready shortly.
void ShowGifProgressNotification() {
  message_center::RichNotificationData optional_fields;
  optional_fields.progress = -1;  // Infinite progress.
  optional_fields.never_timeout = true;
  AddNotificationToMessageCenter(CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_PROGRESS, kScreenCaptureNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_GIF_PROGRESS_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_GIF_PROGRESS_MESSAGE),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kScreenCaptureNotifierId,
                                 NotificationCatalogName::kScreenCapture),
      optional_fields, /*delegate=*/nullptr, kCaptureModeIcon,
      message_center::SystemNotificationWarningLevel::NORMAL));
}

// Returns the ID of the message or the title for the notification based on
// |allowance| and |for_title|.
int GetDisabledNotificationMessageId(CaptureAllowance allowance,
                                     bool for_title) {
  switch (allowance) {
    case CaptureAllowance::kDisallowedByPolicy:
      return for_title ? IDS_ASH_SCREEN_CAPTURE_POLICY_DISABLED_TITLE
                       : IDS_ASH_SCREEN_CAPTURE_POLICY_DISABLED_MESSAGE;
    case CaptureAllowance::kDisallowedByHdcp:
      return for_title ? IDS_ASH_SCREEN_CAPTURE_HDCP_STOPPED_TITLE
                       : IDS_ASH_SCREEN_CAPTURE_HDCP_BLOCKED_MESSAGE;
    case CaptureAllowance::kAllowed:
      NOTREACHED();
  }
}

// Shows a notification informing the user that Capture Mode operations are
// currently disabled. |allowance| identifies the reason why the operation is
// currently disabled.
void ShowDisabledNotification(CaptureAllowance allowance) {
  DCHECK(allowance != CaptureAllowance::kAllowed);
  ShowNotification(
      kScreenCaptureNotificationId,
      GetDisabledNotificationMessageId(allowance, /*for_title=*/true),
      GetDisabledNotificationMessageId(allowance, /*for_title=*/false),
      /*optional_fields=*/{}, /*delegate=*/nullptr,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
      allowance == CaptureAllowance::kDisallowedByHdcp
          ? kCaptureModeIcon
          : vector_icons::kBusinessIcon);
}

// Shows a notification informing the user that video recording was stopped due
// to a content-enforced protection.
void ShowVideoRecordingStoppedByHdcpNotification() {
  ShowNotification(
      kScreenCaptureStoppedNotificationId,
      IDS_ASH_SCREEN_CAPTURE_HDCP_STOPPED_TITLE,
      IDS_ASH_SCREEN_CAPTURE_HDCP_BLOCKED_MESSAGE,
      /*optional_fields=*/{}, /*delegate=*/nullptr,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
      kCaptureModeIcon);
}

// Copies the bitmap representation of the given |image| to the clipboard.
void CopyImageToClipboard(const gfx::Image& image) {
  ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
      .WriteImage(image.AsBitmap());
}

// Emits UMA samples for the |status| of the recording as reported by the
// recording service.
void EmitServiceRecordingStatus(recording::mojom::RecordingStatus status) {
  using recording::mojom::RecordingStatus;
  switch (status) {
    case RecordingStatus::kSuccess:
      // We emit no samples for success status, as in this case the recording
      // was ended normally by the client, and the end reason for that is
      // emitted else where.
      break;
    case RecordingStatus::kServiceClosing:
      RecordEndRecordingReason(EndRecordingReason::kServiceClosing);
      break;
    case RecordingStatus::kVizVideoCapturerDisconnected:
      RecordEndRecordingReason(
          EndRecordingReason::kVizVideoCaptureDisconnected);
      break;
    case RecordingStatus::kAudioEncoderInitializationFailure:
      RecordEndRecordingReason(
          EndRecordingReason::kAudioEncoderInitializationFailure);
      break;
    case RecordingStatus::kVideoEncoderInitializationFailure:
      RecordEndRecordingReason(
          EndRecordingReason::kVideoEncoderInitializationFailure);
      break;
    case RecordingStatus::kAudioEncodingError:
      RecordEndRecordingReason(EndRecordingReason::kAudioEncodingError);
      break;
    case RecordingStatus::kVideoEncodingError:
      RecordEndRecordingReason(EndRecordingReason::kVideoEncodingError);
      break;
    case RecordingStatus::kIoError:
      RecordEndRecordingReason(EndRecordingReason::kFileIoError);
      break;
    case RecordingStatus::kLowDiskSpace:
      RecordEndRecordingReason(EndRecordingReason::kLowDiskSpace);
      break;
    case RecordingStatus::kLowDriveFsQuota:
      RecordEndRecordingReason(EndRecordingReason::kLowDriveFsQuota);
      break;
    case RecordingStatus::kVideoEncoderReconfigurationFailure:
      RecordEndRecordingReason(
          EndRecordingReason::kVideoEncoderReconfigurationFailure);
      break;
  }
}

PrefService* GetActiveUserPrefService() {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(pref_service);
  return pref_service;
}

base::FilePath GetTempDir() {
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir))
    LOG(ERROR) << "Failed to find the temporary directory.";
  return temp_dir;
}

int GetNotificationTitleIdForFile(const base::FilePath& file_path) {
  if (file_path.MatchesExtension(".gif")) {
    return IDS_ASH_SCREEN_CAPTURE_GIF_RECORDING_TITLE;
  }

  if (file_path.MatchesExtension(".webm")) {
    return IDS_ASH_SCREEN_CAPTURE_RECORDING_TITLE;
  }

  DCHECK(file_path.MatchesExtension(".png"));
  return IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_TITLE;
}

// Returns the size of the file at the given `file_path` in KBs. Returns -1 when
// a failure occurs.
int GetFileSizeInKB(const base::FilePath& file_path) {
  int64_t size_in_bytes = 0;
  if (!base::GetFileSize(file_path, &size_in_bytes)) {
    return -1;
  }
  // Convert the value to KBs.
  return size_in_bytes / 1024;
}

// Creates a new `CaptureModeSession` based on the given `session_type`. Can be
// a regular session or a null session.
std::unique_ptr<BaseCaptureModeSession> CreateSession(
    SessionType session_type,
    CaptureModeController* controller,
    CaptureModeBehavior* active_behavior) {
  switch (session_type) {
    case SessionType::kReal:
      return std::make_unique<CaptureModeSession>(controller, active_behavior);

    case SessionType::kNull:
      return std::make_unique<NullCaptureModeSession>(controller,
                                                      active_behavior);
  }

  NOTREACHED();
}

// Hides the cursor to avoid capturing it in the screenshot. Returns true if the
// cursor is already locked, in which case there is no need to unlock it after.
bool MaybeLockCursor() {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  bool was_cursor_originally_blocked = cursor_manager->IsCursorLocked();
  if (!was_cursor_originally_blocked) {
    cursor_manager->HideCursor();
    cursor_manager->LockCursor();
  }
  return was_cursor_originally_blocked;
}

// Re-shows the cursor after the image capture, if the cursor was locked by us.
void MaybeUnlockCursor(bool was_cursor_originally_blocked) {
  if (!was_cursor_originally_blocked) {
    auto* cursor_manager = Shell::Get()->cursor_manager();
    if (!display::Screen::GetScreen()->InTabletMode()) {
      cursor_manager->ShowCursor();
    }
    cursor_manager->UnlockCursor();
  }
}

// Given a `CaptureModeEntryType`, returns the `BehaviorType` associated with
// it, or default behavior if none exists.
BehaviorType ToBehaviorType(CaptureModeEntryType entry_type) {
  switch (entry_type) {
    case CaptureModeEntryType::kProjector:
      return BehaviorType::kProjector;
    case CaptureModeEntryType::kGameDashboard:
      CHECK(features::IsGameDashboardEnabled());
      return BehaviorType::kGameDashboard;
    case CaptureModeEntryType::kSunfish:
      CHECK(features::IsSunfishFeatureEnabled());
      return BehaviorType::kSunfish;
    default:
      return BehaviorType::kDefault;
  }
}

}  // namespace

CaptureModeController::CaptureModeController(
    std::unique_ptr<CaptureModeDelegate> delegate)
    : delegate_(std::move(delegate)),
      camera_controller_(
          std::make_unique<CaptureModeCameraController>(delegate_.get())),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // A task priority of BEST_EFFORT is good enough for this runner,
          // since it's used for blocking file IO such as saving the
          // screenshots.
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      num_consecutive_screenshots_scheduler_(
          FROM_HERE,
          kConsecutiveScreenshotThreshold,
          this,
          &CaptureModeController::RecordAndResetConsecutiveScreenshots),
      education_controller_(
          std::make_unique<CaptureModeEducationController>()) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // Schedule recording of the number of screenshots taken per day.
  num_screenshots_taken_in_last_day_scheduler_.Start(
      FROM_HERE, base::Days(1),
      base::BindRepeating(
          &CaptureModeController::RecordAndResetScreenshotsTakenInLastDay,
          weak_ptr_factory_.GetWeakPtr()));

  // Schedule recording of the number of screenshots taken per week.
  num_screenshots_taken_in_last_week_scheduler_.Start(
      FROM_HERE, base::Days(7),
      base::BindRepeating(
          &CaptureModeController::RecordAndResetScreenshotsTakenInLastWeek,
          weak_ptr_factory_.GetWeakPtr()));

  DCHECK(!MessageViewFactory::HasCustomNotificationViewFactory(
      kScreenShotNotificationType));
  DCHECK(!MessageViewFactory::HasCustomNotificationViewFactory(
      kScreenRecordingNotificationType));

  MessageViewFactory::SetCustomNotificationViewFactory(
      kScreenShotNotificationType,
      base::BindRepeating(&CaptureModeAshNotificationView::CreateForImage));
  MessageViewFactory::SetCustomNotificationViewFactory(
      kScreenRecordingNotificationType,
      base::BindRepeating(&CaptureModeAshNotificationView::CreateForVideo));

  Shell::Get()->session_controller()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

CaptureModeController::~CaptureModeController() {
  if (IsActive()) {
    // If for some reason a session was started after `OnChromeTerminating()`
    // was called (see https://crbug.com/1350711), we must explicitly shut it
    // down, so that it can stop observing the things it observes.
    Stop();
  }

  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  // Remove the custom notification view factories.
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kScreenShotNotificationType);
  MessageViewFactory::ClearCustomNotificationViewFactory(
      kScreenRecordingNotificationType);

  if (features::IsVideoConferenceEnabled()) {
    delegate_->UnregisterVideoConferenceManagerClient(vc_client_id_);
  }

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CaptureModeController* CaptureModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void CaptureModeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(kCustomCapturePathPrefName,
                                 /*default_value=*/base::FilePath());
  registry->RegisterBooleanPref(kUsesDefaultCapturePathPrefName,
                                /*default_value=*/false);
  registry->RegisterBooleanPref(kCanShowDemoToolsNudge,
                                /*default_value=*/true);
}

bool CaptureModeController::IsActive() const {
  return capture_mode_session_ && !capture_mode_session_->is_shutting_down();
}

AudioRecordingMode CaptureModeController::GetEffectiveAudioRecordingMode()
    const {
  return IsAudioCaptureDisabledByPolicy() ? AudioRecordingMode::kOff
                                          : audio_recording_mode_;
}

bool CaptureModeController::IsAudioCaptureDisabledByPolicy() const {
  return delegate_->IsAudioCaptureDisabledByPolicy();
}

bool CaptureModeController::IsCustomFolderManagedByPolicy() const {
  return delegate_->GetPolicyCapturePath().enforcement ==
         CaptureModeDelegate::CapturePathEnforcement::kManaged;
}

bool CaptureModeController::IsAudioRecordingInProgress() const {
  return video_recording_watcher_ &&
         !video_recording_watcher_->is_shutting_down() &&
         video_recording_watcher_->is_recording_audio();
}

bool CaptureModeController::IsShowingCameraPreview() const {
  return !!camera_controller_->camera_preview_widget();
}

bool CaptureModeController::SupportsBehaviorChange(
    CaptureModeEntryType new_entry_type) const {
  // If no active session is running, we always support a new behavior type.
  if (!IsActive()) {
    return true;
  }
  // We only allow switching between sunfish and non-sunfish behaviors.
  return capture_mode_session_->active_behavior()->behavior_type() ==
             BehaviorType::kSunfish ||
         new_entry_type == CaptureModeEntryType::kSunfish;
}

void CaptureModeController::SetSource(CaptureModeSource source) {
  if (source == source_)
    return;

  source_ = source;
  if (IsActive()) {
    capture_mode_session_->OnCaptureSourceChanged(source_);
  }
}

void CaptureModeController::SetType(CaptureModeType type) {
  if (!can_start_new_recording() && type == CaptureModeType::kVideo) {
    // Overwrite video capture types to image, as we can't have more than one
    // recording at a time.
    type = CaptureModeType::kImage;
  }

  if (type == type_)
    return;

  type_ = type;
  if (IsActive()) {
    capture_mode_session_->OnCaptureTypeChanged(type_);
  }
}

void CaptureModeController::SetRecordingType(RecordingType recording_type) {
  if (recording_type == recording_type_)
    return;

  recording_type_ = recording_type;
  if (IsActive()) {
    capture_mode_session_->OnRecordingTypeChanged();
  }
}

void CaptureModeController::SetAudioRecordingMode(AudioRecordingMode mode) {
  audio_recording_mode_ = mode;

  if (IsActive()) {
    capture_mode_session_->OnAudioRecordingModeChanged();
  }
}

void CaptureModeController::EnableDemoTools(bool enable) {
  enable_demo_tools_ = enable;

  if (IsActive()) {
    capture_mode_session_->OnDemoToolsSettingsChanged();
  }
}

void CaptureModeController::Start(CaptureModeEntryType entry_type,
                                  OnSessionStartAttemptCallback callback) {
  StartInternal(SessionType::kReal, entry_type, std::move(callback));
}

void CaptureModeController::StartForGameDashboard(aura::Window* game_window) {
  CHECK(GameDashboardController::IsGameWindow(game_window));
  CaptureModeBehavior* behavior = GetBehavior(BehaviorType::kGameDashboard);
  behavior->SetPreSelectedWindow(game_window);
  StartInternal(SessionType::kReal, CaptureModeEntryType::kGameDashboard);
}

void CaptureModeController::StartRecordingInstantlyForGameDashboard(
    aura::Window* game_window) {
  CHECK(GameDashboardController::IsGameWindow(game_window));
  CaptureModeBehavior* behavior = GetBehavior(BehaviorType::kGameDashboard);
  behavior->SetPreSelectedWindow(game_window);
  StartInternal(SessionType::kNull, CaptureModeEntryType::kGameDashboard,
                base::BindOnce([](bool success) {
                  if (success) {
                    // Session initialization was successful.
                    CaptureModeController::Get()->PerformCapture();
                  }
                }));
}

void CaptureModeController::StartSunfishSession() {
  DCHECK(features::IsSunfishFeatureEnabled());
  // TODO(b/357658506): Determine whether to close the results panel.
  StartInternal(SessionType::kReal, CaptureModeEntryType::kSunfish);
}

void CaptureModeController::Stop() {
  CHECK(IsActive());
  capture_mode_session_->ReportSessionHistograms();
  capture_mode_session_->Shutdown();
  capture_mode_session_.reset();

  delegate_->OnSessionStateChanged(/*started=*/false);
}

void CaptureModeController::NotifyRecordingStartAborted() {
  for (auto& observer : observers_) {
    observer.OnRecordingStartAborted();
  }
}

void CaptureModeController::SetUserCaptureRegion(const gfx::Rect& region,
                                                 bool by_user) {
  user_capture_region_ = region;
  if (!user_capture_region_.IsEmpty() && by_user)
    last_capture_region_update_time_ = base::TimeTicks::Now();

  if (!is_recording_in_progress() && source_ == CaptureModeSource::kRegion)
    camera_controller_->MaybeReparentPreviewWidget();
}

bool CaptureModeController::CanShowUserNudge() const {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller->IsActiveUserSessionStarted());

  std::optional<user_manager::UserType> user_type =
      session_controller->GetUserType();
  // This can only be called while a user is logged in, so `user_type` should
  // never be empty.
  DCHECK(user_type);
  switch (*user_type) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      // We only allow regular and child accounts to see the nudge.
      break;
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return false;
  }

  auto* pref_service = session_controller->GetActivePrefService();
  DCHECK(pref_service);
  return pref_service->GetBoolean(kCanShowDemoToolsNudge);
}

void CaptureModeController::DisableUserNudgeForever() {
  GetActiveUserPrefService()->SetBoolean(kCanShowDemoToolsNudge, false);
}

void CaptureModeController::SetUsesDefaultCaptureFolder(bool value) {
  DCHECK(!IsCustomFolderManagedByPolicy());
  GetActiveUserPrefService()->SetBoolean(kUsesDefaultCapturePathPrefName,
                                         value);

  if (IsActive())
    capture_mode_session_->OnDefaultCaptureFolderSelectionChanged();
}

void CaptureModeController::SetCustomCaptureFolder(const base::FilePath& path) {
  DCHECK(!IsCustomFolderManagedByPolicy());
  auto* pref_service = GetActiveUserPrefService();
  pref_service->SetFilePath(kCustomCapturePathPrefName, path);

  // When this function is called, it means the user is switching back to the
  // custom capture folder, and we need to reset the setting to force using the
  // default downloads folder.
  pref_service->SetBoolean(kUsesDefaultCapturePathPrefName, false);

  if (IsActive())
    capture_mode_session_->OnCaptureFolderMayHaveChanged();
}

base::FilePath CaptureModeController::GetCustomCaptureFolder() const {
  base::FilePath custom_path =
      GetActiveUserPrefService()->GetFilePath(kCustomCapturePathPrefName);
  const auto policy_path = delegate_->GetPolicyCapturePath();
  // If admin forced or recommended and there is no user chosen value - use it.
  if (policy_path.enforcement ==
          CaptureModeDelegate::CapturePathEnforcement::kManaged ||
      (custom_path.empty() &&
       policy_path.enforcement ==
           CaptureModeDelegate::CapturePathEnforcement::kRecommended)) {
    custom_path = policy_path.path;
  }
  return custom_path != delegate_->GetUserDefaultDownloadsFolder()
             ? custom_path
             : base::FilePath();
}

CaptureModeController::CaptureFolder
CaptureModeController::GetCurrentCaptureFolder() const {
  auto* session_controller = Shell::Get()->session_controller();
  if (!session_controller->IsActiveUserSessionStarted())
    return {GetTempDir(), /*is_default_downloads_folder=*/false};

  auto* pref_service = session_controller->GetActivePrefService();
  const auto default_downloads_folder =
      delegate_->GetUserDefaultDownloadsFolder();
  const auto policy_path = delegate_->GetPolicyCapturePath();
  // If admin forced - use it.
  if (policy_path.enforcement ==
      CaptureModeDelegate::CapturePathEnforcement::kManaged) {
    return {policy_path.path,
            /*is_default_downloads_folder=*/policy_path.path ==
                default_downloads_folder};
  }
  // Otherwise use user chosen custom one, if present.
  if (pref_service &&
      !pref_service->GetBoolean(kUsesDefaultCapturePathPrefName)) {
    const auto custom_path =
        pref_service->GetFilePath(kCustomCapturePathPrefName);
    if (!custom_path.empty()) {
      return {custom_path,
              /*is_default_downloads_folder=*/custom_path ==
                  default_downloads_folder};
    }
  }
  // Otherwise use the recommended by admin.
  if (policy_path.enforcement ==
      CaptureModeDelegate::CapturePathEnforcement::kRecommended) {
    return {policy_path.path,
            /*is_default_downloads_folder=*/policy_path.path ==
                default_downloads_folder};
  }

  // By default - downloads folder.
  return {default_downloads_folder,
          /*is_default_downloads_folder=*/true};
}

void CaptureModeController::CaptureScreenshotsOfAllDisplays() {
  CaptureInstantScreenshot(
      CaptureModeEntryType::kCaptureAllDisplays, CaptureModeSource::kFullscreen,
      base::BindOnce(&CaptureModeController::PerformScreenshotsOfAllDisplays,
                     weak_ptr_factory_.GetWeakPtr(), BehaviorType::kDefault),
      BehaviorType::kDefault);
}

void CaptureModeController::CaptureScreenshotOfGivenWindow(
    aura::Window* given_window) {
  CaptureInstantScreenshot(
      CaptureModeEntryType::kCaptureGivenWindow, CaptureModeSource::kWindow,
      base::BindOnce(&CaptureModeController::PerformScreenshotOfGivenWindow,
                     weak_ptr_factory_.GetWeakPtr(), given_window,
                     BehaviorType::kGameDashboard),
      BehaviorType::kGameDashboard);
}

void CaptureModeController::PerformCapture() {
  DCHECK(IsActive());

  if (pending_dlp_check_)
    return;

  const std::optional<CaptureParams> capture_params = GetCaptureParams();
  if (!capture_params)
    return;

  DCHECK(!pending_dlp_check_);
  pending_dlp_check_ = true;
  capture_mode_session_->OnWaitingForDlpConfirmationStarted();
  capture_mode_session_->MaybeDismissUserNudgeForever();
  delegate_->CheckCaptureOperationRestrictionByDlp(
      capture_params->window, capture_params->bounds,
      base::BindOnce(
          &CaptureModeController::OnDlpRestrictionCheckedAtPerformingCapture,
          weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeController::PerformImageSearch() {
  DCHECK_EQ(capture_mode_session_->active_behavior()->behavior_type(),
            BehaviorType::kSunfish);
  DCHECK(delegate_->IsCaptureAllowedByPolicy());

  const std::optional<CaptureParams> capture_params = GetCaptureParams();
  CHECK(capture_params);

  const bool was_cursor_originally_blocked = MaybeLockCursor();

  // Capture the image for search. We use JPEG bytes for low file size and fast
  // compression speed.
  ui::GrabWindowSnapshotAsJPEG(
      capture_params->window, capture_params->bounds,
      base::BindOnce(&CaptureModeController::OnImageCapturedForSearch,
                     weak_ptr_factory_.GetWeakPtr(),
                     was_cursor_originally_blocked));

  delegate_->OnCaptureImageAttempted(capture_params->window,
                                     capture_params->bounds);
}

void CaptureModeController::EndVideoRecording(EndRecordingReason reason) {
  if (!is_recording_in_progress()) {
    // A user may click on the stop recording button multiple times while still
    // in the process of hiding. See http://b/270625738.
    return;
  }

  RecordEndRecordingReason(reason);
  recording_service_remote_->StopRecording();
  TerminateRecordingUiElements();
}

void CaptureModeController::CheckFolderAvailability(
    const base::FilePath& folder,
    base::OnceCallback<void(bool available)> callback) {
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, folder),
      std::move(callback));
}

void CaptureModeController::SetWindowProtectionMask(aura::Window* window,
                                                    uint32_t protection_mask) {
  if (protection_mask == display::CONTENT_PROTECTION_METHOD_NONE)
    protected_windows_.erase(window);
  else
    protected_windows_[window] = protection_mask;

  RefreshContentProtection();
}

void CaptureModeController::RefreshContentProtection() {
  if (!is_recording_in_progress())
    return;

  DCHECK(video_recording_watcher_);
  if (ShouldBlockRecordingForContentProtection(
          video_recording_watcher_->window_being_recorded())) {
    // HDCP violation is also considered a failure, and we're going to terminate
    // the service immediately so as not to record any further frames.
    RecordEndRecordingReason(EndRecordingReason::kHdcpInterruption);
    FinalizeRecording(/*success=*/false, gfx::ImageSkia());
    ShowVideoRecordingStoppedByHdcpNotification();
  }
}

bool CaptureModeController::IsRootDriveFsPath(
    const base::FilePath& path) const {
  base::FilePath mounted_path;
  if (delegate_->GetDriveFsMountPointPath(&mounted_path)) {
    if (path == mounted_path.Append("root"))
      return true;
  }
  return false;
}

bool CaptureModeController::IsAndroidFilesPath(
    const base::FilePath& path) const {
  return path == delegate_->GetAndroidFilesPath();
}

bool CaptureModeController::IsLinuxFilesPath(const base::FilePath& path) const {
  return path == delegate_->GetLinuxFilesPath();
}

bool CaptureModeController::IsRootOneDriveFilesPath(
    const base::FilePath& path) const {
  return path == delegate_->GetOneDriveMountPointPath();
}

std::unique_ptr<AshWebView> CaptureModeController::CreateSearchResultsView()
    const {
  return delegate_->CreateSearchResultsView();
}

aura::Window* CaptureModeController::GetOnCaptureSurfaceWidgetParentWindow()
    const {
  // Trying to get camera preview's parent from `video_recording_watcher_` first
  // if a video recording is in progress. As a capture session can be started
  // with `kImage` type while recording, and we should get the parent of the
  // camera preview with the settings inside VideoRecordingWatcher in this case,
  // e.g, CaptureModeSource for taking the video.
  if (is_recording_in_progress())
    return video_recording_watcher_->GetOnCaptureSurfaceWidgetParentWindow();

  if (IsActive())
    return capture_mode_session_->GetOnCaptureSurfaceWidgetParentWindow();

  return nullptr;
}

gfx::Rect CaptureModeController::GetCaptureSurfaceConfineBounds() const {
  // Getting the bounds from `video_recording_watcher_` first if a video
  // recording is in progress. As a capture session can be started with `kImage`
  // type while recording, and we should get the bounds with the settings inside
  // VideoRecordingWatcher in this case, e.g, user-selected region.
  if (is_recording_in_progress())
    return video_recording_watcher_->GetCaptureSurfaceConfineBounds();

  if (IsActive())
    return capture_mode_session_->GetCaptureSurfaceConfineBounds();

  return gfx::Rect();
}

std::vector<aura::Window*>
CaptureModeController::GetWindowsForCollisionAvoidance() const {
  std::vector<aura::Window*> windows_to_be_avoided;
  if (IsActive()) {
    const auto* capture_bar_widget =
        capture_mode_session_->GetCaptureModeBarWidget();
    CHECK(capture_bar_widget);
    auto* capture_bar_window = capture_bar_widget->GetNativeWindow();
    windows_to_be_avoided.push_back(capture_bar_window);
  }

  auto* camera_preview_widget = camera_controller_->camera_preview_widget();
  if (camera_preview_widget && camera_preview_widget->IsVisible()) {
    windows_to_be_avoided.push_back(camera_preview_widget->GetNativeView());
  }

  if (video_recording_watcher_ &&
      !video_recording_watcher_->is_shutting_down() &&
      video_recording_watcher_->recording_source() !=
          CaptureModeSource::kWindow) {
    if (auto* key_combo_widget =
            video_recording_watcher_->GetKeyComboWidgetIfVisible()) {
      windows_to_be_avoided.push_back(key_combo_widget->GetNativeWindow());
    }
  }

  return windows_to_be_avoided;
}

void CaptureModeController::MaybeUpdateVcPanel() {
  if (!features::IsVideoConferenceEnabled()) {
    return;
  }

  const bool is_camera_used = IsShowingCameraPreview();
  const bool is_recording_audio = IsAudioRecordingInProgress();
  const bool has_media_app = is_camera_used || is_recording_audio;

  delegate_->UpdateVideoConferenceManager(
      crosapi::mojom::VideoConferenceMediaUsageStatus::New(
          /*client_id=*/vc_client_id_,
          /*has_media_app=*/has_media_app,
          /*has_camera_permission=*/has_media_app,
          /*has_microphone_permission=*/has_media_app,
          /*is_capturing_camera=*/is_camera_used,
          /*is_capturing_microphone=*/is_recording_audio,
          /*is_capturing_screen=*/false));

  // If the camera is being recorded while disabled (e.g. privacy switch is
  // turned on), or the microphone is being recorded while mic input is muted,
  // we need to notify the user through the video conference manager.
  if (is_camera_used && is_camera_muted_) {
    delegate_->NotifyDeviceUsedWhileDisabled(
        crosapi::mojom::VideoConferenceMediaDevice::kCamera);
  }

  if (is_recording_audio && is_microphone_muted_) {
    delegate_->NotifyDeviceUsedWhileDisabled(
        crosapi::mojom::VideoConferenceMediaDevice::kMicrophone);
  }
}

void CaptureModeController::CheckScreenCaptureDlpRestrictions(
    OnCaptureModeDlpRestrictionChecked callback) {
  delegate_->CheckCaptureModeInitRestrictionByDlp(std::move(callback));
}

bool CaptureModeController::ShouldAllowAnnotating() const {
  return is_recording_in_progress() && IsAnnotatingSupported();
}

bool CaptureModeController::IsAnnotatingSupported() const {
  return video_recording_watcher_ &&
         video_recording_watcher_->active_behavior()
             ->ShouldCreateAnnotationsOverlayController();
}

void CaptureModeController::OnRecordingEnded(
    recording::mojom::RecordingStatus status,
    const gfx::ImageSkia& thumbnail) {
  low_disk_space_threshold_reached_ =
      status == recording::mojom::RecordingStatus::kLowDiskSpace ||
      status == recording::mojom::RecordingStatus::kLowDriveFsQuota;
  EmitServiceRecordingStatus(status);
  FinalizeRecording(status == recording::mojom::RecordingStatus::kSuccess,
                    thumbnail);
}

void CaptureModeController::GetDriveFsFreeSpaceBytes(
    GetDriveFsFreeSpaceBytesCallback callback) {
  delegate_->GetDriveFsFreeSpaceBytes(std::move(callback));
}

void CaptureModeController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  EndSessionOrRecording(EndRecordingReason::kActiveUserChange);

  camera_controller_->OnActiveUserSessionChanged();

  // Remove the previous notification when switching to another user.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(kScreenCaptureNotificationId,
                                     /*by_user=*/false);
}

void CaptureModeController::OnFirstSessionStarted() {
  if (features::IsVideoConferenceEnabled()) {
    auto* vc_tray_controller = VideoConferenceTrayController::Get();
    is_camera_muted_ = vc_tray_controller->GetCameraMuted();
    is_microphone_muted_ = vc_tray_controller->GetMicrophoneMuted();
    delegate_->RegisterVideoConferenceManagerClient(this, vc_client_id_);
  }
}

void CaptureModeController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    EndSessionOrRecording(EndRecordingReason::kSessionBlocked);
}

void CaptureModeController::OnChromeTerminating() {
  // Order here matters. We may shutdown while a session with a camera is active
  // before recording starts, we need to inform the camera controller first to
  // destroy the camera preview first.
  camera_controller_->OnShuttingDown();
  EndSessionOrRecording(EndRecordingReason::kShuttingDown);
}

void CaptureModeController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  EndSessionOrRecording(EndRecordingReason::kImminentSuspend);
}

void CaptureModeController::GetMediaApps(GetMediaAppsCallback callback) {
  std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr> apps;

  if (is_recording_in_progress()) {
    apps.push_back(crosapi::mojom::VideoConferenceMediaAppInfo::New(
        /*id=*/capture_mode_media_app_id_,
        /*last_activity_time=*/base::Time::Now(),
        /*is_capturing_camera=*/IsShowingCameraPreview(),
        /*is_capturing_microphone=*/IsAudioRecordingInProgress(),
        /*is_capturing_screen=*/false,
        /*title=*/
        l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
        /*url=*/std::nullopt,
        /*app_type=*/crosapi::mojom::VideoConferenceAppType::kAshCaptureMode));
  }

  std::move(callback).Run(std::move(apps));
}

void CaptureModeController::ReturnToApp(const base::UnguessableToken& token,
                                        ReturnToAppCallback callback) {
  // The return-to-app feature is only available when recording an app window
  // (rather than the fullscreen or region). In this case, it simply "returns"
  // to that window by activating it.
  bool success = false;
  if (video_recording_watcher_ &&
      !video_recording_watcher_->is_shutting_down() &&
      video_recording_watcher_->recording_source() ==
          CaptureModeSource::kWindow) {
    wm::ActivateWindow(video_recording_watcher_->window_being_recorded());
    success = true;
  }
  std::move(callback).Run(success);
}

void CaptureModeController::SetSystemMediaDeviceStatus(
    crosapi::mojom::VideoConferenceMediaDevice device,
    bool disabled,
    SetSystemMediaDeviceStatusCallback callback) {
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      is_camera_muted_ = disabled;
      std::move(callback).Run(true);
      return;
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      is_microphone_muted_ = disabled;
      std::move(callback).Run(true);
      return;
    case crosapi::mojom::VideoConferenceMediaDevice::kUnusedDefault:
      std::move(callback).Run(false);
      return;
  }
}

void CaptureModeController::StopAllScreenShare() {
  // Our screen recordings are not considered screen shares, and we already have
  // the stop recording button, so this does nothing.
}

void CaptureModeController::StartVideoRecordingImmediatelyForTesting() {
  DCHECK(IsActive());
  DCHECK_EQ(type_, CaptureModeType::kVideo);
  OnVideoRecordCountDownFinished();
}

void CaptureModeController::AddObserver(CaptureModeObserver* observer) {
  observers_.AddObserver(observer);
}

void CaptureModeController::RemoveObserver(CaptureModeObserver* observer) {
  observers_.RemoveObserver(observer);
}

void CaptureModeController::StartInternal(
    SessionType session_type,
    CaptureModeEntryType entry_type,
    OnSessionStartAttemptCallback callback) {
  // To be invoked at the exit of this function or
  // `OnDlpRestrictionCheckedAtSessionInit()`.
  base::ScopedClosureRunner deferred_runner(base::BindOnce(
      [](base::WeakPtr<CaptureModeController> controller,
         OnSessionStartAttemptCallback callback, bool was_active) {
        std::move(callback).Run(!was_active && controller &&
                                controller->IsActive());
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), IsActive()));

  education_controller_->CloseAllEducationNudgesAndTutorials();

  if (pending_dlp_check_) {
    return;
  }

  if (capture_mode_session_) {
    if (capture_mode_session_->is_shutting_down()) {
      return;
    }

    // If the active behavior type has not changed, no need to shutdown and
    // restart.
    if (capture_mode_session_->active_behavior()->behavior_type() ==
        ToBehaviorType(entry_type)) {
      return;
    }

    // Else if the behavior type has changed, shut down and restart with the new
    // behavior type.
    Stop();
  }

  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    return;
  }

  pending_dlp_check_ = true;
  delegate_->CheckCaptureModeInitRestrictionByDlp(base::BindOnce(
      &CaptureModeController::OnDlpRestrictionCheckedAtSessionInit,
      weak_ptr_factory_.GetWeakPtr(), session_type, entry_type,
      deferred_runner.Release()));
}

void CaptureModeController::PushNewRootSizeToRecordingService(
    const gfx::Size& root_size,
    float device_scale_factor) {
  DCHECK(is_recording_in_progress());
  DCHECK(video_recording_watcher_);
  DCHECK(recording_service_remote_);

  recording_service_remote_->OnFrameSinkSizeChanged(root_size,
                                                    device_scale_factor);
}

void CaptureModeController::OnRecordedWindowChangingRoot(
    aura::Window* window,
    aura::Window* new_root) {
  DCHECK(is_recording_in_progress());
  DCHECK(video_recording_watcher_);
  DCHECK_EQ(window, video_recording_watcher_->window_being_recorded());
  DCHECK(recording_service_remote_);
  DCHECK(new_root);

  // When a window being recorded changes displays either due to a display
  // getting disconnected, or moved by the user, the stop-recording button
  // should follow that window to that display.
  capture_mode_util::SetStopRecordingButtonVisibility(window->GetRootWindow(),
                                                      false);
  capture_mode_util::SetStopRecordingButtonVisibility(new_root, true);

  for (auto& observer : observers_) {
    observer.OnRecordedWindowChangingRoot(new_root);
  }

  recording_service_remote_->OnRecordedWindowChangingRoot(
      new_root->GetFrameSinkId(), new_root->GetBoundsInRootWindow().size(),
      new_root->GetHost()->device_scale_factor());
}

void CaptureModeController::OnRecordedWindowSizeChanged(
    const gfx::Size& new_size) {
  DCHECK(is_recording_in_progress());
  DCHECK(video_recording_watcher_);
  DCHECK(recording_service_remote_);

  recording_service_remote_->OnRecordedWindowSizeChanged(new_size);
}

bool CaptureModeController::ShouldBlockRecordingForContentProtection(
    aura::Window* window_being_recorded) const {
  DCHECK(window_being_recorded);

  // The protected window can be a descendant of the window being recorded, for
  // examples:
  //   - When recording a fullscreen or partial region of it, the
  //     |window_being_recorded| in this case is the root window, and a
  //     protected window on this root will be a descendant.
  //   - When recording a browser window showing a page with protected content,
  //     the |window_being_recorded| in this case is the BrowserFrame, while the
  //     protected window will be the RenderWidgetHostViewAura, which is also a
  //     descendant.
  for (const auto& iter : protected_windows_) {
    if (window_being_recorded->Contains(iter.first))
      return true;
  }

  return false;
}

void CaptureModeController::EndSessionOrRecording(EndRecordingReason reason) {
  if (IsActive()) {
    // Suspend or user session changes can happen while the capture mode session
    // is active or after the three-second countdown had started but not
    // finished yet.
    Stop();
  }

  if (!is_recording_in_progress())
    return;

  if (reason == EndRecordingReason::kImminentSuspend ||
      reason == EndRecordingReason::kShuttingDown) {
    // If suspend or shutdown happen while recording is in progress, we consider
    // this a failure, and cut the recording immediately. The recording service
    // will flush any remaining buffered chunks in the muxer before it
    // terminates.
    RecordEndRecordingReason(reason);
    FinalizeRecording(/*success=*/false, gfx::ImageSkia());
    return;
  }

  EndVideoRecording(reason);
}

std::optional<CaptureModeController::CaptureParams>
CaptureModeController::GetCaptureParams() const {
  DCHECK(IsActive());

  aura::Window* window = nullptr;
  gfx::Rect bounds;
  switch (source_) {
    case CaptureModeSource::kFullscreen:
      window = capture_mode_session_->current_root();
      DCHECK(window);
      DCHECK(window->IsRootWindow());
      bounds = window->bounds();
      break;

    case CaptureModeSource::kWindow:
      window = capture_mode_session_->GetSelectedWindow();
      if (!window) {
        // TODO(afakhry): Consider showing a toast or a notification that no
        // window was selected.
        return std::nullopt;
      }
      // window->bounds() are in root coordinates, but we want to get the
      // capture area in |window|'s coordinates.
      bounds = gfx::Rect(window->bounds().size());
      break;

    case CaptureModeSource::kRegion:
      window = capture_mode_session_->current_root();
      DCHECK(window);
      DCHECK(window->IsRootWindow());
      if (user_capture_region_.IsEmpty()) {
        // TODO(afakhry): Consider showing a toast or a notification that no
        // region was selected.
        return std::nullopt;
      }
      // TODO(afakhry): Consider any special handling of display scale changes
      // while video recording is in progress.
      bounds = user_capture_region_;
      break;
  }

  DCHECK(window);

  return CaptureParams{window, bounds};
}

void CaptureModeController::LaunchRecordingServiceAndStartRecording(
    const CaptureParams& capture_params,
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay>
        cursor_overlay,
    AudioRecordingMode effective_audio_mode) {
  DCHECK(!recording_service_remote_.is_bound())
      << "Should not launch a new recording service while one is already "
         "running.";

  recording_service_remote_.reset();
  recording_service_client_receiver_.reset();
  drive_fs_quota_delegate_receiver_.reset();

  recording_service_remote_ = delegate_->LaunchRecordingService();
  recording_service_remote_.set_disconnect_handler(
      base::BindOnce(&CaptureModeController::OnRecordingServiceDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));

  // Prepare the pending remotes of the client, the video capturer, and the
  // audio stream factory.
  mojo::PendingRemote<recording::mojom::RecordingServiceClient> client =
      recording_service_client_receiver_.BindNewPipeAndPassRemote();
  mojo::Remote<viz::mojom::FrameSinkVideoCapturer> video_capturer_remote;
  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->CreateVideoCapturer(video_capturer_remote.BindNewPipeAndPassReceiver());

  // The overlay is to be rendered on top of the video frames.
  constexpr int kStackingIndex = 1;
  video_capturer_remote->CreateOverlay(kStackingIndex,
                                       std::move(cursor_overlay));

  // We bind the microphone and/or system audio stream factories only if their
  // corresponding audio recording modes are enabled. This is ok since the
  // `microphone_stream_factory` and `system_audio_stream_factory` parameters in
  // the recording service APIs are optional, and can be not bound.
  mojo::PendingRemote<media::mojom::AudioStreamFactory>
      microphone_stream_factory;
  if (effective_audio_mode == AudioRecordingMode::kMicrophone ||
      effective_audio_mode == AudioRecordingMode::kSystemAndMicrophone) {
    delegate_->BindAudioStreamFactory(
        microphone_stream_factory.InitWithNewPipeAndPassReceiver());
  }
  mojo::PendingRemote<media::mojom::AudioStreamFactory>
      system_audio_stream_factory;
  if (effective_audio_mode == AudioRecordingMode::kSystem ||
      effective_audio_mode == AudioRecordingMode::kSystemAndMicrophone) {
    delegate_->BindAudioStreamFactory(
        system_audio_stream_factory.InitWithNewPipeAndPassReceiver());
  }

  if (microphone_stream_factory || system_audio_stream_factory) {
    capture_mode_util::MaybeUpdateCaptureModePrivacyIndicators();
    MaybeUpdateVcPanel();
  }

  // Only act as a `DriveFsQuotaDelegate` for the recording service if the video
  // file will be saved to a location in DriveFS.
  mojo::PendingRemote<recording::mojom::DriveFsQuotaDelegate>
      drive_fs_quota_delegate;
  const auto file_location = GetSaveToOption(current_video_file_path_);
  if (file_location == CaptureModeSaveToLocation::kDrive ||
      file_location == CaptureModeSaveToLocation::kDriveFolder) {
    drive_fs_quota_delegate =
        drive_fs_quota_delegate_receiver_.BindNewPipeAndPassRemote();
  }

  auto* root_window = capture_params.window->GetRootWindow();
  const auto frame_sink_id = root_window->GetFrameSinkId();
  DCHECK(frame_sink_id.is_valid());
  const float device_scale_factor =
      root_window->GetHost()->device_scale_factor();
  const gfx::Size frame_sink_size_dip = root_window->bounds().size();

  const auto bounds = capture_params.bounds;
  switch (source_) {
    case CaptureModeSource::kFullscreen:
      recording_service_remote_->RecordFullscreen(
          std::move(client), video_capturer_remote.Unbind(),
          std::move(microphone_stream_factory),
          std::move(system_audio_stream_factory),
          std::move(drive_fs_quota_delegate), current_video_file_path_,
          frame_sink_id, frame_sink_size_dip, device_scale_factor);
      break;

    case CaptureModeSource::kWindow:
      // Non-root window are not capturable by the |FrameSinkVideoCapturer|
      // unless its layer tree is identified by a |viz::SubtreeCaptureId|.
      // The |VideoRecordingWatcher| that we create while recording is in
      // progress creates a request to mark that window as capturable.
      // See https://crbug.com/1143930 for more details.
      DCHECK(!capture_params.window->IsRootWindow());
      DCHECK(capture_params.window->subtree_capture_id().is_valid());

      recording_service_remote_->RecordWindow(
          std::move(client), video_capturer_remote.Unbind(),
          std::move(microphone_stream_factory),
          std::move(system_audio_stream_factory),
          std::move(drive_fs_quota_delegate), current_video_file_path_,
          frame_sink_id, frame_sink_size_dip, device_scale_factor,
          capture_params.window->subtree_capture_id(), bounds.size());
      break;

    case CaptureModeSource::kRegion:
      recording_service_remote_->RecordRegion(
          std::move(client), video_capturer_remote.Unbind(),
          std::move(microphone_stream_factory),
          std::move(system_audio_stream_factory),
          std::move(drive_fs_quota_delegate), current_video_file_path_,
          frame_sink_id, frame_sink_size_dip, device_scale_factor, bounds);
      break;
  }
}

void CaptureModeController::OnRecordingServiceDisconnected() {
  // TODO(afakhry): Consider what to do if the service crashes during an ongoing
  // video recording. Do we try to resume recording, or notify with failure?
  // For now, just end the recording.
  // Note that the service could disconnect between the time we ask it to
  // StopRecording(), and it calling us back with OnRecordingEnded(), so we call
  // FinalizeRecording() in all cases.
  RecordEndRecordingReason(EndRecordingReason::kRecordingServiceDisconnected);
  FinalizeRecording(/*success=*/false, gfx::ImageSkia());
}

void CaptureModeController::FinalizeRecording(bool success,
                                              const gfx::ImageSkia& thumbnail) {
  // If |success| is false, then recording has been force-terminated due to a
  // failure on the service side, or a disconnection to it. We need to terminate
  // the recording-related UI elements.
  if (!success) {
    // TODO(afakhry): Show user a failure message.
    TerminateRecordingUiElements();
  }

  // Resetting the service remote would terminate its process.
  recording_service_remote_.reset();
  delegate_->OnServiceRemoteReset();
  recording_service_client_receiver_.reset();
  drive_fs_quota_delegate_receiver_.reset();
  const CaptureModeBehavior* behavior =
      video_recording_watcher_->active_behavior();
  video_recording_watcher_.reset();
  capture_mode_util::MaybeUpdateCaptureModePrivacyIndicators();
  MaybeUpdateVcPanel();

  delegate_->StopObservingRestrictedContent(base::BindOnce(
      &CaptureModeController::OnDlpRestrictionCheckedAtVideoEnd,
      weak_ptr_factory_.GetWeakPtr(), thumbnail, success, behavior));
}

void CaptureModeController::TerminateRecordingUiElements() {
  if (!is_recording_in_progress())
    return;

  capture_mode_util::SetStopRecordingButtonVisibility(
      video_recording_watcher_->window_being_recorded()->GetRootWindow(),
      false);

  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_RECORDING_STOPPED);

  // Reset the camera selection if it was auto-selected in the
  // client-initiated capture mode session after video recording is completed
  // to avoid the camera selection settings of the normal capture mode session
  // being overridden by the client-initiated capture mode session.
  camera_controller_->MaybeRevertAutoCameraSelection();

  video_recording_watcher_->ShutDown();

  for (auto& observer : observers_) {
    observer.OnRecordingEnded();
  }

  // GIF files take a while to finalize and fully get written to disk. Therefore
  // we show a notification to the user to let them know that the file will be
  // ready shortly.
  if (current_video_file_path_.MatchesExtension(".gif")) {
    ShowGifProgressNotification();
  }
}

void CaptureModeController::CaptureImage(const CaptureParams& capture_params,
                                         const base::FilePath& path,
                                         const CaptureModeBehavior* behavior) {
  // Note that |type_| may not necessarily be |kImage| here, since this may be
  // called to take an instant fullscreen screenshot for the keyboard shortcut,
  // which doesn't go through the capture mode UI, and doesn't change |type_|.
  CHECK(delegate_->IsCaptureAllowedByPolicy());

  // A screenshot can be requested via the fullscreen screenshot keyboard
  // shortcut (which uses the default `behavior`) even though an active capture
  // mode session belongs to a different `behavior` kind (e.g. Projector or
  // Game Dashboard). In this case, the assumption is that the user wants to
  // take a screenshot of the screen in its current state (i.e. while keeping
  // the session active). Therefore, we don't stop the session in this case.
  // See http://b/353908198 for more details.
  if (IsActive() && behavior == capture_mode_session_->active_behavior()) {
    // Other than the above mentioned case, we stop the session now, so the
    // capture UIs don't end up in the screenshot.
    Stop();
  }

  CHECK(!capture_params.bounds.IsEmpty());

  const bool was_cursor_originally_blocked = MaybeLockCursor();

  // Attempt the capture image. Note the callback `OnImageCaptured()` will only
  // be invoked if an image was successfully captured.
  ui::GrabWindowSnapshotAsPNG(
      capture_params.window, capture_params.bounds,
      base::BindOnce(&CaptureModeController::OnImageCaptured,
                     weak_ptr_factory_.GetWeakPtr(), path,
                     was_cursor_originally_blocked, behavior));

  ++num_screenshots_taken_in_last_day_;
  ++num_screenshots_taken_in_last_week_;

  ++num_consecutive_screenshots_;
  num_consecutive_screenshots_scheduler_.Reset();

  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_SCREENSHOT_CAPTURED);

  // Notifies DLP that taking a screenshot was attempted so that it may report
  // the event or show a warning if restricted content was captured.
  delegate_->OnCaptureImageAttempted(capture_params.window,
                                     capture_params.bounds);
}

void CaptureModeController::CaptureVideo(const CaptureParams& capture_params) {
  DCHECK_EQ(CaptureModeType::kVideo, type_);
  DCHECK(delegate_->IsCaptureAllowedByPolicy());

  if (skip_count_down_ui_) {
    OnVideoRecordCountDownFinished();
    return;
  }

  capture_mode_session_->StartCountDown(
      base::BindOnce(&CaptureModeController::OnVideoRecordCountDownFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  capture_mode_util::TriggerAccessibilityAlert(
      IDS_ASH_SCREEN_CAPTURE_ALERT_RECORDING_STARTING);
}

void CaptureModeController::OnImageCaptured(
    const base::FilePath& path,
    bool was_cursor_originally_blocked,
    const CaptureModeBehavior* behavior,
    scoped_refptr<base::RefCountedMemory> png_bytes) {
  MaybeUnlockCursor(was_cursor_originally_blocked);

  if (!png_bytes || !png_bytes->size()) {
    LOG(ERROR) << "Failed to capture image.";
    ShowFailureNotification();
    return;
  }
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SaveFile, png_bytes, delegate_->RedirectFilePath(path),
                     GetFallbackFilePathFromFile(path)),
      base::BindOnce(&CaptureModeController::OnImageFileSaved,
                     weak_ptr_factory_.GetWeakPtr(), png_bytes, behavior));
}

void CaptureModeController::OnImageCapturedForSearch(
    bool was_cursor_originally_blocked,
    scoped_refptr<base::RefCountedMemory> jpeg_bytes) {
  // Capture mode session may end before the `jpeg_bytes` are received, no-op if
  // the session is no longer active.
  if (!IsActive()) {
    return;
  }
  MaybeUnlockCursor(was_cursor_originally_blocked);

  if (auto* scanner_controller = Shell::Get()->scanner_controller()) {
    scanner_controller->FetchActionsForImage(
        jpeg_bytes,
        base::BindOnce(&CaptureModeController::OnScannerActionsFetched,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // TODO(b/356878705): This currently shows the results panel immediately for
  // debugging purposes. After the backend interface is implemented, we might
  // want to wait for the backend response before showing the results panel.
  const std::unique_ptr<SkBitmap> bitmap =
      gfx::JPEGCodec::Decode(jpeg_bytes->data(), jpeg_bytes->size());
  const gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(*bitmap);
  capture_mode_session_->ShowSearchResultsPanel(image);

  if (on_image_captured_for_search_callback_for_test_) {
    std::move(on_image_captured_for_search_callback_for_test_).Run();
  }
}

void CaptureModeController::OnScannerActionsFetched(
    std::vector<ScannerAction> scanner_actions) {
  for (const ScannerAction& _ : scanner_actions) {
    // TODO(b/369470078): Replace the placeholders with a real callback, text
    // and icon.
    capture_mode_util::AddActionButton(views::Button::PressedCallback(),
                                       /*text=*/u"Placeholder action",
                                       &kCaptureModeIcon);
  }
}

void CaptureModeController::OnImageFileSaved(
    scoped_refptr<base::RefCountedMemory> png_bytes,
    const CaptureModeBehavior* behavior,
    const base::FilePath& file_saved_path) {
  if (file_saved_path.empty()) {
    OnImageFileFinalized(/*image=*/gfx::Image(), behavior, /*success=*/false,
                         file_saved_path);
    return;
  }
  const auto image = gfx::Image::CreateFrom1xPNGBytes(png_bytes);
  delegate_->FinalizeSavedFile(
      base::BindOnce(&CaptureModeController::OnImageFileFinalized,
                     weak_ptr_factory_.GetWeakPtr(), image, behavior),
      file_saved_path, image);
}

void CaptureModeController::OnImageFileFinalized(
    const gfx::Image& image,
    const CaptureModeBehavior* behavior,
    bool success,
    const base::FilePath& file_saved_path) {
  if (!success) {
    ShowFailureNotification();
    return;
  }
  if (on_file_saved_callback_for_test_) {
    std::move(on_file_saved_callback_for_test_).Run(file_saved_path);
  }

  DCHECK(!image.IsEmpty());
  CopyImageToClipboard(image);
  ShowPreviewNotification(file_saved_path, image, CaptureModeType::kImage,
                          behavior);
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    RecordSaveToLocation(GetSaveToOption(file_saved_path), behavior);
  }

  // NOTE: Holding space `client` may be `nullptr` in tests.
  if (auto* client = HoldingSpaceController::Get()->client()) {
    client->AddItemOfType(HoldingSpaceItem::Type::kScreenshot, file_saved_path);
  }
}

void CaptureModeController::OnVideoFileSaved(
    const gfx::ImageSkia& video_thumbnail,
    const CaptureModeBehavior* behavior,
    bool success,
    const base::FilePath& saved_video_file_path) {
  DCHECK(base::CurrentUIThread::IsSet());

  if (!success) {
    ShowFailureNotification();
  } else {
    const bool is_gif = saved_video_file_path.MatchesExtension(".gif");
    if (behavior->ShouldShowPreviewNotification()) {
      ShowPreviewNotification(saved_video_file_path,
                              gfx::Image(video_thumbnail),
                              CaptureModeType::kVideo, behavior);
      // NOTE: Holding space `client` may be `nullptr` in tests.
      if (auto* client = HoldingSpaceController::Get()->client()) {
        client->AddItemOfType(is_gif
                                  ? HoldingSpaceItem::Type::kScreenRecordingGif
                                  : HoldingSpaceItem::Type::kScreenRecording,
                              saved_video_file_path);
      }

      auto reply = base::BindOnce(&RecordVideoFileSizeKB, is_gif, behavior);
      if (on_file_saved_callback_for_test_) {
        reply = std::move(reply).Then(
            base::BindOnce(std::move(on_file_saved_callback_for_test_),
                           saved_video_file_path));
      }

      // We only record the file size histogram if the recording is not saved on
      // DriveFs.
      blocking_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&GetFileSizeInKB, saved_video_file_path),
          std::move(reply));
    }

    CHECK(!recording_start_time_.is_null());
    RecordCaptureModeRecordingDuration(
        (base::TimeTicks::Now() - recording_start_time_), behavior, is_gif);
  }
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    RecordSaveToLocation(GetSaveToOption(saved_video_file_path), behavior);
  }

  // If `on_file_saved_callback_for_test_` is not empty, it means that it hasn't
  // been consumed yet since file size metric will not be recorded if saved on
  // DriveFs for example the projector-initiated capture mode. In this case, we
  // need to explicitly run the callback to let the running wait runloop quit on
  // file saved.
  if (on_file_saved_callback_for_test_) {
    std::move(on_file_saved_callback_for_test_).Run(saved_video_file_path);
  }
}

void CaptureModeController::ShowPreviewNotification(
    const base::FilePath& screen_capture_path,
    const gfx::Image& preview_image,
    const CaptureModeType type,
    const CaptureModeBehavior* behavior) {
  const bool for_video = type == CaptureModeType::kVideo;
  const int title_id = GetNotificationTitleIdForFile(screen_capture_path);
  const int message_id = for_video && low_disk_space_threshold_reached_
                             ? IDS_ASH_SCREEN_CAPTURE_LOW_STORAGE_SPACE_MESSAGE
                             : kNoMessage;

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons = behavior->GetNotificationButtonsInfo(for_video);

  optional_fields.image = preview_image;
  optional_fields.image_path = screen_capture_path;

  ShowNotification(
      kScreenCaptureNotificationId, title_id, message_id, optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&CaptureModeController::HandleNotificationClicked,
                              weak_ptr_factory_.GetWeakPtr(),
                              screen_capture_path, type,
                              behavior->behavior_type())),
      message_center::SystemNotificationWarningLevel::NORMAL, kCaptureModeIcon,
      for_video);
}

void CaptureModeController::HandleNotificationClicked(
    const base::FilePath& screen_capture_path,
    const CaptureModeType type,
    const BehaviorType behavior_type,
    std::optional<int> button_index) {
  if (!button_index.has_value()) {
    // Open the item with the default handler.
    delegate_->OpenScreenCaptureItem(screen_capture_path);
    RecordScreenshotNotificationQuickAction(CaptureQuickAction::kOpenDefault);
  } else {
    const int button_index_value = button_index.value();
    if (type == CaptureModeType::kVideo) {
      if (behavior_type == BehaviorType::kGameDashboard) {
        switch (button_index_value) {
          case GameDashboardVideoNotificationButtonIndex::
              kButtonShareToYoutube:
            OnShareToYouTubeButtonPressed();
            break;
          case GameDashboardVideoNotificationButtonIndex::
              kButtonDeleteGameVideo:
            DeleteFileAsync(blocking_task_runner_, screen_capture_path,
                            std::move(on_file_deleted_callback_for_test_));
            break;
          default:
            NOTREACHED();
        }
      } else {
        CHECK_EQ(VideoNotificationButtonIndex::kButtonDeleteVideo,
                 button_index_value);
        DeleteFileAsync(blocking_task_runner_, screen_capture_path,
                        std::move(on_file_deleted_callback_for_test_));
      }
    } else {
      CHECK_EQ(type, CaptureModeType::kImage);
      switch (button_index_value) {
        case ScreenshotNotificationButtonIndex::kButtonEdit:
          delegate_->OpenScreenshotInImageEditor(screen_capture_path);
          RecordScreenshotNotificationQuickAction(
              CaptureQuickAction::kBacklight);
          break;
        case ScreenshotNotificationButtonIndex::kButtonDelete:
          DeleteFileAsync(blocking_task_runner_, screen_capture_path,
                          std::move(on_file_deleted_callback_for_test_));
          RecordScreenshotNotificationQuickAction(CaptureQuickAction::kDelete);
          break;
        default:
          NOTREACHED();
      }
    }
  }

  // This has to be done at the end to avoid a use-after-free crash, since
  // removing the notification will delete its delegate, which owns the callback
  // to this function. The callback's state owns any passed-by-ref arguments,
  // such as |screen_capture_path| which we use in this function.
  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenCaptureNotificationId, /*by_user=*/false);
}

base::FilePath CaptureModeController::BuildImagePath() const {
  return BuildPathNoExtension(kScreenshotFileNameFmtStr, base::Time::Now())
      .AddExtension("png");
}

base::FilePath CaptureModeController::BuildVideoPath() const {
  return BuildPathNoExtension(kVideoFileNameFmtStr, base::Time::Now())
      .AddExtension(GetVideoExtension(recording_type_, source_));
}

base::FilePath CaptureModeController::BuildImagePathForDisplay(
    int display_index) const {
  auto path_str =
      BuildPathNoExtension(kScreenshotFileNameFmtStr, base::Time::Now())
          .value();
  auto full_path = base::StringPrintf("%s - Display %d.png", path_str.c_str(),
                                      display_index);
  return base::FilePath(full_path);
}

base::FilePath CaptureModeController::BuildPathNoExtension(
    const char* const format_string,
    base::Time timestamp) const {
  return GetCurrentCaptureFolder().path.AppendASCII(
      base::StringPrintfNonConstexpr(
          format_string,
          base::UnlocalizedTimeFormatWithPattern(timestamp, "y-MM-dd").c_str(),
          base::UnlocalizedTimeFormatWithPattern(
              timestamp,
              delegate_->Uses24HourFormat() ? "HH.mm.ss" : "h.mm.ss a")
              .c_str()));
}

base::FilePath CaptureModeController::GetFallbackFilePathFromFile(
    const base::FilePath& path) {
  auto* session_controller = Shell::Get()->session_controller();
  const auto fallback_dir = session_controller->IsActiveUserSessionStarted()
                                ? delegate_->GetUserDefaultDownloadsFolder()
                                : GetTempDir();
  return fallback_dir.Append(path.BaseName());
}

void CaptureModeController::RecordAndResetScreenshotsTakenInLastDay() {
  RecordNumberOfScreenshotsTakenInLastDay(num_screenshots_taken_in_last_day_);
  num_screenshots_taken_in_last_day_ = 0;
}

void CaptureModeController::RecordAndResetScreenshotsTakenInLastWeek() {
  RecordNumberOfScreenshotsTakenInLastWeek(num_screenshots_taken_in_last_week_);
  num_screenshots_taken_in_last_week_ = 0;
}

void CaptureModeController::RecordAndResetConsecutiveScreenshots() {
  RecordNumberOfConsecutiveScreenshots(num_consecutive_screenshots_);
  num_consecutive_screenshots_ = 0;
}

void CaptureModeController::OnVideoRecordCountDownFinished() {
  // Ensure `on_countdown_finished_callback_for_test_` is run after this
  // function.
  base::ScopedClosureRunner scoped_closure(
      std::move(on_countdown_finished_callback_for_test_));

  // If this event is dispatched after the capture session was cancelled or
  // destroyed, this should be a no-op.
  if (!IsActive())
    return;

  const std::optional<CaptureParams> capture_params = GetCaptureParams();
  if (!capture_params) {
    // There's nothing to capture, so we'll stop the session and skip the rest.
    Stop();
    return;
  }

  // During the 3-second count down, screen content might have changed. We must
  // check again the DLP restrictions.
  DCHECK(!pending_dlp_check_);
  pending_dlp_check_ = true;
  capture_mode_session_->OnWaitingForDlpConfirmationStarted();
  delegate_->CheckCaptureOperationRestrictionByDlp(
      capture_params->window, capture_params->bounds,
      base::BindOnce(
          &CaptureModeController::OnDlpRestrictionCheckedAtCountDownFinished,
          weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeController::OnCaptureFolderCreated(
    const CaptureParams& capture_params,
    const base::FilePath& capture_file_full_path) {
  if (!IsActive()) {
    // This function gets called asynchronously, and until it gets called, the
    // session could end due e.g. locking the screen, suspending, or switching
    // users.
    return;
  }

  // An empty path is sent to indicate an error.
  if (capture_file_full_path.empty()) {
    Stop();
    return;
  }

  BeginVideoRecording(capture_params, capture_file_full_path);
}

void CaptureModeController::BeginVideoRecording(
    const CaptureParams& capture_params,
    const base::FilePath& video_file_path) {
  CHECK(!video_file_path.empty());
  CHECK(IsVideoFileExtensionSupported(video_file_path));
  CHECK(can_start_new_recording());

  if (!IsActive()) {
    // This function gets called asynchronously, and until it gets called, the
    // session could end due to e.g. locking the screen, suspending, or
    // switching users.
    return;
  }

  base::AutoReset<bool> initializing_resetter(&is_initializing_recording_,
                                              true);

  // Do not trigger an alert when exiting the session, since we end the session
  // to start recording.
  capture_mode_session_->set_a11y_alert_on_session_exit(false);

  // Acquire the session's layer in order to potentially reuse it for painting
  // a highlight around the region being recorded.
  std::unique_ptr<ui::Layer> session_layer =
      capture_mode_session_->ReleaseLayer();
  session_layer->set_delegate(nullptr);

  // At this point, recording is guaranteed to start, and cannot be prevented by
  // DLP or user cancellation.
  capture_mode_session_->set_is_stopping_to_start_video_recording(true);

  // Cache the active behavior of the capture session to be passed to video
  // recording watcher after stopping the capture mode session.
  CaptureModeBehavior* active_behavior =
      capture_mode_session_->active_behavior();

  // Stop the capture session now, so the bar doesn't show up in the captured
  // video.
  Stop();

  // Use the `video_file_path` instead of `recording_type_` to determine if the
  // recording format supports audio recording. This is because the actual
  // format can be different, since GIF for example is only supported when the
  // recording `source_` is `kRegion`.
  const AudioRecordingMode effective_audio_mode =
      SupportsAudioRecording(video_file_path) ? GetEffectiveAudioRecordingMode()
                                              : AudioRecordingMode::kOff;
  const bool should_record_audio =
      effective_audio_mode != AudioRecordingMode::kOff;
  mojo::PendingRemote<viz::mojom::FrameSinkVideoCaptureOverlay>
      cursor_capture_overlay;
  auto cursor_overlay_receiver =
      cursor_capture_overlay.InitWithNewPipeAndPassReceiver();
  video_recording_watcher_ = std::make_unique<VideoRecordingWatcher>(
      this, active_behavior, capture_params.window,
      std::move(cursor_capture_overlay), should_record_audio);

  aura::Window* root_window = capture_params.window->GetRootWindow();
  for (auto& observer : observers_) {
    observer.OnRecordingStarted(root_window);
  }

  // We only paint the recorded area highlight for window and region captures.
  if (source_ != CaptureModeSource::kFullscreen)
    video_recording_watcher_->Reset(std::move(session_layer));

  DCHECK(current_video_file_path_.empty());
  recording_start_time_ = base::TimeTicks::Now();
  current_video_file_path_ = video_file_path;

  LaunchRecordingServiceAndStartRecording(
      capture_params, std::move(cursor_overlay_receiver), effective_audio_mode);

  // Intentionally record the metrics before `DetachFromSession` as
  // `enable_demo_tools_` may be overwritten otherwise.
  RecordRecordingStartsWithDemoTools(enable_demo_tools_, active_behavior);

  // Restore the cached capture mode configs when the capture mode session ends
  // to start video recording in case another default capture mode session
  // starts while video recording in progress.
  active_behavior->DetachFromSession();

  capture_mode_util::SetStopRecordingButtonVisibility(root_window, true);

  delegate_->StartObservingRestrictedContent(
      capture_params.window, capture_params.bounds,
      base::BindOnce(&CaptureModeController::InterruptVideoRecording,
                     weak_ptr_factory_.GetWeakPtr()));

  if (on_video_recording_started_callback_for_test_) {
    std::move(on_video_recording_started_callback_for_test_).Run();
  }
}

void CaptureModeController::InterruptVideoRecording() {
  EndVideoRecording(EndRecordingReason::kDlpInterruption);
}

void CaptureModeController::OnDlpRestrictionCheckedAtPerformingCapture(
    bool proceed) {
  pending_dlp_check_ = false;

  if (!IsActive()) {
    // This function gets called asynchronously, and until it gets called, the
    // session could end due to e.g. locking the screen, suspending, or
    // switching users.
    return;
  }

  // We don't need to bring capture mode UIs back if `proceed` is false or if
  // the session is about to shutdown. See also
  // `CaptureModeBehavior::ShouldReShowUisAtPerformingCapture()`.
  auto* active_behavior = capture_mode_session_->active_behavior();
  capture_mode_session_->OnWaitingForDlpConfirmationEnded(
      /*reshow_uis=*/proceed &&
      active_behavior->ShouldReShowUisAtPerformingCapture());

  if (!proceed) {
    Stop();
    return;
  }

  const std::optional<CaptureParams> capture_params = GetCaptureParams();
  CHECK(capture_params);

  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    Stop();
    return;
  }

  if (type_ == CaptureModeType::kImage) {
    if (active_behavior->behavior_type() == BehaviorType::kSunfish) {
      // Sunfish behavior doesn't need the file path and does specific image
      // capture handling.
      PerformImageSearch();
    } else {
      CaptureImage(*capture_params, BuildImagePath(),
                   capture_mode_session_->active_behavior());
    }
  } else {
    // HDCP affects only video recording.
    if (ShouldBlockRecordingForContentProtection(capture_params->window)) {
      ShowDisabledNotification(CaptureAllowance::kDisallowedByHdcp);
      Stop();
      return;
    }

    CaptureVideo(*capture_params);
  }
}

void CaptureModeController::OnDlpRestrictionCheckedAtCountDownFinished(
    bool proceed) {
  pending_dlp_check_ = false;

  if (!IsActive()) {
    // This function gets called asynchronously, and until it gets called, the
    // session could end due to e.g. locking the screen, suspending, or
    // switching users.
    return;
  }

  // We don't need to bring back capture mode UIs on 3-second count down
  // finished, since the session is about to shutdown anyways for starting the
  // video recording.
  capture_mode_session_->OnWaitingForDlpConfirmationEnded(/*reshow_uis=*/false);

  if (!proceed) {
    Stop();
    return;
  }

  const std::optional<CaptureParams> capture_params = GetCaptureParams();
  if (!capture_params) {
    Stop();
    return;
  }

  // Now that we're done with DLP restrictions checks, we can perform the policy
  // and HDCP checks, which may have changed during the 3-second count down and
  // during the time the DLP warning dialog was shown.
  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    Stop();
    return;
  }

  if (ShouldBlockRecordingForContentProtection(capture_params->window)) {
    Stop();
    ShowDisabledNotification(CaptureAllowance::kDisallowedByHdcp);
    return;
  }

  // The creation of the required capture folder that will host the video is
  // asynchronous. We don't want the user to be able to bail out of the
  // session at this point, since we don't want to create that folder in vain.
  capture_mode_session_->set_can_exit_on_escape(false);

  CaptureModeBehavior* active_behavior =
      capture_mode_session_->active_behavior();
  if (!active_behavior->SupportsAudioRecordingMode(
          GetEffectiveAudioRecordingMode())) {
    // Before asking the client to create a folder to host the video file, we
    // check if they require audio recording to be enabled, but it can't be
    // allowed due to admin policy. In this case we just abort the recording by
    // stopping the capture mode session without starting any recording. This
    // will eventually call `CaptureModeObserver::OnRecordingStartAborted()`
    // which should let clients do any necessary clean ups.
    Stop();
    return;
  }

  if (active_behavior->RequiresCaptureFolderCreation()) {
    active_behavior->CreateCaptureFolder(
        base::BindOnce(&CaptureModeController::OnCaptureFolderCreated,
                       weak_ptr_factory_.GetWeakPtr(), *capture_params));
    return;
  }

  const base::FilePath current_path = BuildVideoPath();

  // If the current capture folder is not the default `Downloads` folder, we
  // need to validate the current folder first before starting the video
  // recording.
  if (!GetCurrentCaptureFolder().is_default_downloads_folder) {
    blocking_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&SelectFilePathForCapturedFile,
                       delegate_->RedirectFilePath(current_path),
                       GetFallbackFilePathFromFile(current_path)),
        base::BindOnce(&CaptureModeController::BeginVideoRecording,
                       weak_ptr_factory_.GetWeakPtr(), *capture_params));
    return;
  }

  BeginVideoRecording(*capture_params, current_path);
}

void CaptureModeController::OnDlpRestrictionCheckedAtSessionInit(
    SessionType session_type,
    CaptureModeEntryType entry_type,
    base::OnceClosure at_exit_closure,
    bool proceed) {
  base::ScopedClosureRunner deferred_runner(std::move(at_exit_closure));

  pending_dlp_check_ = false;

  if (!proceed) {
    return;
  }

  CHECK(!capture_mode_session_);

  // Check policy again even though we checked in Start(), but due to the DLP
  // warning dialog can be accepted after a long wait, maybe something changed
  // in the middle.
  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    return;
  }

  // Before we start the session, if video recording is in progress, we need to
  // set the current type to image (except if the new behavior type is sunfish),
  // as we can't have more than one recording at a time. The video toggle button
  // in the capture mode bar will be disabled.
  if (!can_start_new_recording()) {
    SetType(CaptureModeType::kImage);
  } else if (entry_type == CaptureModeEntryType::kProjector) {
    CHECK(!delegate_->IsAudioCaptureDisabledByPolicy())
        << "A projector session should not be allowed to begin if audio "
           "capture is disabled by policy.";
  }
  const BehaviorType behavior_type = ToBehaviorType(entry_type);

  RecordCaptureModeEntryType(entry_type);
  if (ShouldClearCaptureRegion(behavior_type)) {
    SetUserCaptureRegion(gfx::Rect(), /*by_user=*/false);
  }

  delegate_->OnSessionStateChanged(/*started=*/true);

  capture_mode_session_ =
      CreateSession(session_type, this, GetBehavior(behavior_type));
  capture_mode_session_->Initialize();
  camera_controller_->OnCaptureSessionStarted();
}

void CaptureModeController::OnDlpRestrictionCheckedAtVideoEnd(
    const gfx::ImageSkia& video_thumbnail,
    bool success,
    const CaptureModeBehavior* behavior,
    bool proceed) {
  const bool should_delete_file = !proceed;
  const auto video_file_path = current_video_file_path_;
  current_video_file_path_.clear();

  if (should_delete_file) {
    // Remove any lingering notification, e.g. the GIF progress notification,
    // before proceeding, since it no longer makes sense as the file will be
    // deleted.
    message_center::MessageCenter::Get()->RemoveNotification(
        kScreenCaptureNotificationId, /*by_user=*/false);

    DeleteFileAsync(blocking_task_runner_, video_file_path,
                    std::move(on_file_deleted_callback_for_test_));
    OnVideoFileFinalized(/*should_delete_file=*/true, video_thumbnail);
  } else {
    if (!success) {
      OnVideoFileSaved(video_thumbnail, behavior, success, video_file_path);
      OnVideoFileFinalized(/*should_delete_file=*/false, video_thumbnail);
      return;
    }
    delegate_->FinalizeSavedFile(
        base::BindOnce(&CaptureModeController::OnVideoFileSaved,
                       weak_ptr_factory_.GetWeakPtr(), video_thumbnail,
                       behavior)
            .Then(base::BindOnce(&CaptureModeController::OnVideoFileFinalized,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 /*should_delete_file=*/false,
                                 video_thumbnail)),
        video_file_path, gfx::Image(video_thumbnail));
  }
}

void CaptureModeController::OnVideoFileFinalized(
    bool should_delete_file,
    const gfx::ImageSkia& video_thumbnail) {
  low_disk_space_threshold_reached_ = false;
  recording_start_time_ = base::TimeTicks();

  for (auto& observer : observers_) {
    observer.OnVideoFileFinalized(should_delete_file, video_thumbnail);
  }
}

void CaptureModeController::CaptureInstantScreenshot(
    CaptureModeEntryType entry_type,
    CaptureModeSource source,
    base::OnceClosure instant_screenshot_callback,
    BehaviorType behavior_type) {
  if (pending_dlp_check_) {
    return;
  }

  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    return;
  }

  pending_dlp_check_ = true;
  delegate_->CheckCaptureModeInitRestrictionByDlp(base::BindOnce(
      &CaptureModeController::OnDlpRestrictionCheckedAtCaptureScreenshot,
      weak_ptr_factory_.GetWeakPtr(), entry_type, source,
      std::move(instant_screenshot_callback), behavior_type));
}

void CaptureModeController::OnDlpRestrictionCheckedAtCaptureScreenshot(
    CaptureModeEntryType entry_type,
    CaptureModeSource source,
    base::OnceClosure instant_screenshot_callback,
    BehaviorType behavior_type,
    bool proceed) {
  pending_dlp_check_ = false;
  if (!proceed) {
    return;
  }

  // Due to the fact that the DLP warning dialog may take a while, check the
  // enterprise policy again even though we checked it in
  // `CaptureInstantScreenshot()`.
  if (!delegate_->IsCaptureAllowedByPolicy()) {
    ShowDisabledNotification(CaptureAllowance::kDisallowedByPolicy);
    return;
  }

  std::move(instant_screenshot_callback).Run();

  // Since this doesn't create a capture mode session, log metrics here.
  RecordCaptureModeEntryType(entry_type);
  RecordCaptureModeConfiguration(
      CaptureModeType::kImage, source,
      // The values of `recording_type_` and `GetEffectiveAudioRecordingMode()`
      // will be ignored, since the type is `kImage`.
      recording_type_, GetEffectiveAudioRecordingMode(),
      GetBehavior(behavior_type));
}

void CaptureModeController::PerformScreenshotsOfAllDisplays(
    BehaviorType behavior_type) {
  // Get a vector of RootWindowControllers with primary root window at first.
  const std::vector<RootWindowController*> controllers =
      RootWindowController::root_window_controllers();
  // Capture screenshot for each individual display.
  int display_index = 1;
  for (RootWindowController* controller : controllers) {
    // TODO(shidi): Check with UX what notification should show if
    // some (but not all) of the displays have restricted content and
    // whether we should localize the display name.
    const CaptureParams capture_params{controller->GetRootWindow(),
                                       controller->GetRootWindow()->bounds()};
    CaptureImage(capture_params,
                 controllers.size() == 1
                     ? BuildImagePath()
                     : BuildImagePathForDisplay(display_index),
                 GetBehavior(behavior_type));
    ++display_index;
  }
}

void CaptureModeController::PerformScreenshotOfGivenWindow(
    aura::Window* given_window,
    BehaviorType behavior_type) {
  const CaptureParams capture_params{given_window,
                                     gfx::Rect(given_window->bounds().size())};
  // TODO(michelefan): Add behavior type as an input parameter, if this API is
  // used for other entry types in future.
  CaptureImage(capture_params, BuildImagePath(), GetBehavior(behavior_type));
}

bool CaptureModeController::ShouldClearCaptureRegion(
    BehaviorType behavior_type) const {
  // Reset the user capture region if enough time has passed as it can be
  // annoying to still have the old capture region from the previous session
  // long time ago, or if the active behavior is sunfish behavior.
  return !user_capture_region_.IsEmpty() &&
         (base::TimeTicks::Now() - last_capture_region_update_time_ >
              kResetCaptureRegionDuration ||
          behavior_type == BehaviorType::kSunfish);
}

CaptureModeSaveToLocation CaptureModeController::GetSaveToOption(
    const base::FilePath& path) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  const auto dir_path = path.DirName();
  if (dir_path == delegate_->GetUserDefaultDownloadsFolder())
    return CaptureModeSaveToLocation::kDefault;
  base::FilePath mounted_path;
  if (delegate_->GetDriveFsMountPointPath(&mounted_path)) {
    const auto drive_root_path = mounted_path.Append("root");
    if (dir_path == drive_root_path)
      return CaptureModeSaveToLocation::kDrive;

    if (drive_root_path.IsParent(dir_path))
      return CaptureModeSaveToLocation::kDriveFolder;
  }
  base::FilePath one_drive_mount_path = delegate_->GetOneDriveMountPointPath();
  if (!one_drive_mount_path.empty()) {
    if (dir_path == one_drive_mount_path) {
      return CaptureModeSaveToLocation::kOneDrive;
    }
    if (one_drive_mount_path.IsParent(dir_path)) {
      return CaptureModeSaveToLocation::kOneDriveFolder;
    }
  }
  return CaptureModeSaveToLocation::kCustomizedFolder;
}

CaptureModeBehavior* CaptureModeController::GetBehavior(
    BehaviorType behavior_type) {
  auto& behavior = behaviors_map_[behavior_type];
  if (!behavior) {
    behavior = CaptureModeBehavior::Create(behavior_type);
  }

  return behavior.get();
}

}  // namespace ash
