// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/values.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/session_manager/session_manager_types.h"
#include "extensions/browser/api_test_utils.h"
#endif

namespace {

constexpr char kExtensionName[] = "loginState API extension";
constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyzabcdef";

}  // namespace

namespace extensions {

class LoginStateApiUnittest : public ExtensionApiUnittest {
 public:
  LoginStateApiUnittest() {}

  LoginStateApiUnittest(const LoginStateApiUnittest&) = delete;
  LoginStateApiUnittest& operator=(const LoginStateApiUnittest&) = delete;

  ~LoginStateApiUnittest() override = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    scoped_refptr<const Extension> extension =
        ExtensionBuilder(kExtensionName).SetID(kExtensionId).Build();
    set_extension(extension);
  }
};

// Test that |loginState.getProfileType()| returns |USER_PROFILE| for
// extensions not running in the signin profile.
TEST_F(LoginStateApiUnittest, GetProfileType_UserProfile) {
  auto function = base::MakeRefCounted<LoginStateGetProfileTypeFunction>();
  EXPECT_EQ("USER_PROFILE",
            RunFunctionAndReturnValue(function.get(), "[]")->GetString());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test that |loginState.getProfileType()| returns |SIGNIN_PROFILE| for
// extensions running in the signin profile.
TEST_F(LoginStateApiUnittest, GetProfileType_SigninProfile) {
  // |ash::ProfileHelper::GetSigninProfile()| cannot be used as the
  // |TestingProfileManager| set up by |BrowserWithTestWindowTest| has an empty
  // user data directory.
  TestingProfile::Builder builder;
  builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
  std::unique_ptr<Profile> profile = builder.Build();

  auto function = base::MakeRefCounted<LoginStateGetProfileTypeFunction>();
  EXPECT_EQ("SIGNIN_PROFILE", api_test_utils::RunFunctionAndReturnSingleResult(
                                  function.get(), "[]", profile.get())
                                  ->GetString());
}

class LoginStateApiAshUnittest : public LoginStateApiUnittest {
 public:
  LoginStateApiAshUnittest() {}

  LoginStateApiAshUnittest(const LoginStateApiAshUnittest&) = delete;
  LoginStateApiAshUnittest& operator=(const LoginStateApiAshUnittest&) = delete;

  ~LoginStateApiAshUnittest() override = default;

  void SetUp() override { LoginStateApiUnittest::SetUp(); }
};

// Test that calling |loginState.getSessionState()| returns the correctly mapped
// session state.
TEST_F(LoginStateApiAshUnittest, GetSessionState) {
  const struct {
    const session_manager::SessionState session_state;
    const std::string expected;
  } kTestCases[] = {
      {session_manager::SessionState::UNKNOWN, "UNKNOWN"},
      {session_manager::SessionState::OOBE, "IN_OOBE_SCREEN"},
      {session_manager::SessionState::LOGIN_PRIMARY, "IN_LOGIN_SCREEN"},
      {session_manager::SessionState::LOGGED_IN_NOT_ACTIVE, "IN_LOGIN_SCREEN"},
      {session_manager::SessionState::LOGIN_SECONDARY, "IN_LOGIN_SCREEN"},
      {session_manager::SessionState::ACTIVE, "IN_SESSION"},
      {session_manager::SessionState::LOCKED, "IN_LOCK_SCREEN"},
      {session_manager::SessionState::RMA, "IN_RMA_SCREEN"},
  };

  for (const auto& test : kTestCases) {
    // SessionManager is created by
    // |AshTestHelper::bluetooth_config_test_helper()|.
    session_manager::SessionManager::Get()->SetSessionState(test.session_state);
    auto function = base::MakeRefCounted<LoginStateGetSessionStateFunction>();
    std::optional<base::Value> result =
        RunFunctionAndReturnValue(function.get(), "[]");
    EXPECT_EQ(test.expected, result->GetString());
  }
}
#endif

}  // namespace extensions
