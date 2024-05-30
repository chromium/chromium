// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Creates policy key file for the user specified in |user_policy|.
void SetUserKeys(const UserPolicyBuilder& user_policy) {
  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);

  const AccountId account_id =
      AccountId::FromUserEmail(user_policy.policy_data().username());
  base::FilePath user_keys_dir;
  ASSERT_TRUE(base::PathService::Get(chromeos::dbus_paths::DIR_USER_POLICY_KEYS,
                                     &user_keys_dir));
  const std::string sanitized_username =
      ash::UserDataAuthClient::GetStubSanitizedUsername(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id));
  const base::FilePath user_key_file =
      user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");
  std::string user_key_bits = user_policy.GetPublicSigningKeyAsString();
  ASSERT_FALSE(user_key_bits.empty());
  ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
  ASSERT_TRUE(base::WriteFile(user_key_file, user_key_bits));
}

}  // namespace

constexpr char AffiliationTestHelper::kFakeRefreshToken[] =
    "fake-refresh-token";
constexpr char AffiliationTestHelper::kEnterpriseUserEmail[] =
    "testuser@example.com";
constexpr char AffiliationTestHelper::kEnterpriseUserGaiaId[] = "01234567890";

// static
AffiliationTestHelper AffiliationTestHelper::CreateForCloud(
    ash::FakeSessionManagerClient* fake_session_manager_client) {
  return AffiliationTestHelper(fake_session_manager_client);
}

AffiliationTestHelper::AffiliationTestHelper(AffiliationTestHelper&& other) =
    default;

AffiliationTestHelper::AffiliationTestHelper(
    ash::FakeSessionManagerClient* fake_session_manager_client)
    : fake_session_manager_client_(fake_session_manager_client) {
  DCHECK(fake_session_manager_client);
}

void AffiliationTestHelper::CheckPreconditions() {
  ASSERT_TRUE(fake_session_manager_client_);
}

void AffiliationTestHelper::SetDeviceAffiliationIDs(
    DevicePolicyCrosTestHelper* test_helper,
    const base::span<const std::string_view>& device_affiliation_ids) {
  ASSERT_NO_FATAL_FAILURE(CheckPreconditions());

  DevicePolicyBuilder* device_policy = test_helper->device_policy();
  for (const auto& device_affiliation_id : device_affiliation_ids) {
    device_policy->policy_data().add_device_affiliation_ids(
        std::string(device_affiliation_id));
  }
  // Create keys and sign policy.
  device_policy->SetDefaultSigningKey();
  device_policy->Build();

  fake_session_manager_client_->set_device_policy(device_policy->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);
}

void AffiliationTestHelper::SetUserAffiliationIDs(
    UserPolicyBuilder* user_policy,
    const AccountId& user_account_id,
    const base::span<const std::string_view>& user_affiliation_ids) {
  ASSERT_NO_FATAL_FAILURE(CheckPreconditions());

  user_policy->policy_data().set_username(user_account_id.GetUserEmail());
  user_policy->policy_data().set_gaia_id(user_account_id.GetGaiaId());
  ASSERT_NO_FATAL_FAILURE(SetUserKeys(*user_policy));
  for (const auto& user_affiliation_id : user_affiliation_ids) {
    user_policy->policy_data().add_user_affiliation_ids(
        std::string(user_affiliation_id));
  }
  user_policy->Build();

  fake_session_manager_client_->set_user_policy(
      cryptohome::CreateAccountIdentifierFromAccountId(user_account_id),
      user_policy->GetBlob());
}

// static
void AffiliationTestHelper::PreLoginUser(const AccountId& account_id) {
  ScopedListPrefUpdate users_pref(g_browser_process->local_state(),
                                  "LoggedInUsers");
  base::Value email_value(account_id.GetUserEmail());
  if (!base::Contains(users_pref.Get(), email_value))
    users_pref->Append(std::move(email_value));

  user_manager::KnownUser(g_browser_process->local_state())
      .SaveKnownUser(account_id);

  ash::StartupUtils::MarkOobeCompleted();
}

// static
void AffiliationTestHelper::LoginUser(const AccountId& account_id) {
  ash::test::UserSessionManagerTestApi session_manager_test_api(
      ash::UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  CHECK(account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY);
  ash::UserContext user_context(user_manager::UserType::kRegular, account_id);
  user_context.SetKey(ash::Key("password"));
  if (account_id.GetUserEmail() == kEnterpriseUserEmail) {
    user_context.SetRefreshToken(kFakeRefreshToken);
  }
  auto* controller = ash::ExistingUserController::current_controller();
  CHECK(controller);
  controller->Login(user_context, ash::SigninSpecifics());
  ash::test::WaitForPrimaryUserSessionStart();

  const user_manager::UserList& logged_users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (user_manager::UserList::const_iterator it = logged_users.begin();
       it != logged_users.end(); ++it) {
    if ((*it)->GetAccountId() == user_context.GetAccountId())
      return;
  }
  ADD_FAILURE() << account_id.Serialize()
                << " was not added via PreLoginUser()";
}

// static
void AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(ash::switches::kLoginManager);
  command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
  // LoginManager tests typically don't stand up a policy test server but
  // instead inject policies directly through a SessionManagerClient. So allow
  // policy fetches to fail - this is expected.
  command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
}

}  // namespace policy
