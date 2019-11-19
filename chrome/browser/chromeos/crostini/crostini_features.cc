// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_features.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace {

bool IsUnaffiliatedCrostiniAllowedByPolicy() {
  bool unaffiliated_crostini_allowed;
  if (chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kDeviceUnaffiliatedCrostiniAllowed,
          &unaffiliated_crostini_allowed)) {
    return unaffiliated_crostini_allowed;
  }
  // If device policy is not set, allow Crostini.
  return true;
}

bool IsAllowedImpl(Profile* profile) {
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }
  if (!crostini::CrostiniManager::IsDevKvmPresent()) {
    // Hardware is physically incapable, no matter what the user wants.
    return false;
  }

  bool kernelnext = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kKernelnextRestrictVMs);
  bool kernelnext_override =
      base::FeatureList::IsEnabled(features::kKernelnextVMs);
  if (kernelnext && !kernelnext_override) {
    // The host kernel is on an experimental version. In future updates this
    // device may not have VM support, so we allow enabling VMs, but guard them
    // on a chrome://flags switch (enable-experimental-kernel-vm-support).
    return false;
  }

  return base::FeatureList::IsEnabled(features::kCrostini);
}

}  // namespace

namespace crostini {

static CrostiniFeatures* g_crostini_features = nullptr;

CrostiniFeatures* CrostiniFeatures::Get() {
  if (!g_crostini_features) {
    g_crostini_features = new CrostiniFeatures();
  }
  return g_crostini_features;
}

void CrostiniFeatures::SetForTesting(CrostiniFeatures* features) {
  g_crostini_features = features;
}

CrostiniFeatures::CrostiniFeatures() = default;

CrostiniFeatures::~CrostiniFeatures() = default;

bool CrostiniFeatures::IsAllowed(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!IsUnaffiliatedCrostiniAllowedByPolicy() && !user->IsAffiliated()) {
    return false;
  }
  if (!profile->GetPrefs()->GetBoolean(
          crostini::prefs::kUserCrostiniAllowedByPolicy)) {
    return false;
  }
  if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
    return false;
  }
  return IsAllowedImpl(profile);
}

bool CrostiniFeatures::IsUIAllowed(Profile* profile, bool check_policy) {
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile)) {
    return false;
  }
  if (check_policy) {
    return g_crostini_features->IsAllowed(profile);
  }
  return IsAllowedImpl(profile);
}

bool CrostiniFeatures::IsEnabled(Profile* profile) {
  return g_crostini_features->IsUIAllowed(profile) &&
         profile->GetPrefs()->GetBoolean(crostini::prefs::kCrostiniEnabled);
}

bool CrostiniFeatures::IsExportImportUIAllowed(Profile* profile) {
  return g_crostini_features->IsUIAllowed(profile, true) &&
         base::FeatureList::IsEnabled(chromeos::features::kCrostiniBackup) &&
         profile->GetPrefs()->GetBoolean(
             crostini::prefs::kUserCrostiniExportImportUIAllowedByPolicy);
}

bool CrostiniFeatures::IsRootAccessAllowed(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kCrostiniAdvancedAccessControls)) {
    return profile->GetPrefs()->GetBoolean(
        crostini::prefs::kUserCrostiniRootAccessAllowedByPolicy);
  }
  return true;
}

}  // namespace crostini
