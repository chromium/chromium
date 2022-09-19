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

// Returns true if the account is managed. Otherwise false.
bool IsAccountManaged(const Profile* profile);

// Returns true if ARC is disabled by --enterprise-disable-arc flag.
bool IsArcDisabledForEnterprise();

// Returns set of packages requested to install from |arc_policy|. |arc_policy|
// has JSON blob format, see
// https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ArcPolicy
std::set<std::string> GetRequestedPackagesFromArcPolicy(
    const std::string& arc_policy);

// Records which install types are present in the policy.
void RecordInstallTypesInPolicy(const std::string& arc_policy);

// Maps an app install type to all packages within the policy that have this
// install type.
std::map<std::string, std::set<std::string>> CreateInstallTypeMap(
    const std::string& policy);

// Converts a string to its corresponding InstallType enum.
InstallType GetInstallTypeEnumFromString(const std::string& install_type);

}  // namespace policy_util
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_POLICY_ARC_POLICY_UTIL_H_
