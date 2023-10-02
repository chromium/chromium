// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/existing_user_controller_base_test.h"

#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "components/user_manager/user_manager.h"

namespace ash {

ExistingUserControllerBaseTest::ExistingUserControllerBaseTest()
    : scoped_local_state_(TestingBrowserProcess::GetGlobal()),
      fake_user_manager_(std::make_unique<FakeChromeUserManager>()),
      auth_events_recorder_(ash::AuthEventsRecorder::CreateForTesting()) {}

ExistingUserControllerBaseTest::~ExistingUserControllerBaseTest() = default;

FakeChromeUserManager* ExistingUserControllerBaseTest::GetFakeUserManager() {
  return fake_user_manager_.Get();
}

}  // namespace ash
