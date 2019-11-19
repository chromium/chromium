// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "url/gurl.h"

namespace {

// Key into the website setting dict for the smart UI.
const char kInfobarLastShownTimeKey[] = "InfobarLastShownTime";

bool ShouldUseSmartUI() {
#if defined(OS_ANDROID)
  return true;
#endif
  return false;
}

}  // namespace

constexpr base::TimeDelta
    SubresourceFilterContentSettingsManager::kDelayBeforeShowingInfobarAgain;

SubresourceFilterContentSettingsManager::
    SubresourceFilterContentSettingsManager(Profile* profile)
    : settings_map_(HostContentSettingsMapFactory::GetForProfile(profile)),
      clock_(std::make_unique<base::DefaultClock>(base::DefaultClock())),
      should_use_smart_ui_(ShouldUseSmartUI()) {
  DCHECK(profile);
  DCHECK(settings_map_);
  if (auto* history_service = HistoryServiceFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)) {
    history_observer_.Add(history_service);
  }
}

SubresourceFilterContentSettingsManager::
    ~SubresourceFilterContentSettingsManager() = default;

ContentSetting SubresourceFilterContentSettingsManager::GetSitePermission(
    const GURL& url) const {
  return settings_map_->GetContentSetting(url, GURL(), ContentSettingsType::ADS,
                                          std::string());
}

void SubresourceFilterContentSettingsManager::WhitelistSite(const GURL& url) {
  settings_map_->SetContentSettingDefaultScope(
      url, GURL(), ContentSettingsType::ADS, std::string(),
      CONTENT_SETTING_ALLOW);
}

void SubresourceFilterContentSettingsManager::OnDidShowUI(const GURL& url) {
  auto dict = std::make_unique<base::DictionaryValue>();
  double now = clock_->Now().ToDoubleT();
  dict->SetDouble(kInfobarLastShownTimeKey, now);
  SetSiteMetadata(url, std::move(dict));
}

bool SubresourceFilterContentSettingsManager::ShouldShowUIForSite(
    const GURL& url) const {
  if (!should_use_smart_ui())
    return true;

  std::unique_ptr<base::DictionaryValue> dict = GetSiteMetadata(url);
  if (!dict)
    return true;

  double last_shown_time_double = 0;
  if (dict->GetDouble(kInfobarLastShownTimeKey, &last_shown_time_double)) {
    base::Time last_shown = base::Time::FromDoubleT(last_shown_time_double);
    if (clock_->Now() - last_shown < kDelayBeforeShowingInfobarAgain)
      return false;
  }
  return true;
}

void SubresourceFilterContentSettingsManager::
    ResetSiteMetadataBasedOnActivation(const GURL& url, bool is_activated) {
  // Do not reset the metadata if it exists already, it could clobber an
  // existing timestamp.
  if (!is_activated) {
    SetSiteMetadata(url, nullptr);
  } else if (!GetSiteMetadata(url)) {
    SetSiteMetadata(url, std::make_unique<base::DictionaryValue>());
  }
}

std::unique_ptr<base::DictionaryValue>
SubresourceFilterContentSettingsManager::GetSiteMetadata(
    const GURL& url) const {
  return base::DictionaryValue::From(settings_map_->GetWebsiteSetting(
      url, GURL(), ContentSettingsType::ADS_DATA, std::string(), nullptr));
}

void SubresourceFilterContentSettingsManager::SetSiteMetadata(
    const GURL& url,
    std::unique_ptr<base::DictionaryValue> dict) {
  settings_map_->SetWebsiteSettingDefaultScope(url, GURL(),
                                               ContentSettingsType::ADS_DATA,
                                               std::string(), std::move(dict));
}

// When history URLs are deleted, clear the metadata for the smart UI.
void SubresourceFilterContentSettingsManager::OnURLsDeleted(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    settings_map_->ClearSettingsForOneType(ContentSettingsType::ADS_DATA);
    return;
  }

  for (const auto& entry : deletion_info.deleted_urls_origin_map()) {
    const GURL& origin = entry.first;
    int remaining_urls = entry.second.first;
    if (!origin.is_empty() && remaining_urls == 0)
      SetSiteMetadata(origin, nullptr);
  }
}
