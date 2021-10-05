// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class PrefService;

// Records privacy-related UMA metrics and is created on profile startup. Allows
// consolidation of metrics which do not otherwise have an obvious home, as well
// as recording metrics which span events across multiple disparate locations
// in the browser.
class PrivacyMetricsService : public KeyedService {
 public:
  explicit PrivacyMetricsService(PrefService* pref_service);
  ~PrivacyMetricsService() override;

 protected:
  void RecordStartupMetrics();

 private:
  PrefService* pref_service_;
};

#endif  // CHROME_BROWSER_PRIVACY_PRIVACY_METRICS_SERVICE_H_
