// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_header_service.h"

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

namespace storage_access_api::trial {

namespace {

const char kTrialName[] = "StorageAccessHeader";

}  // namespace

StorageAccessHeaderService::StorageAccessHeaderService(
    content::BrowserContext* browser_context)
    : origin_trials_controller_(
          browser_context->GetOriginTrialsControllerDelegate()),
      browser_context_(
          raw_ref<content::BrowserContext>::from_ptr(browser_context)) {
  if (origin_trials_controller_) {
    origin_trials_controller_->AddObserver(this);
  }
}

StorageAccessHeaderService::~StorageAccessHeaderService() = default;

void StorageAccessHeaderService::Shutdown() {
  if (origin_trials_controller_) {
    origin_trials_controller_->RemoveObserver(this);
  }
}

void StorageAccessHeaderService::UpdateSettingsForTesting(
    const OriginTrialStatusChangeDetails& details) {
  UpdateSettings(details);
}

void StorageAccessHeaderService::UpdateSettings(
    const OriginTrialStatusChangeDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(&*browser_context_);
  CHECK(settings_map);

  const GURL origin_as_url = details.origin.GetURL();
  const GURL partition_site_as_url = GURL(details.partition_site);

  content_settings::SettingInfo existing_setting_info;
  bool setting_exists =
      settings_map->GetContentSetting(
          origin_as_url, partition_site_as_url,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
          &existing_setting_info) == CONTENT_SETTING_ALLOW &&
      existing_setting_info.primary_pattern.HasDomainWildcard() ==
          details.match_subdomains &&
      !existing_setting_info.primary_pattern.MatchesAllHosts() &&
      !existing_setting_info.secondary_pattern.MatchesAllHosts();

  // No need to update if the trial settings have not changed.
  if (details.enabled == setting_exists) {
    return;
  }

  if (details.enabled) {
    if (details.match_subdomains) {
      settings_map->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(origin_as_url),
          ContentSettingsPattern::FromURLToSchemefulSitePattern(
              partition_site_as_url),
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
          CONTENT_SETTING_ALLOW);
    } else {
      settings_map->SetContentSettingDefaultScope(
          origin_as_url, partition_site_as_url,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
          CONTENT_SETTING_ALLOW);
    }
  } else {
    CHECK(setting_exists);

    // Remove settings for expired/unused pairs.
    auto matches_pair =
        [&](const ContentSettingPatternSource& setting) -> bool {
      return setting.primary_pattern.Matches(origin_as_url) &&
             setting.secondary_pattern.Matches(partition_site_as_url);
    };

    settings_map->ClearSettingsForOneTypeWithPredicate(
        ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, matches_pair);
  }

  SyncSettingsToNetworkService(settings_map);
}

void StorageAccessHeaderService::SyncSettingsToNetworkService(
    HostContentSettingsMap* settings_map) {
  ContentSettingsForOneType storage_access_headers_trial_settings =
      settings_map->GetSettingsForOneType(
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL);

  browser_context_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetContentSettings(
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL,
          std::move(storage_access_headers_trial_settings),
          base::NullCallback());
}

void StorageAccessHeaderService::OnStatusChanged(
    const OriginTrialStatusChangeDetails& details) {
  UpdateSettings(details);
}

void StorageAccessHeaderService::OnPersistedTokensCleared() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(&*browser_context_);
  CHECK(settings_map);

  settings_map->ClearSettingsForOneType(
      ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL);
  SyncSettingsToNetworkService(settings_map);
}

std::string StorageAccessHeaderService::trial_name() {
  return kTrialName;
}

}  // namespace storage_access_api::trial
