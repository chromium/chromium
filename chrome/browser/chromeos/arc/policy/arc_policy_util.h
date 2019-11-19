// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_UTIL_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/optional.h"

class Profile;

namespace enterprise_management {
class CloudPolicySettings;
}

namespace arc {
namespace policy_util {

// The action that should be taken when an ecryptfs user home which needs
// migration is detected. Each valid value of the
// EcryptfsMigrationStrategy policy must either map directly to one of these
// entries (by having the same numerical value) or be aliased to one of them by
// the EcryptfsMigrationStrategyPolicyHandler.
enum class EcryptfsMigrationAction : int32_t {
  // Don't migrate.
  kDisallowMigration = 0,
  // Migrate without asking the user.
  kMigrate = 1,
  // Wipe the user home and start again.
  kWipe = 2,
  // Ask the user if migration should be performed (available to consumers
  // only).
  kAskUser = 3,
  // Minimal migration - similar to kWipe, but runs migration code with a small
  // whitelist of files to preserve authentication data.
  kMinimalMigrate = 4,
  // No longer supported.
  kAskForEcryptfsArcUsersNoLongerSupported = 5,
};

// Returns true if the account is managed. Otherwise false.
bool IsAccountManaged(const Profile* profile);

// Returns true if ARC is disabled by --enterprise-disable-arc flag.
bool IsArcDisabledForEnterprise();

// Decodes the EcryptfsMigrationStrategy user policy into the
// EcryptfsMigrationAction enum. If the policy is present and has a valid value,
// returns the value. Otherwise returns base::nullopt.
base::Optional<EcryptfsMigrationAction> DecodeMigrationActionFromPolicy(
    const enterprise_management::CloudPolicySettings& policy);

// Returns set of packages requested to install from |arc_policy|. |arc_policy|
// has JSON blob format, see
// https://cloud.google.com/docs/chrome-enterprise/policies/?policy=ArcPolicy
std::set<std::string> GetRequestedPackagesFromArcPolicy(
    const std::string& arc_policy);

}  // namespace policy_util
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_POLICY_ARC_POLICY_UTIL_H_
