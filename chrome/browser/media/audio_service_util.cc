// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_service_util.h"

#include <string>

#include "base/feature_list.h"
#include "base/optional.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_features.h"

namespace {

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
bool GetPolicyOrFeature(const char* policy_name, const base::Feature& feature) {
  const policy::PolicyMap& policies =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()));
  base::Optional<bool> policy_value;
  if (const base::Value* value = policies.GetValue(policy_name)) {
    policy_value.emplace();
    value->GetAsBoolean(&policy_value.value());
  }
  return policy_value.value_or(base::FeatureList::IsEnabled(feature));
}
#endif

}  // namespace

bool IsAudioServiceSandboxEnabled() {
// TODO(crbug.com/1052397): Remove !IS_CHROMEOS_LACROS once lacros starts being
// built with OS_CHROMEOS instead of OS_LINUX.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  return GetPolicyOrFeature(policy::key::kAudioSandboxEnabled,
                            features::kAudioServiceSandbox);
#else
  return base::FeatureList::IsEnabled(features::kAudioServiceSandbox);
#endif
}

#if defined(OS_WIN)
bool IsAudioProcessHighPriorityEnabled() {
  return GetPolicyOrFeature(policy::key::kAudioProcessHighPriorityEnabled,
                            features::kAudioProcessHighPriorityWin);
}
#endif
