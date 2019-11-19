// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_handlers.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/enterprise_util.h"
#endif

namespace extensions {
namespace {
// Returns true if extensions_ids contains a list of valid extension ids,
// divided by comma.
bool IsValidIdList(const std::string& extension_ids) {
  std::vector<base::StringPiece> ids = base::SplitStringPiece(
      extension_ids, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (ids.size() == 0)
    return false;
  for (const auto& id : ids) {
    if (!crx_file::id_util::IdIsValid(id.as_string()))
      return false;
  }
  return true;
}
}  // namespace
// ExtensionListPolicyHandler implementation -----------------------------------

ExtensionListPolicyHandler::ExtensionListPolicyHandler(const char* policy_name,
                                                       const char* pref_path,
                                                       bool allow_wildcards)
    : policy::ListPolicyHandler(policy_name, base::Value::Type::STRING),
      pref_path_(pref_path),
      allow_wildcards_(allow_wildcards) {}

ExtensionListPolicyHandler::~ExtensionListPolicyHandler() {}

bool ExtensionListPolicyHandler::CheckListEntry(const base::Value& value) {
  const std::string& str = value.GetString();
  if (allow_wildcards_ && str == "*")
    return true;

  // Make sure str is an extension id.
  return crx_file::id_util::IdIsValid(str);
}

void ExtensionListPolicyHandler::ApplyList(
    std::unique_ptr<base::ListValue> filtered_list,
    PrefValueMap* prefs) {
  DCHECK(filtered_list);
  prefs->SetValue(pref_path_,
                  base::Value::FromUniquePtrValue(std::move(filtered_list)));
}

// ExtensionInstallListPolicyHandler implementation ----------------------------

ExtensionInstallListPolicyHandler::ExtensionInstallListPolicyHandler(
    const char* policy_name,
    const char* pref_name)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_name_(pref_name) {}

bool ExtensionInstallListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value;
  return CheckAndGetValue(policies, errors, &value) &&
         ParseList(value, nullptr, errors);
}

void ExtensionInstallListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = nullptr;
  base::DictionaryValue dict;
  if (CheckAndGetValue(policies, nullptr, &value) && value &&
      ParseList(value, &dict, nullptr)) {
    prefs->SetValue(pref_name_, std::move(dict));
  }
}

bool ExtensionInstallListPolicyHandler::ParseList(
    const base::Value* policy_value,
    base::DictionaryValue* extension_dict,
    policy::PolicyErrorMap* errors) {
  if (!policy_value)
    return true;

  const base::ListValue* policy_list_value = nullptr;
  if (!policy_value->GetAsList(&policy_list_value)) {
    // This should have been caught in CheckPolicySettings.
    NOTREACHED();
    return false;
  }

  for (auto entry(policy_list_value->begin());
       entry != policy_list_value->end(); ++entry) {
    std::string entry_string;
    if (!entry->GetAsString(&entry_string)) {
      if (errors) {
        errors->AddError(policy_name(), entry - policy_list_value->begin(),
                         IDS_POLICY_TYPE_ERROR,
                         base::Value::GetTypeName(base::Value::Type::STRING));
      }
      continue;
    }

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

    if (!crx_file::id_util::IdIsValid(extension_id) ||
        !GURL(update_url).is_valid()) {
      if (errors) {
        errors->AddError(policy_name(),
                         entry - policy_list_value->begin(),
                         IDS_POLICY_VALUE_FORMAT_ERROR);
      }
      continue;
    }

    if (extension_dict) {
      ExternalPolicyLoader::AddExtension(extension_dict, extension_id,
                                         update_url);
    }
  }

  return true;
}

// ExtensionInstallForcelistPolicyHandler implementation -----------------------

ExtensionInstallForcelistPolicyHandler::ExtensionInstallForcelistPolicyHandler()
    : ExtensionInstallListPolicyHandler(policy::key::kExtensionInstallForcelist,
                                        pref_names::kInstallForceList) {}

// ExtensionInstallLoginScreenExtensionsPolicyHandler implementation -----------

ExtensionInstallLoginScreenExtensionsPolicyHandler::
    ExtensionInstallLoginScreenExtensionsPolicyHandler()
    : ExtensionInstallListPolicyHandler(
          policy::key::kDeviceLoginScreenExtensions,
          pref_names::kLoginScreenExtensions) {}

// ExtensionURLPatternListPolicyHandler implementation -------------------------

ExtensionURLPatternListPolicyHandler::ExtensionURLPatternListPolicyHandler(
    const char* policy_name,
    const char* pref_path)
    : policy::TypeCheckingPolicyHandler(policy_name, base::Value::Type::LIST),
      pref_path_(pref_path) {}

ExtensionURLPatternListPolicyHandler::~ExtensionURLPatternListPolicyHandler() {}

bool ExtensionURLPatternListPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = NULL;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (!value)
    return true;

  const base::ListValue* list_value = NULL;
  if (!value->GetAsList(&list_value)) {
    NOTREACHED();
    return false;
  }

  // Check that the list contains valid URLPattern strings only.
  for (auto entry(list_value->begin()); entry != list_value->end(); ++entry) {
    std::string url_pattern_string;
    if (!entry->GetAsString(&url_pattern_string)) {
      errors->AddError(policy_name(), entry - list_value->begin(),
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::STRING));
      return false;
    }

    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(url_pattern_string) !=
        URLPattern::ParseResult::kSuccess) {
      errors->AddError(policy_name(),
                       entry - list_value->begin(),
                       IDS_POLICY_VALUE_FORMAT_ERROR);
      return false;
    }
  }

  return true;
}

void ExtensionURLPatternListPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  if (!pref_path_)
    return;
  const base::Value* value = policies.GetValue(policy_name());
  if (value)
    prefs->SetValue(pref_path_, value->Clone());
}

// ExtensionSettingsPolicyHandler implementation  ------------------------------

ExtensionSettingsPolicyHandler::ExtensionSettingsPolicyHandler(
    const policy::Schema& chrome_schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kExtensionSettings,
          chrome_schema.GetKnownProperty(policy::key::kExtensionSettings),
          policy::SCHEMA_ALLOW_UNKNOWN) {
}

ExtensionSettingsPolicyHandler::~ExtensionSettingsPolicyHandler() {
}

bool ExtensionSettingsPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, errors, &policy_value))
    return false;
  if (!policy_value)
    return true;

  // |policy_value| is expected to conform to the defined schema. But it's
  // not strictly valid since there are additional restrictions.
  const base::DictionaryValue* dict_value = NULL;
  DCHECK(policy_value->is_dict());
  policy_value->GetAsDictionary(&dict_value);

  for (base::DictionaryValue::Iterator it(*dict_value); !it.IsAtEnd();
       it.Advance()) {
    DCHECK(it.key() == schema_constants::kWildcard || IsValidIdList(it.key()));
    DCHECK(it.value().is_dict());

    // Extracts sub dictionary.
    const base::DictionaryValue* sub_dict = NULL;
    it.value().GetAsDictionary(&sub_dict);

    std::string installation_mode;
    if (sub_dict->GetString(schema_constants::kInstallationMode,
                            &installation_mode)) {
      if (installation_mode == schema_constants::kForceInstalled ||
          installation_mode == schema_constants::kNormalInstalled) {
        DCHECK(it.key() != schema_constants::kWildcard);
        // Verifies that 'update_url' is specified for 'force_installed' and
        // 'normal_installed' mode.
        std::string update_url;
        if (!sub_dict->GetString(schema_constants::kUpdateUrl, &update_url) ||
            update_url.empty()) {
          errors->AddError(policy_name(),
                           it.key() + "." + schema_constants::kUpdateUrl,
                           IDS_POLICY_NOT_SPECIFIED_ERROR);
          return false;
        }
        if (GURL(update_url).is_valid()) {
// Unless enterprise managed only extensions from the Chrome Webstore
// can be force installed.
#if defined(OS_WIN)
          // We can't use IsWebstoreUpdateUrl() here since the ExtensionClient
          // isn't set this early during startup.
          if (!base::IsMachineExternallyManaged() &&
              !base::LowerCaseEqualsASCII(
                  update_url, extension_urls::kChromeWebstoreUpdateURL)) {
            errors->AddError(policy_name(), it.key(),
                             IDS_POLICY_OFF_CWS_URL_ERROR,
                             extension_urls::kChromeWebstoreUpdateURL);
            return false;
          }
#endif
        } else {
          // Warns about an invalid update URL.
          errors->AddError(
              policy_name(), IDS_POLICY_INVALID_UPDATE_URL_ERROR, it.key());
          return false;
        }
      }
    }
    // Host keys that don't support user defined paths.
    const char* host_keys[] = {schema_constants::kPolicyBlockedHosts,
                               schema_constants::kPolicyAllowedHosts};
    const int extension_scheme_mask =
        URLPattern::GetValidSchemeMaskForExtensions();
    for (const char* key : host_keys) {
      const base::ListValue* unparsed_urls;
      if (sub_dict->GetList(key, &unparsed_urls)) {
        for (size_t i = 0; i < unparsed_urls->GetSize(); ++i) {
          std::string unparsed_url;
          unparsed_urls->GetString(i, &unparsed_url);
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
              errors->AddError(
                  policy_name(), it.key(),
                  "The URL pattern '" + unparsed_url + "' for attribute " +
                      key + " has a path specified. Paths are not " +
                      "supported, please remove the path and try again. " +
                      "e.g. *://example.com/ => *://example.com");
              return false;
            }
          }
          if (parse_result != URLPattern::ParseResult::kSuccess) {
            errors->AddError(policy_name(), it.key(),
                             "Invalid URL pattern '" + unparsed_url +
                                 "' for attribute " + key);
            return false;
          }
        }
      }
    }

    const base::ListValue* runtime_blocked_hosts = nullptr;
    if (sub_dict->GetList(schema_constants::kPolicyBlockedHosts,
                          &runtime_blocked_hosts) &&
        runtime_blocked_hosts->GetList().size() >
            schema_constants::kMaxItemsURLPatternSet) {
      errors->AddError(
          policy_name(), it.key() + "." + schema_constants::kPolicyBlockedHosts,
          IDS_POLICY_EXTENSION_SETTINGS_ORIGIN_LIMIT_WARNING,
          base::NumberToString(schema_constants::kMaxItemsURLPatternSet));
    }

    const base::ListValue* runtime_allowed_hosts = nullptr;
    if (sub_dict->GetList(schema_constants::kPolicyAllowedHosts,
                          &runtime_allowed_hosts) &&
        runtime_allowed_hosts->GetList().size() >
            schema_constants::kMaxItemsURLPatternSet) {
      errors->AddError(
          policy_name(), it.key() + "." + schema_constants::kPolicyAllowedHosts,
          IDS_POLICY_EXTENSION_SETTINGS_ORIGIN_LIMIT_WARNING,
          base::NumberToString(schema_constants::kMaxItemsURLPatternSet));
    }
  }

  return true;
}

void ExtensionSettingsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, NULL, &policy_value) || !policy_value)
    return;
  prefs->SetValue(pref_names::kExtensionManagement,
                  base::Value::FromUniquePtrValue(std::move(policy_value)));
}

}  // namespace extensions
