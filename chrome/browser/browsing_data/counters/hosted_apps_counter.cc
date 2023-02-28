// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"

#include <algorithm>
#include <string>

#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_constants/constants.h"
#include "components/browsing_data/core/pref_names.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

HostedAppsCounter::HostedAppsCounter(Profile* profile)
    : profile_(profile) {}

HostedAppsCounter::~HostedAppsCounter() = default;

const char* HostedAppsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteHostedAppsData;
}

void HostedAppsCounter::Count() {
  std::vector<std::string> names;

  const extensions::ExtensionSet extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  auto* special_storage_policy = profile_->GetExtensionSpecialStoragePolicy();

  for (const auto& extension : extensions) {
    // Exclude kChromeAppId because this is not a proper hosted app. It is just
    // a shortcut to launch Chrome on Chrome OS.
    if (special_storage_policy->NeedsProtection(extension.get()) &&
        extension->id() != app_constants::kChromeAppId) {
      names.push_back(extension->short_name());
    }
  }

  int count = names.size();

  // Give the first two names (alphabetically) as examples.
  std::sort(names.begin(), names.end());
  names.resize(std::min<size_t>(2u, names.size()));

  ReportResult(std::make_unique<HostedAppsResult>(this, count, names));
}

// HostedAppsCounter::HostedAppsResult -----------------------------------------

HostedAppsCounter::HostedAppsResult::HostedAppsResult(
    const HostedAppsCounter* source,
    ResultInt num_apps,
    const std::vector<std::string>& examples)
    : FinishedResult(source, num_apps), examples_(examples) {}

HostedAppsCounter::HostedAppsResult::~HostedAppsResult() {}
