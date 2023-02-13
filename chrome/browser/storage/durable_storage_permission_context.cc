// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/origin_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

using bookmarks::BookmarkModel;

DurableStoragePermissionContext::DurableStoragePermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::DURABLE_STORAGE,
                            blink::mojom::PermissionsPolicyFeature::kNotFound) {
}

void DurableStoragePermissionContext::DecidePermission(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
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
                        std::move(callback), /*persist=*/false,
                        CONTENT_SETTING_DEFAULT, /*is_one_time=*/false,
                        /*is_final_decision=*/true);
    return;
  }

  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  // Don't grant durable for session-only storage, since it won't be persisted
  // anyway. Don't grant durable if we can't write cookies.
  if (cookie_settings->IsCookieSessionOnly(requesting_origin) ||
      !cookie_settings->IsFullCookieAccessAllowed(
          requesting_origin, net::SiteForCookies::FromUrl(requesting_origin),
          url::Origin::Create(requesting_origin),
          net::CookieSettingOverrides())) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(callback), /*persist=*/false,
                        CONTENT_SETTING_DEFAULT, /*is_one_time=*/false,
                        /*is_final_decision=*/true);
    return;
  }

  std::string registerable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          requesting_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (registerable_domain.empty() && requesting_origin.HostIsIPAddress())
    registerable_domain = requesting_origin.host();

  std::set<std::string> installed_registerable_domains =
      site_engagement::ImportantSitesUtil::GetInstalledRegisterableDomains(
          Profile::FromBrowserContext(browser_context()));
  if (base::Contains(installed_registerable_domains, registerable_domain)) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin,
                        std::move(callback), /*persist=*/true,
                        CONTENT_SETTING_ALLOW, /*is_one_time=*/false,
                        /*is_final_decision=*/true);
    return;
  }

  const size_t kMaxImportantResults = 10;
  std::vector<site_engagement::ImportantSitesUtil::ImportantDomainInfo>
      important_sites =
          site_engagement::ImportantSitesUtil::GetImportantRegisterableDomains(
              Profile::FromBrowserContext(browser_context()),
              kMaxImportantResults);

  for (const auto& important_site : important_sites) {
    if (important_site.registerable_domain == registerable_domain) {
      NotifyPermissionSet(id, requesting_origin, embedding_origin,
                          std::move(callback), /*persist=*/true,
                          CONTENT_SETTING_ALLOW, /*is_one_time=*/false,
                          /*is_final_decision=*/true);
      return;
    }
  }

  NotifyPermissionSet(id, requesting_origin, embedding_origin,
                      std::move(callback), /*persist=*/false,
                      CONTENT_SETTING_DEFAULT, /*is_one_time=*/false,
                      /*is_final_decision=*/true);
}

void DurableStoragePermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin_ignored,
    ContentSetting content_setting,
    bool is_one_time) {
  DCHECK(!is_one_time);
  DCHECK_EQ(requesting_origin, requesting_origin.DeprecatedGetOriginAsURL());
  DCHECK_EQ(embedding_origin_ignored,
            embedding_origin_ignored.DeprecatedGetOriginAsURL());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  HostContentSettingsMapFactory::GetForProfile(browser_context())
      ->SetContentSettingDefaultScope(requesting_origin, GURL(),
                                      ContentSettingsType::DURABLE_STORAGE,
                                      content_setting);
}
