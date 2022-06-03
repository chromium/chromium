// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include <utility>

ChromeProcessSingleton::ChromeProcessSingleton(
    const base::FilePath& user_data_dir,
    const ProcessSingleton::NotificationCallback& notification_callback)
    : startup_lock_(notification_callback),
      modal_dialog_lock_(startup_lock_.AsNotificationCallback()),
      process_singleton_(user_data_dir,
                         modal_dialog_lock_.AsNotificationCallback()) {
}

ChromeProcessSingleton::~ChromeProcessSingleton() {
}

ProcessSingleton::NotifyResult
    ChromeProcessSingleton::NotifyOtherProcessOrCreate() {
  return process_singleton_.NotifyOtherProcessOrCreate();
}

void ChromeProcessSingleton::Cleanup() {
  process_singleton_.Cleanup();
}

void ChromeProcessSingleton::SetModalDialogNotificationHandler(
    base::RepeatingClosure notification_handler) {
  modal_dialog_lock_.SetModalDialogNotificationHandler(
      std::move(notification_handler));
}

void ChromeProcessSingleton::Unlock() {
  startup_lock_.Unlock();
}
