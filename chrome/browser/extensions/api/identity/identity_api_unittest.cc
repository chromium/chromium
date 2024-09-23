// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity/identity_api.h"

#include <memory>
#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "google_apis/gaia/core_account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
constexpr char kTestAccount[] = "test@example.com";

namespace extensions {

class IdentityAPITest : public testing::Test {
 public:
  using MockOnSigninChangedCallback =
      testing::StrictMock<base::MockRepeatingCallback<void(Event*)>>;

  IdentityAPITest()
      : prefs_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        event_router_(prefs_.profile(), prefs_.prefs()),
        api_(CreateIdentityAPI()) {
    // IdentityAPITest requires the extended account info callbacks to be fired
    // on account update/removal.
    identity_env_.EnableRemovalOfExtendedAccountInfo();
  }

  ~IdentityAPITest() override { api_->Shutdown(); }

  std::unique_ptr<IdentityAPI> CreateIdentityAPI() {
    auto identity_api = base::WrapUnique(
        new IdentityAPI(prefs_.profile(), identity_env_.identity_manager(),
                        prefs_.prefs(), &event_router_));
    identity_api->set_on_signin_changed_callback_for_testing(
        mock_on_signin_changed_callback_.Get());
    return identity_api;
  }

  void ResetIdentityAPI(std::unique_ptr<IdentityAPI> new_api) {
    api_ = std::move(new_api);
  }

  content::BrowserTaskEnvironment* task_env() { return &task_env_; }

  signin::IdentityTestEnvironment* identity_env() { return &identity_env_; }

  TestExtensionPrefs* prefs() { return &prefs_; }

  IdentityAPI* api() { return api_.get(); }

  MockOnSigninChangedCallback& mock_on_signin_changed_callback() {
    return mock_on_signin_changed_callback_;
  }

 private:
  content::BrowserTaskEnvironment task_env_;
  signin::IdentityTestEnvironment identity_env_;
  TestExtensionPrefs prefs_;
  EventRouter event_router_;
  MockOnSigninChangedCallback mock_on_signin_changed_callback_;
  std::unique_ptr<IdentityAPI> api_;
};

// Tests that all accounts in extensions is enabled for regular profiles.
TEST_F(IdentityAPITest, AllAccountsExtensionEnabled) {
  EXPECT_FALSE(api()->AreExtensionsRestrictedToPrimaryAccount());
}

TEST_F(IdentityAPITest, GetGaiaIdForExtension) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  std::string gaia_id = identity_env()->MakeAccountAvailable(kTestAccount).gaia;
  api()->SetGaiaIdForExtension(extension_id, gaia_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), gaia_id);

  std::string another_extension_id =
      prefs()->AddExtensionAndReturnId("another_extension");
  EXPECT_EQ(api()->GetGaiaIdForExtension(another_extension_id), std::nullopt);
}

TEST_F(IdentityAPITest, GetGaiaIdForExtensionSurvivesShutdown) {
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_));
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  std::string gaia_id = identity_env()
                            ->MakePrimaryAccountAvailable(
                                kTestAccount, signin::ConsentLevel::kSignin)
                            .gaia;
  api()->SetGaiaIdForExtension(extension_id, gaia_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), gaia_id);

  api()->Shutdown();
  ResetIdentityAPI(CreateIdentityAPI());
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), gaia_id);
}

TEST_F(IdentityAPITest, EraseGaiaIdForExtension) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  CoreAccountInfo account = identity_env()->MakeAccountAvailable(kTestAccount);
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  api()->EraseGaiaIdForExtension(extension_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), std::nullopt);
}

TEST_F(IdentityAPITest, GaiaIdErasedAfterSignOut) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  CoreAccountInfo account = identity_env()->MakeAccountAvailable(kTestAccount);
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  identity_env()->RemoveRefreshTokenForAccount(account.account_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), std::nullopt);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(IdentityAPITest, GaiaIdErasedAfterClearPrimaryAccount) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_)).Times(2);
  CoreAccountInfo account = identity_env()->MakePrimaryAccountAvailable(
      kTestAccount, signin::ConsentLevel::kSignin);
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  identity_env()->ClearPrimaryAccount();
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), std::nullopt);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(IdentityAPITest, GaiaIdErasedAfterSignOutTwoAccounts) {
  std::string extension1_id = prefs()->AddExtensionAndReturnId("extension1");
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_)).Times(3);
  CoreAccountInfo account1 = identity_env()->MakePrimaryAccountAvailable(
      kTestAccount, signin::ConsentLevel::kSignin);
  api()->SetGaiaIdForExtension(extension1_id, account1.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension1_id), account1.gaia);

  std::string extension2_id = prefs()->AddExtensionAndReturnId("extension2");
  CoreAccountInfo account2 =
      identity_env()->MakeAccountAvailable("test2@example.com");
  api()->SetGaiaIdForExtension(extension2_id, account2.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension2_id), account2.gaia);

  identity_env()->RemoveRefreshTokenForAccount(account2.account_id);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension1_id), account1.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension2_id), std::nullopt);
}

TEST_F(IdentityAPITest, GaiaIdErasedAfterSignOutAfterShutdown) {
  std::string extension_id = prefs()->AddExtensionAndReturnId("extension");
  CoreAccountInfo account = identity_env()->MakeAccountAvailable(kTestAccount);
  api()->SetGaiaIdForExtension(extension_id, account.gaia);
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), account.gaia);

  api()->Shutdown();
  ResetIdentityAPI(nullptr);

  identity_env()->RemoveRefreshTokenForAccount(account.account_id);
  ResetIdentityAPI(CreateIdentityAPI());
  EXPECT_EQ(api()->GetGaiaIdForExtension(extension_id), std::nullopt);
}

TEST_F(IdentityAPITest, FireOnAccountSignInChangedOnlyIfSignedIn) {
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_)).Times(0);
  CoreAccountInfo account = identity_env()->MakeAccountAvailable(kTestAccount);

  // Add second account.
  CoreAccountInfo account_2 =
      identity_env()->MakeAccountAvailable("test2@example.com");
  CoreAccountInfo account_3 =
      identity_env()->MakeAccountAvailable("test3@example.com");
  Mock::VerifyAndClearExpectations(&mock_on_signin_changed_callback());

  // Only notify when there is a primary account.
  // Notify with the 3 accounts.
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_)).Times(3);
  identity_env()->SetPrimaryAccount(account.email,
                                    signin::ConsentLevel::kSignin);
  ASSERT_TRUE(identity_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  Mock::VerifyAndClearExpectations(&mock_on_signin_changed_callback());

  // Remove one refresh token.
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_)).Times(1);
  identity_env()->RemoveRefreshTokenForAccount(account_3.account_id);
  Mock::VerifyAndClearExpectations(&mock_on_signin_changed_callback());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Clear primary account is expected to remove two accounts and fire two
  // notifications.
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_)).Times(2);
  identity_env()->ClearPrimaryAccount();
  Mock::VerifyAndClearExpectations(&mock_on_signin_changed_callback());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(IdentityAPITest, MaybeShowChromeSigninDialogChromeAlreadySignedIn) {
  EXPECT_CALL(mock_on_signin_changed_callback(), Run(_));
  identity_env()->MakePrimaryAccountAvailable(kTestAccount,
                                              signin::ConsentLevel::kSignin);
  ASSERT_TRUE(identity_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  base::test::TestFuture<void> on_complete;
  api()->MaybeShowChromeSigninDialog("Extension name",
                                     on_complete.GetCallback());
  // The UI is not shown and the callback is shown immediately.
  EXPECT_TRUE(on_complete.IsReady());
}

TEST_F(IdentityAPITest, MaybeShowChromeSigninDialogNoAccountsOnTheWeb) {
  ASSERT_TRUE(identity_env()
                  ->identity_manager()
                  ->GetAccountsWithRefreshTokens()
                  .empty());
  base::test::TestFuture<void> on_complete;
  api()->MaybeShowChromeSigninDialog("Extension name",
                                     on_complete.GetCallback());
  // The UI is not shown and the callback is shown immediately.
  EXPECT_TRUE(on_complete.IsReady());
}

TEST_F(IdentityAPITest, MaybeShowChromeSigninDialog) {
  identity_env()->MakeAccountAvailable(kTestAccount);
  ASSERT_FALSE(identity_env()
                   ->identity_manager()
                   ->GetAccountsWithRefreshTokens()
                   .empty());

  const size_t kShowTwice = 2;
  for (size_t i = 0; i < kShowTwice; i++) {
    base::test::TestFuture<base::OnceClosure> on_ui_triggered;
    api()->SetSkipUIForTesting(on_ui_triggered.GetCallback());

    base::test::TestFuture<void> on_complete;
    api()->MaybeShowChromeSigninDialog("Extension name",
                                       on_complete.GetCallback());

    EXPECT_FALSE(on_complete.IsReady());
    EXPECT_TRUE(on_ui_triggered.IsReady());

    // Complete the dialog.
    std::move(on_ui_triggered.Take()).Run();
    EXPECT_TRUE(on_complete.IsReady());
  }
}

TEST_F(IdentityAPITest, MaybeShowChromeSigninDialogConcurrent) {
  identity_env()->MakeAccountAvailable(kTestAccount);
  ASSERT_FALSE(identity_env()
                   ->identity_manager()
                   ->GetAccountsWithRefreshTokens()
                   .empty());

  base::test::TestFuture<base::OnceClosure> on_ui_triggered;
  api()->SetSkipUIForTesting(on_ui_triggered.GetCallback());

  base::test::TestFuture<void> on_complete_1;
  base::test::TestFuture<void> on_complete_2;
  api()->MaybeShowChromeSigninDialog("Extension name",
                                     on_complete_1.GetCallback());

  EXPECT_TRUE(on_ui_triggered.IsReady());
  // Should crash if UI is shown as `on_ui_triggered` should have been already
  // consumed.
  api()->MaybeShowChromeSigninDialog("Extension name",
                                     on_complete_2.GetCallback());

  EXPECT_FALSE(on_complete_1.IsReady());
  EXPECT_FALSE(on_complete_2.IsReady());
  // Complete the dialog.
  std::move(on_ui_triggered.Take()).Run();
  EXPECT_TRUE(on_complete_1.IsReady());
  EXPECT_TRUE(on_complete_2.IsReady());
}
#endif
}  // namespace extensions
