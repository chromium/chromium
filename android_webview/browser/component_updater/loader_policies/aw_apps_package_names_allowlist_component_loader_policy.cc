// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android_webview {

namespace {

constexpr int kBitsPerByte = 8;

// It returns an empty (null) absl::optional<base::Time> if loading of the
// allowlist file fails. Otherwise it returns:
// - `expiry_date` if the given `package_name` is in the allowlist.
// - `base::Time::Min()` if the given `package_name` isn't in the allowlist.
absl::optional<base::Time> GetExpiryTimeIfPackageNameLoggable(
    base::ScopedFD allowlist_fd,
    int num_hash,
    int num_bits,
    const std::string& package_name,
    const base::Time& expiry_date) {
  // Transfer the ownership of the file from `allowlist_fd` to `file_stream`.
  base::ScopedFILE file_stream(fdopen(allowlist_fd.release(), "r"));
  if (!file_stream.get())
    return absl::optional<base::Time>();

  // TODO(https://crbug.com/1219496): use mmap instead of reading the whole
  // file.
  std::string bloom_filter_data;
  if (!base::ReadStreamToString(file_stream.get(), &bloom_filter_data) ||
      bloom_filter_data.empty()) {
    return absl::optional<base::Time>();
  }

  // Make sure the bloomfilter binary data is of the correct length.
  if (bloom_filter_data.size() !=
      size_t((num_bits + kBitsPerByte - 1) / kBitsPerByte)) {
    return absl::optional<base::Time>();
  }

  return optimization_guide::BloomFilter(num_hash, num_bits, bloom_filter_data)
                 .Contains(package_name)
             ? expiry_date
             : base::Time::Min();
}

void SetShouldRecordPackageName(absl::optional<base::Time> expiry_date) {
  auto* metrics_service_client = AwMetricsServiceClient::GetInstance();
  DCHECK(metrics_service_client);
  metrics_service_client->SetShouldRecordPackageName(expiry_date);

  if (expiry_date.has_value()) {
    VLOG(2) << "WebView apps package allowlist is loaded, expiry_date ="
            << (expiry_date.value() - base::Time::UnixEpoch()).InMilliseconds();
  } else {
    VLOG(2) << "Failed to load WebView apps package allowlist";
  }
}

}  // namespace

AwAppsPackageNamesAllowlistComponentLoaderPolicy::
    AwAppsPackageNamesAllowlistComponentLoaderPolicy(
        std::string app_package_name,
        AllowListLookupCallback lookup_callback)
    : app_package_name_(std::move(app_package_name)),
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
    std::unique_ptr<base::DictionaryValue> manifest) {
  // TODO(https://crbug.com/1216202): store the allowlist version in the local
  // cache, don't lookup the allowlist if it's the same version.

  // Have to use double because base::DictionaryValue doesn't support int64
  // values.
  absl::optional<double> expiry_date_ms =
      manifest->FindDoublePath(kExpiryDateKey);
  absl::optional<int> num_hash = manifest->FindIntPath(kBloomFilterNumHashKey);
  absl::optional<int> num_bits = manifest->FindIntPath(kBloomFilterNumBitsKey);
  // Being conservative and consider the allowlist expired when a valid expiry
  // date is absent.
  if (!expiry_date_ms.has_value() || !num_hash.has_value() ||
      !num_bits.has_value() || num_hash.value() <= 0 || num_bits.value() <= 0) {
    ComponentLoadFailed();
    return;
  }

  base::Time expiry_date =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMillisecondsD(expiry_date_ms.value_or(0.0));
  if (expiry_date <= base::Time::Now()) {
    ComponentLoadFailed();
    return;
  }

  auto allowlist_iterator = fd_map.find(kAllowlistBloomFilterFileName);
  if (allowlist_iterator == fd_map.end()) {
    ComponentLoadFailed();
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetExpiryTimeIfPackageNameLoggable,
                     std::move(allowlist_iterator->second), num_hash.value(),
                     num_bits.value(), std::move(app_package_name_),
                     expiry_date),
      std::move(lookup_callback_));
}

void AwAppsPackageNamesAllowlistComponentLoaderPolicy::ComponentLoadFailed() {
  DCHECK(lookup_callback_);
  std::move(lookup_callback_)
      .Run(/* expiry_date= */ absl::optional<base::Time>());
}

void AwAppsPackageNamesAllowlistComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetWebViewAppsPackageNamesAllowlistPublicKeyHash(hash);
}

void LoadPackageNamesAllowlistComponent(
    component_updater::ComponentLoaderPolicyVector& policies) {
  if (!base::FeatureList::IsEnabled(
          android_webview::features::kWebViewAppsPackageNamesAllowlist)) {
    return;
  }
  policies.push_back(
      std::make_unique<AwAppsPackageNamesAllowlistComponentLoaderPolicy>(
          AwMetricsServiceClient::GetInstance()->GetAppPackageName(),
          base::BindOnce(&SetShouldRecordPackageName)));
}

}  // namespace android_webview
