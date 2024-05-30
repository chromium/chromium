// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

// Calls all crosapi::mojom::MagicBoostControlle methods over mojo.
void CallMagicBoostControllerMethods(
    mojo::Remote<mojom::MagicBoostController>& remote) {
  remote->ShowDisclaimerUi(
      /*display_id=*/0,
      /*action=*/crosapi::mojom::MagicBoostController::TransitionAction::
          kDoNothing);
  remote->ShowDisclaimerUi(
      /*display_id=*/0,
      /*action=*/crosapi::mojom::MagicBoostController::TransitionAction::
          kShowEditorPanel);
}

using MagicBoostLacrosBrowserTest = InProcessBrowserTest;

// Tests `MagicBoostController` api calls over mojo don't crash.
IN_PROC_BROWSER_TEST_F(MagicBoostLacrosBrowserTest, Basics) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(
      lacros_service->IsRegistered<crosapi::mojom::MagicBoostController>());

  if (!lacros_service->IsAvailable<crosapi::mojom::MagicBoostController>()) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Tests that multiple clients can bind to this API.
  mojo::Remote<mojom::MagicBoostController> remote1;
  lacros_service->BindMagicBoostController(
      remote1.BindNewPipeAndPassReceiver());

  {
    mojo::Remote<mojom::MagicBoostController> remote2;
    lacros_service->BindMagicBoostController(
        remote2.BindNewPipeAndPassReceiver());

    // Calls and verifies that `MagicBoostController` methods don't
    // crash.
    CallMagicBoostControllerMethods(remote2);
  }

  // Calls and verifies that `MagicBoostController` methods don't crash
  // after a client has disconnected.
  mojo::Remote<mojom::MagicBoostController> remote3;
  lacros_service->BindMagicBoostController(
      remote3.BindNewPipeAndPassReceiver());
  CallMagicBoostControllerMethods(remote3);

  // TODO(b/341832244): Test that widget is shown after `ShowDisclaimerUi` is
  // called.
}

}  // namespace

}  // namespace crosapi
