// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_handler.h"

#include <optional>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/extension_developer_mode_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

using Availability = DeveloperToolsPolicyHandler::Availability;

// The result of checking a policy value.
enum class PolicyCheckResult {
  // The policy is not set.
  kNotSet,
  // The policy is set to an invalid value.
  kInvalid,
  // The policy is set to a valid value.
  kValid
};

// Checks the value of the DeveloperToolsDisabled policy. |errors| may be
// nullptr.
PolicyCheckResult CheckDeveloperToolsDisabled(
    const base::Value* developer_tools_disabled,
    policy::PolicyErrorMap* errors) {
  if (!developer_tools_disabled)
    return PolicyCheckResult::kNotSet;

  if (!developer_tools_disabled->is_bool()) {
    if (errors) {
      errors->AddError(key::kDeveloperToolsDisabled, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::BOOLEAN));
    }
    return PolicyCheckResult::kInvalid;
  }

  return PolicyCheckResult::kValid;
}

// Returns the target value of the |kDevToolsAvailability| pref derived only
// from the legacy DeveloperToolsDisabled policy. If this policy is not set or
// does not have a valid value, returns |nullopt|.
std::optional<Availability> GetValueFromDeveloperToolsDisabledPolicy(
    const PolicyMap& policies) {
  const base::Value* developer_tools_disabled = policies.GetValue(
      key::kDeveloperToolsDisabled, base::Value::Type::BOOLEAN);

  if (CheckDeveloperToolsDisabled(developer_tools_disabled,
                                  nullptr /*error*/) !=
      PolicyCheckResult::kValid) {
    return std::nullopt;
  }

  return developer_tools_disabled->GetBool() ? Availability::kDisallowed
                                             : Availability::kAllowed;
}

// Returns true if |value| is within the valid range of the
// DeveloperToolsAvailability enum policy.
bool IsValidDeveloperToolsAvailabilityValue(int value) {
  return value >= 0 && value <= static_cast<int>(Availability::kMaxValue);
}

// Checks the value of the DeveloperToolsAvailability policy. |errors| may be
// nullptr.
PolicyCheckResult CheckDeveloperToolsAvailability(
    const base::Value* developer_tools_availability,
    policy::PolicyErrorMap* errors) {
  if (!developer_tools_availability)
    return PolicyCheckResult::kNotSet;

  if (!developer_tools_availability->is_int()) {
    if (errors) {
      errors->AddError(key::kDeveloperToolsAvailability, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
    }
    return PolicyCheckResult::kInvalid;
  }

  const int value = developer_tools_availability->GetInt();
  if (!IsValidDeveloperToolsAvailabilityValue(value)) {
    if (errors) {
      errors->AddError(key::kDeveloperToolsAvailability,
                       IDS_POLICY_OUT_OF_RANGE_ERROR,
                       base::NumberToString(value));
    }
    return PolicyCheckResult::kInvalid;
  }
  return PolicyCheckResult::kValid;
}

// Returns the target value of the |kDevToolsAvailability| pref derived only
// from the DeveloperToolsAvailability policy. If this policy is not set or does
// not have a valid value, returns |nullopt|.
std::optional<Availability> GetValueFromDeveloperToolsAvailabilityPolicy(
    const PolicyMap& policies) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* developer_tools_availability =
      policies.GetValueUnsafe(key::kDeveloperToolsAvailability);

  if (CheckDeveloperToolsAvailability(developer_tools_availability,
                                      nullptr /*error*/) !=
      PolicyCheckResult::kValid) {
    return std::nullopt;
  }

  return static_cast<Availability>(developer_tools_availability->GetInt());
}

// Returns the target value of the |kDevToolsAvailability| pref, derived from
// both the DeveloperToolsDisabled policy and the
// DeveloperToolsAvailability policy. If both policies are set,
// DeveloperToolsAvailability wins.
std::optional<Availability> GetValueFromBothPolicies(
    const PolicyMap& policies) {
  const std::optional<Availability> developer_tools_availability =
      GetValueFromDeveloperToolsAvailabilityPolicy(policies);

  if (developer_tools_availability.has_value()) {
    // DeveloperToolsAvailability overrides DeveloperToolsDisabled policy.
    return developer_tools_availability;
  }

  return GetValueFromDeveloperToolsDisabledPolicy(policies);
}

// Returns the current policy-set developer tools availability according to
// the values in |pref_service|. If no policy mandating developer tools
// availability is set, the default will be
// |Availability::kDisallowedForForceInstalledExtensions|.
Availability GetDevToolsAvailability(const PrefService* pref_sevice) {
  int value = pref_sevice->GetInteger(prefs::kDevToolsAvailability);
  if (!IsValidDeveloperToolsAvailabilityValue(value)) {
    // This should never happen, because the |kDevToolsAvailability| pref is
    // only set by DeveloperToolsPolicyHandler which validates the value range.
    // If it is not set, it will have its default value which is also valid, see
    // |RegisterProfilePrefs|.
    NOTREACHED_IN_MIGRATION();
    return Availability::kAllowed;
  }

  return static_cast<Availability>(value);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Returns true if developer tools availability is set by an active policy in
// |pref_service|.
bool IsDevToolsAvailabilitySetByPolicy(const PrefService* pref_service) {
  return pref_service->IsManagedPreference(prefs::kDevToolsAvailability);
}

// Returns the most restrictive availability within [|availability_1|,
// |availability_2|].
Availability GetMostRestrictiveAvailability(Availability availability_1,
                                            Availability availability_2) {
  if (availability_1 == Availability::kDisallowed ||
      availability_2 == Availability::kDisallowed) {
    return Availability::kDisallowed;
  }
  if (availability_1 == Availability::kDisallowedForForceInstalledExtensions ||
      availability_2 == Availability::kDisallowedForForceInstalledExtensions) {
    return Availability::kDisallowedForForceInstalledExtensions;
  }
  return Availability::kAllowed;
}

#endif

}  // namespace

DeveloperToolsPolicyHandler::DeveloperToolsPolicyHandler() = default;

DeveloperToolsPolicyHandler::~DeveloperToolsPolicyHandler() = default;

bool DeveloperToolsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  // Deprecated boolean policy DeveloperToolsDisabled.
  const base::Value* developer_tools_disabled =
      policies.GetValueUnsafe(key::kDeveloperToolsDisabled);
  PolicyCheckResult developer_tools_disabled_result =
      CheckDeveloperToolsDisabled(developer_tools_disabled, errors);

  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  // Enumerated policy DeveloperToolsAvailability.
  const base::Value* developer_tools_availability =
      policies.GetValueUnsafe(key::kDeveloperToolsAvailability);
  PolicyCheckResult developer_tools_availability_result =
      CheckDeveloperToolsAvailability(developer_tools_availability, errors);

  if (developer_tools_disabled_result == PolicyCheckResult::kValid &&
      developer_tools_availability_result == PolicyCheckResult::kValid) {
    errors->AddError(key::kDeveloperToolsDisabled, IDS_POLICY_OVERRIDDEN,
                     key::kDeveloperToolsAvailability);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const std::optional<Availability> policy = GetValueFromBothPolicies(policies);

  if (policy.has_value() && *policy == Availability::kDisallowed &&
      extension_developer_mode_policy_handler_.IsValidPolicySet(policies)) {
    errors->AddError(key::kDeveloperToolsAvailability,
                     IDS_POLICY_DEVELOPER_TOOLS_EXTENSIONS_CONFLICT_MESSAGE,
                     key::kExtensionDeveloperModeSettings,
                     key::kDeveloperToolsAvailability,
                     /*error_path=*/{}, PolicyMap::MessageType::kInfo);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Always continue to ApplyPolicySettings which can handle invalid policy
  // values.
  return true;
}

void DeveloperToolsPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const std::optional<Availability> policy = GetValueFromBothPolicies(policies);

  if (policy.has_value()) {
    prefs->SetInteger(prefs::kDevToolsAvailability, static_cast<int>(*policy));

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // ExtensionDeveloperModePolicySettings takes precedence over this policy.
    // Thus, we only set the value of kExtensionsUIDeveloperMode if the former
    // is not set.
    if (*policy == Availability::kDisallowed &&
        !extension_developer_mode_policy_handler_.IsValidPolicySet(policies)) {
      prefs->SetValue(prefs::kExtensionsUIDeveloperMode, base::Value(false));
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }
}

// static
void DeveloperToolsPolicyHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // The default for this pref is |kDisallowedForForceInstalledExtensions|, both
  // for managed and for unmanaged users. This is fine for unmanaged users too,
  // because even if they have force-installed extensions (which could happen
  // e.g. through GPO for Chrome on Windows), developer tools should be disabled
  // for these by default.
  registry->RegisterIntegerPref(
      prefs::kDevToolsAvailability,
      static_cast<int>(Availability::kDisallowedForForceInstalledExtensions));
}

policy::DeveloperToolsPolicyHandler::Availability
DeveloperToolsPolicyHandler::GetEffectiveAvailability(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceDevToolsAvailable)) {
    return Availability::kAllowed;
  }
#endif

  Availability availability = GetDevToolsAvailability(profile->GetPrefs());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not create DevTools if it's disabled for primary profile.
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (primary_profile &&
      IsDevToolsAvailabilitySetByPolicy(primary_profile->GetPrefs())) {
    availability = GetMostRestrictiveAvailability(
        availability, GetDevToolsAvailability(primary_profile->GetPrefs()));
  }
#endif
  return availability;
}

}  // namespace policy
