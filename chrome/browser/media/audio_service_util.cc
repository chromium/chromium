// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_service_util.h"

#include <string>

#include "base/feature_list.h"
#include "base/optional.h"
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

bool IsAudioServiceSandboxEnabled() {
  base::Optional<bool> force_enable_audio_sandbox;
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  const policy::PolicyMap& policies =
      g_browser_process->browser_policy_connector()
          ->GetPolicyService()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()));
  const base::Value* audio_sandbox_enabled_policy_value =
      policies.GetValue(policy::key::kAudioSandboxEnabled);
  if (audio_sandbox_enabled_policy_value) {
    force_enable_audio_sandbox.emplace();
    audio_sandbox_enabled_policy_value->GetAsBoolean(
        &force_enable_audio_sandbox.value());
  }
#endif
  return force_enable_audio_sandbox.value_or(
      base::FeatureList::IsEnabled(features::kAudioServiceSandbox));
}

