// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_map.h"

// Holds Google Update state fetched from the updater.
struct GoogleUpdateState {
  // Version reported by Google Update.
  std::string version;

  // Time at which the updater last performed an update check.
  base::Time last_checked_time;
};

// Holds Google Update policies and state fetched from the updater.
struct GoogleUpdatePoliciesAndState {
  // Policies parsed from the updater's JSON output.
  std::optional<policy::PolicyMap> policies;

  // State fetched from the updater (version, last checked time, etc.).
  std::optional<GoogleUpdateState> state;
};
// JSON field names for the JSON policy blob received from Google Update.
inline constexpr char kAutoUpdateCheckPeriodMinutes[] =
    "AutoUpdateCheckPeriodMinutes";
inline constexpr char kDownloadPreference[] = "DownloadPreference";
inline constexpr char kForceInstallApps[] = "ForceInstallApps";
inline constexpr char kInstallPolicy[] = "InstallPolicy";
inline constexpr char kProxyMode[] = "ProxyMode";
inline constexpr char kProxyPacUrl[] = "ProxyPacUrl";
inline constexpr char kProxyServer[] = "ProxyServer";
inline constexpr char kRollbackToTargetVersion[] = "RollbackToTargetVersion";
inline constexpr char kTargetVersionPrefix[] = "TargetVersionPrefix";
inline constexpr char kTargetChannel[] = "TargetChannel";
inline constexpr char kUpdatePolicy[] = "UpdatePolicy";
inline constexpr char kUpdatesSuppressedDurationMin[] =
    "UpdatesSuppressedDurationMin";
inline constexpr char kUpdatesSuppressedStartHour[] =
    "UpdatesSuppressedStartHour";
inline constexpr char kUpdatesSuppressedStartMinute[] =
    "UpdatesSuppressedStartMinute";
inline constexpr char kCloudPolicyOverridesPlatformPolicy[] =
    "CloudPolicyOverridesPlatformPolicy";
inline constexpr char kMajorVersionRolloutPolicy[] =
    "MajorVersionRolloutPolicy";
inline constexpr char kMinorVersionRolloutPolicy[] =
    "MinorVersionRolloutPolicy";

// Returns a list of all the supported Google Update policies.
base::Value GetGoogleUpdatePolicyNames();

// Returns a map of all the supported Google Update policies and their schemas.
policy::PolicyConversions::PolicyToSchemaMap GetGoogleUpdatePolicySchemas();

// Parses the policies JSON from the updater and populates the given PolicyMap.
// For details on the JSON structure (the "Policy Set" format), see
// chrome/browser/google/README.md.
// `app_id` is used to filter per-app policies.
void ParsePoliciesJsonIntoPolicyMap(std::string_view json,
                                    std::string_view app_id,
                                    policy::PolicyMap* policies);

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_POLICY_FETCHER_H_
