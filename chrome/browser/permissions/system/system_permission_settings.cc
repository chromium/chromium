// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <map>
#include <memory>

#include "base/supports_user_data.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {
std::map<ContentSettingsType, bool>& GlobalTestingBlockOverrides() {
  static std::map<ContentSettingsType, bool> g_testing_block_overrides;
  return g_testing_block_overrides;
}

const void* const kSystemPermissionSettingsKey = &kSystemPermissionSettingsKey;

}  // namespace

std::unique_ptr<base::SupportsUserData::Data>
SystemPermissionSettings::Clone() {
  return nullptr;
}

void SystemPermissionSettings::Create(Profile* profile) {
  CHECK(profile);
  profile->SetUserData(kSystemPermissionSettingsKey, CreateImpl());
}

SystemPermissionSettings* SystemPermissionSettings::GetInstance() {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  CHECK(profile);
  return static_cast<SystemPermissionSettings*>(
      profile->GetUserData(kSystemPermissionSettingsKey));
}

bool SystemPermissionSettings::IsDenied(ContentSettingsType type) const {
  if (GlobalTestingBlockOverrides().find(type) !=
      GlobalTestingBlockOverrides().end()) {
    return GlobalTestingBlockOverrides().at(type);
  }
  return IsDeniedImpl(type);
}

bool SystemPermissionSettings::IsAllowed(ContentSettingsType type) const {
  if (GlobalTestingBlockOverrides().find(type) !=
      GlobalTestingBlockOverrides().end()) {
    return GlobalTestingBlockOverrides().at(type);
  }
  return IsAllowedImpl(type);
}

ScopedSystemPermissionSettingsForTesting::
    ScopedSystemPermissionSettingsForTesting(ContentSettingsType type,
                                             bool blocked)
    : type_(type) {
  CHECK(GlobalTestingBlockOverrides().find(type) ==
        GlobalTestingBlockOverrides().end());
  GlobalTestingBlockOverrides()[type] = blocked;
}

ScopedSystemPermissionSettingsForTesting::
    ~ScopedSystemPermissionSettingsForTesting() {
  GlobalTestingBlockOverrides().erase(type_);
}
