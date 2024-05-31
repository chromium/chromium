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
#include "components/content_settings/core/browser/content_settings_type_set.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
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

using ThirdPartyCookieAllowMechanism =
    content_settings::CookieSettingsBase::ThirdPartyCookieAllowMechanism;

bool g_disabled_for_testing = false;

bool IsThirdParty(const GURL& url, const GURL& first_party_url) {
  return !net::SiteForCookies::FromUrl(first_party_url).IsFirstParty(url);
}

std::optional<ContentSettingsType> GetTrialContentSettingsType(
    ThirdPartyCookieAllowMechanism mechanism) {
  switch (mechanism) {
    case ThirdPartyCookieAllowMechanism::kAllowBy3PCD:
      return ContentSettingsType::TPCD_TRIAL;
    case ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD:
      return ContentSettingsType::TOP_LEVEL_TPCD_TRIAL;
    default:
      // The other mechanisms do not map to a |ContentSettingsType| for a
      // third-party cookie deprecation trial.
      return std::nullopt;
  }
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

void ValidityService::UpdateTrialSettings(
    const ContentSettingsType trial_settings_type,
    const GURL& url,
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
  bool setting_exists = CheckTrialContentSetting(url, first_party_url,
                                                 trial_settings_type, &info);

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
  settings_map->ClearSettingsForOneTypeWithPredicate(trial_settings_type,
                                                     matches);

  SyncTrialSettingsToNetworkService(
      trial_settings_type,
      settings_map->GetSettingsForOneType(trial_settings_type));
}

void ValidityService::SyncTrialSettingsToNetworkService(
    const ContentSettingsType trial_settings_type,
    const ContentSettingsForOneType& trial_settings) {
  web_contents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(trial_settings_type, std::move(trial_settings),
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

  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile);
  CHECK(cookie_settings);

  // Check for an existing trial setting applicable to the pair.
  ThirdPartyCookieAllowMechanism allow_mechanism =
      cookie_settings->GetThirdPartyCookieAllowMechanism(
          details.url, net::SiteForCookies::FromUrl(details.first_party_url),
          details.first_party_url, details.cookie_setting_overrides);
  std::optional<ContentSettingsType> setting_type =
      GetTrialContentSettingsType(allow_mechanism);

  if (setting_type.has_value()) {
    CheckTrialStatusAsync(
        base::BindOnce(&ValidityService::UpdateTrialSettings,
                       weak_factory_.GetWeakPtr(), setting_type.value()),
        setting_type.value(), details.url, details.first_party_url);
  }
}

void ValidityService::CheckTrialStatusAsync(
    ContentSettingUpdateCallback update_callback,
    const ContentSettingsType trial_settings_type,
    const GURL& url,
    const GURL& first_party_url) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ValidityService::CheckTrialStatusOnUiThread,
                                weak_factory_.GetWeakPtr(),
                                std::move(update_callback), trial_settings_type,
                                std::move(url), std::move(first_party_url)));
}

// Persistent origin trials can only be checked on the UI thread.
void ValidityService::CheckTrialStatusOnUiThread(
    ContentSettingUpdateCallback update_callback,
    const ContentSettingsType trial_settings_type,
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

  bool enabled = false;
  url::Origin partition_origin = url::Origin::Create(first_party_url);

  switch (trial_settings_type) {
    case ContentSettingsType::TPCD_TRIAL:
      enabled = trial_delegate->IsFeaturePersistedForOrigin(
          url::Origin::Create(url), partition_origin,
          blink::mojom::OriginTrialFeature::kTpcd, base::Time::Now());
      break;
    case ContentSettingsType::TOP_LEVEL_TPCD_TRIAL:
      enabled = trial_delegate->IsFeaturePersistedForOrigin(
          url::Origin::Create(first_party_url), partition_origin,
          blink::mojom::OriginTrialFeature::kTopLevelTpcd, base::Time::Now());

      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "ContentSettingsType::" << trial_settings_type
          << " is not associated with a 3PCD trial.";
      return;
  }

  std::move(update_callback)
      .Run(std::move(url), std::move(first_party_url), enabled);
}

bool ValidityService::CheckTrialContentSetting(
    const GURL& url,
    const GURL& first_party_url,
    ContentSettingsType trial_settings_type,
    content_settings::SettingInfo* info) {
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          web_contents()->GetBrowserContext());
  CHECK(settings_map);

  switch (trial_settings_type) {
    case ContentSettingsType::TPCD_TRIAL:
      return (settings_map->GetContentSetting(url, first_party_url,
                                              trial_settings_type,
                                              info) == CONTENT_SETTING_ALLOW);
    case ContentSettingsType::TOP_LEVEL_TPCD_TRIAL:
      // Top-level 3pcd trial settings use
      // |WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE| by default and as a result
      // only use a primary pattern (with wildcard placeholder for the secondary
      // pattern).
      return (settings_map->GetContentSetting(first_party_url, first_party_url,
                                              trial_settings_type,
                                              info) == CONTENT_SETTING_ALLOW);
    default:
      NOTREACHED_IN_MIGRATION()
          << "ContentSettingsType::" << trial_settings_type
          << " is not associated with a 3PCD trial.";
      return false;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ValidityService);

}  // namespace tpcd::trial
