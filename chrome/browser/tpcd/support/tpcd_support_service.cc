// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/tpcd_support_service.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/origin_trials/browser/origin_trials.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace tpcd::support {
namespace {

const char kTrialName[] = "Tpcd";
}  // namespace

TpcdSupportService::TpcdSupportService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  ot_controller_ = browser_context->GetOriginTrialsControllerDelegate();

  if (ot_controller_) {
    ot_controller_->AddObserver(this);
  }
}

TpcdSupportService::~TpcdSupportService() = default;

void TpcdSupportService::Shutdown() {
  if (ot_controller_) {
    ot_controller_->RemoveObserver(this);
  }

  ot_controller_ = nullptr;
  browser_context_ = nullptr;
}

void TpcdSupportService::Update3pcdSupportSettingsForTesting(
    const url::Origin& request_origin,
    const std::string& partition_site,
    bool match_subdomains,
    bool enabled) {
  Update3pcdSupportSettings(request_origin, partition_site, match_subdomains,
                            enabled);
}

void TpcdSupportService::Update3pcdSupportSettings(
    const url::Origin& request_origin,
    const std::string& partition_site,
    bool includes_subdomains,
    bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  const GURL request_origin_as_url = request_origin.GetURL();
  const GURL partition_site_as_url = GURL(partition_site);

  // Check for an existing `TPCD_SUPPORT` setting that allows the pair.
  content_settings::SettingInfo existing_setting_info;
  bool setting_exists =
      (settings_map->GetContentSetting(
           request_origin_as_url, partition_site_as_url,
           ContentSettingsType::TPCD_SUPPORT,
           &existing_setting_info) == CONTENT_SETTING_ALLOW) &&
      (existing_setting_info.primary_pattern.HasDomainWildcard() ==
       includes_subdomains) &&
      !existing_setting_info.primary_pattern.MatchesAllHosts() &&
      !existing_setting_info.secondary_pattern.MatchesAllHosts();

  // If the trial status matches existing settings, there is no need to
  // update `settings_map`.
  if (enabled == setting_exists) {
    return;
  }

  ContentSettingsPattern primary_setting_pattern;
  ContentSettingsPattern secondary_setting_pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(
          partition_site_as_url);
  if (includes_subdomains) {
    primary_setting_pattern =
        ContentSettingsPattern::FromURL(request_origin_as_url);
  } else {
    // In this case, the combination of `primary_setting_pattern` and
    // `secondary_setting_pattern` is equivalent to
    // `ContentSettingsType::TPCD_SUPPORT`'s default scope
    // (`REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE`).
    primary_setting_pattern =
        ContentSettingsPattern::FromURLNoWildcard(request_origin_as_url);
  }

  if (enabled) {
    settings_map->SetContentSettingCustomScope(
        primary_setting_pattern, secondary_setting_pattern,
        ContentSettingsType::TPCD_SUPPORT, CONTENT_SETTING_ALLOW);
  } else {
    CHECK(setting_exists);

    // Remove settings for expired/unused pairs to avoid memory bloat.
    auto matches_pair =
        [&](const ContentSettingPatternSource& setting) -> bool {
      return (setting.primary_pattern ==
              existing_setting_info.primary_pattern) &&
             (setting.secondary_pattern ==
              existing_setting_info.secondary_pattern);
    };

    settings_map->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::TPCD_SUPPORT, matches_pair);
  }

  ContentSettingsForOneType tpcd_support_settings =
      settings_map->GetSettingsForOneType(ContentSettingsType::TPCD_SUPPORT);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::TPCD_SUPPORT,
                           std::move(tpcd_support_settings),
                           base::NullCallback());
}

void TpcdSupportService::ClearTpcdSupportSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  settings_map->ClearSettingsForOneType(ContentSettingsType::TPCD_SUPPORT);
}

void TpcdSupportService::OnStatusChanged(const url::Origin& origin,
                                         const std::string& partition_site,
                                         bool includes_subdomains,
                                         bool enabled) {
  Update3pcdSupportSettings(origin, partition_site, includes_subdomains,
                            enabled);
}

void TpcdSupportService::OnPersistedTokensCleared() {
  ClearTpcdSupportSettings();
}

std::string TpcdSupportService::trial_name() {
  return kTrialName;
}

}  // namespace tpcd::support
