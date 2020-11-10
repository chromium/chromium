// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_ambient_api.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "components/prefs/pref_service.h"

namespace ash {

using AutotestAmbientApiTest = AmbientAshTestBase;

TEST_F(AutotestAmbientApiTest,
       ShouldSuccessfullyWaitForPhotoTransitionAnimation) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  prefs->SetInteger(ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds, 2);

  ShowAmbientScreen();

  // Wait for 10 photo transition animation to complete.
  base::RunLoop run_loop;
  AutotestAmbientApi test_api;
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/10, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace ash
