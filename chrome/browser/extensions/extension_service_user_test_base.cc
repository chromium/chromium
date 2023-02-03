// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/test/base/testing_profile.h"

namespace extensions {

ExtensionServiceUserTestBase::ExtensionServiceUserTestBase() = default;
ExtensionServiceUserTestBase::~ExtensionServiceUserTestBase() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ExtensionServiceUserTestBase::SetUp() {
  ExtensionServiceTestBase::SetUp();
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::make_unique<ash::FakeChromeUserManager>());
  account_id_ =
      AccountId::FromUserEmailGaiaId("test-user@testdomain.com", "1234567890");
}

void ExtensionServiceUserTestBase::TearDown() {
  ExtensionServiceTestBase::TearDown();
  profile_.reset();
  scoped_user_manager_.reset();
}

void ExtensionServiceUserTestBase::LoginChromeOSAshUser(
    const user_manager::User* user,
    const AccountId& account_id) {
  ASSERT_TRUE(user);
  GetFakeUserManager()->LoginUser(account_id,
                                  /*set_profile_created_flag=*/false);
  ASSERT_TRUE(GetFakeUserManager()->IsUserLoggedIn());
  ASSERT_TRUE(user == GetFakeUserManager()->GetActiveUser());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ExtensionServiceUserTestBase::MaybeSetUpTestUser(bool is_guest) {
  profile_->SetGuestSession(is_guest);

  ASSERT_EQ(is_guest, profile_->IsGuestSession());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_manager::User* user;
  AccountId account_id = account_id_;
  if (is_guest) {
    user = GetFakeUserManager()->AddGuestUser();
    account_id = GetFakeUserManager()->GetGuestAccountId();
  } else {
    user = GetFakeUserManager()->AddUser(account_id_);
  }
  ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(user, account_id));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace extensions
