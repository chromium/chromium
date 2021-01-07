// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <utility>

#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/video_recording_watcher.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/aura/env.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

namespace {

CaptureModeController* g_instance = nullptr;

// The amount of time that can elapse from the prior screenshot to be considered
// consecutive.
constexpr base::TimeDelta kConsecutiveScreenshotThreshold =
    base::TimeDelta::FromSeconds(5);

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";
constexpr char kScreenCaptureStoppedNotificationId[] =
    "capture_mode_stopped_notification";
constexpr char kScreenCaptureNotifierId[] = "ash.capture_mode_controller";

// The format strings of the file names of captured images.
// TODO(afakhry): Discuss with UX localizing "Screenshot" and "Screen
// recording".
constexpr char kScreenshotFileNameFmtStr[] = "Screenshot %s %s";
constexpr char kVideoFileNameFmtStr[] = "Screen recording %s %s";
constexpr char kDateFmtStr[] = "%d-%02d-%02d";
constexpr char k24HourTimeFmtStr[] = "%02d.%02d.%02d";
constexpr char kAmPmTimeFmtStr[] = "%d.%02d.%02d";

// Duration to clear the capture region selection from the previous session.
constexpr base::TimeDelta kResetCaptureRegionDuration =
    base::TimeDelta::FromMinutes(8);

// The screenshot notification button index.
enum ScreenshotNotificationButtonIndex {
  BUTTON_EDIT = 0,
  BUTTON_DELETE,
};

// The video notification button index.
enum VideoNotificationButtonIndex {
  BUTTON_DELETE_VIDEO = 0,
};

// Returns the date extracted from |timestamp| as a string to be part of
// captured file names. Note that naturally formatted dates includes slashes
// (e.g. 2020/09/02), which will cause problems when used in file names since
// slash is a path separator.
std::string GetDateStr(const base::Time::Exploded& timestamp) {
  return base::StringPrintf(kDateFmtStr, timestamp.year, timestamp.month,
                            timestamp.day_of_month);
}

// Returns the time extracted from |timestamp| as a string to be part of
// captured file names. Also note that naturally formatted times include colons
// (e.g. 11:20 AM), which is restricted in file names in most file systems.
// https://en.wikipedia.org/wiki/Filename#Comparison_of_filename_limitations.
std::string GetTimeStr(const base::Time::Exploded& timestamp,
                       bool use_24_hour) {
  if (use_24_hour) {
    return base::StringPrintf(k24HourTimeFmtStr, timestamp.hour,
                              timestamp.minute, timestamp.second);
  }

  int hour = timestamp.hour % 12;
  if (hour <= 0)
    hour += 12;

  std::string time = base::StringPrintf(kAmPmTimeFmtStr, hour, timestamp.minute,
                                        timestamp.second);
  return time.append(timestamp.hour >= 12 ? " PM" : " AM");
}

// Writes the given |data| in a file with |path|. Returns true if saving
// succeeded, or false otherwise.
bool SaveFile(scoped_refptr<base::RefCountedMemory> data,
              const base::FilePath& path) {
  DCHECK(data);
  const int size = static_cast<int>(data->size());
  DCHECK(size);
  DCHECK(!base::CurrentUIThread::IsSet());
  DCHECK(!path.empty());

  if (!base::PathExists(path.DirName())) {
    LOG(ERROR) << "File path doesn't exist: " << path.DirName();
    return false;
  }

  if (size != base::WriteFile(
                  path, reinterpret_cast<const char*>(data->front()), size)) {
    LOG(ERROR) << "Failed to save file: " << path;
    return false;
  }

  return true;
}

void DeleteFileAsync(scoped_refptr<base::SequencedTaskRunner> task_runner,
                     const base::FilePath& path) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::DeleteFile, path),
      base::BindOnce(
          [](const base::FilePath& path, bool success) {
            // TODO(afakhry): Show toast?
            if (!success)
              LOG(ERROR) << "Failed to delete the file: " << path;
          },
          path));
}

// Shows a Capture Mode related notification with the given parameters.
void ShowNotification(
    const base::string16& title,
    const base::string16& message,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate) {
  const auto type = optional_fields.image.IsEmpty()
                        ? message_center::NOTIFICATION_TYPE_SIMPLE
                        : message_center::NOTIFICATION_TYPE_IMAGE;
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          type, kScreenCaptureNotificationId, title, message,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kScreenCaptureNotifierId),
          optional_fields, delegate, kCaptureModeIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  // Remove the previous notification before showing the new one if there is
  // any.
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(kScreenCaptureNotificationId,
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

// Shows a notification informing the user that Capture Mode operations are
// currently disabled.
void ShowDisabledNotification() {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kScreenCaptureNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISABLED_TITLE),
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISABLED_MESSAGE),
          /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kScreenCaptureNotifierId),
          /*optional_fields=*/{}, /*delegate=*/nullptr,
          vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

// Shows a notification informing the user that a Capture Mode operation has
// failed.
void ShowFailureNotification() {
  ShowNotification(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_FAILURE_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_FAILURE_MESSAGE),
      /*optional_fields=*/{}, /*delegate=*/nullptr);
}

// Shows a notification informing the user that video recording was stopped.
void ShowVideoRecordingStoppedNotification() {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kScreenCaptureStoppedNotificationId,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_STOPPED_TITLE),
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_STOPPED_MESSAGE),
          /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kScreenCaptureNotifierId),
          /*optional_fields=*/{}, /*delegate=*/nullptr,
          vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

// Copies the bitmap representation of the given |image| to the clipboard.
void CopyImageToClipboard(const gfx::Image& image) {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  DCHECK(clipboard);
  auto clipboard_data = std::make_unique<ui::ClipboardData>();
  clipboard_data->SetBitmapData(image.AsBitmap());
  clipboard->WriteClipboardData(std::move(clipboard_data));
}

}  // namespace

CaptureModeController::CaptureModeController(
    std::unique_ptr<CaptureModeDelegate> delegate)
    : delegate_(std::move(delegate)),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // A task priority of BEST_EFFORT is good enough for this runner,
          // since it's used for blocking file IO such as saving the screenshots
          // or the successive webm video chunks received from the recording
          // service.
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      recording_service_client_receiver_(this),
      num_consecutive_screenshots_scheduler_(
          FROM_HERE,
          kConsecutiveScreenshotThreshold,
          this,
          &CaptureModeController::RecordAndResetConsecutiveScreenshots) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  on_video_file_status_ =
      base::BindRepeating(&CaptureModeController::OnVideoFileStatus,
                          weak_ptr_factory_.GetWeakPtr());

  // Schedule recording of the number of screenshots taken per day.
  num_screenshots_taken_in_last_day_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(1),
      base::BindRepeating(
          &CaptureModeController::RecordAndResetScreenshotsTakenInLastDay,
          weak_ptr_factory_.GetWeakPtr()));

  // Schedule recording of the number of screenshots taken per week.
  num_screenshots_taken_in_last_week_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(7),
      base::BindRepeating(
          &CaptureModeController::RecordAndResetScreenshotsTakenInLastWeek,
          weak_ptr_factory_.GetWeakPtr()));

  Shell::Get()->session_controller()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

CaptureModeController::~CaptureModeController() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  Shell::Get()->session_controller()->RemoveObserver(this);
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CaptureModeController* CaptureModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void CaptureModeController::SetSource(CaptureModeSource source) {
  if (source == source_)
    return;

  source_ = source;
  if (capture_mode_session_)
    capture_mode_session_->OnCaptureSourceChanged(source_);
}

void CaptureModeController::SetType(CaptureModeType type) {
  if (is_recording_in_progress_ && type == CaptureModeType::kVideo) {
    // Overwrite video capture types to image, as we can't have more than one
    // recording at a time.
    type = CaptureModeType::kImage;
  }

  if (type == type_)
    return;

  type_ = type;
  if (capture_mode_session_)
    capture_mode_session_->OnCaptureTypeChanged(type_);
}

void CaptureModeController::Start(CaptureModeEntryType entry_type) {
  if (capture_mode_session_)
    return;

  if (delegate_->IsCaptureModeInitRestricted()) {
    ShowDisabledNotification();
    return;
  }

  // Before we start the session, if video recording is in progress, we need to
  // set the current type to image, as we can't have more than one recording at
  // a time. The video toggle button in the capture mode bar will be disabled.
  if (is_recording_in_progress_)
    SetType(CaptureModeType::kImage);

  RecordCaptureModeEntryType(entry_type);
  // Reset the user capture region if enough time has passed as it can be
  // annoying to still have the old capture region from the previous session
  // long time ago.
  if (!user_capture_region_.IsEmpty() &&
      base::TimeTicks::Now() - last_capture_region_update_time_ >
          kResetCaptureRegionDuration) {
    SetUserCaptureRegion(gfx::Rect(), /*by_user=*/false);
  }
  capture_mode_session_ = std::make_unique<CaptureModeSession>(this);
}

void CaptureModeController::Stop() {
  DCHECK(IsActive());
  capture_mode_session_.reset();
}

void CaptureModeController::SetUserCaptureRegion(const gfx::Rect& region,
                                                 bool by_user) {
  user_capture_region_ = region;
  if (!user_capture_region_.IsEmpty() && by_user)
    last_capture_region_update_time_ = base::TimeTicks::Now();
}

void CaptureModeController::CaptureScreenshotsOfAllDisplays() {
  if (delegate_->IsCaptureModeInitRestricted()) {
    ShowDisabledNotification();
    return;
  }
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
    CaptureImage(capture_params, controllers.size() == 1
                                     ? BuildImagePath()
                                     : BuildImagePathForDisplay(display_index));
    ++display_index;
  }
}

void CaptureModeController::PerformCapture() {
  DCHECK(IsActive());
  const base::Optional<CaptureParams> capture_params = GetCaptureParams();
  if (!capture_params)
    return;

  if (!IsCaptureAllowed(*capture_params)) {
    ShowDisabledNotification();
    Stop();
    return;
  }

  DCHECK(capture_mode_session_);
  capture_mode_session_->ReportSessionHistograms();

  if (type_ == CaptureModeType::kImage) {
    CaptureImage(*capture_params, BuildImagePath());
  }

  else
    CaptureVideo(*capture_params);
}

void CaptureModeController::EndVideoRecording(EndRecordingReason reason) {
  RecordEndRecordingReason(reason);
  recording_service_remote_->StopRecording();
  TerminateRecordingUiElements();
}

void CaptureModeController::OpenFeedbackDialog() {
  delegate_->OpenFeedbackDialog();
}

void CaptureModeController::OnMuxerOutput(const std::string& chunk) {
  DCHECK(video_file_handler_);
  video_file_handler_.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(const_cast<std::string&>(chunk))
      .Then(on_video_file_status_);
}

void CaptureModeController::OnRecordingEnded(bool success) {
  delegate_->StopObservingRestrictedContent();

  // If |success| is false, then recording has been force-terminated due to a
  // failure on the service side, or a disconnection to it. We need to terminate
  // the recording-related UI elements.
  if (!success) {
    // TODO(afakhry): Show user a failure message.
    TerminateRecordingUiElements();
  }

  // Resetting the service remote would terminate its process.
  recording_service_remote_.reset();
  recording_service_client_receiver_.reset();

  DCHECK(video_file_handler_);
  video_file_handler_.AsyncCall(&VideoFileHandler::FlushBufferedChunks)
      .Then(base::BindOnce(&CaptureModeController::OnVideoFileSaved,
                           weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  EndSessionOrRecording(EndRecordingReason::kActiveUserChange);
}

void CaptureModeController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    EndSessionOrRecording(EndRecordingReason::kSessionBlocked);
}

void CaptureModeController::OnChromeTerminating() {
  EndSessionOrRecording(EndRecordingReason::kShuttingDown);
}

void CaptureModeController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  EndSessionOrRecording(EndRecordingReason::kImminentSuspend);
}

void CaptureModeController::StartVideoRecordingImmediatelyForTesting() {
  DCHECK(IsActive());
  DCHECK_EQ(type_, CaptureModeType::kVideo);
  OnVideoRecordCountDownFinished();
}

void CaptureModeController::EndSessionOrRecording(EndRecordingReason reason) {
  if (IsActive()) {
    // Suspend or user session changes can happen while the capture mode session
    // is active or after the three-second countdown had started but not
    // finished yet.
    Stop();
    return;
  }

  if (!is_recording_in_progress_)
    return;

  if (reason == EndRecordingReason::kImminentSuspend) {
    // If suspend happens while recording is in progress, we consider this a
    // failure, and cut the recording immediately. The recording service may
    // have some buffered chunks that will never be received, and as a result,
    // the a few seconds at the end of the recording may get lost.
    // TODO(afakhry): Think whether this is what we want. We might be able to
    // end the recording normally by asking the service to StopRecording(), and
    // block the suspend until all chunks have been received, and then we can
    // resume it.
    RecordEndRecordingReason(EndRecordingReason::kImminentSuspend);
    OnRecordingEnded(/*success=*/false);
    return;
  }

  EndVideoRecording(reason);
}

base::Optional<CaptureModeController::CaptureParams>
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
        return base::nullopt;
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
        return base::nullopt;
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
    const CaptureParams& capture_params) {
  DCHECK(!recording_service_remote_.is_bound())
      << "Should not launch a new recording service while one is already "
         "running.";

  recording_service_remote_.reset();
  recording_service_client_receiver_.reset();

  recording_service_remote_ = delegate_->LaunchRecordingService();
  recording_service_remote_.set_disconnect_handler(
      base::BindOnce(&CaptureModeController::OnRecordingServiceDisconnected,
                     base::Unretained(this)));

  // Prepare the pending remotes of the client, the video capturer, and the
  // audio stream factory.
  mojo::PendingRemote<recording::mojom::RecordingServiceClient> client =
      recording_service_client_receiver_.BindNewPipeAndPassRemote();
  mojo::PendingRemote<viz::mojom::FrameSinkVideoCapturer> video_capturer;
  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->CreateVideoCapturer(video_capturer.InitWithNewPipeAndPassReceiver());

  // We bind the audio stream factory only if audio recording is enabled. This
  // is ok since the |audio_stream_factory| parameter in the recording service
  // APIs is optional, and can be not bound.
  mojo::PendingRemote<audio::mojom::StreamFactory> audio_stream_factory;
  if (enable_audio_recording_) {
    delegate_->BindAudioStreamFactory(
        audio_stream_factory.InitWithNewPipeAndPassReceiver());
  }

  const auto frame_sink_id =
      capture_params.window->GetRootWindow()->GetFrameSinkId();

  const auto bounds = capture_params.bounds;
  switch (source_) {
    case CaptureModeSource::kFullscreen:
      recording_service_remote_->RecordFullscreen(
          std::move(client), std::move(video_capturer),
          std::move(audio_stream_factory), frame_sink_id, bounds.size());
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
          std::move(client), std::move(video_capturer),
          std::move(audio_stream_factory), frame_sink_id,
          capture_params.window->subtree_capture_id(), bounds.size(),
          capture_params.window->GetRootWindow()
              ->GetBoundsInRootWindow()
              .size());
      break;

    case CaptureModeSource::kRegion:
      recording_service_remote_->RecordRegion(
          std::move(client), std::move(video_capturer),
          std::move(audio_stream_factory), frame_sink_id,
          capture_params.window->GetRootWindow()
              ->GetBoundsInRootWindow()
              .size(),
          bounds);
      break;
  }
}

void CaptureModeController::OnRecordingServiceDisconnected() {
  // TODO(afakhry): Consider what to do if the service crashes during an ongoing
  // video recording. Do we try to resume recording, or notify with failure?
  // For now, just end the recording.
  // Note that the service could disconnect between the time we ask it to
  // StopRecording(), and it calling us back with OnRecordingEnded(), so we call
  // OnRecordingEnded() in all cases.
  RecordEndRecordingReason(EndRecordingReason::kRecordingServiceDisconnected);
  OnRecordingEnded(/*success=*/false);
}

bool CaptureModeController::IsCaptureAllowed(
    const CaptureParams& capture_params) const {
  return delegate_->IsCaptureAllowed(
      capture_params.window, capture_params.bounds,
      /*for_video=*/type_ == CaptureModeType::kVideo);
}

void CaptureModeController::TerminateRecordingUiElements() {
  if (!is_recording_in_progress_)
    return;

  is_recording_in_progress_ = false;
  Shell::Get()->UpdateCursorCompositingEnabled();
  capture_mode_util::SetStopRecordingButtonVisibility(
      video_recording_watcher_->window_being_recorded()->GetRootWindow(),
      false);
  video_recording_watcher_.reset();
}

void CaptureModeController::CaptureImage(const CaptureParams& capture_params,
                                         const base::FilePath& path) {
  DCHECK_EQ(CaptureModeType::kImage, type_);
  DCHECK(IsCaptureAllowed(capture_params));

  // Stop the capture session now, so as not to take a screenshot of the capture
  // bar.
  if (IsActive())
    Stop();

  DCHECK(!capture_params.bounds.IsEmpty());
  ui::GrabWindowSnapshotAsyncPNG(
      capture_params.window, capture_params.bounds,
      base::BindOnce(&CaptureModeController::OnImageCaptured,
                     weak_ptr_factory_.GetWeakPtr(), path));

  ++num_screenshots_taken_in_last_day_;
  ++num_screenshots_taken_in_last_week_;

  ++num_consecutive_screenshots_;
  num_consecutive_screenshots_scheduler_.Reset();
}

void CaptureModeController::CaptureVideo(const CaptureParams& capture_params) {
  DCHECK_EQ(CaptureModeType::kVideo, type_);
  DCHECK(IsCaptureAllowed(capture_params));

  if (skip_count_down_ui_) {
    OnVideoRecordCountDownFinished();
    return;
  }

  capture_mode_session_->StartCountDown(
      base::BindOnce(&CaptureModeController::OnVideoRecordCountDownFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeController::OnImageCaptured(
    const base::FilePath& path,
    scoped_refptr<base::RefCountedMemory> png_bytes) {
  if (!png_bytes || !png_bytes->size()) {
    LOG(ERROR) << "Failed to capture image.";
    ShowFailureNotification();
    return;
  }

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&SaveFile, png_bytes, path),
      base::BindOnce(&CaptureModeController::OnImageFileSaved,
                     weak_ptr_factory_.GetWeakPtr(), png_bytes, path));
}

void CaptureModeController::OnImageFileSaved(
    scoped_refptr<base::RefCountedMemory> png_bytes,
    const base::FilePath& path,
    bool success) {
  if (!success) {
    ShowFailureNotification();
    return;
  }

  if (!on_file_saved_callback_.is_null())
    std::move(on_file_saved_callback_).Run(path);

  DCHECK(png_bytes && png_bytes->size());
  const auto image = gfx::Image::CreateFrom1xPNGBytes(png_bytes);
  CopyImageToClipboard(image);
  ShowPreviewNotification(path, image, CaptureModeType::kImage);

  if (features::IsTemporaryHoldingSpaceEnabled()) {
    HoldingSpaceClient* client = HoldingSpaceController::Get()->client();
    if (client)  // May be `nullptr` in tests.
      client->AddScreenshot(path);
  }
}

void CaptureModeController::OnVideoFileStatus(bool success) {
  if (success)
    return;

  // TODO(afakhry): Show the user a message about IO failure.
  EndVideoRecording(EndRecordingReason::kFileIoError);
}

void CaptureModeController::OnVideoFileSaved(bool success) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(video_file_handler_);

  if (!success) {
    ShowFailureNotification();
  } else {
    ShowPreviewNotification(current_video_file_path_, gfx::Image(),
                            CaptureModeType::kVideo);
    DCHECK(!recording_start_time_.is_null());
    RecordCaptureModeRecordTime(
        (base::TimeTicks::Now() - recording_start_time_).InSeconds());

    if (features::IsTemporaryHoldingSpaceEnabled()) {
      HoldingSpaceClient* client = HoldingSpaceController::Get()->client();
      if (client)  // May be `nullptr` in tests.
        client->AddScreenRecording(current_video_file_path_);
    }
  }

  if (!on_file_saved_callback_.is_null())
    std::move(on_file_saved_callback_).Run(current_video_file_path_);

  low_disk_space_threshold_reached_ = false;
  recording_start_time_ = base::TimeTicks();
  current_video_file_path_.clear();
  video_file_handler_.Reset();
}

void CaptureModeController::ShowPreviewNotification(
    const base::FilePath& screen_capture_path,
    const gfx::Image& preview_image,
    const CaptureModeType type) {
  const bool for_video = type == CaptureModeType::kVideo;
  const base::string16 title = l10n_util::GetStringUTF16(
      for_video ? IDS_ASH_SCREEN_CAPTURE_RECORDING_TITLE
                : IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_TITLE);
  const base::string16 message =
      for_video && low_disk_space_threshold_reached_
          ? l10n_util::GetStringUTF16(
                IDS_ASH_SCREEN_CAPTURE_LOW_DISK_SPACE_MESSAGE)
          : l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_MESSAGE);

  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo edit_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT));
  if (!for_video)
    optional_fields.buttons.push_back(edit_button);
  message_center::ButtonInfo delete_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_DELETE));
  optional_fields.buttons.push_back(delete_button);

  optional_fields.image = preview_image;

  ShowNotification(
      title, message, optional_fields,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&CaptureModeController::HandleNotificationClicked,
                              weak_ptr_factory_.GetWeakPtr(),
                              screen_capture_path, type)));
}

void CaptureModeController::HandleNotificationClicked(
    const base::FilePath& screen_capture_path,
    const CaptureModeType type,
    base::Optional<int> button_index) {
  if (!button_index.has_value()) {
    // Show the item in the folder.
    delegate_->ShowScreenCaptureItemInFolder(screen_capture_path);
    RecordScreenshotNotificationQuickAction(CaptureQuickAction::kFiles);
  } else {
    const int button_index_value = button_index.value();
    if (type == CaptureModeType::kVideo) {
      DCHECK_EQ(button_index_value,
                VideoNotificationButtonIndex::BUTTON_DELETE_VIDEO);
      DeleteFileAsync(blocking_task_runner_, screen_capture_path);
    } else {
      DCHECK_EQ(type, CaptureModeType::kImage);
      switch (button_index_value) {
        case ScreenshotNotificationButtonIndex::BUTTON_EDIT:
          delegate_->OpenScreenshotInImageEditor(screen_capture_path);
          RecordScreenshotNotificationQuickAction(
              CaptureQuickAction::kBacklight);
          break;
        case ScreenshotNotificationButtonIndex::BUTTON_DELETE:
          DeleteFileAsync(blocking_task_runner_, screen_capture_path);
          RecordScreenshotNotificationQuickAction(CaptureQuickAction::kDelete);
          break;
        default:
          NOTREACHED();
          break;
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
      .AddExtension("webm");
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
  const base::FilePath path = delegate_->GetActiveUserDownloadsDir();
  base::Time::Exploded exploded_time;
  timestamp.LocalExplode(&exploded_time);

  return path.AppendASCII(base::StringPrintf(
      format_string, GetDateStr(exploded_time).c_str(),
      GetTimeStr(exploded_time, delegate_->Uses24HourFormat()).c_str()));
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
  // If this event is dispatched after the capture session was cancelled or
  // destroyed, this should be a no-op.
  if (!IsActive())
    return;

  const base::Optional<CaptureParams> capture_params = GetCaptureParams();
  // Stop the capture session now, so the bar doesn't show up in the captured
  // video.
  Stop();

  if (!capture_params)
    return;

  // We enable the software-composited cursor, in order for the video capturer
  // to be able to record it.
  is_recording_in_progress_ = true;
  Shell::Get()->UpdateCursorCompositingEnabled();
  video_recording_watcher_ =
      std::make_unique<VideoRecordingWatcher>(this, capture_params->window);

  constexpr size_t kVideoBufferCapacityBytes = 512 * 1024;

  // We use a threshold of 512 MB to end the video recording due to low disk
  // space, which is the same threshold as that used by the low disk space
  // notification (See low_disk_notification.cc).
  constexpr size_t kLowDiskSpaceThresholdInBytes = 512 * 1024 * 1024;

  // The |video_file_handler_| performs all its tasks on the
  // |blocking_task_runner_|. However, we want the low disk space callback to be
  // run on the UI thread.
  base::OnceClosure on_low_disk_space_callback =
      base::BindPostTask(base::ThreadTaskRunnerHandle::Get(),
                         base::BindOnce(&CaptureModeController::OnLowDiskSpace,
                                        weak_ptr_factory_.GetWeakPtr()));

  DCHECK(current_video_file_path_.empty());
  recording_start_time_ = base::TimeTicks::Now();
  current_video_file_path_ = BuildVideoPath();
  video_file_handler_ = VideoFileHandler::Create(
      blocking_task_runner_, current_video_file_path_,
      kVideoBufferCapacityBytes, kLowDiskSpaceThresholdInBytes,
      std::move(on_low_disk_space_callback));
  video_file_handler_.AsyncCall(&VideoFileHandler::Initialize)
      .Then(on_video_file_status_);

  LaunchRecordingServiceAndStartRecording(*capture_params);

  delegate_->StartObservingRestrictedContent(
      capture_params->window, capture_params->bounds,
      base::BindOnce(&CaptureModeController::InterruptVideoRecording,
                     weak_ptr_factory_.GetWeakPtr()));

  capture_mode_util::SetStopRecordingButtonVisibility(
      capture_params->window->GetRootWindow(), true);
}

void CaptureModeController::InterruptVideoRecording() {
  ShowVideoRecordingStoppedNotification();
  EndVideoRecording(EndRecordingReason::kDlpInterruption);
}

void CaptureModeController::OnLowDiskSpace() {
  DCHECK(base::CurrentUIThread::IsSet());

  low_disk_space_threshold_reached_ = true;
  // We end the video recording normally (i.e. we don't consider this to be a
  // failure). The low disk space threashold was chosen to be big enough to
  // allow the remaining chunks to be saved normally. However,
  // |low_disk_space_threshold_reached_| will be used to display a different
  // message in the notification.
  EndVideoRecording(EndRecordingReason::kLowDiskSpace);
}

}  // namespace ash
