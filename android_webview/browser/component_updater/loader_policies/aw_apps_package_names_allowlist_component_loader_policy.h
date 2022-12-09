// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_LOADER_POLICY_H_
#define ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "android_webview/common/metrics/app_package_name_logging_rule.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "components/component_updater/android/component_loader_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
class Version;
}  // namespace base

namespace android_webview {

class AwMetricsServiceClient;

// All these constants have to be kept in sync with the allowlist generation
// server, see http://go/aw-package-names-allowlist-bloomfilter.
constexpr char kAllowlistBloomFilterFileName[] = "allowlistbloomfilter";
constexpr char kBloomFilterNumHashKey[] = "bloomfilter_num_hash";
constexpr char kBloomFilterNumBitsKey[] = "bloomfilter_num_bits";
constexpr char kExpiryDateKey[] = "expiry_date";

// Minimum time to throttle querying the app package names allowlist from the
// component updater service, used when there is no valid cached allowlist
// result. Exposed for testing only.
extern const base::TimeDelta kWebViewAppsMinAllowlistThrottleTimeDelta;
// Maximum time to throttle querying the app package names allowlist from the
// component updater service, used when there is a valid cached allowlist
// result. Exposed for testing only.
extern const base::TimeDelta kWebViewAppsMaxAllowlistThrottleTimeDelta;

// A callback for the result of loading and looking up the allowlist. If the
// allowlist loading fails, it will be called with a null record.
using AllowListLookupCallback =
    base::OnceCallback<void(absl::optional<AppPackageNameLoggingRule>)>;

// Defines a loader responsible for receiving the allowlist for apps package
// names that can be included in UMA records and lookup the embedding app's name
// in that list.
class AwAppsPackageNamesAllowlistComponentLoaderPolicy
    : public component_updater::ComponentLoaderPolicy {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AllowlistPraseStatus {
    kSuccess = 0,
    kUsingCache = 1,
    kMissingFields = 2,
    kExpiredAllowlist = 3,
    kMissingAllowlistFile = 4,
    kIOError = 5,
    kMalformedBloomFilter = 6,
    kMaxValue = kMalformedBloomFilter,
  };

  // `app_package_name` the embedding app package name.
  // `cached_record`    the cached lookup result of a previous successfully
  //                    loaded allowlist, if any.
  // `lookup_callback`  callback to report the result of looking up
  //                    `app_package_name` in the packages names allowlist.
  AwAppsPackageNamesAllowlistComponentLoaderPolicy(
      std::string app_package_name,
      absl::optional<AppPackageNameLoggingRule> cached_record,
      AllowListLookupCallback lookup_callback);
  ~AwAppsPackageNamesAllowlistComponentLoaderPolicy() override;

  AwAppsPackageNamesAllowlistComponentLoaderPolicy(
      const AwAppsPackageNamesAllowlistComponentLoaderPolicy&) = delete;
  AwAppsPackageNamesAllowlistComponentLoaderPolicy& operator=(
      const AwAppsPackageNamesAllowlistComponentLoaderPolicy&) = delete;

  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override;
  void ComponentLoadFailed(
      component_updater::ComponentLoadResult error) override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetMetricsSuffix() const override;

 private:
  std::string app_package_name_;
  absl::optional<AppPackageNameLoggingRule> cached_record_;

  AllowListLookupCallback lookup_callback_;
};

void LoadPackageNamesAllowlistComponent(
    component_updater::ComponentLoaderPolicyVector& policies,
    AwMetricsServiceClient* metrics_service_client);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_LOADER_POLICY_H_
