// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_H_

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/metrics/metrics_provider.h"

namespace internal {
extern const char kHttpsEngagementSessionPercentage[];
}  // namespace internal

class HttpsEngagementService : public KeyedService {
 public:
  enum PageScheme { HTTP, HTTPS, OTHER };

  HttpsEngagementService();

  HttpsEngagementService(const HttpsEngagementService&) = delete;
  HttpsEngagementService& operator=(const HttpsEngagementService&) = delete;

  ~HttpsEngagementService() override;

  // Save that the user spent |time| on either HTTPS or HTTP.
  void RecordTimeOnPage(base::TimeDelta time_spent, PageScheme scheme);

  // Persist the current state with the metrics service, and reset state.
  void StoreMetricsAndClear();

 private:
  base::TimeDelta time_on_https_;
  base::TimeDelta time_on_http_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_SERVICE_H_
