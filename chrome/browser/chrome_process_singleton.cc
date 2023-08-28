// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_singleton.h"

#include <utility>

#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/chrome_switches.h"

namespace {

constexpr char kEarlySingletonForceEnabledGroup[] = "Enabled_Forced3";
constexpr char kEarlySingletonEnabledGroup[] = "Enabled3";
constexpr char kEarlySingletonDisabledMergeGroup[] = "Disabled_Merge3";
constexpr char kEarlySingletonDefaultGroup[] = "Default3";

const char* g_early_singleton_feature_group_ = nullptr;
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
  if (headless::IsHeadlessMode()) {
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
  DCHECK(!g_early_singleton_feature_group_);
  if (command_line.HasSwitch(switches::kEnableEarlyProcessSingleton)) {
    g_early_singleton_feature_group_ = kEarlySingletonForceEnabledGroup;
    return;
  }

  g_early_singleton_feature_group_ = kEarlySingletonDefaultGroup;
}

// static
void ChromeProcessSingleton::RegisterEarlySingletonFeature() {
  DCHECK(g_early_singleton_feature_group_);
  // The synthetic trial needs to use kCurrentLog to ensure that UMA report will
  // be generated from the metrics log that is open at the time of registration.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "EarlyProcessSingleton", g_early_singleton_feature_group_,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}

// static
bool ChromeProcessSingleton::IsEarlySingletonFeatureEnabled() {
  return g_early_singleton_feature_group_ == kEarlySingletonEnabledGroup ||
         g_early_singleton_feature_group_ == kEarlySingletonForceEnabledGroup;
}

// static
bool ChromeProcessSingleton::ShouldMergeMetrics() {
  // This should not be called when the early singleton feature is enabled.
  DCHECK(g_early_singleton_feature_group_ && !IsEarlySingletonFeatureEnabled());

  return g_early_singleton_feature_group_ == kEarlySingletonDisabledMergeGroup;
}

bool ChromeProcessSingleton::NotificationCallback(
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  DCHECK(notification_callback_);
  return notification_callback_.Run(command_line, current_directory);
}
