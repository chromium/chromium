// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <memory>
#include <string>
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
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "base/bind.h"
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

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";
constexpr char kScreenCaptureStoppedNotificationId[] =
    "capture_mode_stopped_notification";
constexpr char kScreenCaptureNotifierId[] = "ash.capture_mode_controller";

// The format strings of the file names of captured images.
// TODO(afakhry): Discuss with UX localizing "Screenshot" and "Screen
// recording".
constexpr char kScreenshotFileNameFmtStr[] = "Screenshot %s %s.png";
constexpr char kVideoFileNameFmtStr[] = "Screen recording %s %s.webm";
constexpr char kDateFmtStr[] = "%d-%02d-%02d";
constexpr char k24HourTimeFmtStr[] = "%02d.%02d.%02d";
constexpr char kAmPmTimeFmtStr[] = "%d.%02d.%02d";

// The amount of time to wait before attempting to relaunch the recording
// service if it crashes and gets disconnected.
constexpr base::TimeDelta kReconnectDelay =
    base::TimeDelta::FromMilliseconds(100);

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
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          // A task priority of BEST_EFFORT is good enough for this runner,
          // since it's used for blocking file IO such as saving the screenshots
          // or the successive webm video chunks received from the recording
          // service.
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      recording_service_client_receiver_(this) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  on_video_file_status_ =
      base::BindRepeating(&CaptureModeController::OnVideoFileStatus,
                          weak_ptr_factory_.GetWeakPtr());

  // Schedule recording of the number of screenshots taken per day.
  num_screenshots_taken_in_last_day_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(1),
      base::BindRepeating(
          &CaptureModeController::RecordNumberOfScreenshotsTakenInLastDay,
          weak_ptr_factory_.GetWeakPtr()));

  // Schedule recording of the number of screenshots taken per week.
  num_screenshots_taken_in_last_week_scheduler_.Start(
      FROM_HERE, base::TimeDelta::FromDays(7),
      base::BindRepeating(
          &CaptureModeController::RecordNumberOfScreenshotsTakenInLastWeek,
          weak_ptr_factory_.GetWeakPtr()));

  // TODO(afakhry): Explore starting this only when a video recording starts, so
  // as not to consume system resources while idle. https://crbug.com/1143411.
  LaunchRecordingService();
}

CaptureModeController::~CaptureModeController() {
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

  RecordCaptureModeEntryType(entry_type);
  capture_mode_session_ = std::make_unique<CaptureModeSession>(this);
}

void CaptureModeController::Stop() {
  DCHECK(IsActive());
  capture_mode_session_.reset();
}

void CaptureModeController::PerformCapture() {
  DCHECK(IsActive());

  if (!IsCaptureAllowed()) {
    ShowDisabledNotification();
    Stop();
    return;
  }

  DCHECK(capture_mode_session_);
  capture_mode_session_->ReportSessionHistograms();

  if (type_ == CaptureModeType::kImage)
    CaptureImage();
  else
    CaptureVideo();
}

void CaptureModeController::EndVideoRecording() {
  recording_service_remote_->StopRecording();
  TerminateRecordingUiElements();
}

void CaptureModeController::OpenFeedbackDialog() {
  delegate_->OpenFeedbackDialog();
}

void CaptureModeController::BindVideoCapturer(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> receiver) {
  if (!is_recording_in_progress_ || !recording_service_remote_.is_connected()) {
    NOTREACHED();
    return;
  }

  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->CreateVideoCapturer(std::move(receiver));
}

void CaptureModeController::BindAudioStreamFactory(
    mojo::PendingReceiver<audio::mojom::StreamFactory> receiver) {
  if (!is_recording_in_progress_ || !recording_service_remote_.is_connected()) {
    NOTREACHED();
    return;
  }

  delegate_->BindAudioStreamFactory(std::move(receiver));
}

void CaptureModeController::OnMuxerOutput(const std::string& chunk) {
  DCHECK(video_file_handler_);
  video_file_handler_.AsyncCall(&VideoFileHandler::AppendChunk)
      .WithArgs(const_cast<std::string&>(chunk))
      .Then(on_video_file_status_);
}

void CaptureModeController::OnRecordingEnded(bool success) {
  delegate_->StopObservingRestrictedContent();
  window_frame_sink_.reset();

  // If |success| is false, then recording has been force-terminated due to a
  // failure on the service side, or a disconnection to it. We need to terminate
  // the recording-related UI elements.
  if (!success) {
    // TODO(afakhry): Show user a failure message.
    TerminateRecordingUiElements();
  }

  DCHECK(video_file_handler_);
  video_file_handler_.AsyncCall(&VideoFileHandler::FlushBufferedChunks)
      .Then(base::BindOnce(&CaptureModeController::OnVideoFileSaved,
                           weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeController::StartVideoRecordingImmediatelyForTesting() {
  DCHECK(IsActive());
  DCHECK_EQ(type_, CaptureModeType::kVideo);
  OnVideoRecordCountDownFinished();
}

void CaptureModeController::LaunchRecordingService() {
  recording_service_remote_.reset();
  recording_service_client_receiver_.reset();
  recording_service_remote_ = delegate_->LaunchRecordingService();
  recording_service_remote_.set_disconnect_handler(
      base::BindOnce(&CaptureModeController::OnRecordingServiceDisconnected,
                     base::Unretained(this)));
  recording_service_remote_->SetClient(
      recording_service_client_receiver_.BindNewPipeAndPassRemote());
}

void CaptureModeController::OnRecordingServiceDisconnected() {
  // TODO(afakhry): Consider what to do if the service crashes during an ongoin
  // video recording. Do we try to resume recording, or notify with failure?
  // For now, just end the recording and relaunch the service.
  if (is_recording_in_progress_)
    OnRecordingEnded(/*success=*/false);

  // TODO(afakhry): Do we need an exponential backoff delay here?
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureModeController::LaunchRecordingService,
                     weak_ptr_factory_.GetWeakPtr()),
      kReconnectDelay);
}

bool CaptureModeController::IsCaptureAllowed() const {
  const base::Optional<CaptureParams> capture_params = GetCaptureParams();
  if (!capture_params)
    return false;
  return delegate_->IsCaptureAllowed(
      capture_params->window, capture_params->bounds,
      /*for_video=*/type_ == CaptureModeType::kVideo);
}

void CaptureModeController::TerminateRecordingUiElements() {
  is_recording_in_progress_ = false;
  Shell::Get()->UpdateCursorCompositingEnabled();
  capture_mode_util::SetStopRecordingButtonVisibility(
      video_recording_watcher_->window_being_recorded()->GetRootWindow(),
      false);
  video_recording_watcher_.reset();
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

void CaptureModeController::CaptureImage() {
  DCHECK_EQ(CaptureModeType::kImage, type_);
  DCHECK(IsCaptureAllowed());

  const base::Optional<CaptureParams> capture_params = GetCaptureParams();
  // Stop the capture session now, so as not to take a screenshot of the capture
  // bar.
  Stop();

  if (!capture_params)
    return;

  DCHECK(!capture_params->bounds.IsEmpty());

  ui::GrabWindowSnapshotAsyncPNG(
      capture_params->window, capture_params->bounds,
      base::BindOnce(&CaptureModeController::OnImageCaptured,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now()));

  ++num_screenshots_taken_in_last_day_;
  ++num_screenshots_taken_in_last_week_;
}

void CaptureModeController::CaptureVideo() {
  DCHECK_EQ(CaptureModeType::kVideo, type_);
  DCHECK(IsCaptureAllowed());

  if (skip_count_down_ui_) {
    OnVideoRecordCountDownFinished();
    return;
  }

  capture_mode_session_->StartCountDown(
      base::BindOnce(&CaptureModeController::OnVideoRecordCountDownFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CaptureModeController::OnImageCaptured(
    base::Time timestamp,
    scoped_refptr<base::RefCountedMemory> png_bytes) {
  if (!png_bytes || !png_bytes->size()) {
    LOG(ERROR) << "Failed to capture image.";
    ShowFailureNotification();
    return;
  }

  const base::FilePath path = BuildImagePath(timestamp);
  task_runner_->PostTaskAndReplyWithResult(
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

  if (features::IsTemporaryHoldingSpaceEnabled())
    HoldingSpaceController::Get()->client()->AddScreenshot(path);
}

void CaptureModeController::OnVideoFileStatus(bool success) {
  if (success)
    return;

  // TODO(afakhry): Show the user a message about IO failure.
  EndVideoRecording();
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
  }

  if (!on_file_saved_callback_.is_null())
    std::move(on_file_saved_callback_).Run(current_video_file_path_);

  recording_start_time_ = base::TimeTicks();
  current_video_file_path_.clear();
  video_file_handler_.Reset();
}

void CaptureModeController::ShowPreviewNotification(
    const base::FilePath& screen_capture_path,
    const gfx::Image& preview_image,
    const CaptureModeType type) {
  const base::string16 title = l10n_util::GetStringUTF16(
      type == CaptureModeType::kImage ? IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_TITLE
                                      : IDS_ASH_SCREEN_CAPTURE_RECORDING_TITLE);
  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_MESSAGE);

  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo edit_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT));
  if (type == CaptureModeType::kImage)
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
  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenCaptureNotificationId, /*by_user=*/false);

  if (!button_index.has_value()) {
    // Show the item in the folder.
    delegate_->ShowScreenCaptureItemInFolder(screen_capture_path);
    return;
  }

  const int button_index_value = button_index.value();

  // Handle a button clicked for a video preview notification.
  if (type == CaptureModeType::kVideo) {
    DCHECK_EQ(button_index_value,
              VideoNotificationButtonIndex::BUTTON_DELETE_VIDEO);
    DeleteFileAsync(task_runner_, screen_capture_path);
    return;
  }

  // Handle a button clicked for an image preview notification.
  DCHECK_EQ(type, CaptureModeType::kImage);
  switch (button_index_value) {
    case ScreenshotNotificationButtonIndex::BUTTON_EDIT:
      delegate_->OpenScreenshotInImageEditor(screen_capture_path);
      break;
    case ScreenshotNotificationButtonIndex::BUTTON_DELETE:
      DeleteFileAsync(task_runner_, screen_capture_path);
      break;
    default:
      NOTREACHED();
      break;
  }
}

base::FilePath CaptureModeController::BuildImagePath(
    base::Time timestamp) const {
  return BuildPath(kScreenshotFileNameFmtStr, timestamp);
}

base::FilePath CaptureModeController::BuildVideoPath(
    base::Time timestamp) const {
  return BuildPath(kVideoFileNameFmtStr, timestamp);
}

base::FilePath CaptureModeController::BuildPath(const char* const format_string,
                                                base::Time timestamp) const {
  const base::FilePath path = delegate_->GetActiveUserDownloadsDir();
  base::Time::Exploded exploded_time;
  timestamp.LocalExplode(&exploded_time);

  return path.AppendASCII(base::StringPrintf(
      format_string, GetDateStr(exploded_time).c_str(),
      GetTimeStr(exploded_time, delegate_->Uses24HourFormat()).c_str()));
}

void CaptureModeController::RecordNumberOfScreenshotsTakenInLastDay() {
  base::UmaHistogramCounts100("Ash.CaptureModeController.ScreenshotsPerDay",
                              num_screenshots_taken_in_last_day_);
  num_screenshots_taken_in_last_day_ = 0;
}

void CaptureModeController::RecordNumberOfScreenshotsTakenInLastWeek() {
  base::UmaHistogramCounts1000("Ash.CaptureModeController.ScreenshotsPerWeek",
                               num_screenshots_taken_in_last_week_);
  num_screenshots_taken_in_last_week_ = 0;
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

  // TODO(afakhry): Choose a real buffer capacity when the recording service is
  // in.
  constexpr size_t kVideoBufferCapacityBytes = 512 * 1024;
  DCHECK(current_video_file_path_.empty());
  recording_start_time_ = base::TimeTicks::Now();
  current_video_file_path_ = BuildVideoPath(base::Time::Now());
  video_file_handler_ = VideoFileHandler::Create(
      task_runner_, current_video_file_path_, kVideoBufferCapacityBytes);
  video_file_handler_.AsyncCall(&VideoFileHandler::Initialize)
      .Then(on_video_file_status_);

  DCHECK(recording_service_remote_.is_bound());
  DCHECK(recording_service_remote_.is_connected());

  auto frame_sink_id = capture_params->window->GetFrameSinkId();
  if (!frame_sink_id.is_valid()) {
    window_frame_sink_ = capture_params->window->CreateLayerTreeFrameSink();
    frame_sink_id = capture_params->window->GetFrameSinkId();
    DCHECK(frame_sink_id.is_valid());
  }
  const auto bounds = capture_params->bounds;
  switch (source_) {
    case CaptureModeSource::kFullscreen:
      recording_service_remote_->RecordFullscreen(frame_sink_id, bounds.size());
      break;

    case CaptureModeSource::kWindow:
      // TODO(crbug.com/1143930): Window recording doesn't produce any frames at
      // the moment.
      recording_service_remote_->RecordWindow(
          frame_sink_id, bounds.size(),
          capture_params->window->GetRootWindow()
              ->GetBoundsInRootWindow()
              .size());
      break;

    case CaptureModeSource::kRegion:
      recording_service_remote_->RecordRegion(
          frame_sink_id,
          capture_params->window->GetRootWindow()
              ->GetBoundsInRootWindow()
              .size(),
          bounds);
      break;
  }

  delegate_->StartObservingRestrictedContent(
      capture_params->window, capture_params->bounds,
      base::BindOnce(&CaptureModeController::InterruptVideoRecording,
                     weak_ptr_factory_.GetWeakPtr()));

  capture_mode_util::SetStopRecordingButtonVisibility(
      capture_params->window->GetRootWindow(), true);
}

void CaptureModeController::InterruptVideoRecording() {
  ShowVideoRecordingStoppedNotification();
  EndVideoRecording();
}

}  // namespace ash
