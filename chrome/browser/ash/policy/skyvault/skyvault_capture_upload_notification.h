// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_CAPTURE_UPLOAD_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_CAPTURE_UPLOAD_NOTIFICATION_H_

#include "base/memory/weak_ptr.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy::skyvault {

// This class owns and manages a `message_center::Notification` to display the
// progress of an uploaded capture item.
class SkyvaultCaptureUploadNotification {
 public:
  explicit SkyvaultCaptureUploadNotification(const base::FilePath& filename);
  SkyvaultCaptureUploadNotification(const SkyvaultCaptureUploadNotification&) =
      delete;
  SkyvaultCaptureUploadNotification& operator=(
      const SkyvaultCaptureUploadNotification&) = delete;
  ~SkyvaultCaptureUploadNotification();

  // Updates the notification to reflect the current progress.
  void UpdateProgress(int64_t bytes_so_far);

  // Returns a weak pointer to the current object
  base::WeakPtr<SkyvaultCaptureUploadNotification> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Sets cancel callback.
  void SetCancelClosure(base::OnceClosure cancel_closure);

 private:
  // Callback that is called when "Cancel" button is pressed.
  void OnButtonPressed(std::optional<int> button_index);

  // Callback once the uploaded file size was retrieved.
  void OnFileSizeRetrieved(std::optional<int64_t> file_size);

  // The owned notification.
  std::unique_ptr<message_center::Notification> notification_;

  // File size, if already retrieved.
  std::optional<int64_t> file_size_;

  // Callback that will be called once upload should be cancelled.
  base::OnceClosure cancel_closure_;

  base::WeakPtrFactory<SkyvaultCaptureUploadNotification> weak_ptr_factory_{
      this};
};

}  // namespace policy::skyvault

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_SKYVAULT_CAPTURE_UPLOAD_NOTIFICATION_H_
