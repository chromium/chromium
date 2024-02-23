// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include <utility>

namespace {

ChromeProcessSingleton* g_chrome_process_singleton_ = nullptr;

}  // namespace

ChromeProcessSingleton::ChromeProcessSingleton(
    const base::FilePath& user_data_dir)
    : startup_lock_(
          base::BindRepeating(&ChromeProcessSingleton::NotificationCallback,
                              base::Unretained(this))),
      process_singleton_(user_data_dir,
                         startup_lock_.AsNotificationCallback()) {}

ChromeProcessSingleton::~ChromeProcessSingleton() = default;

ProcessSingleton::NotifyResult
    ChromeProcessSingleton::NotifyOtherProcessOrCreate() {
  CHECK(!is_singleton_instance_);
  ProcessSingleton::NotifyResult result =
      process_singleton_.NotifyOtherProcessOrCreate();
  if (result == ProcessSingleton::PROCESS_NONE) {
    is_singleton_instance_ = true;
  }
  return result;
}

void ChromeProcessSingleton::StartWatching() {
  process_singleton_.StartWatching();
}

void ChromeProcessSingleton::Cleanup() {
  if (is_singleton_instance_) {
    process_singleton_.Cleanup();
  }
}

void ChromeProcessSingleton::Unlock(
    const ProcessSingleton::NotificationCallback& notification_callback) {
  notification_callback_ = notification_callback;
  startup_lock_.Unlock();
}

// static
void ChromeProcessSingleton::CreateInstance(
    const base::FilePath& user_data_dir) {
  DCHECK(!g_chrome_process_singleton_);
  DCHECK(!user_data_dir.empty());
  g_chrome_process_singleton_ = new ChromeProcessSingleton(user_data_dir);
}

// static
void ChromeProcessSingleton::DeleteInstance() {
  if (g_chrome_process_singleton_) {
    delete g_chrome_process_singleton_;
    g_chrome_process_singleton_ = nullptr;
  }
}

// static
ChromeProcessSingleton* ChromeProcessSingleton::GetInstance() {
  CHECK(g_chrome_process_singleton_);
  return g_chrome_process_singleton_;
}

// static
bool ChromeProcessSingleton::IsSingletonInstance() {
  return g_chrome_process_singleton_ &&
         g_chrome_process_singleton_->is_singleton_instance_;
}

bool ChromeProcessSingleton::NotificationCallback(
    base::CommandLine command_line,
    const base::FilePath& current_directory) {
  DCHECK(notification_callback_);
  return notification_callback_.Run(std::move(command_line), current_directory);
}
