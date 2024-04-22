// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/one_shot_event.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/standalone_browser/test_util.h"
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
  ash_starter_.StartLacros(this);

  CHECK(crosapi::TestControllerAsh::Get());
  base::test::TestFuture<void> waiter;
  crosapi::TestControllerAsh::Get()
      ->on_standalone_browser_test_controller_bound()
      .Post(FROM_HERE, waiter.GetCallback());
  EXPECT_TRUE(waiter.Wait());

  ASSERT_TRUE(browser_util::IsLacrosEnabled());
}

void AshRequiresLacrosBrowserTestBase::EnableFeaturesInLacros(
    const std::vector<base::test::FeatureRef>& features) {
  ash_starter_.EnableFeaturesInLacros(features);
}

mojom::StandaloneBrowserTestController*
AshRequiresLacrosBrowserTestBase::GetStandaloneBrowserTestController() {
  CHECK(crosapi::TestControllerAsh::Get());
  return crosapi::TestControllerAsh::Get()
      ->GetStandaloneBrowserTestController();
}

Profile* AshRequiresLacrosBrowserTestBase::GetAshProfile() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);
  return profile;
}
}  // namespace crosapi
