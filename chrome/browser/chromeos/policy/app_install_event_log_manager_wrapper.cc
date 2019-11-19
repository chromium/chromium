// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_manager_wrapper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"

namespace policy {

AppInstallEventLogManagerWrapper::~AppInstallEventLogManagerWrapper() = default;

// static
AppInstallEventLogManagerWrapper*
AppInstallEventLogManagerWrapper::CreateForProfile(Profile* profile) {
  AppInstallEventLogManagerWrapper* wrapper =
      new AppInstallEventLogManagerWrapper(profile);
  wrapper->Init();
  return wrapper;
}

// static
void AppInstallEventLogManagerWrapper::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kArcAppInstallEventLoggingEnabled,
                                false);
}

void AppInstallEventLogManagerWrapper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

AppInstallEventLogManagerWrapper::AppInstallEventLogManagerWrapper(
    Profile* profile)
    : profile_(profile) {
  log_task_runner_ =
      std::make_unique<AppInstallEventLogManager::LogTaskRunnerWrapper>();

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kArcAppInstallEventLoggingEnabled,
      base::BindRepeating(&AppInstallEventLogManagerWrapper::EvaluatePref,
                          base::Unretained(this)));
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());
}

void AppInstallEventLogManagerWrapper::Init() {
  EvaluatePref();
}

void AppInstallEventLogManagerWrapper::CreateManager() {
  log_manager_ = std::make_unique<AppInstallEventLogManager>(
      log_task_runner_.get(),
      profile_->GetUserCloudPolicyManagerChromeOS()
          ->GetAppInstallEventLogUploader(),
      profile_);
}

void AppInstallEventLogManagerWrapper::DestroyManager() {
  log_manager_.reset();
}

void AppInstallEventLogManagerWrapper::EvaluatePref() {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kArcAppInstallEventLoggingEnabled)) {
    if (!log_manager_) {
      CreateManager();
    }
  } else {
    DestroyManager();
    AppInstallEventLogManager::Clear(log_task_runner_.get(), profile_);
  }
}

}  // namespace policy
