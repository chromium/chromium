// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace password_manager {
namespace {

class PasswordStoreSigninNotifierImplTest : public testing::Test {
 public:
  PasswordStoreSigninNotifierImplTest() {
    store_ = new MockPasswordStore();
  }

  ~PasswordStoreSigninNotifierImplTest() override {
    store_->ShutdownOnUIThread();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<MockPasswordStore> store_;
};

// Checks that if a notifier is subscribed on sign-in events, then
// a password store receives sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, Subscribed) {
  PasswordStoreSigninNotifierImpl notifier(identity_manager());
  notifier.SubscribeToSigninEvents(store_.get());
  identity_test_env()->MakePrimaryAccountAvailable("test@example.com");
  testing::Mock::VerifyAndClearExpectations(store_.get());
  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash());
  identity_test_env()->ClearPrimaryAccount();
  notifier.UnsubscribeFromSigninEvents();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// a password store receives no sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, Unsubscribed) {
  PasswordStoreSigninNotifierImpl notifier(identity_manager());
  notifier.SubscribeToSigninEvents(store_.get());
  notifier.UnsubscribeFromSigninEvents();
  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash()).Times(0);
  identity_test_env()->MakePrimaryAccountAvailable("test@example.com");
  identity_test_env()->ClearPrimaryAccount();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// a password store receives no sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, SignOutContentArea) {
  PasswordStoreSigninNotifierImpl notifier(identity_manager());
  notifier.SubscribeToSigninEvents(store_.get());

  identity_test_env()->MakePrimaryAccountAvailable("username");
  testing::Mock::VerifyAndClearExpectations(store_.get());
  EXPECT_CALL(*store_, ClearGaiaPasswordHash("username2"));
  auto* identity_manager = identity_test_env()->identity_manager();
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      /*gaia_id=*/"secondary_account_id",
      /*email=*/"username2",
      /*refresh_token=*/"refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  identity_test_env()->EnableRemovalOfExtendedAccountInfo();
  identity_manager->GetAccountsMutator()->RemoveAccount(
      CoreAccountId("secondary_account_id"),
      signin_metrics::SourceForRefreshTokenOperation::kUserMenu_RemoveAccount);
  testing::Mock::VerifyAndClearExpectations(store_.get());

  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash());
  identity_test_env()->ClearPrimaryAccount();
  notifier.UnsubscribeFromSigninEvents();
}

}  // namespace
}  // namespace password_manager
