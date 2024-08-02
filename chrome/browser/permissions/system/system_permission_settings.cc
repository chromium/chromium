// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/permissions/system/platform_handle.h"

namespace system_permission_settings {

namespace {

PlatformHandle* instance_for_testing_ = nullptr;

std::map<ContentSettingsType, bool>& GlobalTestingBlockOverrides() {
  static std::map<ContentSettingsType, bool> g_testing_block_overrides;
  return g_testing_block_overrides;
}

PlatformHandle* GetPlatformHandle() {
  if (instance_for_testing_) {
    return instance_for_testing_;
  }
  return CHECK_DEREF(CHECK_DEREF(g_browser_process).GetFeatures())
      .system_permissions_platform_handle();
}

}  // namespace

// static
void SetInstanceForTesting(PlatformHandle* instance_for_testing) {
  instance_for_testing_ = instance_for_testing;
}

// static
bool CanPrompt(ContentSettingsType type) {
  return GetPlatformHandle()->CanPrompt(type);
}

// static
bool IsDenied(ContentSettingsType type) {
  if (GlobalTestingBlockOverrides().find(type) !=
      GlobalTestingBlockOverrides().end()) {
    return GlobalTestingBlockOverrides().at(type);
  }
  return GetPlatformHandle()->IsDenied(type);
}

// static
bool IsAllowed(ContentSettingsType type) {
  if (GlobalTestingBlockOverrides().find(type) !=
      GlobalTestingBlockOverrides().end()) {
    return !GlobalTestingBlockOverrides().at(type);
  }
  return GetPlatformHandle()->IsAllowed(type);
}

// static
void OpenSystemSettings(content::WebContents* web_contents,
                        ContentSettingsType type) {
  GetPlatformHandle()->OpenSystemSettings(web_contents, type);
}

// static
void Request(ContentSettingsType type,
             SystemPermissionResponseCallback callback) {
  GetPlatformHandle()->Request(type, std::move(callback));
}

// static
std::unique_ptr<ScopedObservation> Observe(
    SystemPermissionChangedCallback observer) {
  return GetPlatformHandle()->Observe(observer);
}

ScopedSettingsForTesting::ScopedSettingsForTesting(ContentSettingsType type,
                                                   bool blocked)
    : type_(type) {
  CHECK(GlobalTestingBlockOverrides().find(type) ==
        GlobalTestingBlockOverrides().end());
  GlobalTestingBlockOverrides()[type] = blocked;
}

ScopedSettingsForTesting::~ScopedSettingsForTesting() {
  GlobalTestingBlockOverrides().erase(type_);
}

}  // namespace system_permission_settings
