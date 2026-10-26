// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_CHROME_USER_SESSION_TEST_ENVIRONMENT_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_CHROME_USER_SESSION_TEST_ENVIRONMENT_DELEGATE_H_

#include <memory>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/test/base/testing_profile.h"
#include "components/session_manager/test/user_session_test_environment.h"

class AccountId;
class TestingBrowserProcess;
class TestingProfileManager;

namespace ash {
class ProfileUserManagerController;
}  // namespace ash

namespace ash::test {

// Implementation of UserSessionTestEnvironment::Delegate to integrate with
// TestingProfileManager in //chrome tests.
class ChromeUserSessionTestEnvironmentDelegate
    : public UserSessionTestEnvironment::Delegate {
 public:
  // Returns the testing factories for a new profile. The factories can't be
  // copied, so each profile gets a fresh set.
  using TestingFactoriesGetter =
      base::RepeatingCallback<TestingProfile::TestingFactories()>;

  // `browser_process` must not be nullptr, and outlive `this`. If set,
  // profiles created for logged-in users get `testing_factories_getter`'s
  // factories.
  explicit ChromeUserSessionTestEnvironmentDelegate(
      TestingBrowserProcess* browser_process,
      TestingFactoriesGetter testing_factories_getter = {});
  ChromeUserSessionTestEnvironmentDelegate(
      const ChromeUserSessionTestEnvironmentDelegate&) = delete;
  ChromeUserSessionTestEnvironmentDelegate& operator=(
      const ChromeUserSessionTestEnvironmentDelegate&) = delete;
  ~ChromeUserSessionTestEnvironmentDelegate() override;

  TestingProfileManager& testing_profile_manager() {
    return CHECK_DEREF(testing_profile_manager_.get());
  }

  // UserSessionTestEnvironment::Delegate:
  void CreateProfileManager() override;
  void DestroyProfileManager() override;
  void CreateProfile(const AccountId& account_id) override;

 private:
  const raw_ref<TestingBrowserProcess> browser_process_;
  const TestingFactoriesGetter testing_factories_getter_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<ProfileUserManagerController>
      profile_user_manager_controller_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_CHROME_USER_SESSION_TEST_ENVIRONMENT_DELEGATE_H_
