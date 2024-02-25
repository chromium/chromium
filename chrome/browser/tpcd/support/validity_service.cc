// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/validity_service.h"

#include <memory>
#include <utility>

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/support/tpcd_support_service.h"
#include "chrome/browser/tpcd/support/tpcd_support_service_factory.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/static_cookie_policy.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace tpcd::trial {

namespace {

bool g_disabled_for_testing = false;

bool IsThirdParty(const GURL& url, const GURL& first_party_url) {
  return !net::SiteForCookies::FromUrl(first_party_url).IsFirstParty(url);
}

}  // namespace

/* static */
void ValidityService::DisableForTesting() {
  g_disabled_for_testing = true;
}

/* static */
void ValidityService::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  auto* tpcd_trial_service = TpcdTrialServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!tpcd_trial_service) {
    return;
  }

  ValidityService::CreateForWebContents(web_contents);
}

ValidityService::ValidityService(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ValidityService>(*web_contents) {}

ValidityService::~ValidityService() = default;

void ValidityService::UpdateTpcdTrialSettings(const GURL& url,
                                              const GURL& first_party_url,
                                              bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_disabled_for_testing || enabled) {
    return;
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          web_contents()->GetBrowserContext());
  CHECK(settings_map);

  // Find the setting that permitted the cookie access for the pair.
  content_settings::SettingInfo info;
  bool setting_exists =
      (settings_map->GetContentSetting(url, first_party_url,
                                       ContentSettingsType::TPCD_TRIAL,
                                       &info) == CONTENT_SETTING_ALLOW);

  // If a matching setting no longer exists, there is no need to update
  // |settings_map|.
  if (!setting_exists) {
    return;
  }

  // Because the same token is used to enable the trial for the request origin
  // under all top-level origins, only the primary_pattern is checked here. This
  // means all settings created with the same token as the setting represented
  // by |info| should be deleted.
  auto matches = [&](const ContentSettingPatternSource& setting) -> bool {
    return (setting.primary_pattern == info.primary_pattern);
  };
  settings_map->ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType::TPCD_TRIAL, matches);

  SyncTpcdTrialSettingsToNetworkService(settings_map);
}

void ValidityService::SyncTpcdTrialSettingsToNetworkService(
    HostContentSettingsMap* settings_map) {
  ContentSettingsForOneType tpcd_trial_settings =
      settings_map->GetSettingsForOneType(ContentSettingsType::TPCD_TRIAL);

  web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::TPCD_TRIAL,
                           std::move(tpcd_trial_settings),
                           base::NullCallback());
}

void ValidityService::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessedImpl(details);
}

void ValidityService::OnCookiesAccessed(
    content::NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessedImpl(details);
}

void ValidityService::OnCookiesAccessedImpl(
    const content::CookieAccessDetails& details) {
  if (details.blocked_by_policy ||
      !IsThirdParty(details.url, details.first_party_url)) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // If third-party cookies are allowed globally, there's no reason to continue
  // with performing checks.
  if (!CookieSettingsFactory::GetForProfile(profile)
           ->ShouldBlockThirdPartyCookies()) {
    return;
  }

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  CHECK(settings_map);

  // Check for an existing enrollment setting for the pair.
  if (settings_map->GetContentSetting(details.url, details.first_party_url,
                                      ContentSettingsType::TPCD_TRIAL) ==
      CONTENT_SETTING_ALLOW) {
    CheckTrialStatusAsync(
        base::BindOnce(&ValidityService::UpdateTpcdTrialSettings,
                       weak_factory_.GetWeakPtr()),
        details.url, details.first_party_url);
  }
}

void ValidityService::CheckTrialStatusAsync(
    ContentSettingUpdateCallback update_callback,
    const GURL& url,
    const GURL& first_party_url) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ValidityService::CheckTrialStatusOnUiThread,
                     weak_factory_.GetWeakPtr(), std::move(update_callback),
                     std::move(url), std::move(first_party_url)));
}

// Persistent origin trials can only be checked on the UI thread.
void ValidityService::CheckTrialStatusOnUiThread(
    ContentSettingUpdateCallback update_callback,
    const GURL& url,
    const GURL& first_party_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::OriginTrialsControllerDelegate* trial_delegate =
      WebContentsObserver::web_contents()
          ->GetBrowserContext()
          ->GetOriginTrialsControllerDelegate();
  if (!trial_delegate) {
    return;
  }

  url::Origin request_origin = url::Origin::Create(url);
  url::Origin partition_origin = url::Origin::Create(first_party_url);

  bool enabled = trial_delegate->IsFeaturePersistedForOrigin(
      request_origin, partition_origin, blink::mojom::OriginTrialFeature::kTpcd,
      base::Time::Now());

  std::move(update_callback)
      .Run(std::move(url), std::move(first_party_url), enabled);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ValidityService);

}  // namespace tpcd::trial
