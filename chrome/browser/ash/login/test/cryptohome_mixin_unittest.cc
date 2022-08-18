// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"

#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CryptohomeMixinTest : public ::testing::Test {
 public:
  CryptohomeMixinTest() {
    cryptohome_mixin_ = std::make_unique<CryptohomeMixin>(&host_);
  }

  void TearDown() override {
    if (FakeUserDataAuthClient::Get() != nullptr) {
      FakeUserDataAuthClient::Shutdown();
    }
  }

 protected:
  void InitializeFakeUserDataAuthClient() {
    UserDataAuthClient::InitializeFake();
  }

  std::unique_ptr<CryptohomeMixin> cryptohome_mixin_;
  AccountId user_ = EmptyAccountId();

 private:
  InProcessBrowserTestMixinHost host_;
};

// When users are added to CryptohomeMixin with FakeUserDataAuthClient
// not yet initialized, users will be pooled into a queue, and added
// all in 1 shot to FakeUserDataAuthClient during
// SetUpOnMainThread, when we are sure that FakeUserDataAuthClient
// has been initialized
TEST_F(CryptohomeMixinTest, PoolUsersWhenUserDataAuthClientIsNull) {
  cryptohome_mixin_->MarkUserAsExisting(user_);
  ASSERT_EQ(cryptohome_mixin_->pending_users_.size(), 1u);
  ASSERT_EQ(cryptohome_mixin_->pending_users_.front(),
            cryptohome::CreateAccountIdentifierFromAccountId(user_));
}

// Tests the opposite of the case above, i.e, FakeUserDataAuthClient
// is initialized and user is added to the client as soon as
// CryptohomeMixin::MarkUserAsExisting is called
TEST_F(CryptohomeMixinTest, UserDataAuthClientCalledWhenAvailable) {
  InitializeFakeUserDataAuthClient();
  cryptohome_mixin_->MarkUserAsExisting(user_);
  ASSERT_TRUE(cryptohome_mixin_->pending_users_.empty());
}

}  // namespace ash
