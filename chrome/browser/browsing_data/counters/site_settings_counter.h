// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_SETTINGS_COUNTER_H_
#define CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_SETTINGS_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

namespace content {
class HostZoomMap;
}

class PrefService;
namespace custom_handlers {
class ProtocolHandlerRegistry;
}

class SiteSettingsCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit SiteSettingsCounter(
      HostContentSettingsMap* map,
      content::HostZoomMap* zoom_map,
      custom_handlers::ProtocolHandlerRegistry* handler_registry,
      PrefService* pref_service);
  ~SiteSettingsCounter() override;

  const char* GetPrefName() const override;

 private:
  void OnInitialized() override;

  void Count() override;

  scoped_refptr<HostContentSettingsMap> map_;
  raw_ptr<content::HostZoomMap> zoom_map_;
  raw_ptr<custom_handlers::ProtocolHandlerRegistry> handler_registry_;
  raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_COUNTERS_SITE_SETTINGS_COUNTER_H_
