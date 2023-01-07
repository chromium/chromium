// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"

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
  bool enabled = base::FeatureList::IsEnabled(kReportFullAVProductDetails);

  if (chrome::GetChannel() == version_info::Channel::CANARY)
    return true;

  return enabled;
}

}  // namespace

BASE_FEATURE(kReportFullAVProductDetails,
             "ReportFullAVProductDetails",
             base::FEATURE_DISABLED_BY_DEFAULT);

AntiVirusMetricsProvider::AntiVirusMetricsProvider() = default;

AntiVirusMetricsProvider::~AntiVirusMetricsProvider() = default;

void AntiVirusMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  for (const auto& av_product : av_products_) {
    metrics::SystemProfileProto_AntiVirusProduct* product =
        system_profile_proto->add_antivirus_product();
    *product = av_product;
  }
}

void AntiVirusMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  if (!remote_util_win_) {
    remote_util_win_ = LaunchUtilWinServiceInstance();
    remote_util_win_.reset_on_idle_timeout(base::Seconds(5));
  }

  // Intentionally don't handle connection errors as not reporting this metric
  // is acceptable in the rare cases it'll happen. The usage of
  // base::Unretained(this) is safe here because |remote_util_win_|, which owns
  // the callback, will be destroyed once this instance goes away.
  remote_util_win_->GetAntiVirusProducts(
      ShouldReportFullNames(),
      base::BindOnce(&AntiVirusMetricsProvider::GotAntiVirusProducts,
                     base::Unretained(this), std::move(done_callback)));
}

void AntiVirusMetricsProvider::GotAntiVirusProducts(
    base::OnceClosure done_callback,
    const std::vector<metrics::SystemProfileProto::AntiVirusProduct>& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_util_win_.reset();
  av_products_ = result;
  std::move(done_callback).Run();
}
