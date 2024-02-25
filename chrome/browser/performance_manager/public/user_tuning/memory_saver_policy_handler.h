// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_MEMORY_SAVER_POLICY_HANDLER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_MEMORY_SAVER_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace performance_manager {

// A policy handler that maps the boolean MemorySaverModeEnabled policy to
// the enum kMemorySaverModeState pref. This is needed because MemorySaver
// was controlled by a boolean pref when the policy was written, but it's now
// controlled by an integer pref. This policy will eventually be deprecated and
// replaced by an integer policy.
class MemorySaverPolicyHandler : public policy::TypeCheckingPolicyHandler {
 public:
  MemorySaverPolicyHandler();
  ~MemorySaverPolicyHandler() override;

 private:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_MEMORY_SAVER_POLICY_HANDLER_H_
