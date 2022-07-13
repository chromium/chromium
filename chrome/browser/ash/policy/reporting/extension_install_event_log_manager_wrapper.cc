// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/extension_install_event_log_manager_wrapper.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

ExtensionInstallEventLogManagerWrapper::
    ~ExtensionInstallEventLogManagerWrapper() = default;

// static
ExtensionInstallEventLogManagerWrapper*
ExtensionInstallEventLogManagerWrapper::CreateForProfile(Profile* profile) {
  if (!profile->GetUserCloudPolicyManagerAsh())
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
  app_terminating_subscription_ = browser_shutdown::AddAppTerminatingCallback(
      base::BindOnce(&ExtensionInstallEventLogManagerWrapper::OnAppTerminating,
                     base::Unretained(this)));
}

void ExtensionInstallEventLogManagerWrapper::Init() {
  EvaluatePref();
}

void ExtensionInstallEventLogManagerWrapper::CreateManager() {
  log_manager_ = std::make_unique<ExtensionInstallEventLogManager>(
      log_task_runner_.get(),
      profile_->GetUserCloudPolicyManagerAsh()
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

void ExtensionInstallEventLogManagerWrapper::OnAppTerminating() {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace policy
