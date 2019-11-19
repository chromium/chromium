// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/origin_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;

DurableStoragePermissionContext::DurableStoragePermissionContext(
    Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::DURABLE_STORAGE,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

void DurableStoragePermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_NE(CONTENT_SETTING_ALLOW,
            GetPermissionStatus(nullptr /* render_frame_host */,
                                requesting_origin, embedding_origin)
                .content_setting);
  DCHECK_NE(CONTENT_SETTING_BLOCK,
            GetPermissionStatus(nullptr /* render_frame_host */,
                                requesting_origin, embedding_origin)
                .content_setting);

  // Durable is only allowed to be granted to the top-level origin. Embedding
  // origin is the last committed navigation origin to the web contents.
  if (requesting_origin != embedding_origin) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(callback), false /* persist */,
                        CONTENT_SETTING_DEFAULT);
    return;
  }

  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile());

  // Don't grant durable for session-only storage, since it won't be persisted
  // anyway. Don't grant durable if we can't write cookies.
  if (cookie_settings->IsCookieSessionOnly(requesting_origin) ||
      !cookie_settings->IsCookieAccessAllowed(requesting_origin,
                                              requesting_origin)) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(callback), false /* persist */,
                        CONTENT_SETTING_DEFAULT);
    return;
  }

  const size_t kMaxImportantResults = 10;
  std::vector<ImportantSitesUtil::ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(profile(),
                                                          kMaxImportantResults);

  std::string registerable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          requesting_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (registerable_domain.empty() && requesting_origin.HostIsIPAddress())
    registerable_domain = requesting_origin.host();

  for (const auto& important_site : important_sites) {
    if (important_site.registerable_domain == registerable_domain) {
      NotifyPermissionSet(id, requesting_origin, embedding_origin,
                          std::move(callback), true /* persist */,
                          CONTENT_SETTING_ALLOW);
      return;
    }
  }

  NotifyPermissionSet(id, requesting_origin, embedding_origin,
                      std::move(callback), false /* persist */,
                      CONTENT_SETTING_DEFAULT);
}

void DurableStoragePermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin_ignored,
    ContentSetting content_setting) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin_ignored, embedding_origin_ignored.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(requesting_origin, GURL(),
                                      ContentSettingsType::DURABLE_STORAGE,
                                      std::string(), content_setting);
}

bool DurableStoragePermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
