// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/ash/magic_boost/magic_boost_controller_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"

namespace crosapi {

namespace {

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;

using MagicBoostAshBrowserTest = InProcessBrowserTest;

// Make sure `MagicBoostController` api calls don't crash.
IN_PROC_BROWSER_TEST_F(MagicBoostAshBrowserTest, Basics) {
  auto* magic_boost_controller = ash::MagicBoostControllerAsh::Get();
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

  // TODO(b/341832244): Test that widget is shown after `ShowDisclaimerUi` is
  // called.
}

}  // namespace

}  // namespace crosapi
