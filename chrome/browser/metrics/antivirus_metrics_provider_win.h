// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_

#include "components/metrics/metrics_provider.h"

#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/metrics_proto/system_profile.pb.h"

// As discussed on http://crbug/1488597#c1, retain this feature.
BASE_DECLARE_FEATURE(kReportFullAVProductDetails);

// AntiVirusMetricsProvider is responsible for adding antivirus information to
// the UMA system profile proto.
class AntiVirusMetricsProvider : public metrics::MetricsProvider {
 public:
  AntiVirusMetricsProvider();

  AntiVirusMetricsProvider(const AntiVirusMetricsProvider&) = delete;
  AntiVirusMetricsProvider& operator=(const AntiVirusMetricsProvider&) = delete;

  ~AntiVirusMetricsProvider() override;

  // metrics::MetricsProvider:
  void AsyncInit(base::OnceClosure done_callback) override;
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;

  void SetRemoteUtilWinForTesting(
      mojo::PendingRemote<chrome::mojom::UtilWin> remote) {
    remote_util_win_.Bind(std::move(remote));
  }

 private:
  // Called when metrics are done being gathered from the FILE thread.
  // |done_callback| is the callback that should be called once all metrics are
  // gathered.
  void GotAntiVirusProducts(
      base::OnceClosure done_callback,
      const std::vector<metrics::SystemProfileProto::AntiVirusProduct>& result);

  mojo::Remote<chrome::mojom::UtilWin> remote_util_win_;

  // Information on installed AntiVirus gathered.
  std::vector<metrics::SystemProfileProto::AntiVirusProduct> av_products_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_
