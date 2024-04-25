// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_service_util.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/content_features.h"

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
const base::Value* GetPolicy(const char* policy_name) {
  const policy::PolicyMap& policies =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()));
  return policies.GetValue(policy_name, base::Value::Type::BOOLEAN);
}

bool GetPolicyOrFeature(const char* policy_name, const base::Feature& feature) {
  const base::Value* value = GetPolicy(policy_name);
  return value ? value->GetBool() : base::FeatureList::IsEnabled(feature);
}
#endif

}  // namespace

bool IsAudioServiceSandboxEnabled() {
// TODO(crbug.com/40118868): Remove !IS_CHROMEOS_LACROS once lacros starts being
// built with OS_CHROMEOS instead of OS_LINUX.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  return GetPolicyOrFeature(policy::key::kAudioSandboxEnabled,
                            features::kAudioServiceSandbox);
#else
  return base::FeatureList::IsEnabled(features::kAudioServiceSandbox);
#endif
}

#if BUILDFLAG(IS_WIN)
// TODO(crbug.com/40242320): Remove the kAudioProcessHighPriorityEnabled policy
// and the code enabled by this function.
bool IsAudioProcessHighPriorityEnabled() {
  const base::Value* value =
      GetPolicy(policy::key::kAudioProcessHighPriorityEnabled);

  return value && value->GetBool();
}
#endif
