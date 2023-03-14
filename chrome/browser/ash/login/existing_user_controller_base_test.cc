// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/existing_user_controller_base_test.h"

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/login/auth/auth_metrics_recorder.h"
#include "components/user_manager/user_manager.h"

namespace ash {

ExistingUserControllerBaseTest::ExistingUserControllerBaseTest()
    : scoped_local_state_(TestingBrowserProcess::GetGlobal()),
      scoped_user_manager_(std::make_unique<FakeChromeUserManager>()),
      auth_metrics_recorder_(ash::AuthMetricsRecorder::CreateForTesting()) {}

ExistingUserControllerBaseTest::~ExistingUserControllerBaseTest() = default;

FakeChromeUserManager* ExistingUserControllerBaseTest::GetFakeUserManager() {
  return static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
}

}  // namespace ash
