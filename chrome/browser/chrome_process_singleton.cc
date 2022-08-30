// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include <utility>

#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/chrome_switches.h"

namespace {
bool g_is_early_singleton_feature_ = false;
ChromeProcessSingleton* g_chrome_process_singleton_ = nullptr;
}  // namespace

ChromeProcessSingleton::ChromeProcessSingleton(
    const base::FilePath& user_data_dir)
    : startup_lock_(
          base::BindRepeating(&ChromeProcessSingleton::NotificationCallback,
                              base::Unretained(this))),
      modal_dialog_lock_(startup_lock_.AsNotificationCallback()),
      process_singleton_(user_data_dir,
                         modal_dialog_lock_.AsNotificationCallback()) {}

ChromeProcessSingleton::~ChromeProcessSingleton() = default;

ProcessSingleton::NotifyResult
    ChromeProcessSingleton::NotifyOtherProcessOrCreate() {
  // In headless mode we don't want to hand off pages to an existing processes,
  // so short circuit process singleton creation and bail out if we're not
  // the only process using this user data dir.
  if (headless::IsChromeNativeHeadless()) {
    return process_singleton_.Create() ? ProcessSingleton::PROCESS_NONE
                                       : ProcessSingleton::PROFILE_IN_USE;
  }
  return process_singleton_.NotifyOtherProcessOrCreate();
}

void ChromeProcessSingleton::StartWatching() {
  process_singleton_.StartWatching();
}

void ChromeProcessSingleton::Cleanup() {
  process_singleton_.Cleanup();
}

void ChromeProcessSingleton::SetModalDialogNotificationHandler(
    base::RepeatingClosure notification_handler) {
  modal_dialog_lock_.SetModalDialogNotificationHandler(
      std::move(notification_handler));
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
void ChromeProcessSingleton::SetupEarlySingletonFeature(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableEarlyProcessSingleton))
    g_is_early_singleton_feature_ = true;

  // TODO(1340599): Set up a synthetic trial for the early process singleton
  // experiment.
}

// static
bool ChromeProcessSingleton::IsEarlySingletonFeatureEnabled() {
  return g_is_early_singleton_feature_;
}

bool ChromeProcessSingleton::NotificationCallback(
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  DCHECK(notification_callback_);
  return notification_callback_.Run(command_line, current_directory);
}
