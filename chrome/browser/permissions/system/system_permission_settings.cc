// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include <map>

namespace {
std::map<ContentSettingsType, bool>& GlobalTestingBlockOverrides() {
  static std::map<ContentSettingsType, bool> g_testing_block_overrides;
  return g_testing_block_overrides;
}
}  // namespace

bool SystemPermissionSettings::IsPermissionDenied(
    ContentSettingsType type) const {
  if (GlobalTestingBlockOverrides().find(type) !=
      GlobalTestingBlockOverrides().end()) {
    return GlobalTestingBlockOverrides().at(type);
  }
  return IsPermissionDeniedImpl(type);
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
