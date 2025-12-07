// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager_wrapper.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_level_logs_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

KioskAppLevelLogsManagerWrapper::KioskAppLevelLogsManagerWrapper(
    ash::KioskAppId app_id)
    : app_id_(app_id) {
  profile_manager_observer_.Observe(g_browser_process->profile_manager());
}

KioskAppLevelLogsManagerWrapper::KioskAppLevelLogsManagerWrapper(
    Profile* profile,
    ash::KioskAppId app_id)
    : app_id_(app_id) {
  Init(profile);
}

KioskAppLevelLogsManagerWrapper::~KioskAppLevelLogsManagerWrapper() = default;

bool KioskAppLevelLogsManagerWrapper::IsLogCollectionEnabled() {
  return logs_manager_ != nullptr;
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
  logs_manager_ = std::make_unique<KioskAppLevelLogsManager>(profile_, app_id_);
}

void KioskAppLevelLogsManagerWrapper::DisableLogging() {
  logs_manager_.reset();
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
