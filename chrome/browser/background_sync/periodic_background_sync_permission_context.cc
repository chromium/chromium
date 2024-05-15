// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webapps/installable/installable_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#endif

namespace features {

// If enabled, the installability criteria for granting PBS permission is
// dropped and the content setting is checked. This only applies if the
// requesting origin matches that of the browser's default search engine.
BASE_FEATURE(kPeriodicSyncPermissionForDefaultSearchEngine,
             "PeriodicSyncPermissionForDefaultSearchEngine",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

PeriodicBackgroundSyncPermissionContext::
    PeriodicBackgroundSyncPermissionContext(
        content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::PERIODIC_BACKGROUND_SYNC,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

PeriodicBackgroundSyncPermissionContext::
    ~PeriodicBackgroundSyncPermissionContext() = default;

bool PeriodicBackgroundSyncPermissionContext::IsPwaInstalled(
    const GURL& origin) const {
  // Because we're only passed the requesting origin from the permissions
  // infrastructure, we can't match the scope of installed PWAs to the exact URL
  // of the permission request. We instead look for any installed PWA for the
  // requesting origin. With this logic, if there's already a PWA installed for
  // google.com/travel, and a request to register Periodic Background Sync comes
  // in from google.com/maps, this method will return true and registration will
  // succeed, provided other required conditions are met.
  return DoesOriginContainAnyInstalledWebApp(browser_context(), origin);
}

#if BUILDFLAG(IS_ANDROID)
bool PeriodicBackgroundSyncPermissionContext::IsTwaInstalled(
    const GURL& origin) const {
  return ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
      origin);
}
#endif

GURL PeriodicBackgroundSyncPermissionContext::GetDefaultSearchEngineUrl()
    const {
  auto* template_url_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));
  DCHECK(template_url_service);

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();
  return default_search_engine ? default_search_engine->GenerateSearchURL(
                                     template_url_service->search_terms_data())
                               : GURL();
}

ContentSetting
PeriodicBackgroundSyncPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(IS_ANDROID)
  if (IsTwaInstalled(requesting_origin))
    return CONTENT_SETTING_ALLOW;
#endif

  bool can_bypass_install_requirement =
      base::FeatureList::IsEnabled(
          features::kPeriodicSyncPermissionForDefaultSearchEngine) &&
      url::IsSameOriginWith(GetDefaultSearchEngineUrl(), requesting_origin);

  if (!can_bypass_install_requirement && !IsPwaInstalled(requesting_origin)) {
    return CONTENT_SETTING_BLOCK;
  }

  // |requesting_origin| either has an installed PWA or matches the default
  // search engine's origin. Check one-shot Background Sync content setting.
  // Expected values are CONTENT_SETTING_BLOCK or CONTENT_SETTING_ALLOW.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  DCHECK(host_content_settings_map);

  auto content_setting = host_content_settings_map->GetContentSetting(
      requesting_origin, embedding_origin,
      ContentSettingsType::BACKGROUND_SYNC);
  DCHECK(content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_ALLOW);
  return content_setting;
}

void PeriodicBackgroundSyncPermissionContext::DecidePermission(
    permissions::PermissionRequestData request_data,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize Periodic Background Sync
  // from PeriodicBackgroundSyncPermissionContext.
  NOTREACHED_IN_MIGRATION();
}

void PeriodicBackgroundSyncPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK(!persist);
  DCHECK(is_final_decision);

  permissions::PermissionContextBase::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting, is_one_time, is_final_decision);
}
