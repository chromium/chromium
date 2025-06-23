// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager_wrapper.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

KioskAppLevelLogsManagerWrapper::KioskAppLevelLogsManagerWrapper() {
  profile_manager_observer_.Observe(g_browser_process->profile_manager());
}

KioskAppLevelLogsManagerWrapper::KioskAppLevelLogsManagerWrapper(
    Profile* profile) {
  Init(profile);
}

KioskAppLevelLogsManagerWrapper::~KioskAppLevelLogsManagerWrapper() = default;

bool KioskAppLevelLogsManagerWrapper::IsLogCollectionEnabled() {
  return log_collection_enabled_;
}

void KioskAppLevelLogsManagerWrapper::OnProfileAdded(Profile* profile) {
  Init(profile);
}

void KioskAppLevelLogsManagerWrapper::Init(Profile* profile) {
  CHECK(!profile_) << "Init() should only be called once.";
  profile_ = profile;

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kKioskApplicationLogCollectionEnabled,
      base::BindRepeating(&KioskAppLevelLogsManagerWrapper::OnPolicyChanged,
                          weak_factory_.GetWeakPtr()));
  OnPolicyChanged();
}

void KioskAppLevelLogsManagerWrapper::EnableLogging() {
  log_collection_enabled_ = true;
  // TODO(b:425622183): Create a KioskAppLevelLogsManager object (if it doesn't
  // exist) to initialize logging and remove `log_collection_enabled_`;
}

void KioskAppLevelLogsManagerWrapper::DisableLogging() {
  log_collection_enabled_ = false;
  // TODO(b:425622183): Destroy the KioskAppLevelLogsManager object (if exist)
  // to disable logging.
}

void KioskAppLevelLogsManagerWrapper::OnPolicyChanged() {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kKioskApplicationLogCollectionEnabled)) {
    EnableLogging();
  } else {
    DisableLogging();
  }
}

}  // namespace chromeos
