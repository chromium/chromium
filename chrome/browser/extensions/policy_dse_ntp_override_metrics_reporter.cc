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
#include "chrome/browser/extensions/low_trust_policy_install_block_manager.h"
#include "chrome/browser/extensions/management/management_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

std::string_view GetModeString(ManagedInstallationMode mode) {
  switch (mode) {
    case ManagedInstallationMode::kForced:
      return "Forced";
    case ManagedInstallationMode::kRecommended:
      return "Recommended";
    case ManagedInstallationMode::kAllowed:
    case ManagedInstallationMode::kBlocked:
    case ManagedInstallationMode::kRemoved:
      return std::string_view();
  }
}

std::string_view GetLegacyOverrideTypeString(util::DseNtpOverrideType type) {
  switch (type) {
    case util::DseNtpOverrideType::kDse:
      return "DseOverride";
    case util::DseNtpOverrideType::kNtp:
      return "NtpOverride";
    case util::DseNtpOverrideType::kBoth:
      return "BothOverride";
    case util::DseNtpOverrideType::kNone:
      return std::string_view();
  }
}

void LogStatus(util::DseNtpOverrideType type,
               bool legacy_is_low_trust,
               bool is_low_trust,
               ManagedInstallationMode mode,
               PolicyExtensionStatus status) {
  std::string_view legacy_override_type_string =
      GetLegacyOverrideTypeString(type);
  std::string_view mode_string = GetModeString(mode);
  if (legacy_override_type_string.empty() || mode_string.empty()) {
    return;
  }

  std::string_view legacy_trust_string =
      legacy_is_low_trust ? "LowTrust" : "HighTrust";
  std::string_view trust_string = is_low_trust ? "LowTrust" : "HighTrust";

  // Report legacy histograms. Note that Blocked and UserInstalled* states are
  // logged under legacy_trust_string even when legacy_is_low_trust is false
  // (e.g. Windows Workplace-Joined devices where
  // GetHigherManagementAuthorityTrustworthiness() is HighTrust while
  // GetHigherManagementAuthorityTrustworthinessForPolicyLoading() is LowTrust)
  // so the legacy HighTrust bucket directly captures the delta between the two
  // trust methods.
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

}  // namespace

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

  LowTrustPolicyInstallBlockManager* block_manager =
      extension_management->low_trust_block_manager();

  // TODO(crbug.com/536913423): Remove legacy histograms once the impact of the
  // revised management authority trustworthiness retrieval method is well
  // understood.
  bool legacy_is_low_trust =
      GetHigherManagementAuthorityTrustworthiness(profile) <
      policy::ManagementAuthorityTrustworthiness::TRUSTED;

  bool is_low_trust =
      GetHigherManagementAuthorityTrustworthinessForPolicyLoading(profile) <
      policy::ManagementAuthorityTrustworthiness::TRUSTED;

  // Pass 1: Report metrics for active installed extensions overriding DSE or
  // NTP.
  auto installed_extensions = registry->GenerateInstalledExtensionsSet();
  for (const auto& extension : installed_extensions) {
    util::DseNtpOverrideType override_type =
        util::GetDseNtpOverrideType(*extension);
    if (override_type == util::DseNtpOverrideType::kNone) {
      continue;
    }

    ManagedInstallationMode mode =
        extension_management->GetConfiguredInstallationMode(*extension);
    if (GetModeString(mode).empty()) {
      continue;
    }

    PolicyExtensionStatus status;
    if (extension_management->IsPolicyInstalled(*extension)) {
      // Active policy-installed extension (forced or recommended).
      bool enabled = registry->enabled_extensions().Contains(extension->id());
      status = enabled ? PolicyExtensionStatus::kEnabled
                       : PolicyExtensionStatus::kDisabled;
    } else if (extension_management->IsExtensionBlockedByLowTrust(
                   extension->id())) {
      // User manually installed extension after policy install was blocked.
      bool enabled = registry->enabled_extensions().Contains(extension->id());
      status = enabled ? PolicyExtensionStatus::kUserInstalledEnabled
                       : PolicyExtensionStatus::kUserInstalledDisabled;
    } else {
      continue;
    }

    LogStatus(override_type, legacy_is_low_trust, is_low_trust, mode, status);
  }

  // Pass 2: Report metrics for low-trust blocked extensions that are NOT
  // installed.
  if (block_manager && extension_management->IsDseNtpOverrideBlockingActive()) {
    for (const auto& [id, info] : block_manager->GetAllBlocked()) {
      if (installed_extensions.Contains(id)) {
        continue;  // Already handled in Pass 1 as user-installed.
      }

      ManagedInstallationMode mode =
          extension_management->GetConfiguredInstallationMode(id,
                                                              info.update_url);

      LogStatus(info.override_type, legacy_is_low_trust, is_low_trust, mode,
                PolicyExtensionStatus::kBlocked);
    }
  }
}

// static
void PolicyDseNtpOverrideMetricsReporter::LogBlockAction(
    LowTrustPolicyBlockAction action) {
  base::UmaHistogramEnumeration("Extensions.LowTrustPolicyBlock.Action",
                                action);
}

}  // namespace extensions
