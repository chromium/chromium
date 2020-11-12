// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace {

bool PluginsEnterpriseSettingEnabled(
    HostContentSettingsMap* host_content_settings_map) {
  std::string provider_id;
  host_content_settings_map->GetDefaultContentSetting(
      ContentSettingsType::PLUGINS, &provider_id);
  return HostContentSettingsMap::GetProviderTypeFromSource(provider_id) ==
         HostContentSettingsMap::POLICY_PROVIDER;
}

}  // namespace

FlashPermissionContext::FlashPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::PLUGINS,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

FlashPermissionContext::~FlashPermissionContext() {}

ContentSetting FlashPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
      host_content_settings_map, url::Origin::Create(embedding_origin),
      requesting_origin, nullptr);
  if (flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT)
    return CONTENT_SETTING_ASK;
  return flash_setting;
}

void FlashPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    bool allowed) {
  if (!allowed)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.render_process_id(),
                                           id.render_frame_id()));
  if (!web_contents)
    return;

  if (PluginsEnterpriseSettingEnabled(
          HostContentSettingsMapFactory::GetForProfile(browser_context()))) {
    // Enable the grant temporarily.
    FlashTemporaryPermissionTracker::Get(browser_context())
        ->FlashEnabledForWebContents(web_contents);
  }

  // Automatically refresh the page.
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
}

void FlashPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting,
    bool is_one_time) {
  DCHECK(!is_one_time);
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  // If there is an enterprise ASK setting in effect, don't store the setting as
  // it won't have any effect anyway.
  if (PluginsEnterpriseSettingEnabled(host_content_settings_map))
    return;

  // If the request was for a file scheme, allow or deny all file:/// URLs.
  ContentSettingsPattern pattern;
  if (embedding_origin.SchemeIsFile())
    pattern = ContentSettingsPattern::FromString("file:///*");
  else
    pattern = ContentSettingsPattern::FromURLNoWildcard(embedding_origin);
  host_content_settings_map->SetContentSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), content_settings_type(),
      content_setting);
}

bool FlashPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
