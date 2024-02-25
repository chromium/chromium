// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"

#include "chromeos/ash/components/login/auth/stub_authenticator_builder.h"

namespace ash {
namespace test {

UserSessionManagerTestApi::UserSessionManagerTestApi(
    UserSessionManager* session_manager)
    : session_manager_(session_manager) {}

void UserSessionManagerTestApi::InjectStubUserContext(
    const UserContext& user_context) {
  session_manager_->InjectAuthenticatorBuilder(
      std::make_unique<StubAuthenticatorBuilder>(user_context));
}

void UserSessionManagerTestApi::InjectAuthenticatorBuilder(
    std::unique_ptr<AuthenticatorBuilder> builder) {
  session_manager_->InjectAuthenticatorBuilder(std::move(builder));
}

void UserSessionManagerTestApi::SetShouldLaunchBrowserInTests(
    bool should_launch_browser) {
  session_manager_->set_should_launch_browser_in_tests(should_launch_browser);
}

void UserSessionManagerTestApi::SetShouldObtainTokenHandleInTests(
    bool should_obtain_handle) {
  session_manager_->SetShouldObtainHandleInTests(should_obtain_handle);
}

void UserSessionManagerTestApi::InitializeDeviceId(
    bool is_ephemeral_user,
    user_manager::KnownUser& known_user) {
  session_manager_->InitializeDeviceId(
      is_ephemeral_user, *session_manager_->mutable_user_context_for_testing(),
      known_user);
}

void UserSessionManagerTestApi::SetAttemptRestartClosureInTests(
    const base::RepeatingClosure& attempt_restart_closure) {
  session_manager_->SetAttemptRestartClosureInTests(attempt_restart_closure);
}

OnboardingUserActivityCounter*
UserSessionManagerTestApi::get_onboarding_user_activity_counter() {
  return session_manager_->onboarding_user_activity_counter_.get();
}

}  // namespace test
}  // namespace ash
