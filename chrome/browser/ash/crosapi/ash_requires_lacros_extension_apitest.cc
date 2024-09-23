// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/ash_requires_lacros_extension_apitest.h"

#include "ash/constants/ash_features.h"
#include "base/location.h"
#include "base/one_shot_event.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/test_controller_ash.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

AshRequiresLacrosExtensionApiTest::AshRequiresLacrosExtensionApiTest() =
    default;

AshRequiresLacrosExtensionApiTest::~AshRequiresLacrosExtensionApiTest() =
    default;

void AshRequiresLacrosExtensionApiTest::SetUpInProcessBrowserTestFixture() {
  ExtensionApiTest::SetUpInProcessBrowserTestFixture();

  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }
  ASSERT_TRUE(ash_starter_.PrepareEnvironmentForLacros());
}

void AshRequiresLacrosExtensionApiTest::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();

  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }
  ash_starter_.StartLacros(this);

  // Wait until StandaloneBrowserTestController binds with test_controller_ash_.
  CHECK(crosapi::TestControllerAsh::Get());
  base::test::TestFuture<void> waiter;
  crosapi::TestControllerAsh::Get()
      ->on_standalone_browser_test_controller_bound()
      .Post(FROM_HERE, waiter.GetCallback());
  EXPECT_TRUE(waiter.Wait());
}

mojom::StandaloneBrowserTestController*
AshRequiresLacrosExtensionApiTest::GetStandaloneBrowserTestController() {
  CHECK(crosapi::TestControllerAsh::Get());
  return crosapi::TestControllerAsh::Get()
      ->GetStandaloneBrowserTestController();
}

}  // namespace crosapi
