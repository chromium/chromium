// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace bruschetta {

namespace {

#if defined(ARCH_CPU_X86_64)
const char kPolicyImageKeyArchSpecific[] = "installer_image_x86_64";
const char kPolicyPflashKeyArchSpecific[] = "uefi_pflash_x86_64";

constexpr policy::PolicyMap::MessageType kUninstallableErrorLevel =
    policy::PolicyMap::MessageType::kError;
#else
const char kPolicyImageKeyArchSpecific[] = "";
const char kPolicyPflashKeyArchSpecific[] = "";

constexpr policy::PolicyMap::MessageType kUninstallableErrorLevel =
    policy::PolicyMap::MessageType::kInfo;
#endif

prefs::PolicyEnabledState EnabledStrToEnum(const std::string& str) {
  if (str == "RUN_ALLOWED") {
    return prefs::PolicyEnabledState::RUN_ALLOWED;
  }
  if (str == "INSTALL_ALLOWED") {
    return prefs::PolicyEnabledState::INSTALL_ALLOWED;
  }

  return prefs::PolicyEnabledState::BLOCKED;
}

prefs::PolicyUpdateAction UpdateActionStrToEnum(const std::string& str) {
  if (str == "NONE") {
    return prefs::PolicyUpdateAction::NONE;
  }
  if (str == "FORCE_SHUTDOWN_ALWAYS") {
    return prefs::PolicyUpdateAction::FORCE_SHUTDOWN_ALWAYS;
  }

  return prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED;
}

}  // namespace

BruschettaPolicyHandler::BruschettaPolicyHandler(policy::Schema schema)
    : policy::SimpleSchemaValidatingPolicyHandler(
          policy::key::kBruschettaVMConfiguration,
          prefs::kBruschettaVMConfiguration,
          schema,
          policy::SchemaOnErrorStrategy::SCHEMA_ALLOW_UNKNOWN,
          policy::SimpleSchemaValidatingPolicyHandler::RECOMMENDED_PROHIBITED,
          policy::SimpleSchemaValidatingPolicyHandler::MANDATORY_ALLOWED) {}

BruschettaPolicyHandler::~BruschettaPolicyHandler() = default;

bool BruschettaPolicyHandler::CheckDownloadableObject(
    policy::PolicyErrorMap* errors,
    const std::string& id,
    const std::string& key,
    const base::Value::Dict& dict) {
  bool retval = true;

  const auto* url_str = dict.FindString(prefs::kPolicyURLKey);
  if (!GURL(*url_str).is_valid()) {
    errors->AddError(policy_name(), IDS_POLICY_INVALID_URL_ERROR,
                     policy::PolicyErrorPath{id, key, prefs::kPolicyURLKey});
    retval = false;
  }

  std::vector<uint8_t> hash_bytes;
  const auto* hash_str = dict.FindString(prefs::kPolicyHashKey);
  if (!base::HexStringToBytes(*hash_str, &hash_bytes) ||
      hash_bytes.size() != crypto::kSHA256Length) {
    errors->AddError(policy_name(), IDS_POLICY_INVALID_HASH_ERROR,
                     policy::PolicyErrorPath{id, key, prefs::kPolicyHashKey});
    retval = false;
  }

  return retval;
}

bool BruschettaPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  // Delegate to our super-class, which checks the JSON schema.
  if (!policy::SimpleSchemaValidatingPolicyHandler::CheckPolicySettings(
          policies, errors)) {
    return false;
  }

  // Aside from outright schema violations we never reject a policy, we only
  // downgrade configs from installable to runnable. This minimizes the damage
  // caused by misconfigurations.

  downgraded_by_error_.clear();
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);

  if (!value) {
    return true;
  }

  for (const auto outer_config : value->GetDict()) {
    const std::string& id = outer_config.first;
    const base::Value::Dict& config = outer_config.second.GetDict();

    bool valid_config = true;

    const auto* installer_image = config.FindDict(kPolicyImageKeyArchSpecific);
    if (installer_image) {
      if (!CheckDownloadableObject(errors, id, kPolicyImageKeyArchSpecific,
                                   *installer_image)) {
        valid_config = false;
      }
    }

    const auto* pflash = config.FindDict(kPolicyPflashKeyArchSpecific);
    if (pflash) {
      if (!CheckDownloadableObject(errors, id, kPolicyPflashKeyArchSpecific,
                                   *pflash)) {
        valid_config = false;
      }
    }

    if (EnabledStrToEnum(*config.FindString(prefs::kPolicyEnabledKey)) ==
        prefs::PolicyEnabledState::INSTALL_ALLOWED) {
      if (!installer_image) {
        // This is an error on x86_64, since that's currently our *only*
        // supported architecture so this definitely indicates a
        // misconfiguration, but we also leave an informational level message
        // for arm devices to be helpful.
        errors->AddError(policy_name(),
                         IDS_POLICY_BRUSCHETTA_UNINSTALLABLE_ERROR,
                         policy::PolicyErrorPath{id}, kUninstallableErrorLevel);
      }

      if (!installer_image || !valid_config) {
        downgraded_by_error_.insert(id);
      }
    }
  }

  return true;
}

void BruschettaPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::DICT);
  if (!value)
    return;

  // We can mostly skip error checking here because by this point the policy has
  // already been checked against the schema.

  base::Value::Dict pref;

  for (const auto outer_config : value->GetDict()) {
    const std::string& id = outer_config.first;
    const base::Value::Dict& config = outer_config.second.GetDict();

    base::Value::Dict pref_config;
    bool installable;

    {
      pref_config.Set(prefs::kPolicyNameKey,
                      *config.FindString(prefs::kPolicyNameKey));
    }

    {
      auto policy_enabled =
          EnabledStrToEnum(*config.FindString(prefs::kPolicyEnabledKey));
      if (downgraded_by_error_.contains(id)) {
        policy_enabled =
            std::min(policy_enabled, prefs::PolicyEnabledState::RUN_ALLOWED);
      }
      pref_config.Set(prefs::kPolicyEnabledKey,
                      static_cast<int>(policy_enabled));
      installable =
          (policy_enabled == prefs::PolicyEnabledState::INSTALL_ALLOWED);
    }

    {
      const auto* installer_image =
          config.FindDict(kPolicyImageKeyArchSpecific);
      if (installer_image && installable) {
        pref_config.Set(prefs::kPolicyImageKey, installer_image->Clone());
      }
    }

    {
      const auto* pflash = config.FindDict(kPolicyPflashKeyArchSpecific);
      if (pflash && installable) {
        pref_config.Set(prefs::kPolicyPflashKey, pflash->Clone());
      }
    }

    {
      const auto* vtpm = config.FindDict(prefs::kPolicyVTPMKey);
      bool vtpm_enabled = false;
      prefs::PolicyUpdateAction vtpm_update_action =
          prefs::PolicyUpdateAction::FORCE_SHUTDOWN_IF_MORE_RESTRICTED;
      if (vtpm) {
        vtpm_enabled = *vtpm->FindBool(prefs::kPolicyVTPMEnabledKey);

        const auto* vtpm_update_action_str =
            vtpm->FindString(prefs::kPolicyVTPMUpdateActionKey);
        if (vtpm_update_action_str) {
          vtpm_update_action = UpdateActionStrToEnum(*vtpm_update_action_str);
        }
      }

      base::Value::Dict pref_vtpm;
      pref_vtpm.Set(prefs::kPolicyVTPMEnabledKey, vtpm_enabled);
      pref_vtpm.Set(prefs::kPolicyVTPMUpdateActionKey,
                    static_cast<int>(vtpm_update_action));

      pref_config.Set(prefs::kPolicyVTPMKey, std::move(pref_vtpm));
    }

    {
      base::Value::List pref_oem_strings;

      const auto* oem_strings = config.FindList(prefs::kPolicyOEMStringsKey);
      if (oem_strings) {
        for (const auto& oem_string : *oem_strings) {
          pref_oem_strings.Append(oem_string.GetString());
        }
      }

      pref_config.Set(prefs::kPolicyOEMStringsKey, std::move(pref_oem_strings));
    }

    {
      pref_config.Set(
          prefs::kPolicyDisplayOrderKey,
          config.FindInt(prefs::kPolicyDisplayOrderKey).value_or(0));
    }

    pref.Set(id, std::move(pref_config));
  }

  prefs->SetValue(prefs::kBruschettaVMConfiguration,
                  base::Value(std::move(pref)));
}

}  // namespace bruschetta
