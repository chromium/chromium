// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/site_settings_counter.h"

#include <set>
#include "build/build_config.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#endif

SiteSettingsCounter::SiteSettingsCounter(
    HostContentSettingsMap* map,
    content::HostZoomMap* zoom_map,
    custom_handlers::ProtocolHandlerRegistry* handler_registry,
    PrefService* pref_service)
    : map_(map),
      zoom_map_(zoom_map),
      handler_registry_(handler_registry),
      pref_service_(pref_service) {
  DCHECK(map_);
  DCHECK(handler_registry_);
#if !BUILDFLAG(IS_ANDROID)
  DCHECK(zoom_map_);
#else
  DCHECK(!zoom_map_);
#endif
  DCHECK(pref_service_);
}

SiteSettingsCounter::~SiteSettingsCounter() = default;

void SiteSettingsCounter::OnInitialized() {}

const char* SiteSettingsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteSiteSettings;
}

void SiteSettingsCounter::Count() {
  std::set<std::string> hosts;
  int empty_host_pattern = 0;
  base::Time period_start = GetPeriodStart();
  base::Time period_end = GetPeriodEnd();

  auto iterate_content_settings_list =
      [&](ContentSettingsType content_type,
          const ContentSettingsForOneType& content_settings_list) {
        for (const auto& content_setting : content_settings_list) {
          // TODO(crbug.com/762560): Check the conceptual SettingSource instead
          // of ContentSettingPatternSource.source
          if (content_setting.source == "preference" ||
              content_setting.source == "notification_android" ||
              content_setting.source == "ephemeral") {
            base::Time last_modified = content_setting.metadata.last_modified();
            if (last_modified >= period_start && last_modified < period_end) {
              if (content_setting.primary_pattern.GetHost().empty())
                empty_host_pattern++;
              else
                hosts.insert(content_setting.primary_pattern.GetHost());
            }
          }
        }
      };

  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info : *registry) {
    ContentSettingsType type = info->website_settings_info()->type();
    iterate_content_settings_list(type, map_->GetSettingsForOneType(type));
  }

  iterate_content_settings_list(
      ContentSettingsType::USB_CHOOSER_DATA,
      map_->GetSettingsForOneType(ContentSettingsType::USB_CHOOSER_DATA));

#if !BUILDFLAG(IS_ANDROID)
  for (const auto& zoom_level : zoom_map_->GetAllZoomLevels()) {
    // zoom_level with non-empty scheme are only used for some internal
    // features and not stored in preferences. They are not counted.
    if (zoom_level.last_modified >= period_start &&
        zoom_level.last_modified < period_end && zoom_level.scheme.empty()) {
      hosts.insert(zoom_level.host);
    }
  }
#endif

  auto handlers =
      handler_registry_->GetUserDefinedHandlers(period_start, period_end);
  for (const custom_handlers::ProtocolHandler& handler : handlers)
    hosts.insert(handler.url().host());

  std::vector<std::string> never_prompt_sites =
      ChromeTranslateClient::CreateTranslatePrefs(pref_service_)
          ->GetNeverPromptSitesBetween(period_start, period_end);
  for (const auto& site : never_prompt_sites)
    hosts.insert(site);

  ReportResult(hosts.size() + empty_host_pattern);
}
