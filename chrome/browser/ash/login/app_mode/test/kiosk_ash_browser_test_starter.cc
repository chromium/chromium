// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/kiosk_ash_browser_test_starter.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_device_ownership_waiter.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

bool KioskAshBrowserTestStarter::HasLacrosArgument() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kLacrosChromePath);
}

void KioskAshBrowserTestStarter::PrepareEnvironmentForKioskLacros() {
  DCHECK(HasLacrosArgument());
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  ASSERT_TRUE(scoped_temp_dir_xdg_.CreateUniqueTempDir());
  env->SetVar("XDG_RUNTIME_DIR", scoped_temp_dir_xdg_.GetPath().AsUTF8Unsafe());

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kAshEnableWaylandServer);

  std::vector<std::string> lacros_args = {
      // Disable gpu process in Lacros since hardware accelerated rendering is
      // not possible yet in Ash X11 backend. See details in crbug/1478369.
      "--disable-gpu",
      // Disable gpu sandbox in Lacros since it fails in Linux emulator
      // environment.
      // See details in crbug/1483530.
      "--disable-gpu-sandbox"};
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kLacrosChromeAdditionalArgs,
      base::JoinString(lacros_args, "####"));
}

void KioskAshBrowserTestStarter::SetLacrosAvailabilityPolicy() {
  DCHECK(HasLacrosArgument());
  policy::PolicyMap policy;
  policy.Set(policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::Value(GetLacrosAvailabilityPolicyName(
                 ash::standalone_browser::LacrosAvailability::kLacrosOnly)),
             /*external_data_fetcher=*/nullptr);
  crosapi::browser_util::CacheLacrosAvailability(policy);
}

void KioskAshBrowserTestStarter::SetUpBrowserManager() {
  DCHECK(HasLacrosArgument());
  crosapi::BrowserManager::Get()->set_device_ownership_waiter_for_testing(
      std::make_unique<crosapi::FakeDeviceOwnershipWaiter>());
}

}  // namespace ash
