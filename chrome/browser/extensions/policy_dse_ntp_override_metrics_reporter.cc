// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_dse_ntp_override_metrics_reporter.h"

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC));

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/management/management_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

// static
void PolicyDseNtpOverrideMetricsReporter::ReportMetrics(Profile* profile) {
  // Skip guest and incognito profiles.
  if (profile->IsOffTheRecord()) {
    return;
  }
  auto* registry = ExtensionRegistry::Get(profile);
  if (!registry) {
    return;
  }

  auto* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile);
  if (!extension_management) {
    return;
  }

  // TODO(crbug.com/536913423): Remove legacy histograms once migration to
  // Extensions.SettingsOverrideV2 is complete.
  bool legacy_is_low_trust =
      GetHigherManagementAuthorityTrustworthiness(profile) <
      policy::ManagementAuthorityTrustworthiness::TRUSTED;
  std::string_view legacy_trust_string =
      legacy_is_low_trust ? "LowTrust" : "HighTrust";

  bool is_low_trust =
      GetHigherManagementAuthorityTrustworthinessForPolicyLoading(profile) <
      policy::ManagementAuthorityTrustworthiness::TRUSTED;
  std::string_view trust_string = is_low_trust ? "LowTrust" : "HighTrust";

  auto installed_extensions = registry->GenerateInstalledExtensionsSet();
  for (const auto& extension : installed_extensions) {
    ManagedInstallationMode mode =
        extension_management->GetInstallationMode(extension.get());
    std::string_view mode_string;
    if (mode == ManagedInstallationMode::kForced) {
      mode_string = "Forced";
    } else if (mode == ManagedInstallationMode::kRecommended) {
      mode_string = "Recommended";
    } else {
      continue;  // Skip if not forced or recommended
    }

    util::DseNtpOverrideType type = util::GetDseNtpOverrideType(*extension);
    std::string_view legacy_override_type_string;
    switch (type) {
      case util::DseNtpOverrideType::kDse:
        legacy_override_type_string = "DseOverride";
        break;
      case util::DseNtpOverrideType::kNtp:
        legacy_override_type_string = "NtpOverride";
        break;
      case util::DseNtpOverrideType::kBoth:
        legacy_override_type_string = "BothOverride";
        break;
      case util::DseNtpOverrideType::kNone:
        continue;  // Skip if no override
    }

    bool enabled = registry->enabled_extensions().Contains(extension->id());
    PolicyExtensionStatus status = enabled ? PolicyExtensionStatus::kEnabled
                                           : PolicyExtensionStatus::kDisabled;

    // Report legacy histograms.
    base::UmaHistogramEnumeration(
        base::StrCat({"Extensions.", legacy_override_type_string, ".",
                      legacy_trust_string, ".", mode_string}),
        status);

    // Report V2 histograms. Extensions overriding both DSE and NTP are reported
    // to both Dse and Ntp histograms.
    if (type == util::DseNtpOverrideType::kDse ||
        type == util::DseNtpOverrideType::kBoth) {
      base::UmaHistogramEnumeration(
          base::StrCat({"Extensions.SettingsOverrideV2.Dse.", trust_string, ".",
                        mode_string}),
          status);
    }
    if (type == util::DseNtpOverrideType::kNtp ||
        type == util::DseNtpOverrideType::kBoth) {
      base::UmaHistogramEnumeration(
          base::StrCat({"Extensions.SettingsOverrideV2.Ntp.", trust_string, ".",
                        mode_string}),
          status);
    }
  }
}

}  // namespace extensions
