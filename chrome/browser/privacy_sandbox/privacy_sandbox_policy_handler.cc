// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_policy_handler.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/strings/grit/components_strings.h"

PrivacySandboxPolicyHandler::PrivacySandboxPolicyHandler() = default;

PrivacySandboxPolicyHandler::~PrivacySandboxPolicyHandler() = default;

bool PrivacySandboxPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  return true;
}

void PrivacySandboxPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* topics_enabled = policies.GetValue(
      policy::key::kPrivacySandboxAdTopicsEnabled, base::Value::Type::BOOLEAN);
  const base::Value* fledge_enabled =
      policies.GetValue(policy::key::kPrivacySandboxSiteEnabledAdsEnabled,
                        base::Value::Type::BOOLEAN);
  const base::Value* ad_measurement_enabled =
      policies.GetValue(policy::key::kPrivacySandboxAdMeasurementEnabled,
                        base::Value::Type::BOOLEAN);

  if (topics_enabled && !topics_enabled->GetBool()) {
    prefs->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled, false);
  }

  if (fledge_enabled && !fledge_enabled->GetBool()) {
    prefs->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled, false);
  }

  if (ad_measurement_enabled && !ad_measurement_enabled->GetBool()) {
    prefs->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled, false);
  }
}
