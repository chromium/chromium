// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/process_singleton_modal_dialog_lock.h"

#include <utility>

#include "base/bind.h"

ProcessSingletonModalDialogLock::ProcessSingletonModalDialogLock(
    const ProcessSingleton::NotificationCallback& original_callback)
    : original_callback_(original_callback) {}

ProcessSingletonModalDialogLock::~ProcessSingletonModalDialogLock() {}

void ProcessSingletonModalDialogLock::SetModalDialogNotificationHandler(
    base::RepeatingClosure notification_handler) {
  notification_handler_ = std::move(notification_handler);
}

ProcessSingleton::NotificationCallback
ProcessSingletonModalDialogLock::AsNotificationCallback() {
  return base::BindRepeating(
      &ProcessSingletonModalDialogLock::NotificationCallbackImpl,
      base::Unretained(this));
}

bool ProcessSingletonModalDialogLock::NotificationCallbackImpl(
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  if (notification_handler_)
    notification_handler_.Run();

  return original_callback_.Run(command_line, current_directory);
}
