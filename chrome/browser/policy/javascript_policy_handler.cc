// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/javascript_policy_handler.h"

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

JavascriptPolicyHandler::JavascriptPolicyHandler() {}

JavascriptPolicyHandler::~JavascriptPolicyHandler() {}

bool JavascriptPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                  PolicyErrorMap* errors) {
  // It is safe to use `GetValueUnsafe()` because type checking is performed
  // before the value is used.
  const base::Value* javascript_enabled =
      policies.GetValueUnsafe(key::kJavascriptEnabled);
  const base::Value* default_setting =
      policies.GetValueUnsafe(key::kDefaultJavaScriptSetting);

  if (javascript_enabled && !javascript_enabled->is_bool()) {
    errors->AddError(key::kJavascriptEnabled, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::BOOLEAN));
  }

  if (default_setting && !default_setting->is_int()) {
    errors->AddError(key::kDefaultJavaScriptSetting, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::INTEGER));
  }

  if (javascript_enabled && default_setting) {
    errors->AddError(key::kJavascriptEnabled,
                     IDS_POLICY_OVERRIDDEN,
                     key::kDefaultJavaScriptSetting);
  }

  return true;
}

void JavascriptPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  int setting = CONTENT_SETTING_DEFAULT;
  const base::Value* default_setting = policies.GetValue(
      key::kDefaultJavaScriptSetting, base::Value::Type::INTEGER);

  if (default_setting) {
    setting = default_setting->GetInt();
  } else {
    const base::Value* javascript_enabled =
        policies.GetValue(key::kJavascriptEnabled, base::Value::Type::BOOLEAN);
    if (javascript_enabled && !javascript_enabled->GetBool()) {
      setting = CONTENT_SETTING_BLOCK;
    }
  }

  if (setting != CONTENT_SETTING_DEFAULT)
    prefs->SetInteger(prefs::kManagedDefaultJavaScriptSetting, setting);
}

}  // namespace policy
