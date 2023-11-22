// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"

#include "ash/constants/ash_switches.h"
#include "base/location.h"
#include "base/one_shot_event.h"
#include "base/test/test_future.h"
#include "base/test/to_vector.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

AshRequiresLacrosBrowserTestBase::AshRequiresLacrosBrowserTestBase() = default;
AshRequiresLacrosBrowserTestBase::~AshRequiresLacrosBrowserTestBase() = default;

void AshRequiresLacrosBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }
  ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
}

void AshRequiresLacrosBrowserTestBase::SetUpOnMainThread() {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }

  CHECK(!browser_util::IsAshWebBrowserEnabled());
  auto* manager = crosapi::CrosapiManager::Get();
  test_controller_ash_ = std::make_unique<crosapi::TestControllerAsh>();
  manager->crosapi_ash()->SetTestControllerForTesting(  // IN-TEST
      test_controller_ash_.get());

  ash_starter_.StartLacros(this);

  base::test::TestFuture<void> waiter;
  test_controller_ash_->on_standalone_browser_test_controller_bound().Post(
      FROM_HERE, waiter.GetCallback());
  EXPECT_TRUE(waiter.Wait());

  ASSERT_TRUE(crosapi::browser_util::IsLacrosEnabled());
}

void AshRequiresLacrosBrowserTestBase::EnableFeaturesInLacros(
    const std::vector<base::test::FeatureRef>& features) {
  CHECK(ash_starter_.HasLacrosArgument());
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string lacros_args = command_line->GetSwitchValueASCII(
      ash::switches::kLacrosChromeAdditionalArgs);
  command_line->RemoveSwitch(ash::switches::kLacrosChromeAdditionalArgs);

  std::vector<std::string> feature_strings = base::test::ToVector(  // IN-TEST
      features, [](base::test::FeatureRef feature) -> std::string {
        return feature->name;
      });

  std::string lacros_args_with_features =
      lacros_args +
      "####--enable-features=" + base::JoinString(feature_strings, ",");

  command_line->AppendSwitchASCII(ash::switches::kLacrosChromeAdditionalArgs,
                                  lacros_args_with_features);
}

mojom::StandaloneBrowserTestController*
AshRequiresLacrosBrowserTestBase::GetStandaloneBrowserTestController() {
  CHECK(test_controller_ash_);
  return test_controller_ash_->GetStandaloneBrowserTestController().get();
}

Profile* AshRequiresLacrosBrowserTestBase::GetAshProfile() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  return profile;
}
}  // namespace crosapi
