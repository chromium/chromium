// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/system/system_permission_settings.h"

#include "base/feature_list.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"

class SystemPermissionSettingsImpl : public SystemPermissionSettings {
  bool IsPermissionDeniedImpl(ContentSettingsType type) const override {
    if (base::FeatureList::IsEnabled(
            content_settings::features::
                kCrosSystemLevelPermissionBlockedWarnings)) {
      return ash::privacy_hub_util::ContentBlocked(type);
    }
    return false;
  }

  void OpenSystemSettings(content::WebContents*,
                          ContentSettingsType type) const override {
    if (base::FeatureList::IsEnabled(
            content_settings::features::
                kCrosSystemLevelPermissionBlockedWarnings)) {
      ash::privacy_hub_util::OpenSystemSettings(
          ProfileManager::GetActiveUserProfile(), type);
    }
  }
};

std::unique_ptr<SystemPermissionSettings> SystemPermissionSettings::Create() {
  return std::make_unique<SystemPermissionSettingsImpl>();
}
