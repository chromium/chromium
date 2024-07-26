// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/skyvault_capture_upload_notification.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy::skyvault {

namespace {

std::optional<int64_t> GetFileSize(const base::FilePath& filename) {
  int64_t size;
  if (!base::GetFileSize(filename, &size)) {
    return std::nullopt;
  }
  return size;
}

constexpr char kUploadNotificationId[] = "skyvault_capture_upload_notification";

}  // namespace

SkyvaultCaptureUploadNotification::SkyvaultCaptureUploadNotification(
    const base::FilePath& filename) {
  message_center::RichNotificationData options;
  options.buttons = {message_center::ButtonInfo(l10n_util::GetStringUTF16(
      IDS_POLICY_SKYVAULT_SCREENCAPTURE_UPLOAD_CANCEL_BUTTON))};
  options.progress_status = l10n_util::GetStringUTF16(
      IDS_POLICY_SKYVAULT_SCREENCAPTURE_UPLOAD_ONEDRIVE_MESSAGE);

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, kUploadNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_SCREENCAPTURE_UPLOAD_TITLE),
      /*message=*/std::u16string(), ui::ImageModel(),
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kUploadNotificationId,
                                 ash::NotificationCatalogName::kScreenCapture),
      options,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &SkyvaultCaptureUploadNotification::OnButtonPressed,
              GetWeakPtr())));

  // Set up indefinite progress first.
  notification_->set_progress(-1);
  notification_->set_vector_small_image(ash::kCaptureModeIcon);

  SystemNotificationHelper::GetInstance()->Display(*notification_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetFileSize, filename),
      base::BindOnce(&SkyvaultCaptureUploadNotification::OnFileSizeRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

SkyvaultCaptureUploadNotification::~SkyvaultCaptureUploadNotification() {
  SystemNotificationHelper::GetInstance()->Close(kUploadNotificationId);
}

void SkyvaultCaptureUploadNotification::UpdateProgress(int64_t bytes_so_far) {
  if (file_size_.has_value()) {
    const auto percent = 100.0 * bytes_so_far / file_size_.value();
    notification_->set_progress(static_cast<int>(percent));
    SystemNotificationHelper::GetInstance()->Display(*notification_);
  }
}

void SkyvaultCaptureUploadNotification::SetCancelClosure(
    base::OnceClosure cancel_closure) {
  cancel_closure_ = std::move(cancel_closure);
}

void SkyvaultCaptureUploadNotification::OnButtonPressed(
    std::optional<int> button_index) {
  if (!button_index || button_index.value() != 0) {
    return;
  }
  if (cancel_closure_) {
    std::move(cancel_closure_).Run();
  }
}

void SkyvaultCaptureUploadNotification::OnFileSizeRetrieved(
    std::optional<int64_t> file_size) {
  file_size_ = file_size;
  UpdateProgress(0);
}

}  // namespace policy::skyvault
