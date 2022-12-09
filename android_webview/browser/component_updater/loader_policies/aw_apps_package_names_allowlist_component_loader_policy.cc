// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/aw_apps_package_names_allowlist_component_loader_policy.h"

#include <stdint.h>
#include <stdio.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"
#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android_webview {

const base::TimeDelta kWebViewAppsMinAllowlistThrottleTimeDelta =
    base::Hours(1);
const base::TimeDelta kWebViewAppsMaxAllowlistThrottleTimeDelta = base::Days(2);

namespace {

// Persisted to logs, should never change.
constexpr char kAppsPackageNamesAllowlistMetricsSuffix[] =
    "WebViewAppsPackageNamesAllowlist";
constexpr int kBitsPerByte = 8;

using AllowlistPraseStatus =
    AwAppsPackageNamesAllowlistComponentLoaderPolicy::AllowlistPraseStatus;

struct AllowListLookupResult {
  AllowlistPraseStatus parse_status;
  absl::optional<AppPackageNameLoggingRule> record_rule;
};

void RecordAndReportResult(AllowListLookupCallback lookup_callback,
                           AllowListLookupResult result) {
  DCHECK(lookup_callback);

  UMA_HISTOGRAM_ENUMERATION(
      "Android.WebView.Metrics.PackagesAllowList.ParseStatus",
      result.parse_status);
  std::move(lookup_callback).Run(result.record_rule);
}

// Lookup the `package_name` in `allowlist_fd`, returns a null
// an `AllowListLookupResult` containing an `AppPackageNameLoggingRule` if it
// fails.
//
// `allowlist_fd` the fd of a file the contain a bloomfilter that represents an
//                allowlist of apps package names.
// `num_hash`     the number of hash functions to use in the
//                `optimization_guide::BloomFilter`.
// `num_bits`     the number of bits in the `optimization_guide::BloomFilter`.
// `package_name` the app package name to look up in the allowlist.
// `version`      the allowlist version.
// `expiry_date`  the expiry date of the allowlist, i.e the date after which
//                this allowlist shouldn't be used.
AllowListLookupResult GetAppPackageNameLoggingRule(
    base::ScopedFD allowlist_fd,
    int num_hash,
    int num_bits,
    const std::string& package_name,
    const base::Version& version,
    const base::Time& expiry_date) {
  // Transfer the ownership of the file from `allowlist_fd` to `file_stream`.
  base::ScopedFILE file_stream(fdopen(allowlist_fd.release(), "r"));
  if (!file_stream.get()) {
    return {AllowlistPraseStatus::kIOError};
  }

  // TODO(https://crbug.com/1219496): use mmap instead of reading the whole
  // file.
  std::string bloom_filter_data;
  if (!base::ReadStreamToString(file_stream.get(), &bloom_filter_data) ||
      bloom_filter_data.empty()) {
    return {AllowlistPraseStatus::kIOError};
  }

  // Make sure the bloomfilter binary data is of the correct length.
  if (bloom_filter_data.size() !=
      size_t((num_bits + kBitsPerByte - 1) / kBitsPerByte)) {
    return {AllowlistPraseStatus::kMalformedBloomFilter};
  }

  if (optimization_guide::BloomFilter(num_hash, num_bits, bloom_filter_data)
          .Contains(package_name)) {
    return {AllowlistPraseStatus::kSuccess,
            AppPackageNameLoggingRule(version, expiry_date)};
  } else {
    return {AllowlistPraseStatus::kSuccess,
            AppPackageNameLoggingRule(version, base::Time::Min())};
  }
}

void SetAppPackageNameLoggingRule(
    absl::optional<AppPackageNameLoggingRule> record) {
  auto* metrics_service_client = AwMetricsServiceClient::GetInstance();
  DCHECK(metrics_service_client);
  metrics_service_client->SetAppPackageNameLoggingRule(record);
  metrics_service_client->SetAppPackageNameLoggingRuleLastUpdateTime(
      base::Time::Now());

  if (!record.has_value()) {
    VLOG(2) << "Failed to load WebView apps package allowlist";
    return;
  }

  VLOG(2) << "WebView apps package allowlist version "
          << record.value().GetVersion() << " is loaded";
  if (record.value().IsAppPackageNameAllowed()) {
    VLOG(2) << "App package name should be recorded until "
            << record.value().GetExpiryDate();
  } else {
    VLOG(2) << "App package name shouldn't be recorded";
  }
}

bool ShouldThrottleAppPackageNamesAllowlistComponent(
    base::Time last_update,
    absl::optional<AppPackageNameLoggingRule> cached_record) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewDisablePackageAllowlistThrottling) ||
      last_update.is_null()) {
    return false;
  }
  base::TimeDelta throttle_time_delta(
      kWebViewAppsMinAllowlistThrottleTimeDelta);
  if (cached_record.has_value()) {
    base::Time expiry_date = cached_record.value().GetExpiryDate();
    bool in_the_allowlist = !expiry_date.is_min();
    bool is_allowlist_expired = expiry_date <= base::Time::Now();
    if (!in_the_allowlist || !is_allowlist_expired) {
      throttle_time_delta = kWebViewAppsMaxAllowlistThrottleTimeDelta;
    }
  }
  return base::Time::Now() - last_update <= throttle_time_delta;
}

}  // namespace

AwAppsPackageNamesAllowlistComponentLoaderPolicy::
    AwAppsPackageNamesAllowlistComponentLoaderPolicy(
        std::string app_package_name,
        absl::optional<AppPackageNameLoggingRule> cached_record,
        AllowListLookupCallback lookup_callback)
    : app_package_name_(std::move(app_package_name)),
      cached_record_(cached_record),
      lookup_callback_(std::move(lookup_callback)) {
  DCHECK(!app_package_name_.empty());
  DCHECK(lookup_callback_);
}

AwAppsPackageNamesAllowlistComponentLoaderPolicy::
    ~AwAppsPackageNamesAllowlistComponentLoaderPolicy() = default;

// `manifest` represents a JSON object that looks like this:
// {
//   "name": "WebViewAppsPackageNamesAllowlist",
//
//   /* The component version string, matches the param `version`. */
//   "version": "xxxx.xx.xx.xx",
//
//   /* Bloomfilter parameters, set by the server side */
//   /* int32: number of hash functions used by the bloomfilter */
//   "bloomfilter_num_hash": xx
//   /* int32: number of bits in the bloomfilter binary array */
//   "bloomfilter_num_bits": xx
//
//   /* The allowlist expiry date after which the allowlist shouldn't be used.
//   Its format is an int64 number representing the number of milliseconds
//   after UNIX Epoch. */
//   "expiry_date": 12345678910
// }
void AwAppsPackageNamesAllowlistComponentLoaderPolicy::ComponentLoaded(
    const base::Version& version,
    base::flat_map<std::string, base::ScopedFD>& fd_map,
    base::Value::Dict manifest) {
  DCHECK(version.IsValid());

  // Have to use double because base::DictionaryValue doesn't support int64
  // values.
  absl::optional<double> expiry_date_ms =
      manifest.FindDoubleByDottedPath(kExpiryDateKey);
  absl::optional<int> num_hash =
      manifest.FindIntByDottedPath(kBloomFilterNumHashKey);
  absl::optional<int> num_bits =
      manifest.FindIntByDottedPath(kBloomFilterNumBitsKey);
  // Being conservative and consider the allowlist expired when a valid expiry
  // date is absent.
  if (!expiry_date_ms.has_value() || !num_hash.has_value() ||
      !num_bits.has_value() || num_hash.value() <= 0 || num_bits.value() <= 0) {
    RecordAndReportResult(std::move(lookup_callback_),
                          {AllowlistPraseStatus::kMissingFields});
    return;
  }

  base::Time expiry_date = base::Time::UnixEpoch() +
                           base::Milliseconds(expiry_date_ms.value_or(0.0));
  if (expiry_date <= base::Time::Now()) {
    RecordAndReportResult(std::move(lookup_callback_),
                          {AllowlistPraseStatus::kExpiredAllowlist});
    return;
  }

  if (cached_record_.has_value() &&
      cached_record_.value().GetVersion() == version) {
    RecordAndReportResult(std::move(lookup_callback_),
                          {AllowlistPraseStatus::kUsingCache, cached_record_});
    return;
  }

  auto allowlist_iterator = fd_map.find(kAllowlistBloomFilterFileName);
  if (allowlist_iterator == fd_map.end()) {
    RecordAndReportResult(std::move(lookup_callback_),
                          {AllowlistPraseStatus::kMissingAllowlistFile});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetAppPackageNameLoggingRule,
                     std::move(allowlist_iterator->second), num_hash.value(),
                     num_bits.value(), std::move(app_package_name_), version,
                     expiry_date),
      base::BindOnce(&RecordAndReportResult, std::move(lookup_callback_)));
}

void AwAppsPackageNamesAllowlistComponentLoaderPolicy::ComponentLoadFailed(
    component_updater::ComponentLoadResult /*error*/) {
  DCHECK(lookup_callback_);
  // TODO(crbug.com/1216200): Record the error in a histogram in the
  // ComponentLoader for each component.
  std::move(lookup_callback_).Run(absl::optional<AppPackageNameLoggingRule>());
}

void AwAppsPackageNamesAllowlistComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetWebViewAppsPackageNamesAllowlistPublicKeyHash(hash);
}

std::string AwAppsPackageNamesAllowlistComponentLoaderPolicy::GetMetricsSuffix()
    const {
  return kAppsPackageNamesAllowlistMetricsSuffix;
}

void LoadPackageNamesAllowlistComponent(
    component_updater::ComponentLoaderPolicyVector& policies,
    AwMetricsServiceClient* metrics_service_client) {
  DCHECK(metrics_service_client);

  // Prevent loading of client-side allowlist if using server-side allowlist
  if (base::FeatureList::IsEnabled(
          android_webview::features::
              kWebViewAppsPackageNamesServerSideAllowlist)) {
    return;
  }
  absl::optional<AppPackageNameLoggingRule> cached_record =
      metrics_service_client->GetCachedAppPackageNameLoggingRule();
  base::Time last_update =
      metrics_service_client->GetAppPackageNameLoggingRuleLastUpdateTime();

  bool should_throttle = ShouldThrottleAppPackageNamesAllowlistComponent(
      last_update, cached_record);
  UMA_HISTOGRAM_BOOLEAN(
      "Android.WebView.Metrics.PackagesAllowList.ThrottleStatus",
      should_throttle);
  if (should_throttle) {
    return;
  }

  policies.push_back(
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          metrics_service_client->GetAppPackageName(), std::move(cached_record),
          base::BindOnce(&SetAppPackageNameLoggingRule)));
}

}  // namespace android_webview
