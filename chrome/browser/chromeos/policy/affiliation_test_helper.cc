// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/session/user_session_manager_test_api.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Creates policy key file for the user specified in |user_policy|.
void SetUserKeys(const policy::UserPolicyBuilder& user_policy) {
  base::FilePath user_data_dir;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);

  const AccountId account_id =
      AccountId::FromUserEmail(user_policy.policy_data().username());
  base::FilePath user_keys_dir;
  ASSERT_TRUE(base::PathService::Get(chromeos::dbus_paths::DIR_USER_POLICY_KEYS,
                                     &user_keys_dir));
  const std::string sanitized_username =
      chromeos::CryptohomeClient::GetStubSanitizedUsername(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id));
  const base::FilePath user_key_file =
      user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");
  std::string user_key_bits = user_policy.GetPublicSigningKeyAsString();
  ASSERT_FALSE(user_key_bits.empty());
  ASSERT_TRUE(base::CreateDirectory(user_key_file.DirName()));
  ASSERT_EQ(base::WriteFile(user_key_file, user_key_bits.data(),
                            user_key_bits.length()),
            base::checked_cast<int>(user_key_bits.length()));
}

}  // namespace

constexpr char AffiliationTestHelper::kFakeRefreshToken[] =
    "fake-refresh-token";
constexpr char AffiliationTestHelper::kEnterpriseUserEmail[] =
    "testuser@example.com";
constexpr char AffiliationTestHelper::kEnterpriseUserGaiaId[] = "01234567890";

// static
AffiliationTestHelper AffiliationTestHelper::CreateForCloud(
    chromeos::FakeSessionManagerClient* fake_session_manager_client) {
  return AffiliationTestHelper(ManagementType::kCloud,
                               fake_session_manager_client,
                               nullptr /* fake_authpolicy_client */);
}

// static
AffiliationTestHelper AffiliationTestHelper::CreateForActiveDirectory(
    chromeos::FakeSessionManagerClient* fake_session_manager_client,
    chromeos::FakeAuthPolicyClient* fake_authpolicy_client) {
  return AffiliationTestHelper(ManagementType::kActiveDirectory,
                               fake_session_manager_client,
                               fake_authpolicy_client);
}

AffiliationTestHelper::AffiliationTestHelper(AffiliationTestHelper&& other) =
    default;

AffiliationTestHelper::AffiliationTestHelper(
    ManagementType management_type,
    chromeos::FakeSessionManagerClient* fake_session_manager_client,
    chromeos::FakeAuthPolicyClient* fake_authpolicy_client)
    : management_type_(management_type),
      fake_session_manager_client_(fake_session_manager_client),
      fake_authpolicy_client_(fake_authpolicy_client) {
  DCHECK(fake_session_manager_client);
}

void AffiliationTestHelper::CheckPreconditions() {
  ASSERT_TRUE(fake_session_manager_client_);
  ASSERT_TRUE(management_type_ != ManagementType::kActiveDirectory ||
              fake_authpolicy_client_);
}

void AffiliationTestHelper::SetDeviceAffiliationIDs(
    policy::DevicePolicyCrosTestHelper* test_helper,
    const std::set<std::string>& device_affiliation_ids) {
  ASSERT_NO_FATAL_FAILURE(CheckPreconditions());

  policy::DevicePolicyBuilder* device_policy = test_helper->device_policy();
  for (const auto& device_affiliation_id : device_affiliation_ids) {
    device_policy->policy_data().add_device_affiliation_ids(
        device_affiliation_id);
  }
  if (management_type_ != ManagementType::kActiveDirectory) {
    // Create keys and sign policy. Note that Active Directory policy is
    // unsigned.
    device_policy->SetDefaultSigningKey();
  }
  device_policy->Build();

  fake_session_manager_client_->set_device_policy(device_policy->GetBlob());
  fake_session_manager_client_->OnPropertyChangeComplete(true);

  if (management_type_ == ManagementType::kActiveDirectory)
    fake_authpolicy_client_->set_device_affiliation_ids(device_affiliation_ids);
}

void AffiliationTestHelper::SetUserAffiliationIDs(
    policy::UserPolicyBuilder* user_policy,
    const AccountId& user_account_id,
    const std::set<std::string>& user_affiliation_ids) {
  ASSERT_NO_FATAL_FAILURE(CheckPreconditions());
  ASSERT_TRUE(management_type_ != ManagementType::kActiveDirectory ||
              user_account_id.GetAccountType() ==
                  AccountType::ACTIVE_DIRECTORY);

  user_policy->policy_data().set_username(user_account_id.GetUserEmail());
  if (management_type_ != ManagementType::kActiveDirectory) {
    user_policy->policy_data().set_gaia_id(user_account_id.GetGaiaId());
    ASSERT_NO_FATAL_FAILURE(SetUserKeys(*user_policy));
  }
  for (const auto& user_affiliation_id : user_affiliation_ids) {
    user_policy->policy_data().add_user_affiliation_ids(user_affiliation_id);
  }
  user_policy->Build();

  fake_session_manager_client_->set_user_policy(
      cryptohome::CreateAccountIdentifierFromAccountId(user_account_id),
      user_policy->GetBlob());

  if (management_type_ == ManagementType::kActiveDirectory)
    fake_authpolicy_client_->set_user_affiliation_ids(user_affiliation_ids);
}

// static
void AffiliationTestHelper::PreLoginUser(const AccountId& account_id) {
  ListPrefUpdate users_pref(g_browser_process->local_state(), "LoggedInUsers");
  users_pref->AppendIfNotPresent(
      std::make_unique<base::Value>(account_id.GetUserEmail()));
  if (user_manager::UserManager::IsInitialized())
    user_manager::known_user::SaveKnownUser(account_id);

  chromeos::StartupUtils::MarkOobeCompleted();
}

// static
void AffiliationTestHelper::LoginUser(const AccountId& account_id) {
  chromeos::test::UserSessionManagerTestApi session_manager_test_api(
      chromeos::UserSessionManager::GetInstance());
  session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

  const bool is_active_directory =
      account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY;
  const user_manager::UserType user_type =
      is_active_directory ? user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY
                          : user_manager::UserType::USER_TYPE_REGULAR;
  chromeos::UserContext user_context(user_type, account_id);
  user_context.SetKey(chromeos::Key("password"));
  if (account_id.GetUserEmail() == kEnterpriseUserEmail) {
    user_context.SetRefreshToken(kFakeRefreshToken);
  }
  chromeos::ExistingUserController* controller =
      chromeos::ExistingUserController::current_controller();
  CHECK(controller);
  controller->Login(user_context, chromeos::SigninSpecifics());
  chromeos::test::WaitForPrimaryUserSessionStart();

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
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitch(chromeos::switches::kForceLoginManagerInTests);
  // LoginManager tests typically don't stand up a policy test server but
  // instead inject policies directly through a SessionManagerClient. So allow
  // policy fetches to fail - this is expected.
  command_line->AppendSwitch(
      chromeos::switches::kAllowFailedPolicyFetchForTest);
}

}  // namespace policy
