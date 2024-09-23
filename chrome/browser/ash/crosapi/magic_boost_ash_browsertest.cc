// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

namespace crosapi {

namespace {

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;

// Calls all crosapi::mojom::MagicBoostControlle methods over mojo.
void CallMagicBoostControllerMethods(
    mojo::Remote<mojom::MagicBoostController>& remote) {
  auto display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  remote->ShowDisclaimerUi(
      /*display_id=*/display_id,
      /*action=*/
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing,
      /*opt_in_features=*/OptInFeatures::kOrcaAndHmr);
  remote->ShowDisclaimerUi(
      /*display_id=*/display_id,
      /*action=*/
      crosapi::mojom::MagicBoostController::TransitionAction::kShowEditorPanel,
      /*opt_in_features=*/OptInFeatures::kOrcaAndHmr);
}

// Calls all crosapi::mojom::MagicBoostController methods directly.
void CallMagicBoostControllerMethods(
    ash::MagicBoostControllerAsh* magic_boost_controller) {
  auto display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  magic_boost_controller->ShowDisclaimerUi(
      /*display_id=*/display_id,
      /*action=*/
      crosapi::mojom::MagicBoostController::TransitionAction::kDoNothing,
      /*opt_in_features=*/OptInFeatures::kOrcaAndHmr);
  magic_boost_controller->ShowDisclaimerUi(
      /*display_id=*/display_id,
      /*action=*/
      crosapi::mojom::MagicBoostController::TransitionAction::kShowEditorPanel,
      /*opt_in_features=*/OptInFeatures::kOrcaAndHmr);
}

using MagicBoostAshBrowserTest = InProcessBrowserTest;

// Make sure `MagicBoostController` api calls don't crash.
IN_PROC_BROWSER_TEST_F(MagicBoostAshBrowserTest, Basics) {
  ASSERT_TRUE(CrosapiManager::IsInitialized());
  auto* magic_boost_controller =
      CrosapiManager::Get()->crosapi_ash()->magic_boost_controller_ash();
  {
    mojo::Remote<mojom::MagicBoostController> remote1;
    magic_boost_controller->BindReceiver(remote1.BindNewPipeAndPassReceiver());

    CallMagicBoostControllerMethods(remote1);
    CallMagicBoostControllerMethods(magic_boost_controller);
  }

  // Disconnect old clients and try again to ensure manager's API doesn't crash
  // after any client disconnects.
  mojo::Remote<mojom::MagicBoostController> remote2;
  magic_boost_controller->BindReceiver(remote2.BindNewPipeAndPassReceiver());
  CallMagicBoostControllerMethods(remote2);
  CallMagicBoostControllerMethods(magic_boost_controller);

  // TODO(b/341832244): Test that widget is shown after `ShowDisclaimerUi` is
  // called.
}

}  // namespace

}  // namespace crosapi
