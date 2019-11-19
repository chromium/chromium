// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_

#include "components/metrics/metrics_provider.h"

#include <vector>

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/metrics_proto/system_profile.pb.h"

// AntiVirusMetricsProvider is responsible for adding antivirus information to
// the UMA system profile proto.
class AntiVirusMetricsProvider : public metrics::MetricsProvider {
 public:
  static constexpr base::Feature kReportNamesFeature = {
      "ReportFullAVProductDetails", base::FEATURE_DISABLED_BY_DEFAULT};

  AntiVirusMetricsProvider();
  ~AntiVirusMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void AsyncInit(const base::Closure& done_callback) override;
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
      const base::Closure& done_callback,
      const std::vector<metrics::SystemProfileProto::AntiVirusProduct>& result);

  mojo::Remote<chrome::mojom::UtilWin> remote_util_win_;

  // Information on installed AntiVirus gathered.
  std::vector<metrics::SystemProfileProto::AntiVirusProduct> av_products_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AntiVirusMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_
