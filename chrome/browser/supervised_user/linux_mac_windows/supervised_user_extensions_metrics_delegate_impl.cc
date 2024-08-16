// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/linux_mac_windows/supervised_user_extensions_metrics_delegate_impl.h"

#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "extensions/common/extension_set.h"

namespace {

int GetExtensionsCount(const extensions::ExtensionSet& extensions) {
  return base::ranges::count_if(extensions, [](const auto& extension) {
    return !extensions::Manifest::IsComponentLocation(extension->location()) &&
           (extension->is_extension() || extension->is_theme());
  });
}

// UMA metrics for a snapshot count of installed and enabled extensions for a
// given supervised user (Family User).
constexpr char kInstalledExtensionsCountHistogramName[] =
    "FamilyUser.InstalledExtensionsCount2";
constexpr char kEnabledExtensionsCountHistogramName[] =
    "FamilyUser.EnabledExtensionsCount2";
constexpr char kDisabledExtensionsCountHistogramName[] =
    "FamilyUser.DisabledExtensionsCount2";
}  // namespace

SupervisedUserExtensionsMetricsDelegateImpl::
    SupervisedUserExtensionsMetricsDelegateImpl(
        const extensions::ExtensionRegistry* extension_registry,
        Profile* profile)
    : extension_registry_(extension_registry), profile_(profile) {}

SupervisedUserExtensionsMetricsDelegateImpl::
    ~SupervisedUserExtensionsMetricsDelegateImpl() = default;

bool SupervisedUserExtensionsMetricsDelegateImpl::RecordExtensionsMetrics() {
  if (!supervised_user::AreExtensionsPermissionsEnabled(profile_.get())) {
    return false;
  }
  const extensions::ExtensionSet all_installed_extensions =
      extension_registry_->GenerateInstalledExtensionsSet();
  int extensions_count = GetExtensionsCount(all_installed_extensions);
  base::UmaHistogramCounts1000(kInstalledExtensionsCountHistogramName,
                               extensions_count);
  extensions_count =
      GetExtensionsCount(extension_registry_->enabled_extensions());
  base::UmaHistogramCounts1000(kEnabledExtensionsCountHistogramName,
                               extensions_count);
  extensions_count =
      GetExtensionsCount(extension_registry_->disabled_extensions());
  base::UmaHistogramCounts1000(kDisabledExtensionsCountHistogramName,
                               extensions_count);
  return true;
}
