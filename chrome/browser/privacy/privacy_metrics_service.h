// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class HostContentSettingsMap;
class PrefService;

// Records privacy-related UMA metrics and is created on profile startup. Allows
// consolidation of metrics which do not otherwise have an obvious home, as well
// as recording metrics which span events across multiple disparate locations
// in the browser.
class PrivacyMetricsService : public KeyedService {
 public:
  explicit PrivacyMetricsService(
      PrefService* pref_service,
      HostContentSettingsMap* host_content_settings_map);
  ~PrivacyMetricsService() override;

 private:
  void RecordStartupMetrics();

  const raw_ptr<const PrefService> pref_service_;
  const raw_ptr<const HostContentSettingsMap> host_content_settings_map_;
};

#endif  // CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_
