// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_GOOGLE_UPDATE_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_GOOGLE_UPDATE_METRICS_PROVIDER_WIN_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_provider.h"

// GoogleUpdateMetricsProviderWin is responsible for filling out the
// GoogleUpdate of the UMA SystemProfileProto.
class GoogleUpdateMetricsProviderWin : public metrics::MetricsProvider {
 public:
  GoogleUpdateMetricsProviderWin();
  ~GoogleUpdateMetricsProviderWin() override;

  // metrics::MetricsProvider
  void AsyncInit(const base::Closure& done_callback) override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

 private:
  // This is a small helper struct containing the Google Update metrics state.
  struct GoogleUpdateMetrics {
    GoogleUpdateMetrics();
    ~GoogleUpdateMetrics();

    // Defines whether this is a user-level or system-level install.
    bool is_system_install;

    // The time at which Google Update last started an automatic update check.
    base::Time last_started_automatic_update_check;

    // The time at which Google Update last successfully received update
    // information from Google servers.
    base::Time last_checked;

    // Details about Google Update's attempts to update itself.
    GoogleUpdateSettings::ProductData google_update_data;

    // Details about Google Update's attempts to update this product.
    GoogleUpdateSettings::ProductData product_data;
  };

  // Retrieve the Google Update data on the blocking pool.
  static GoogleUpdateMetrics GetGoogleUpdateDataBlocking();

  // Receives |google_update_metrics| from a blocking pool thread and runs
  // |done_callback|.
  void ReceiveGoogleUpdateData(
      const base::Closure& done_callback,
      const GoogleUpdateMetrics& google_update_metrics);

  // Google Update metrics that were fetched via GetGoogleUpdateData(). Will be
  // filled in only after the successful completion of GetGoogleUpdateData().
  GoogleUpdateMetrics google_update_metrics_;

  base::WeakPtrFactory<GoogleUpdateMetricsProviderWin> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GoogleUpdateMetricsProviderWin);
};

#endif  // CHROME_BROWSER_METRICS_GOOGLE_UPDATE_METRICS_PROVIDER_WIN_H_
