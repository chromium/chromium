// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/policy/arc_policy_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_value_map.h"

namespace arc {
namespace policy_util {

namespace {

// Constants used to parse ARC++ JSON policy.
constexpr char kApplicationsKey[] = "applications";
constexpr char kInstallTypeKey[] = "installType";
constexpr char kPackageNameKey[] = "packageName";
constexpr char kInstallTypeRequired[] = "REQUIRED";
constexpr char kInstallTypeForceInstalled[] = "FORCE_INSTALLED";

}  // namespace

bool IsAccountManaged(const Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

bool IsArcDisabledForEnterprise() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnterpriseDisableArc);
}

base::Optional<EcryptfsMigrationAction> DecodeMigrationActionFromPolicy(
    const enterprise_management::CloudPolicySettings& policy) {
  if (!policy.has_ecryptfsmigrationstrategy())
    return base::nullopt;
  const enterprise_management::IntegerPolicyProto& policy_proto =
      policy.ecryptfsmigrationstrategy();
  if (!policy_proto.has_value())
    return base::nullopt;

  // Use |policy::EcryptfsMigrationStrategyPolicyHandler| to translate from
  // policy to enum, as some obsolete policy settings need to be aliased to
  // other enum values.
  policy::PolicyMap policy_map;
  policy_map.Set(
      policy::key::kEcryptfsMigrationStrategy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(static_cast<int>(policy_proto.value())),
      nullptr);
  PrefValueMap prefs;
  policy::EcryptfsMigrationStrategyPolicyHandler handler;
  handler.ApplyPolicySettings(policy_map, &prefs);
  int strategy = 0;
  if (!prefs.GetInteger(arc::prefs::kEcryptfsMigrationStrategy, &strategy))
    return base::nullopt;
  return static_cast<EcryptfsMigrationAction>(strategy);
}

std::set<std::string> GetRequestedPackagesFromArcPolicy(
    const std::string& arc_policy) {
  std::unique_ptr<base::Value> dict =
      base::JSONReader::ReadDeprecated(arc_policy);
  if (!dict || !dict->is_dict())
    return {};

  const base::Value* const packages =
      dict->FindKeyOfType(kApplicationsKey, base::Value::Type::LIST);
  if (!packages)
    return {};

  std::set<std::string> requested_packages;
  for (const auto& package : packages->GetList()) {
    if (!package.is_dict())
      continue;
    const base::Value* const install_type =
        package.FindKeyOfType(kInstallTypeKey, base::Value::Type::STRING);
    if (!install_type)
      continue;
    if (install_type->GetString() != kInstallTypeRequired &&
        install_type->GetString() != kInstallTypeForceInstalled) {
      continue;
    }
    const base::Value* const package_name =
        package.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);
    if (!package_name || package_name->GetString().empty())
      continue;
    requested_packages.insert(package_name->GetString());
  }
  return requested_packages;
}

}  // namespace policy_util
}  // namespace arc
