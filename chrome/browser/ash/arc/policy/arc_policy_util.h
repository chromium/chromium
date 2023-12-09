// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_UTIL_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>

#include "base/values.h"

class Profile;

namespace arc {
namespace policy_util {

constexpr char kArcPolicyKeyAccountTypesWithManagementDisabled[] =
    "accountTypesWithManagementDisabled";
constexpr char kArcPolicyKeyAlwaysOnVpnPackage[] = "alwaysOnVpnPackage";
constexpr char kArcPolicyKeyApplications[] = "applications";
constexpr char kArcPolicyKeyAvailableAppSetPolicyDeprecated[] =
    "availableAppSetPolicy";
constexpr char kArcPolicyKeyComplianceRules[] = "complianceRules";
constexpr char kArcPolicyKeyInstallUnknownSourcesDisabled[] =
    "installUnknownSourcesDisabled";
constexpr char kArcPolicyKeyMaintenanceWindow[] = "maintenanceWindow";
constexpr char kArcPolicyKeyModifyAccountsDisabled[] = "modifyAccountsDisabled";
constexpr char kArcPolicyKeyPermissionGrants[] = "permissionGrants";
constexpr char kArcPolicyKeyPermittedAccessibilityServices[] =
    "permittedAccessibilityServices";
constexpr char kArcPolicyKeyPlayStoreMode[] = "playStoreMode";
constexpr char kArcPolicyKeyShortSupportMessage[] = "shortSupportMessage";
constexpr char kArcPolicyKeyStatusReportingSettings[] =
    "statusReportingSettings";
constexpr char kArcPolicyKeyWorkAccountAppWhitelistDeprecated[] =
    "workAccountAppWhitelist";
constexpr char kArcPolicyKeyGuid[] = "guid";
constexpr char kArcPolicyKeyApkCacheEnabled[] = "apkCacheEnabled";
constexpr char kArcPolicyKeyMountPhysicalMediaDisabled[] =
    "mountPhysicalMediaDisabled";
constexpr char kArcPolicyKeyDebuggingFeaturesDisabled[] =
    "debuggingFeaturesDisabled";
constexpr char kArcPolicyKeyCameraDisabled[] = "cameraDisabled";
constexpr char kArcPolicyKeyPrintingDisabled[] = "printingDisabled";
constexpr char kArcPolicyKeyScreenCaptureDisabled[] = "screenCaptureDisabled";
constexpr char kArcPolicyKeyShareLocationDisabled[] = "shareLocationDisabled";
constexpr char kArcPolicyKeyUnmuteMicrophoneDisabled[] =
    "unmuteMicrophoneDisabled";
constexpr char kArcPolicyKeySetWallpaperDisabled[] = "setWallpaperDisabled";
constexpr char kArcPolicyKeyVpnConfigDisabled[] = "vpnConfigDisabled";
constexpr char kArcPolicyKeyPrivateKeySelectionEnabled[] =
    "privateKeySelectionEnabled";
constexpr char kArcPolicyKeyChoosePrivateKeyRules[] = "choosePrivateKeyRules";
constexpr char kArcPolicyKeyCredentialsConfigDisabled[] =
    "credentialsConfigDisabled";
constexpr char kArcPolicyKeyCaCerts[] = "caCerts";
constexpr char kArcPolicyKeyRequiredKeyPairs[] = "requiredKeyPairs";
constexpr char kArcPolicyKeyDpsInteractionsDisabled[] =
    "dpsInteractionsDisabled";
constexpr char kArcPolicyKeyEnabledSystemAppPackageNames[] =
    "enabledSystemAppPackageNames";

// An app's install type specified by the policy.
// See google3/wireless/android/enterprise/clouddps/proto/schema.proto.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "AppInstallType" in src/tools/metrics/histograms/enums.xml.
enum class InstallType {
  kUnknown = 0,
  kOptional = 1,
  kRequired = 2,
  kPreload = 3,
  kForceInstalled = 4,
  kBlocked = 5,
  kAvailable = 6,
  kRequiredForSetup = 7,
  kKiosk = 8,
  kMaxValue = kKiosk,
};

// A sub policy key inside ArcPolicy object.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "ArcPolicyKey" in src/tools/metrics/histograms/enums.xml.
enum class ArcPolicyKey {
  kUnknown = 0,
  kAccountTypesWithManagementDisabled = 1,
  kAlwaysOnVpnPackage = 2,
  kApplications = 3,
  kAvailableAppSetPolicyDeprecated = 4,
  kComplianceRules = 5,
  kInstallUnknownSourcesDisabled = 6,
  kMaintenanceWindow = 7,
  kModifyAccountsDisabled = 8,
  kPermissionGrants = 9,
  kPermittedAccessibilityServices = 10,
  kPlayStoreMode = 11,
  kShortSupportMessage = 12,
  kStatusReportingSettings = 13,
  kWorkAccountAppWhitelistDeprecated = 14,
  kApkCacheEnabled = 15,
  kDebuggingFeaturesDisabled = 16,
  kCameraDisabled = 17,
  kPrintingDisabled = 18,
  kScreenCaptureDisabled = 19,
  kShareLocationDisabled = 20,
  kUnmuteMicrophoneDisabled = 21,
  kSetWallpaperDisabled = 22,
  kVpnConfigDisabled = 23,
  kPrivateKeySelectionEnabled = 24,
  kChoosePrivateKeyRules = 25,
  kCredentialsConfigDisabled = 26,
  kCaCerts = 27,
  kRequiredKeyPairs = 28,
  kEnabledSystemAppPackageNames = 29,
  kMaxValue = kEnabledSystemAppPackageNames,
};

// Returns true if the account is managed. Otherwise false.
bool IsAccountManaged(const Profile* profile);

// Returns true if ARC is disabled by --enterprise-disable-arc flag.
bool IsArcDisabledForEnterprise();

// Returns set of packages requested to install from |arc_policy|. |arc_policy|
// has JSON blob format, see
// https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ArcPolicy
std::set<std::string> GetRequestedPackagesFromArcPolicy(
    const std::string& arc_policy);

// Records metrics about the policy e.g. keys, install types, etc.
void RecordPolicyMetrics(const std::string& arc_policy);

// Maps an app install type to all packages within the policy that have this
// install type.
std::map<std::string, std::set<std::string>> CreateInstallTypeMap(
    const base::Value& dict);

// Converts a string to its corresponding InstallType enum.
InstallType GetInstallTypeEnumFromString(const std::string& install_type);

// Converts a string to its corresponding ArcPolicyKey enum.
std::optional<ArcPolicyKey> GetPolicyKeyFromString(
    const std::string& policy_key);

// Parses policy JSON string to a dictionary.
std::optional<base::Value> ParsePolicyJson(const std::string& arc_policy);

}  // namespace policy_util
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_UTIL_H_
