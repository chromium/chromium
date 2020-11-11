// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_manager_wrapper.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
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

ExtensionInstallEventLogManagerWrapper::
    ~ExtensionInstallEventLogManagerWrapper() = default;

// static
ExtensionInstallEventLogManagerWrapper*
ExtensionInstallEventLogManagerWrapper::CreateForProfile(Profile* profile) {
  if (!profile->GetUserCloudPolicyManagerChromeOS())
    return nullptr;
  ExtensionInstallEventLogManagerWrapper* wrapper =
      new ExtensionInstallEventLogManagerWrapper(profile);
  wrapper->Init();
  return wrapper;
}

// static
void ExtensionInstallEventLogManagerWrapper::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kExtensionInstallEventLoggingEnabled,
                                true);
}

void ExtensionInstallEventLogManagerWrapper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

ExtensionInstallEventLogManagerWrapper::ExtensionInstallEventLogManagerWrapper(
    Profile* profile)
    : profile_(profile) {
  log_task_runner_ =
      std::make_unique<ExtensionInstallEventLogManager::LogTaskRunnerWrapper>();

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kExtensionInstallEventLoggingEnabled,
      base::BindRepeating(&ExtensionInstallEventLogManagerWrapper::EvaluatePref,
                          base::Unretained(this)));
  notification_registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                              content::NotificationService::AllSources());
}

void ExtensionInstallEventLogManagerWrapper::Init() {
  EvaluatePref();
}

void ExtensionInstallEventLogManagerWrapper::CreateManager() {
  log_manager_ = std::make_unique<ExtensionInstallEventLogManager>(
      log_task_runner_.get(),
      profile_->GetUserCloudPolicyManagerChromeOS()
          ->GetExtensionInstallEventLogUploader(),
      profile_);
}

void ExtensionInstallEventLogManagerWrapper::DestroyManager() {
  log_manager_.reset();
}

void ExtensionInstallEventLogManagerWrapper::EvaluatePref() {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kExtensionInstallEventLoggingEnabled)) {
    if (!log_manager_) {
      CreateManager();
    }
  } else {
    DestroyManager();
    ExtensionInstallEventLogManager::Clear(log_task_runner_.get(), profile_);
  }
}

}  // namespace policy
