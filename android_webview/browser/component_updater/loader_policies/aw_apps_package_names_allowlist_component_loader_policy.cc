// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/component_updater/loader_policies/aw_apps_package_names_allowlist_component_loader_policy.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/common/components/aw_apps_package_names_allowlist_component_utils.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
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

// Creates a bloomfilter after loading its data from file in `allowlist_fd`
// and lookup the given `package_name` in it.
bool IsLoggingPackageNameAllowed(int allowlist_fd,
                                 int numHash,
                                 int numBits,
                                 const std::string& package_name) {
  base::ScopedFILE file_stream(fdopen(allowlist_fd, "r"));
  if (!file_stream.get())
    return false;

  // TODO(https://crbug.com/1219496): use mmap instead of reading the whole
  // file.
  std::string bloom_filter_data;
  if (!base::ReadStreamToString(file_stream.get(), &bloom_filter_data) ||
      bloom_filter_data.empty())
    return false;

  // Make sure the bloomfilter binary data is of the correct length.
  if (bloom_filter_data.size() !=
      size_t((numBits + kBitsPerByte - 1) / kBitsPerByte)) {
    return false;
  }

  return optimization_guide::BloomFilter(numHash, numBits, bloom_filter_data)
      .Contains(package_name);
}

}  // namespace

AwAppsPackageNamesAllowlistComponentLoaderPolicy::
    AwAppsPackageNamesAllowlistComponentLoaderPolicy(
        std::string app_package_name,
        base::OnceCallback<void(bool)> lookup_callback)
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
    const base::flat_map<std::string, int>& fd_map,
    std::unique_ptr<base::DictionaryValue> manifest) {
  // Have to use double because base::DictionaryValue doesn't support int64
  // values.
  absl::optional<double> expiry_date_ms =
      manifest->FindDoublePath(kExpiryDateKey);
  absl::optional<int> num_hash = manifest->FindIntPath(kBloomFilterNumHashKey);
  absl::optional<int> num_bits = manifest->FindIntPath(kBloomFilterNumBitsKey);
  auto allowlist_iterator = fd_map.end();

  // Being conservative and consider the allowlist expired when a valid expiry
  // date is absent.
  if (num_hash.has_value() && num_bits.has_value() &&
      expiry_date_ms.has_value() &&
      base::Time::UnixEpoch() +
              base::TimeDelta::FromMillisecondsD(expiry_date_ms.value()) >
          base::Time::Now() &&
      (allowlist_iterator = fd_map.find(kAllowlistBloomFilterFileName)) !=
          fd_map.end()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&IsLoggingPackageNameAllowed, allowlist_iterator->second,
                       num_hash.value(), num_bits.value(),
                       std::move(app_package_name_)),
        std::move(lookup_callback_));
  } else {
    ComponentLoadFailed();
  }

  // Close unused files.
  // TODO(https://crbug.com/1219672): use base::ScopedFD instead.
  for (auto& iterator : fd_map) {
    if (iterator != *allowlist_iterator)
      close(iterator.second);
  }
}

void AwAppsPackageNamesAllowlistComponentLoaderPolicy::ComponentLoadFailed() {
  DCHECK(lookup_callback_);
  std::move(lookup_callback_).Run(/* lookup_result= */ false);
}

void AwAppsPackageNamesAllowlistComponentLoaderPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetWebViewAppsPackageNamesAllowlistPublicKeyHash(hash);
}

}  // namespace android_webview
