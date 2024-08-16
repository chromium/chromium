// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chrome/browser/ash/policy/core/device_attributes_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "third_party/re2/src/re2/re2.h"

namespace arc {

namespace {

// Part before "@" of the given |email| address.
// "some_email@domain.com" => "some_email"
//
// Returns empty string if |email| does not contain an "@".
std::string EmailName(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos) {
    return "";
  }
  return email.substr(0, at_sign_pos);
}

// Part after "@" of an email address.
// "some_email@domain.com" => "domain.com"
//
// Returns empty string if |email| does not contain an "@".
std::string EmailDomain(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos) {
    return "";
  }
  return email.substr(at_sign_pos + 1);
}

std::string SignedInUserEmail(const Profile* profile) {
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  CoreAccountInfo info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return info.email;
}

std::string DeviceDirectoryId(policy::DeviceAttributes* device_attributes) {
  return device_attributes->GetDirectoryApiID();
}

std::string DeviceAssetId(policy::DeviceAttributes* device_attributes) {
  return device_attributes->GetDeviceAssetID();
}

std::string DeviceAnnotatedLocation(
    policy::DeviceAttributes* device_attributes) {
  return device_attributes->GetDeviceAnnotatedLocation();
}

std::string DeviceSerialNumber() {
  return std::string(
      ash::system::StatisticsProvider::GetInstance()->GetMachineID().value_or(
          ""));
}

// Map associating known variables to functions that return the corresponding
// values. For example, "USER_EMAIL" is associated to |SignedInUserEmail|.
typedef base::flat_map<std::string, base::RepeatingCallback<std::string()>>
    VariableResolver;

bool IsAffiliatedUser(const Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
}

// Build a |VariableResolver| from all known variables.
const VariableResolver BuildVariableResolver(
    const Profile* profile,
    policy::DeviceAttributes* attributes) {
  // Use |empty_string_getter| for device attributes if user is not affiliated.
  const bool is_affiliated = IsAffiliatedUser(profile);
  const auto empty_string_getter =
      base::BindRepeating([]() { return std::string(); });

  return VariableResolver{
      {kUserEmail,
       base::BindRepeating(
           [](const Profile* profile) { return SignedInUserEmail(profile); },
           profile)},
      {kUserEmailName, base::BindRepeating(
                           [](const Profile* profile) {
                             return EmailName(SignedInUserEmail(profile));
                           },
                           profile)},
      {kUserEmailDomain, base::BindRepeating(
                             [](const Profile* profile) {
                               return EmailDomain(SignedInUserEmail(profile));
                             },
                             profile)},
      {kDeviceDirectoryId, is_affiliated
                               ? base::BindRepeating(
                                     [](policy::DeviceAttributes* attributes) {
                                       return DeviceDirectoryId(attributes);
                                     },
                                     attributes)
                               : empty_string_getter},
      {kDeviceSerialNumber, is_affiliated
                                ? base::BindRepeating(&DeviceSerialNumber)
                                : empty_string_getter},
      {kDeviceAssetId, is_affiliated
                           ? base::BindRepeating(
                                 [](policy::DeviceAttributes* attributes) {
                                   return DeviceAssetId(attributes);
                                 },
                                 attributes)
                           : empty_string_getter},
      {kDeviceAnnotatedLocation,
       is_affiliated ? base::BindRepeating(
                           [](policy::DeviceAttributes* attributes) {
                             return DeviceAnnotatedLocation(attributes);
                           },
                           attributes)
                     : empty_string_getter},
  };
}

// Return the value associated to the first item in |variables| that is not
// empty.
std::string ResolveVariableChain(const VariableResolver& resolver,
                                 std::vector<std::string_view> variables) {
  for (const auto& variable : variables) {
    // Variables should always be valid and have a mapping in |resolver|.
    DCHECK(resolver.find(variable) != resolver.end());

    // Resolve the given variable and return if it has a value.
    std::string result = resolver.at(variable).Run();
    if (!result.empty())
      return result;
  }
  return "";
}

std::vector<std::string_view> SplitByColon(std::string_view input) {
  return base::SplitStringPiece(input, ":", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

// Return a new string where all captures of |regex| in |search_input| have been
// replaced with the output of |replacement_getter.Run(capture)|.
std::string SearchAndReplace(
    const re2::RE2& regex,
    base::RepeatingCallback<std::string(std::string_view)> replacement_getter,
    std::string_view search_input) {
  std::vector<std::string> output;
  std::string_view capture;

  // Loop as long as |regex| matches |search_input|.
  while (re2::RE2::PartialMatch(search_input, regex, &capture)) {
    DCHECK(capture.data() != nullptr);
    // Output the prefix skipped by PartialMatch until |capture| is found.
    DCHECK(capture.begin() >= search_input.begin());
    size_t prefix_size = capture.begin() - search_input.begin();
    output.emplace_back(search_input.data(), prefix_size);
    // Output the replacement for |capture|.
    output.emplace_back(replacement_getter.Run(capture));

    // Update |search_input| to the suffix after |capture|.
    DCHECK(search_input.length() >= prefix_size + capture.length());
    size_t remaining_size =
        search_input.length() - (prefix_size + capture.length());
    search_input =
        std::string_view(capture.data() + capture.size(), remaining_size);
  }
  // Output the remaining |search_input|.
  output.emplace_back(search_input);
  return base::JoinString(output, /*separator=*/"");
}

// Returns a regular expression that matches any one variable in |resolver|.
std::string ResolverKeyMatcher(const VariableResolver& resolver) {
  std::vector<std::string_view> keys;
  for (const auto& item : resolver) {
    keys.emplace_back(item.first);
  }
  return base::JoinString(keys, /*separator=*/"|");
}

// Replace all variable chains in |configuration| in-place using the provided
// |resolver| rules.
//
// A variable chain is separated by ":", e.g. "${DEVICE_ASSET_ID:USER_EMAIL}".
//
// Chains resolve to the first value that is non-empty. In the example above if
// the asset ID is empty, the chain resolves to the email of the current user.
void ReplaceVariables(const VariableResolver& resolver,
                      std::string& configuration) {
  // |variable_matcher| matches any of the supported variables in |resolver|.
  const std::string variable_matcher = ResolverKeyMatcher(resolver);

  // |variable_capture| will match and capture a variable template including a
  // variable chain. This regex does not match templates with invalid variables.
  const std::string variable_capture =
      base::StringPrintf("(\\$\\{(?:%s)(?::(?:%s))*\\})",
                         variable_matcher.c_str(), variable_matcher.c_str());
  const re2::RE2 regex(variable_capture);
  DCHECK(regex.ok()) << "Error compiling regex: " << regex.error();

  // Callback to compute values of variable chains matched with |regex|.
  auto chain_resolver = base::BindRepeating(
      [](const VariableResolver& resolver, std::string_view variable) {
        // Remove the "${" prefix and the "}" suffix from |variable|.
        DCHECK(variable.starts_with("${") && variable.ends_with("}"));
        const std::string_view chain = variable.substr(2, variable.size() - 3);
        const std::vector<std::string_view> variables = SplitByColon(chain);

        const std::string chain_value =
            ResolveVariableChain(resolver, variables);

        return chain_value;
      },
      resolver);

  std::string replaced_configuration =
      SearchAndReplace(regex, std::move(chain_resolver), configuration);
  configuration = std::move(replaced_configuration);
}

void ReplaceVariables(const VariableResolver& resolver,
                      base::Value& configuration);

void ReplaceVariables(const VariableResolver& resolver,
                      base::Value::Dict& configuration) {
  for (auto entry : configuration) {
    ReplaceVariables(resolver, entry.second);
  }
}

void ReplaceVariables(const VariableResolver& resolver,
                      base::Value::List& configuration) {
  for (auto& entry : configuration) {
    ReplaceVariables(resolver, entry);
  }
}

void ReplaceVariables(const VariableResolver& resolver,
                      base::Value& configuration) {
  if (configuration.is_dict()) {
    ReplaceVariables(resolver, configuration.GetDict());
  } else if (configuration.is_string()) {
    ReplaceVariables(resolver, configuration.GetString());
  } else if (configuration.is_list()) {
    ReplaceVariables(resolver, configuration.GetList());
  }
}

}  // namespace

const char kUserEmail[] = "USER_EMAIL";
const char kUserEmailName[] = "USER_EMAIL_NAME";
const char kUserEmailDomain[] = "USER_EMAIL_DOMAIN";
const char kDeviceDirectoryId[] = "DEVICE_DIRECTORY_ID";
const char kDeviceSerialNumber[] = "DEVICE_SERIAL_NUMBER";
const char kDeviceAssetId[] = "DEVICE_ASSET_ID";
const char kDeviceAnnotatedLocation[] = "DEVICE_ANNOTATED_LOCATION";

void RecursivelyReplaceManagedConfigurationVariables(
    const Profile* profile,
    base::Value::Dict& managedConfiguration) {
  policy::DeviceAttributesImpl device_attributes;
  RecursivelyReplaceManagedConfigurationVariables(profile, &device_attributes,
                                                  managedConfiguration);
}

void RecursivelyReplaceManagedConfigurationVariables(
    const Profile* profile,
    policy::DeviceAttributes* device_attributes,
    base::Value::Dict& managedConfiguration) {
  const VariableResolver resolver =
      BuildVariableResolver(profile, device_attributes);
  ReplaceVariables(resolver, managedConfiguration);
}

}  // namespace arc
