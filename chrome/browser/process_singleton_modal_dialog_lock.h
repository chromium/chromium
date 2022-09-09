// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_MODAL_DIALOG_LOCK_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_MODAL_DIALOG_LOCK_H_

#include "base/callback.h"
#include "chrome/browser/process_singleton.h"

namespace base {
class CommandLine;
class FilePath;
}

// Provides a ProcessSingleton::NotificationCallback that allows for closing a
// modal dialog that is active during startup. The client must ensure that
// SetModalDialogCallback is called appropriately when such dialogs are
// displayed or dismissed.
//
// After invoking the modal dialog's callback, this process will continue normal
// processing of the command line by forwarding the notification to a wrapped
// NotificationCallback.
class ProcessSingletonModalDialogLock {
 public:
  explicit ProcessSingletonModalDialogLock(
      const ProcessSingleton::NotificationCallback& original_callback);

  ProcessSingletonModalDialogLock(const ProcessSingletonModalDialogLock&) =
      delete;
  ProcessSingletonModalDialogLock& operator=(
      const ProcessSingletonModalDialogLock&) = delete;

  ~ProcessSingletonModalDialogLock();

  // Receives a callback to be run to close the active modal dialog, or an empty
  // closure if the active dialog is dismissed.
  void SetModalDialogNotificationHandler(
      base::RepeatingClosure notification_handler);

  // Returns the ProcessSingleton::NotificationCallback.
  // The callback is only valid during the lifetime of the
  // ProcessSingletonModalDialogLock instance.
  ProcessSingleton::NotificationCallback AsNotificationCallback();

 private:
  bool NotificationCallbackImpl(const base::CommandLine& command_line,
                                const base::FilePath& current_directory);

  base::RepeatingClosure notification_handler_;
  ProcessSingleton::NotificationCallback original_callback_;
};

#endif  // CHROME_BROWSER_PROCESS_SINGLETON_MODAL_DIALOG_LOCK_H_
