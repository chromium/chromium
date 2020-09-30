// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
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
constexpr char kScreenCaptureNotifierId[] = "ash.capture_mode_controller";

// The format strings of the file names of captured images.
// TODO(afakhry): Discuss with UX localizing "Screenshot" and "Screen
// recording".
constexpr char kScreenshotFileNameFmtStr[] = "Screenshot %s %s.png";
constexpr char kVideoFileNameFmtStr[] = "Screen recording %s %s.webm";
constexpr char kDateFmtStr[] = "%d-%02d-%02d";
constexpr char k24HourTimeFmtStr[] = "%02d.%02d.%02d";
constexpr char kAmPmTimeFmtStr[] = "%d.%02d.%02d";

// The notification button index.
enum NotificationButtonIndex {
  BUTTON_EDIT = 0,
  BUTTON_DELETE,
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

void DeleteFileAsync(const base::FilePath& path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&base::DeleteFile, path),
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
  ShowNotification(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISABLED_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISABLED_MESSAGE),
      /*optional_fields=*/{}, /*delegate=*/nullptr);
}

// Shows a notification informing the user that a Capture Mode operation has
// failed.
void ShowFailureNotification() {
  ShowNotification(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_FAILURE_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_FAILURE_MESSAGE),
      /*optional_fields=*/{}, /*delegate=*/nullptr);
}

// Copies the bitmap representation of the given |image| to the clipboard.
void CopyImageToClipboard(const gfx::Image& image) {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  DCHECK(clipboard);
  auto clipboard_data = std::make_unique<ui::ClipboardData>();
  clipboard_data->SetBitmapData(image.AsBitmap());
  clipboard->WriteClipboardData(std::move(clipboard_data));
}

// Shows the stop-recording button in the Shelf's status area widget. Note that
// the button hides itself when clicked.
void ShowStopRecordingButton(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto* stop_recording_button = RootWindowController::ForWindow(root)
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  stop_recording_button->SetVisiblePreferred(true);
}

}  // namespace

CaptureModeController::CaptureModeController(
    std::unique_ptr<CaptureModeDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

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

void CaptureModeController::Start() {
  if (capture_mode_session_)
    return;

  // TODO(afakhry): Use root window of the mouse cursor or the one for new
  // windows.
  capture_mode_session_ =
      std::make_unique<CaptureModeSession>(this, Shell::GetPrimaryRootWindow());
}

void CaptureModeController::Stop() {
  capture_mode_session_.reset();
}

void CaptureModeController::PerformCapture() {
  DCHECK(IsActive());

  if (!IsCaptureAllowed()) {
    ShowDisabledNotification();
    Stop();
    return;
  }

  if (type_ == CaptureModeType::kImage)
    CaptureImage();
  else
    CaptureVideo();

  // The above capture functions should have ended the session.
  DCHECK(!IsActive());
}

void CaptureModeController::EndVideoRecording() {
  // TODO(afakhry): We should instead ask the recording service to stop
  // recording, and only do the below when the service tells us that it's done
  // with all the frames.
  is_recording_in_progress_ = false;
  Shell::Get()->UpdateCursorCompositingEnabled();
}

bool CaptureModeController::IsCaptureAllowed() const {
  // TODO(afakhry): Fill in here.
  return true;
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
      // In video mode, the recording service is not given any bounds as it
      // should just use the same bounds of the frame captured from the root
      // window.
      if (type_ == CaptureModeType::kImage)
        bounds = window->bounds();
      break;

    case CaptureModeSource::kWindow:
      window = capture_mode_session_->GetSelectedWindow();
      if (!window) {
        // TODO(afakhry): Consider showing a toast or a notification that no
        // window was selected.
        return base::nullopt;
      }
      // Also here, the recording service will use the same frame size as
      // captured from |window| and does not need any crop bounds.
      if (type_ == CaptureModeType::kImage) {
        // window->bounds() are in root coordinates, but we want to get the
        // capture area in |window|'s coordinates.
        bounds = gfx::Rect(window->bounds().size());
      }
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

  const base::Optional<CaptureParams> capture_params = GetCaptureParams();
  // Stop the capture session now, so the bar doesn't show up in the captured
  // video.
  Stop();

  if (!capture_params)
    return;

  // We provide the service with no crop bounds except when we're capturing a
  // custom region.
  DCHECK_EQ(source_ != CaptureModeSource::kRegion,
            capture_params->bounds.IsEmpty());

  // We enable the software-composited cursor, in order for the video capturer
  // to be able to record it.
  is_recording_in_progress_ = true;
  Shell::Get()->UpdateCursorCompositingEnabled();

  // TODO(afakhry): Call into the recording service.

  ShowStopRecordingButton(capture_params->window->GetRootWindow());
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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&SaveFile, png_bytes, path),
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

  DCHECK(png_bytes && png_bytes->size());
  const auto image = gfx::Image::CreateFrom1xPNGBytes(png_bytes);
  CopyImageToClipboard(image);
  ShowPreviewNotification(path, image);

  if (features::IsTemporaryHoldingSpaceEnabled())
    HoldingSpaceController::Get()->client()->AddScreenshot(path);
}

void CaptureModeController::ShowPreviewNotification(
    const base::FilePath& screen_capture_path,
    const gfx::Image& preview_image) {
  const base::string16 title =
      l10n_util::GetStringUTF16(type_ == CaptureModeType::kImage
                                    ? IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_TITLE
                                    : IDS_ASH_SCREEN_CAPTURE_RECORDING_TITLE);
  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_MESSAGE);

  message_center::RichNotificationData optional_fields;
  message_center::ButtonInfo edit_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT));
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
                              screen_capture_path)));
}

void CaptureModeController::HandleNotificationClicked(
    const base::FilePath& screen_capture_path,
    base::Optional<int> button_index) {
  if (!button_index.has_value()) {
    // Show the item in the folder.
    delegate_->ShowScreenCaptureItemInFolder(screen_capture_path);
  } else {
    // TODO: fill in here.
    switch (button_index.value()) {
      case NotificationButtonIndex::BUTTON_EDIT:
        break;
      case NotificationButtonIndex::BUTTON_DELETE:
        DeleteFileAsync(screen_capture_path);
        break;
    }
  }

  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenCaptureNotificationId, /*by_user=*/false);
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

}  // namespace ash
