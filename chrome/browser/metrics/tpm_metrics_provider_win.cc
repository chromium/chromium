// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tpm_metrics_provider_win.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/win/util_win_service.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace {

bool ShouldReportFullNames() {
  // The expectation is that this will be disabled for the majority of users,
  // but this allows a small group to be enabled on other channels if there are
  // a large percentage of hashes collected on these channels that are not
  // resolved to names previously collected on Canary channel.
  if (chrome::GetChannel() == version_info::Channel::CANARY) {
    return true;
  }

  return base::FeatureList::IsEnabled(kReportFullTPMIdentifierDetails);
}

}  // namespace

BASE_FEATURE(kReportFullTPMIdentifierDetails,
             "ReportFullTPMIdentifierDetails",
             base::FEATURE_DISABLED_BY_DEFAULT);

TPMMetricsProvider::TPMMetricsProvider() = default;

TPMMetricsProvider::~TPMMetricsProvider() = default;

void TPMMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  // If the `tpm_identifier_` manufacturer id is not set, the ID is unknown.
  // Metrics will not be reported to the system proto.
  if (!tpm_identifier_.has_value()) {
    return;
  }
  metrics::SystemProfileProto_TpmIdentifier* identifier =
      system_profile_proto->mutable_tpm_identifier();
  *identifier = tpm_identifier_.value();
}

void TPMMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  if (!remote_util_win_) {
    remote_util_win_ = LaunchUtilWinServiceInstance();
    remote_util_win_.reset_on_idle_timeout(base::Seconds(5));
  }

  // Intentionally don't handle connection errors as not reporting this metric
  // is acceptable in the rare cases it'll happen. The usage of
  // base::Unretained(this) is safe here because `remote_util_win_`, which owns
  // the callback, will be destroyed once this instance goes away.
  remote_util_win_->GetTpmIdentifier(
      ShouldReportFullNames(),
      base::BindOnce(&TPMMetricsProvider::GotTPMProduct, base::Unretained(this),
                     std::move(done_callback)));
}

void TPMMetricsProvider::GotTPMProduct(
    base::OnceClosure done_callback,
    const std::optional<metrics::SystemProfileProto::TpmIdentifier>& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_util_win_.reset();
  if (result.has_value()) {
    tpm_identifier_ = std::move(result);
  }
  std::move(done_callback).Run();
}
