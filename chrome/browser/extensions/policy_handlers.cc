// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_handlers.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/enterprise_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {
namespace {
// Returns true if extensions_ids contains a list of valid extension ids,
// divided by comma.
bool IsValidIdList(const std::string& extension_ids) {
  std::vector<std::string_view> ids = base::SplitStringPiece(
      extension_ids, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (ids.size() == 0) {
    return false;
  }
  for (const auto& id : ids) {
    if (!crx_file::id_util::IdIsValid(std::string(id))) {
      return false;
    }
  }
  return true;
}

// Returns true if URL is valid and uses one of the supported schemes.
bool IsValidUpdateUrl(const std::string& update_url) {
  GURL update_gurl(update_url);
  if (!update_gurl.is_valid()) {
    return false;
  }
  return update_gurl.SchemeIsHTTPOrHTTPS() || update_gurl.SchemeIsFile();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// If Ash Chrome is no longer functioning as a browser and the extension is not
// meant to run in Ash, do not load the extension.
void FilterOutExtensionsMeantToRunInLacros(base::Value::Dict& extensions) {
  auto iterator = extensions.begin();

  while (iterator != extensions.end()) {
    const std::string& extension_id = iterator->first;
    if (ExtensionRunsInOS(extension_id) || ExtensionAppRunsInOS(extension_id)) {
      // Keep extension meant to run in Ash
      iterator++;
    } else {
      // Remove extension meant to run in Lacros
      iterator = extensions.erase(iterator);
    }
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
// ExtensionListPolicyHandler implementation -----------------------------------

ExtensionListPolicyHandler::ExtensionListPolicyHandler(const char* policy_name,
                                                       const char* pref_path,
                                                       bool allow_wildcards)
    : policy::ListPolicyHandler(policy_name, base::Value::Type::STRING),
      pref_path_(pref_path),
      allow_wildcards_(allow_wildcards) {}

ExtensionListPolicyHandler::~ExtensionListPolicyHandler() = default;

bool ExtensionListPolicyHandler::CheckListEntry(const base::Value& value) {
  const std::string& str = value.GetString();
  if (allow_wildcards_ && str == "*") {
    return true;
  }

  // Make sure str is an extension id.
  return crx_file::id_util::IdIsValid(str);
}

void ExtensionListPolicyHandler::ApplyList(base::Value::List filtered_list,
                                           PrefValueMap* prefs) {
  prefs->SetValue(pref_path_, base::Value(std::move(filtered_list)));
}

// ExtensionInstallForceListPolicyHandler implementation -----------------------

ExtensionInstallForceListPolicyHandler::ExtensionInstallForceListPolicyHandler()
    : policy::TypeCheckingPolicyHandler(policy::key::kExtensionInstallForcelist,
                                        base::Value::Type::LIST) {}

bool ExtensionInstallForceListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
         ParseList(value, nullptr, errors);
}

void ExtensionInstallForceListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto dict = GetAshPolicyDict(policies);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto dict = GetLacrosPolicyDict(policies);
#else
  auto dict = GetPolicyDict(policies);
#endif

  if (dict.has_value()) {
    prefs->SetValue(pref_names::kInstallForceList,
                    base::Value(std::move(dict).value()));
  }
}

bool ExtensionInstallForceListPolicyHandler::ParseList(
    const base::Value* policy_value,
    base::Value::Dict* extension_dict,
    policy::PolicyErrorMap* errors) {
  if (!policy_value) {
    return true;
  }

  if (!policy_value->is_list()) {
    // This should have been caught in CheckPolicySettings.
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  int index = -1;
  for (const auto& entry : policy_value->GetList()) {
    ++index;
    if (!entry.is_string()) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING),
                         policy::PolicyErrorPath{index});
      }
      continue;
    }
    std::string entry_string = entry.GetString();

    // Each string item of the list should be of one of the following forms:
    // * <extension_id>
    // * <extension_id>;<update_url>
    // Note: The update URL might also contain semicolons.
    std::string extension_id;
    std::string update_url;
    size_t pos = entry_string.find(';');
    if (pos == std::string::npos) {
      extension_id = entry_string;
      update_url = extension_urls::kChromeWebstoreUpdateURL;
    } else {
      extension_id = entry_string.substr(0, pos);
      update_url = entry_string.substr(pos + 1);
    }

    if (!crx_file::id_util::IdIsValid(extension_id)) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_INVALID_EXTENSION_ID_ERROR,
                         policy::PolicyErrorPath{index});
      }
      continue;
    }

    // Check that url is valid and uses one of the supported schemes.
    if (!IsValidUpdateUrl(update_url)) {
      if (errors) {
        errors->AddError(policy_name(), IDS_POLICY_INVALID_UPDATE_URL_ERROR,
                         extension_id, policy::PolicyErrorPath{index});
      }
      continue;
    }

    if (extension_dict) {
      ExternalPolicyLoader::AddExtension(*extension_dict, extension_id,
                                         update_url);
    }
  }

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::optional<base::Value::Dict>
ExtensionInstallForceListPolicyHandler::GetAshPolicyDict(
    const policy::PolicyMap& policies) {
  std::optional<base::Value::Dict> dict = GetPolicyDict(policies);
  if (dict.has_value() && crosapi::browser_util::IsLacrosEnabled()) {
    FilterOutExtensionsMeantToRunInLacros(dict.value());
  }
  return dict;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
std::optional<base::Value::Dict>
ExtensionInstallForceListPolicyHandler::GetLacrosPolicyDict(
    const policy::PolicyMap& policies) {
  // TODO(b/335121961): Currently always returns all extensions on Lacros,
  // even the ones that run in Ash. This is consistent with the pre-existing
  // behavior but it should be investigated if this is the correct behavior.
  return GetPolicyDict(policies);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::optional<base::Value::Dict>
ExtensionInstallForceListPolicyHandler::GetPolicyDict(
    const policy::PolicyMap& policies) {
  const base::Value* value = nullptr;
  base::Value::Dict dict;
  if (CheckAndGetValue(policies, nullptr, &value) && value &&
      ParseList(value, &dict, nullptr)) {
    return dict;
  }
  return std::nullopt;
}

// ExtensionInstallBlockListPolicyHandler implementation ----------------------

ExtensionInstallBlockListPolicyHandler::ExtensionInstallBlockListPolicyHandler()
    : list_handler_(policy::key::kExtensionInstallBlocklist,
                    pref_names::kInstallDenyList,
                    /*allow_wildcards*/ true) {}

ExtensionInstallBlockListPolicyHandler::
    ~ExtensionInstallBlockListPolicyHandler() = default;

bool ExtensionInstallBlockListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  return list_handler_.CheckPolicySettings(policies, errors);
}

void ExtensionInstallBlockListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (crosapi::browser_util::IsLacrosEnabled()) {
    // When Lacros is enabled extensions are managed by Lacros, not Ash
    // (except for some very specific extensions, see `ExtensionsAppRunsInOS`
    // and `ExtensionsRunsInOS`), so keep the block list empty on the Ash side.
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  list_handler_.ApplyPolicySettings(policies, prefs);
}

// ExtensionURLPatternListPolicyHandler implementation -------------------------

ExtensionURLPatternListPolicyHandler::ExtensionURLPatternListPolicyHandler(
    const char* policy_name,
    const char* pref_path)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path) {}

ExtensionURLPatternListPolicyHandler::~ExtensionURLPatternListPolicyHandler() =
    default;

bool ExtensionURLPatternListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value)) {
    return false;
  }

  if (!value) {
    return true;
  }

  if (!value->is_list()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  // Check that the list contains valid URLPattern strings only.
  int index = 0;
  for (const auto& entry : value->GetList()) {
    if (!entry.is_string()) {
      errors->AddError(policy_name(), IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::STRING),
                       policy::PolicyErrorPath{index});
      return false;
    }
    std::string url_pattern_string = entry.GetString();

    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(url_pattern_string) !=
        URLPattern::ParseResult::kSuccess) {
      errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR,
                       policy::PolicyErrorPath{index});
      return false;
    }
    ++index;
  }

  return true;
}

void ExtensionURLPatternListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_) {
    return;
  }
  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* value = policies.GetValueUnsafe(policy_name());
  if (value) {
    prefs->SetValue(pref_path_, value->Clone());
  }
}

// ExtensionSettingsPolicyHandler implementation  ------------------------------

ExtensionSettingsPolicyHandler::ExtensionSettingsPolicyHandler(
    const policy::Schema& chrome_schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kExtensionSettings,
          chrome_schema.GetKnownProperty(policy::key::kExtensionSettings),
          policy::SCHEMA_ALLOW_UNKNOWN) {}

ExtensionSettingsPolicyHandler::~ExtensionSettingsPolicyHandler() = default;

void ExtensionSettingsPolicyHandler::SanitizePolicySettings(
    base::Value* policy_value,
    policy::PolicyErrorMap* errors) {
  DCHECK(policy_value);

  // |policy_value| is expected to conform to the defined schema. But it's
  // not strictly valid since there are additional restrictions.
  DCHECK(policy_value->is_dict());

  // Dictionary entries with any invalid setting get removed at the end. We
  // can't mutate the dict while iterating, so store them here.
  std::unordered_set<std::string> invalid_keys;

  // Check each entry, populating |invalid_keys| and |errors|.
  for (const auto [extension_ids, policy] : policy_value->GetDict()) {
    DCHECK(extension_ids == schema_constants::kWildcard ||
           IsValidIdList(extension_ids));
    DCHECK(policy.is_dict());

    // Extracts sub dictionary.
    const base::Value::Dict& sub_dict = policy.GetDict();

    const std::string* installation_mode =
        sub_dict.FindString(schema_constants::kInstallationMode);
    if (installation_mode) {
      if (*installation_mode == schema_constants::kForceInstalled ||
          *installation_mode == schema_constants::kNormalInstalled) {
        DCHECK(extension_ids != schema_constants::kWildcard);
        // Verifies that 'update_url' is specified for 'force_installed' and
        // 'normal_installed' mode.
        const std::string* update_url =
            sub_dict.FindString(schema_constants::kUpdateUrl);
        if (!update_url || update_url->empty()) {
          if (errors) {
            errors->AddError(policy_name(), IDS_POLICY_NOT_SPECIFIED_ERROR,
                             policy::PolicyErrorPath{
                                 extension_ids, schema_constants::kUpdateUrl});
          }
          invalid_keys.insert(extension_ids);
          continue;
        }

        // Check that url is valid and uses one of the supported schemes.
        if (!IsValidUpdateUrl(*update_url)) {
          if (errors) {
            errors->AddError(policy_name(), IDS_POLICY_INVALID_UPDATE_URL_ERROR,
                             extension_ids);
          }
          invalid_keys.insert(extension_ids);
          continue;
        }
      }
    }
    // Host keys that don't support user defined paths.
    const char* host_keys[] = {schema_constants::kPolicyBlockedHosts,
                               schema_constants::kPolicyAllowedHosts};
    const int extension_scheme_mask =
        URLPattern::GetValidSchemeMaskForExtensions();
    for (const char* key : host_keys) {
      const base::Value::List* unparsed_urls = sub_dict.FindList(key);
      if (unparsed_urls != nullptr) {
        for (const auto& url_value : *unparsed_urls) {
          const std::string& unparsed_url = url_value.GetString();
          URLPattern pattern(extension_scheme_mask);
          URLPattern::ParseResult parse_result = pattern.Parse(unparsed_url);
          // These keys don't support paths due to how we track the initiator
          // of a webRequest and cookie security policy. We expect a valid
          // pattern to return a PARSE_ERROR_EMPTY_PATH.
          if (parse_result == URLPattern::ParseResult::kEmptyPath) {
            // Add a wildcard path to the URL as it should match any path.
            parse_result = pattern.Parse(unparsed_url + "/*");
          } else if (parse_result == URLPattern::ParseResult::kSuccess) {
            // The user supplied a path, notify them that this is not supported.
            if (!pattern.match_all_urls()) {
              if (errors) {
                errors->AddError(
                    policy_name(), IDS_POLICY_URL_PATH_SPECIFIED_ERROR,
                    unparsed_url, policy::PolicyErrorPath{extension_ids, key});
              }
              invalid_keys.insert(extension_ids);
              break;
            }
          }
          if (parse_result != URLPattern::ParseResult::kSuccess) {
            if (errors) {
              errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR,
                               policy::PolicyErrorPath{extension_ids, key});
            }
            invalid_keys.insert(extension_ids);
            break;
          }
        }
      }
    }

    const base::Value::List* runtime_blocked_hosts =
        sub_dict.FindList(schema_constants::kPolicyBlockedHosts);
    if (runtime_blocked_hosts != nullptr &&
        runtime_blocked_hosts->size() >
            schema_constants::kMaxItemsURLPatternSet) {
      if (errors) {
        policy::PolicyErrorPath error_path = {
            extension_ids, schema_constants::kPolicyBlockedHosts};
        errors->AddError(
            policy_name(), IDS_POLICY_EXTENSION_SETTINGS_ORIGIN_LIMIT_WARNING,
            base::NumberToString(schema_constants::kMaxItemsURLPatternSet),
            error_path);
      }
    }

    const base::Value::List* runtime_allowed_hosts =
        sub_dict.FindList(schema_constants::kPolicyAllowedHosts);
    if (runtime_allowed_hosts != nullptr &&
        runtime_allowed_hosts->size() >
            schema_constants::kMaxItemsURLPatternSet) {
      if (errors) {
        policy::PolicyErrorPath error_path = {
            extension_ids, schema_constants::kPolicyAllowedHosts};
        errors->AddError(
            policy_name(), IDS_POLICY_EXTENSION_SETTINGS_ORIGIN_LIMIT_WARNING,
            base::NumberToString(schema_constants::kMaxItemsURLPatternSet),
            error_path);
      }
    }
  }

  // Remove |invalid_keys| from the dictionary.
  for (const std::string& key : invalid_keys) {
    policy_value->GetDict().Remove(key);
  }
}

bool ExtensionSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, errors, &policy_value)) {
    return false;
  }
  if (!policy_value) {
    return true;
  }

  SanitizePolicySettings(policy_value.get(), errors);
  return true;
}

void ExtensionSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, nullptr, &policy_value) || !policy_value) {
    return;
  }
  SanitizePolicySettings(policy_value.get(), nullptr);
  prefs->SetValue(pref_names::kExtensionManagement,
                  base::Value::FromUniquePtrValue(std::move(policy_value)));
}

}  // namespace extensions
