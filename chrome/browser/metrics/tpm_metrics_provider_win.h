// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TPM_METRICS_PROVIDER_WIN_H_
#define CHROME_BROWSER_METRICS_TPM_METRICS_PROVIDER_WIN_H_

#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "components/metrics/metrics_provider.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/metrics_proto/system_profile.pb.h"

// Allows for full TPM metrics reporting on non-Canary channels in case
// the hash values collected are not able to map to their string
// counterparts
BASE_DECLARE_FEATURE(kReportFullTPMIdentifierDetails);

// TPMMetricsProvider is responsible for adding TPM information to
// the UMA system profile proto.
class TPMMetricsProvider : public metrics::MetricsProvider {
 public:
  TPMMetricsProvider();

  TPMMetricsProvider(const TPMMetricsProvider&) = delete;
  TPMMetricsProvider& operator=(const TPMMetricsProvider&) = delete;

  ~TPMMetricsProvider() override;

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
  // `done_callback` is the callback that should be called once all metrics are
  // gathered.
  void GotTPMProduct(
      base::OnceClosure done_callback,
      const std::optional<metrics::SystemProfileProto::TpmIdentifier>& result);

  mojo::Remote<chrome::mojom::UtilWin> remote_util_win_;

  // Information on installed TPM gathered.
  std::optional<metrics::SystemProfileProto::TpmIdentifier> tpm_identifier_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_TPM_METRICS_PROVIDER_WIN_H_
