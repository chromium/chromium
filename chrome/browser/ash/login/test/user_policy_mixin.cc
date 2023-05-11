// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/user_policy_mixin.h"

#include <utility>

#include "ash/constants/ash_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace ash {

UserPolicyMixin::UserPolicyMixin(InProcessBrowserTestMixinHost* mixin_host,
                                 const AccountId& account_id)
    : InProcessBrowserTestMixin(mixin_host), account_id_(account_id) {}

UserPolicyMixin::UserPolicyMixin(InProcessBrowserTestMixinHost* mixin_host,
                                 const AccountId& account_id,
                                 EmbeddedPolicyTestServerMixin* policy_server)
    : InProcessBrowserTestMixin(mixin_host),
      account_id_(account_id),
      embedded_policy_server_(policy_server) {}

UserPolicyMixin::~UserPolicyMixin() = default;

void UserPolicyMixin::SetUpInProcessBrowserTestFixture() {
  SetUpUserKeysFile(user_policy_builder_.GetPublicSigningKeyAsString());

  // Make sure session manager client has been initialized as in-memory. This is
  // requirement for setting policy blobs.
  if (!SessionManagerClient::Get())
    SessionManagerClient::InitializeFakeInMemory();

  session_manager_initialized_ = true;

  if (set_policy_in_setup_)
    SetUpPolicy();
}

std::unique_ptr<ScopedUserPolicyUpdate> UserPolicyMixin::RequestPolicyUpdate() {
  return std::make_unique<ScopedUserPolicyUpdate>(
      &user_policy_builder_, base::BindOnce(&UserPolicyMixin::SetUpPolicy,
                                            weak_factory_.GetWeakPtr()));
}

void UserPolicyMixin::SetUpUserKeysFile(const std::string& user_key_bits) {
  DCHECK(!user_key_bits.empty());

  // Make sure chrome paths are overridden before proceeding - this is usually
  // done in chrome main, which has not happened yet.
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));

  base::ScopedAllowBlockingForTesting allow_io;
  RegisterStubPathOverrides(user_data_dir);
  chromeos::dbus_paths::RegisterStubPathOverrides(user_data_dir);

  base::FilePath user_keys_dir;
  CHECK(base::PathService::Get(chromeos::dbus_paths::DIR_USER_POLICY_KEYS,
                               &user_keys_dir));

  const std::string sanitized_username =
      UserDataAuthClient::GetStubSanitizedUsername(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id_));
  const base::FilePath user_key_file =
      user_keys_dir.AppendASCII(sanitized_username).AppendASCII("policy.pub");

  CHECK(base::CreateDirectory(user_key_file.DirName()));
  bool success = base::WriteFile(user_key_file, user_key_bits);
  DCHECK(success);
}

void UserPolicyMixin::SetUpPolicy() {
  if (!session_manager_initialized_) {
    set_policy_in_setup_ = true;
    return;
  }

  user_policy_builder_.policy_data().set_username(account_id_.GetUserEmail());
  user_policy_builder_.policy_data().set_gaia_id(account_id_.GetGaiaId());
  user_policy_builder_.policy_data().set_public_key_version(1);

  user_policy_builder_.SetDefaultSigningKey();
  user_policy_builder_.Build();

  const std::string policy_blob = user_policy_builder_.GetBlob();

  const cryptohome::AccountIdentifier cryptohome_id =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_);
  FakeSessionManagerClient::Get()->set_user_policy(cryptohome_id, policy_blob);

  if (embedded_policy_server_) {
    embedded_policy_server_->UpdateUserPolicy(user_policy_builder_.payload(),
                                              account_id_.GetUserEmail());
  }
}

}  // namespace ash
