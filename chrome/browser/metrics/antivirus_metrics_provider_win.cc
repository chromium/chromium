// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/win/util_win_service.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

enum class State {
  kNotStarted,
  kFetching,
  kReady,
};

struct AvCache {
  State state = State::kNotStarted;
  std::vector<metrics::SystemProfileProto::AntiVirusProduct> products;
  std::vector<base::OnceClosure> pending_callbacks;
  mojo::Remote<chrome::mojom::UtilWin> remote_util_win;
  std::optional<base::ElapsedTimer> timer;
  SEQUENCE_CHECKER(sequence_checker);
};

AvCache& GetAvCache() {
  static base::NoDestructor<AvCache> av_cache;
  return *av_cache;
}

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

BASE_FEATURE(kReportFullAVProductDetails, base::FEATURE_DISABLED_BY_DEFAULT);

AntiVirusMetricsProvider::AntiVirusMetricsProvider() = default;

AntiVirusMetricsProvider::~AntiVirusMetricsProvider() = default;

void AntiVirusMetricsProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  AvCache& cache = GetAvCache();
  DCHECK_CALLED_ON_VALID_SEQUENCE(cache.sequence_checker);

  // Safely return if cache is not ready yet.
  if (cache.state != State::kReady) {
    return;
  }

  for (const auto& av_product : cache.products) {
    *system_profile_proto->add_antivirus_product() = av_product;
  }
}

void AntiVirusMetricsProvider::AsyncInit(base::OnceClosure done_callback) {
  AvCache& cache = GetAvCache();
  DCHECK_CALLED_ON_VALID_SEQUENCE(cache.sequence_checker);

  // If already cached, run callback immediately.
  if (cache.state == State::kReady) {
    std::move(done_callback).Run();
    return;
  }

  // Queue the callback in the cache list.
  cache.pending_callbacks.push_back(std::move(done_callback));

  // If the query is already in-flight, return without starting a duplicate
  // query.
  if (cache.state == State::kFetching) {
    return;
  }

  // Start the timer and launch the Mojo service.
  cache.state = State::kFetching;
  cache.timer.emplace();
  if (!cache.remote_util_win) {
    cache.remote_util_win = LaunchUtilWinServiceInstance();
    cache.remote_util_win.reset_on_idle_timeout(base::Seconds(5));
  }

  // Bind to static handler so Mojo isn't tied to this instance's lifetime.
  auto callback =
      base::BindOnce(&AntiVirusMetricsProvider::GotAntiVirusProducts);

  // Start antivirus product query.
  cache.remote_util_win->GetAntiVirusProducts(
      ShouldReportFullNames(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          std::vector<metrics::SystemProfileProto::AntiVirusProduct>()));
}

void AntiVirusMetricsProvider::GotAntiVirusProducts(
    const std::vector<metrics::SystemProfileProto::AntiVirusProduct>& result) {
  AvCache& cache = GetAvCache();
  DCHECK_CALLED_ON_VALID_SEQUENCE(cache.sequence_checker);

  cache.remote_util_win.reset();
  cache.products = result;
  cache.state = State::kReady;

  if (!cache.products.empty() && cache.timer.has_value()) {
    base::UmaHistogramTimes("UMA.AntiVirusMetricsProvider.Latency",
                            cache.timer->Elapsed());
  }
  cache.timer.reset();

  std::vector<base::OnceClosure> callbacks = std::move(cache.pending_callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void AntiVirusMetricsProvider::SetRemoteUtilWinForTesting(
    mojo::PendingRemote<chrome::mojom::UtilWin> remote) {
  AvCache& cache = GetAvCache();
  DCHECK_CALLED_ON_VALID_SEQUENCE(cache.sequence_checker);
  cache.state = State::kNotStarted;
  cache.products.clear();
  cache.pending_callbacks.clear();
  cache.remote_util_win.reset();
  cache.timer.reset();
  cache.remote_util_win =
      mojo::Remote<chrome::mojom::UtilWin>(std::move(remote));
}
