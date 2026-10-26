// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/login/users/profile_user_manager_controller.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"

namespace ash::test {

ChromeUserSessionTestEnvironmentDelegate::
    ChromeUserSessionTestEnvironmentDelegate(
        TestingBrowserProcess* browser_process,
        TestingFactoriesGetter testing_factories_getter)
    : browser_process_(CHECK_DEREF(browser_process)),
      testing_factories_getter_(std::move(testing_factories_getter)) {}

ChromeUserSessionTestEnvironmentDelegate::
    ~ChromeUserSessionTestEnvironmentDelegate() = default;

void ChromeUserSessionTestEnvironmentDelegate::CreateProfileManager() {
  CHECK(!testing_profile_manager_);
  // Attach LoginState if initialized (e.g. by ChromeRenderViewHostTestHarness).
  // Don't call LoginState::Initialize() here: ChromeAshTestBase subclasses
  // construct this delegate before AshTestHelper::SetUp() initializes
  // LoginState.
  if (LoginState::IsInitialized()) {
    LoginState::Get()->OnUserManagerCreated(user_manager::UserManager::Get());
  }
  // Ensure ProfileHelper and BrowserContextHelper singletons are instantiated.
  ProfileHelper::Get();

  testing_profile_manager_ =
      std::make_unique<TestingProfileManager>(&browser_process_.get());
  CHECK(testing_profile_manager_->SetUp());

  profile_user_manager_controller_ =
      std::make_unique<ProfileUserManagerController>(
          testing_profile_manager_->profile_manager(),
          user_manager::UserManager::Get());
}

void ChromeUserSessionTestEnvironmentDelegate::DestroyProfileManager() {
  CHECK(testing_profile_manager_);
  testing_profile_manager_->DeleteAllTestingProfiles();
  profile_user_manager_controller_.reset();
  testing_profile_manager_.reset();
  if (LoginState::IsInitialized()) {
    LoginState::Get()->OnUserManagerWillBeDestroyed(
        user_manager::UserManager::Get());
  }
}

void ChromeUserSessionTestEnvironmentDelegate::CreateProfile(
    const AccountId& account_id) {
  CHECK(testing_profile_manager_);
  ScopedAccountIdAnnotator annotator(
      testing_profile_manager_->profile_manager(), account_id);
  testing_profile_manager_->CreateTestingProfile(
      account_id.GetUserEmail(), testing_factories_getter_
                                     ? testing_factories_getter_.Run()
                                     : TestingProfile::TestingFactories());
}

}  // namespace ash::test
