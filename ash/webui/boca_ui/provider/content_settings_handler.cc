// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/content_settings_handler.h"

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "url/gurl.h"

namespace ash::boca {
namespace {

ContentSettingsType GetPermissionType(const mojom::Permission permission) {
  switch (permission) {
    case mojom::Permission::kMicrophone:
      return ContentSettingsType::MEDIASTREAM_MIC;
    case mojom::Permission::kCamera:
      return ContentSettingsType::MEDIASTREAM_CAMERA;
  }
}

ContentSetting GetPermissionSettingType(
    const mojom::PermissionSetting permission_setting) {
  switch (permission_setting) {
    case mojom::PermissionSetting::kAllow:
      return ContentSetting::CONTENT_SETTING_ALLOW;
    case mojom::PermissionSetting::kAsk:
      return ContentSetting::CONTENT_SETTING_ASK;
    case mojom::PermissionSetting::kBlock:
      return ContentSetting::CONTENT_SETTING_BLOCK;
  }
}
}  // namespace

ContentSettingsHandler::ContentSettingsHandler(Profile* profile)
    : profile_(profile) {}

ContentSettingsHandler::~ContentSettingsHandler() = default;

bool ContentSettingsHandler::SetContentSettingForOrigin(
    const std::string& url,
    mojom::Permission permission,
    mojom::PermissionSetting permission_setting) {
  if (!profile_) {
    return false;
  }

  const GURL origin(url);
  HostContentSettingsMap* const host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  ContentSettingsType content_type = GetPermissionType(permission);
  ContentSetting setting = GetPermissionSettingType(permission_setting);

  host_content_settings_map->SetContentSettingDefaultScope(
      origin, origin, content_type, setting);

  // Reset the per-site embargo state
  if (setting != ContentSetting::CONTENT_SETTING_BLOCK) {
    auto blocker = std::make_unique<permissions::PermissionDecisionAutoBlocker>(
        host_content_settings_map);
    blocker.get()->RemoveEmbargoAndResetCounts(origin, content_type);
  }
  return true;
}

}  // namespace ash::boca
