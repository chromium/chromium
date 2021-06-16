// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_LOADER_POLICY_H_
#define ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_LOADER_POLICY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "components/component_updater/android/component_loader_policy.h"

namespace base {
class DictionaryValue;
class Version;
}  // namespace base

namespace android_webview {

// All these constants have to be kept in sync with the allowlist generation
// server, see http://go/aw-package-names-allowlist-bloomfilter.
constexpr char kAllowlistBloomFilterFileName[] = "allowlistbloomfilter";
constexpr char kBloomFilterNumHashKey[] = "bloomfilter_num_hash";
constexpr char kBloomFilterNumBitsKey[] = "bloomfilter_num_bits";
constexpr char kExpiryDateKey[] = "expiry_date";

// Defines a loader responsible for receiving the allowlist for apps package
// names that can be included in UMA records and lookup the embedding app's name
// in that list.
class AwAppsPackageNamesAllowlistComponentLoaderPolicy
    : public component_updater::ComponentLoaderPolicy {
 public:
  // `app_package_name` the embedding app package name.
  // `lookup_callback` callback to report the result of looking up
  //                   `app_package_name` in the packages names allowlist.
  AwAppsPackageNamesAllowlistComponentLoaderPolicy(
      std::string app_package_name,
      base::OnceCallback<void(bool)> lookup_callback);
  ~AwAppsPackageNamesAllowlistComponentLoaderPolicy() override;

  AwAppsPackageNamesAllowlistComponentLoaderPolicy(
      const AwAppsPackageNamesAllowlistComponentLoaderPolicy&) = delete;
  AwAppsPackageNamesAllowlistComponentLoaderPolicy& operator=(
      const AwAppsPackageNamesAllowlistComponentLoaderPolicy&) = delete;

  // The following methods override ComponentLoaderPolicy.
  void ComponentLoaded(
      const base::Version& version,
      const base::flat_map<std::string, int>& fd_map,
      std::unique_ptr<base::DictionaryValue> manifest) override;
  void ComponentLoadFailed() override;
  void GetHash(std::vector<uint8_t>* hash) const override;

 private:
  std::string app_package_name_;
  base::OnceCallback<void(bool)> lookup_callback_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPONENT_UPDATER_LOADER_POLICIES_AW_APPS_PACKAGE_NAMES_ALLOWLIST_COMPONENT_LOADER_POLICY_H_
