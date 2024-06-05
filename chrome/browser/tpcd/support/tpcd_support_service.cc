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
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace tpcd::trial {
namespace {

const char kTrialName[] = "Tpcd";
}  // namespace

TpcdTrialService::TpcdTrialService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  ot_controller_ = browser_context->GetOriginTrialsControllerDelegate();
  if (ot_controller_) {
    ot_controller_->AddObserver(this);
  }
}

TpcdTrialService::~TpcdTrialService() = default;

void TpcdTrialService::Shutdown() {
  if (ot_controller_) {
    ot_controller_->RemoveObserver(this);
  }

  ot_controller_ = nullptr;
  browser_context_ = nullptr;
}

void TpcdTrialService::Update3pcdTrialSettingsForTesting(
    const OriginTrialStatusChangeDetails& details) {
  Update3pcdTrialSettings(details);
}

void TpcdTrialService::Update3pcdTrialSettings(
    const OriginTrialStatusChangeDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  const GURL origin_as_url = details.origin.GetURL();
  const GURL partition_site_as_url = GURL(details.partition_site);

  // Check for an existing `TPCD_TRIAL` setting that allows the pair.
  content_settings::SettingInfo existing_setting_info;
  bool setting_exists =
      (settings_map->GetContentSetting(origin_as_url, partition_site_as_url,
                                       ContentSettingsType::TPCD_TRIAL,
                                       &existing_setting_info) ==
       CONTENT_SETTING_ALLOW) &&
      (existing_setting_info.primary_pattern.HasDomainWildcard() ==
       details.match_subdomains) &&
      !existing_setting_info.primary_pattern.MatchesAllHosts() &&
      !existing_setting_info.secondary_pattern.MatchesAllHosts();

  // If the trial status matches existing settings, there is no need to
  // update `settings_map`.
  if (details.enabled == setting_exists) {
    return;
  }

  ContentSettingsPattern primary_setting_pattern;
  ContentSettingsPattern secondary_setting_pattern =
      ContentSettingsPattern::FromURLToSchemefulSitePattern(
          partition_site_as_url);
  if (details.match_subdomains) {
    primary_setting_pattern = ContentSettingsPattern::FromURL(origin_as_url);
  } else {
    // In this case, the combination of `primary_setting_pattern` and
    // `secondary_setting_pattern` is equivalent to
    // `ContentSettingsType::TPCD_TRIAL`'s default scope
    // (`REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE`).
    primary_setting_pattern =
        ContentSettingsPattern::FromURLNoWildcard(origin_as_url);
  }

  if (details.enabled) {
    settings_map->SetContentSettingCustomScope(
        primary_setting_pattern, secondary_setting_pattern,
        ContentSettingsType::TPCD_TRIAL, CONTENT_SETTING_ALLOW);
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
        ContentSettingsType::TPCD_TRIAL, matches_pair);
  }

  SyncTpcdTrialSettingsToNetworkService(settings_map);
}

void TpcdTrialService::ClearTpcdTrialSettings() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context_);
  CHECK(settings_map);

  settings_map->ClearSettingsForOneType(ContentSettingsType::TPCD_TRIAL);
  SyncTpcdTrialSettingsToNetworkService(settings_map);
}

void TpcdTrialService::SyncTpcdTrialSettingsToNetworkService(
    HostContentSettingsMap* settings_map) {
  ContentSettingsForOneType tpcd_trial_settings =
      settings_map->GetSettingsForOneType(ContentSettingsType::TPCD_TRIAL);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(ContentSettingsType::TPCD_TRIAL,
                           std::move(tpcd_trial_settings),
                           base::NullCallback());
}

void TpcdTrialService::OnStatusChanged(
    const OriginTrialStatusChangeDetails& details) {
  Update3pcdTrialSettings(details);
}

void TpcdTrialService::OnPersistedTokensCleared() {
  ClearTpcdTrialSettings();
}

std::string TpcdTrialService::trial_name() {
  return kTrialName;
}

}  // namespace tpcd::trial
