// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_AND_SYNC_STATUS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_AND_SYNC_STATUS_METRICS_PROVIDER_H_

#include <optional>

#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "components/signin/core/browser/signin_status_metrics_provider_helpers.h"

namespace metrics {
class ChromeUserMetricsExtension;
}  // namespace metrics

// A simple class that provides the sign-in and sync status for including
// in log records.
class ChromeSigninAndSyncStatusMetricsProvider
    : public metrics::MetricsProvider {
 public:
  ChromeSigninAndSyncStatusMetricsProvider();
  ~ChromeSigninAndSyncStatusMetricsProvider() override;

  ChromeSigninAndSyncStatusMetricsProvider(
      const ChromeSigninAndSyncStatusMetricsProvider&) = delete;
  ChromeSigninAndSyncStatusMetricsProvider& operator=(
      const ChromeSigninAndSyncStatusMetricsProvider&) = delete;

  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  signin_metrics::ProfilesStatus GetStatusOfAllProfiles() const;
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_AND_SYNC_STATUS_METRICS_PROVIDER_H_
