// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"

#include <memory>
#include <utility>

#include "ash/capture_mode/capture_mode_session.h"
#include "ash/public/cpp/capture_mode_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

namespace {

CaptureModeController* g_instance = nullptr;

const char kScreenCaptureNotificationId[] = "capture_mode_notification";
const char kScreenCaptureNotifierId[] = "ash.capture_mode_controller";

// The notification button index.
enum NotificationButtonIndex {
  BUTTON_EDIT = 0,
  BUTTON_DELETE,
};

}  // namespace

CaptureModeController::CaptureModeController(
    std::unique_ptr<CaptureModeDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
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
  // TODO(afakhry): Fill in here.
  Stop();
}

void CaptureModeController::EndVideoRecording() {
  // TODO(afakhry): Fill in here.
}

void CaptureModeController::ShowNotification(
    const base::FilePath& screen_capture_path) {
  const base::string16 title =
      l10n_util::GetStringUTF16(type_ == CaptureModeType::kImage
                                    ? IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_TITLE
                                    : IDS_ASH_SCREEN_CAPTURE_RECORDING_TITLE);
  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_MESSAGE);

  message_center::RichNotificationData optional_field;
  message_center::ButtonInfo edit_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_EDIT));
  optional_field.buttons.push_back(edit_button);
  message_center::ButtonInfo delete_button(
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_BUTTON_DELETE));
  optional_field.buttons.push_back(delete_button);

  // TODO: Assign image for screenshot or screenrecording preview. For now it's
  // an empty image.
  optional_field.image = gfx::Image();

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_IMAGE, kScreenCaptureNotificationId,
          title, message,
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kScreenCaptureNotifierId),
          optional_field,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &CaptureModeController::HandleNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr(), screen_capture_path)),
          kCaptureModeIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  // Remove the previous notification before showing the new one if there is
  // one.
  message_center::MessageCenter::Get()->RemoveNotification(
      kScreenCaptureNotificationId, /*by_user=*/false);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void CaptureModeController::HandleNotificationClicked(
    const base::FilePath& screen_capture_path,
    base::Optional<int> button_index) {
  if (!button_index.has_value()) {
    // Show the item in the folder.
    delegate_->ShowScreenCaptureItemInFolder(screen_capture_path);
    message_center::MessageCenter::Get()->RemoveNotification(
        kScreenCaptureNotificationId, /*by_user=*/false);
    return;
  }

  // TODO: fill in here.
  switch (button_index.value()) {
    case NotificationButtonIndex::BUTTON_EDIT:
      break;
    case NotificationButtonIndex::BUTTON_DELETE:
      break;
  }
}

}  // namespace ash
