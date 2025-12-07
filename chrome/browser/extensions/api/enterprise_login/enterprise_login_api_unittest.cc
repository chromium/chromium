// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_login/enterprise_login_api.h"

#include <string>

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class EnterpriseLoginApiUnittest : public ExtensionApiUnittest {
 public:
  EnterpriseLoginApiUnittest() = default;
  ~EnterpriseLoginApiUnittest() override = default;

  EnterpriseLoginApiUnittest(const EnterpriseLoginApiUnittest&) = delete;
  EnterpriseLoginApiUnittest& operator=(const EnterpriseLoginApiUnittest&) =
      delete;
};

TEST_F(EnterpriseLoginApiUnittest, ExitCurrentManagedGuestSessionNoSession) {
  EXPECT_EQ("Not a managed guest session.",
            RunFunctionAndReturnError(
                base::MakeRefCounted<
                    EnterpriseLoginExitCurrentManagedGuestSessionFunction>(),
                "[]"));
}

TEST_F(EnterpriseLoginApiUnittest, ExitCurrentManagedGuestSessionUserSession) {
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager> user_manager{
      std::make_unique<ash::FakeChromeUserManager>()};
  const AccountId user_account_id =
      AccountId::FromUserEmail("user-session@example.com");
  user_manager->AddUser(user_account_id);
  user_manager->LoginUser(user_account_id);

  EXPECT_EQ("Not a managed guest session.",
            RunFunctionAndReturnError(
                base::MakeRefCounted<
                    EnterpriseLoginExitCurrentManagedGuestSessionFunction>(),
                "[]"));
}

TEST_F(EnterpriseLoginApiUnittest,
       ExitCurrentManagedGuestSessionCleanDataInManagedGuestSession) {
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager> user_manager{
      std::make_unique<ash::FakeChromeUserManager>()};
  const AccountId mgs_account_id =
      AccountId::FromUserEmail("managed-guest-session@example.com");
  user_manager->AddPublicAccountUser(mgs_account_id);
  user_manager->LoginUser(mgs_account_id);

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         "hello world");

  RunFunction(base::MakeRefCounted<
                  EnterpriseLoginExitCurrentManagedGuestSessionFunction>(),
              "[]");

  ASSERT_EQ("", local_state->GetString(
                    prefs::kLoginExtensionApiDataForNextLoginAttempt));
}

}  // namespace extensions
