// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_

#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "components/metrics/metrics_provider.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/metrics_proto/system_profile.pb.h"

// As discussed on http://crbug.com/40283669#comment2, retain this feature.
BASE_DECLARE_FEATURE(kReportFullAVProductDetails);

// If enabled, then if mojo disconnects e.g. due to utility process failure,
// then the `done_callback` passed in `AsyncInit` is still called, and empty
// metrics are provided in `ProvideSystemProfileMetrics`. If disabled, then
// `done_callback` is never called in this scenario, as was the behavior prior
// to M150. See https://crbug.com/512423663 for details.
BASE_DECLARE_FEATURE(kReportEmptyAVMetricsOnFailure);

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
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    remote_util_win_.Bind(std::move(remote));
  }

 private:
  // Called when metrics are done being gathered from the FILE thread.
  void GotAntiVirusProducts(
      const std::vector<metrics::SystemProfileProto::AntiVirusProduct>& result);

  mojo::Remote<chrome::mojom::UtilWin> remote_util_win_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Information on installed AntiVirus gathered.
  std::vector<metrics::SystemProfileProto::AntiVirusProduct> av_products_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::OnceClosure done_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<base::ElapsedTimer> timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_ANTIVIRUS_METRICS_PROVIDER_WIN_H_
