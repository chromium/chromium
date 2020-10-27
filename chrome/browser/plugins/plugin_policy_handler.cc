// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_policy_handler.h"

#include <string>

#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_constants.h"

namespace {

// Retrieves a list typed policy or nullptr if not present or not a list.
const base::ListValue* GetListPolicy(const policy::PolicyMap& policies,
                                     const std::string& policy) {
  const base::Value* value = policies.GetValue(policy);
  if (!value)
    return nullptr;

  const base::ListValue* policy_value = nullptr;
  value->GetAsList(&policy_value);
  return policy_value;
}

}  // namespace

PluginPolicyHandler::PluginPolicyHandler() {}

PluginPolicyHandler::~PluginPolicyHandler() {}

void PluginPolicyHandler::ProcessPolicy(const policy::PolicyMap& policies,
                                        PrefValueMap* prefs,
                                        const std::string& policy,
                                        bool disable_pdf_plugin,
                                        ContentSetting flash_content_setting) {
  const base::ListValue* plugins = GetListPolicy(policies, policy);
  if (!plugins)
    return;

  const int size = plugins->GetSize();
  for (int i = 0; i < size; ++i) {
    std::string plugin;
    if (!plugins->GetString(i, &plugin))
      continue;
    if ((base::MatchPattern(ChromeContentClient::kPDFExtensionPluginName,
                            plugin) ||
         base::MatchPattern(ChromeContentClient::kPDFInternalPluginName,
                            plugin)) &&
        !policies.GetValue(policy::key::kAlwaysOpenPdfExternally)) {
      prefs->SetValue(prefs::kPluginsAlwaysOpenPdfExternally,
                      base::Value(disable_pdf_plugin));
    }
  }
}

bool PluginPolicyHandler::CheckPolicySettings(const policy::PolicyMap& policies,
                                              policy::PolicyErrorMap* errors) {
  const std::string checked_policies[] = {
      policy::key::kEnabledPlugins, policy::key::kDisabledPlugins,
      policy::key::kDisabledPluginsExceptions};
  bool ok = true;
  for (size_t i = 0; i < base::size(checked_policies); ++i) {
    const base::Value* value = policies.GetValue(checked_policies[i]);
    if (value && !value->is_list()) {
      errors->AddError(checked_policies[i], IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::LIST));
      ok = false;
    }
  }
  return ok;
}

void PluginPolicyHandler::ApplyPolicySettings(const policy::PolicyMap& policies,
                                              PrefValueMap* prefs) {
  // This order makes enabled plugins take precedence as is now.
  ProcessPolicy(policies, prefs, policy::key::kDisabledPlugins, true,
                CONTENT_SETTING_BLOCK);
  ProcessPolicy(policies, prefs, policy::key::kEnabledPlugins, false,
                CONTENT_SETTING_ALLOW);

  // Finally check if any of the two is in the exceptions list and remove the
  // policy changes.
  const base::ListValue* plugins =
      GetListPolicy(policies, policy::key::kDisabledPluginsExceptions);
  if (!plugins)
    return;
  const int size = plugins->GetSize();
  for (int i = 0; i < size; ++i) {
    std::string plugin;
    if (!plugins->GetString(i, &plugin))
      continue;
    if ((base::MatchPattern(ChromeContentClient::kPDFExtensionPluginName,
                            plugin) ||
         base::MatchPattern(ChromeContentClient::kPDFInternalPluginName,
                            plugin)) &&
        !policies.GetValue(policy::key::kAlwaysOpenPdfExternally)) {
      prefs->RemoveValue(prefs::kPluginsAlwaysOpenPdfExternally);
    }
  }
}
