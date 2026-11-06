// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}

namespace crostini {

class CrostiniExportImportClickCloseDelegate
    : public message_center::HandleNotificationClickDelegate {
 public:
  using HandleNotificationClickDelegate::ButtonClickCallback;

  CrostiniExportImportClickCloseDelegate();

  void SetCloseCallback(base::RepeatingClosure close_closure) {
    close_closure_ = std::move(close_closure);
  }

  // NotificationDelegate overrides:
  void Close(bool by_user) override;

 private:
  ~CrostiniExportImportClickCloseDelegate() override;

  base::RepeatingClosure close_closure_{};
};

enum class ExportImportType;

// Controller for Crostini's Export Import notification UI. It also tracks the
// status of the export/import operation, which CrostiniExportImport updates via
// the SetStatus*() methods.
// Upon construction the Controller will create a new notification, displaying
// the StatusRunning UI. If the controller is freed the notification will
// continue to be shown and handle click events until the user closes it.
class CrostiniExportImportNotificationController {
 public:
  enum class Status {
    NONE,
    RUNNING,
    CANCELLING,
    DONE,
    CANCELLED,
    FAILED_UNKNOWN_REASON,
    FAILED_ARCHITECTURE_MISMATCH,
    FAILED_INSUFFICIENT_SPACE,
    FAILED_CONCURRENT_OPERATION,
    FAILED_BAD_IMAGE,
  };

  CrostiniExportImportNotificationController(Profile* profile,
                                             ExportImportType type,
                                             const std::string& notification_id,
                                             base::FilePath path,
                                             guest_os::GuestId container_id);

  CrostiniExportImportNotificationController(
      const CrostiniExportImportNotificationController&) = delete;
  CrostiniExportImportNotificationController& operator=(
      const CrostiniExportImportNotificationController&) = delete;

  ~CrostiniExportImportNotificationController();

  Status status() const { return status_; }
  ExportImportType type() const { return type_; }
  const base::FilePath& path() const { return path_; }

  // Can be used to draw attention to the UI without changing its
  // status, even if it has been hidden.
  void ForceRedisplay();

  void SetStatusRunning(int progress_percent);
  void SetStatusCancelling();
  void SetStatusDone();
  void SetStatusCancelled();

  void SetStatusFailed();
  void SetStatusFailedArchitectureMismatch(
      const std::string& architecture_container,
      const std::string& architecture_device);
  void SetStatusFailedInsufficientSpace(uint64_t additional_required_space);
  void SetStatusFailedInsufficientSpaceUnknownAmount();
  void SetStatusFailedConcurrentOperation(
      ExportImportType in_progress_operation_type);
  void SetStatusFailedBadImage();

  // Getters for testing.
  message_center::Notification* get_notification() {
    return notification_.get();
  }
  base::WeakPtr<CrostiniExportImportNotificationController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }
  message_center::NotificationObserver* get_delegate() {
    return delegate_.get();
  }

 private:
  void SetStatusRunningUI(int progress_percent);
  void SetStatusCancellingUI();
  void SetStatusDoneUI();
  void SetStatusCancelledUI();
  void SetStatusFailedWithMessage(Status status, const std::u16string& message);
  void SetStatusFailedWithMessageUI(Status status,
                                    const std::u16string& message);

  void on_notification_closed() { hidden_ = true; }

  ExportImportType type_;
  base::FilePath path_;
  Status status_ = Status::NONE;
  raw_ptr<Profile> profile_;  // Not owned.
  guest_os::GuestId container_id_;
  // |delegate_| is responsible for handling click events. It is separate from
  // the controller because it needs to live as long as the notification is in
  // the UI, but the controller's lifetime ends once the notification is in a
  // final state (done, canceled, or failed).
  scoped_refptr<CrostiniExportImportClickCloseDelegate> delegate_;

  // Time when the operation started.  Used for estimating time remaining.
  base::TimeTicks started_ = base::TimeTicks::Now();
  // |notification_| acts as a handle to the notification UI, it is used to
  // update the UI's progress/message/buttons. Freeing |notification_| doesn't
  // close the notification UI; the UI system is responsible for closing it.
  std::unique_ptr<message_center::Notification> notification_;
  bool hidden_ = false;
  base::WeakPtrFactory<CrostiniExportImportNotificationController>
      weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_CONTROLLER_H_
