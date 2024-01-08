// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_export_import_status_tracker.h"
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

// Controller for Crostini's Export Import notification UI.
// Upon construction the Controller will create a new notification, displaying
// the StatusRunning UI. If the controller is freed the notification will
// continue to be shown and handle click events until the user closes it.
class CrostiniExportImportNotificationController
    : public CrostiniExportImportStatusTracker {
 public:
  CrostiniExportImportNotificationController(Profile* profile,
                                             ExportImportType type,
                                             const std::string& notification_id,
                                             base::FilePath path,
                                             guest_os::GuestId container_id);

  CrostiniExportImportNotificationController(
      const CrostiniExportImportNotificationController&) = delete;
  CrostiniExportImportNotificationController& operator=(
      const CrostiniExportImportNotificationController&) = delete;

  ~CrostiniExportImportNotificationController() override;

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
  // CrostiniExportImportStatusTracker:
  void ForceRedisplay() override;
  void SetStatusRunningUI(int progress_percent) override;
  void SetStatusCancellingUI() override;
  void SetStatusDoneUI() override;
  void SetStatusCancelledUI() override;
  void SetStatusFailedWithMessageUI(Status status,
                                    const std::u16string& message) override;

  void on_notification_closed() { hidden_ = true; }

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
