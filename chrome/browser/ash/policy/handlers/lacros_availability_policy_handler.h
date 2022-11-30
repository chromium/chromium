// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_LACROS_AVAILABILITY_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_LACROS_AVAILABILITY_POLICY_HANDLER_H_

#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#error This file shall only be used in ash.
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class PrefValueMap;

namespace policy {

class PolicyMap;

class LacrosAvailabilityPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  LacrosAvailabilityPolicyHandler();

  ~LacrosAvailabilityPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;

  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  absl::optional<crosapi::browser_util::LacrosAvailability> GetValue(
      const PolicyMap& policies,
      PolicyErrorMap* errors);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_LACROS_AVAILABILITY_POLICY_HANDLER_H_
