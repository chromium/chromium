// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/browser_with_test_window_test.h"
#endif

// ChromeOS has its own network delay logic.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

class CallbackTester {
 public:
  CallbackTester() : called_(0) {}

  void Increment();
  void IncrementAndUnblock(base::RunLoop* run_loop);
  bool WasCalledExactlyOnce();

 private:
  int called_;
};

void CallbackTester::Increment() {
  called_++;
}

void CallbackTester::IncrementAndUnblock(base::RunLoop* run_loop) {
  Increment();
  run_loop->QuitWhenIdle();
}

bool CallbackTester::WasCalledExactlyOnce() {
  return called_ == 1;
}

}  // namespace

class ChromeSigninClientTest : public testing::Test {
 public:
  ChromeSigninClientTest() {
    // Create a signed-in profile.
    TestingProfile::Builder builder;
    profile_ = builder.Build();

    signin_client_ = ChromeSigninClientFactory::GetForProfile(profile());
  }

 protected:
  void SetUpNetworkConnection(bool respond_synchronously,
                              network::mojom::ConnectionType connection_type) {
    auto* tracker = network::TestNetworkConnectionTracker::GetInstance();
    tracker->SetRespondSynchronously(respond_synchronously);
    tracker->SetConnectionType(connection_type);
  }

  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  Profile* profile() { return profile_.get(); }
  SigninClient* signin_client() { return signin_client_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;
  raw_ptr<SigninClient> signin_client_;
};

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsImmediatelyWithNetwork) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_3G);
  CallbackTester tester;
  signin_client()->DelayNetworkCall(
      base::BindOnce(&CallbackTester::Increment, base::Unretained(&tester)));
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsAfterGetConnectionType) {
  SetUpNetworkConnection(false, network::mojom::ConnectionType::CONNECTION_3G);

  base::RunLoop run_loop;
  CallbackTester tester;
  signin_client()->DelayNetworkCall(
      base::BindOnce(&CallbackTester::IncrementAndUnblock,
                     base::Unretained(&tester), &run_loop));
  ASSERT_FALSE(tester.WasCalledExactlyOnce());
  run_loop.Run();  // Wait for IncrementAndUnblock().
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsAfterNetworkChange) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_NONE);

  base::RunLoop run_loop;
  CallbackTester tester;
  signin_client()->DelayNetworkCall(
      base::BindOnce(&CallbackTester::IncrementAndUnblock,
                     base::Unretained(&tester), &run_loop));

  ASSERT_FALSE(tester.WasCalledExactlyOnce());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_3G);
  run_loop.Run();  // Wait for IncrementAndUnblock().
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

#if !BUILDFLAG(IS_ANDROID)

class MockChromeSigninClient : public ChromeSigninClient {
 public:
  explicit MockChromeSigninClient(Profile* profile)
      : ChromeSigninClient(profile) {}

  MOCK_METHOD1(ShowUserManager, void(const base::FilePath&));
  MOCK_METHOD1(LockForceSigninProfile, void(const base::FilePath&));

  MOCK_METHOD3(SignOutCallback,
               void(signin_metrics::ProfileSignout,
                    signin_metrics::SignoutDelete,
                    SigninClient::SignoutDecision signout_decision));
};

class ChromeSigninClientSignoutTest : public BrowserWithTestWindowTest {
 public:
  ChromeSigninClientSignoutTest() : forced_signin_setter_(true) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    CreateClient(browser()->profile());
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void CreateClient(Profile* profile) {
    client_ = std::make_unique<MockChromeSigninClient>(profile);
  }

  void PreSignOut(signin_metrics::ProfileSignout source_metric,
                  signin_metrics::SignoutDelete delete_metric) {
    client_->PreSignOut(base::BindOnce(&MockChromeSigninClient::SignOutCallback,
                                       base::Unretained(client_.get()),
                                       source_metric, delete_metric),
                        source_metric);
  }

  signin_util::ScopedForceSigninSetterForTesting forced_signin_setter_;
  std::unique_ptr<MockChromeSigninClient> client_;
};

TEST_F(ChromeSigninClientSignoutTest, SignOut) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(
      *client_,
      SignOutCallback(source_metric, delete_metric,
                      SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);

  PreSignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutForceSignin) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(false);
  CreateClient(browser()->profile());

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(
      *client_,
      SignOutCallback(source_metric, delete_metric,
                      SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);
  PreSignOut(source_metric, delete_metric);
}

class ChromeSigninClientSignoutSourceTest
    : public ::testing::WithParamInterface<signin_metrics::ProfileSignout>,
      public ChromeSigninClientSignoutTest {
 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  signin::IdentityTestEnvironment identity_test_env_;
};

// Returns true if signout can be disallowed by policy for the given source.
bool IsSignoutDisallowedByPolicy(
    Profile* profile,
    signin_metrics::ProfileSignout signout_source) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  if (identity_manager &&
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return false;
  }

  switch (signout_source) {
    // NOTE: SIGNOUT_TEST == SIGNOUT_PREF_CHANGED.
    case signin_metrics::ProfileSignout::SIGNOUT_PREF_CHANGED:
    case signin_metrics::ProfileSignout::GOOGLE_SERVICE_NAME_PATTERN_CHANGED:
    case signin_metrics::ProfileSignout::SIGNIN_PREF_CHANGED_DURING_SIGNIN:
    case signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS:
    case signin_metrics::ProfileSignout::SERVER_FORCED_DISABLE:
    case signin_metrics::ProfileSignout::TRANSFER_CREDENTIALS:
    case signin_metrics::ProfileSignout::
        AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN:
    case signin_metrics::ProfileSignout::SIGNIN_NOT_ALLOWED_ON_PROFILE_INIT:
    case signin_metrics::ProfileSignout::USER_TUNED_OFF_SYNC_FROM_DICE_UI:
      return true;
    case signin_metrics::ProfileSignout::ACCOUNT_REMOVED_FROM_DEVICE:
    case signin_metrics::ProfileSignout::
        IOS_ACCOUNT_REMOVED_FROM_DEVICE_AFTER_RESTORE:
      // TODO(msarda): Add more of the above cases to this "false" branch.
      // For now only ACCOUNT_REMOVED_FROM_DEVICE is here to preserve the status
      // quo. Additional internal sources of sign-out will be moved here in a
      // follow up CL.
      return false;
    case signin_metrics::ProfileSignout::ABORT_SIGNIN:
      // Allow signout because data has not been synced yet.
      return false;
    case signin_metrics::ProfileSignout::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST:
      // Allow signout for tests that want to force it.
      return false;
    case signin_metrics::ProfileSignout::ACCOUNT_ID_MIGRATION:
      // Allowed to force finish the account id migration.
      return false;
    case signin_metrics::ProfileSignout::USER_DELETED_ACCOUNT_COOKIES:
    case signin_metrics::ProfileSignout::MOBILE_IDENTITY_CONSISTENCY_ROLLBACK:
      // There's no special-casing for these in ChromeSigninClient, as they only
      // happen when there's no sync account and policies aren't enforced.
      // PrimaryAccountManager won't actually invoke PreSignOut in this case,
      // thus it is fine for ChromeSigninClient to not have any special-casing.
      return true;
    case signin_metrics::ProfileSignout::NUM_PROFILE_SIGNOUT_METRICS:
      NOTREACHED();
      return false;
  }
}

TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutAllowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  ASSERT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));

  // Verify IdentityManager gets callback indicating sign-out is always allowed.
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(
      *client_,
      SignOutCallback(signout_source, delete_metric,
                      SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  ASSERT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));
  signin_util::SetUserSignoutAllowedForProfile(profile.get(), false);
  ASSERT_FALSE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsSignoutDisallowedByPolicy(profile.get(), signout_source)
          ? SigninClient::SignoutDecision::DISALLOW_SIGNOUT
          : SigninClient::SignoutDecision::ALLOW_SIGNOUT;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(*client_,
              SignOutCallback(signout_source, delete_metric, signout_decision))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}

TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutDisallowedWithSync) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  ASSERT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));
  signin_util::SetUserSignoutAllowedForProfile(profile.get(), false);
  ASSERT_FALSE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsSignoutDisallowedByPolicy(profile.get(), signout_source)
          ? SigninClient::SignoutDecision::DISALLOW_SIGNOUT
          : SigninClient::SignoutDecision::ALLOW_SIGNOUT;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  identity_test_env()->MakePrimaryAccountAvailable("bob@example.com",
                                                   signin::ConsentLevel::kSync);
  EXPECT_CALL(*client_,
              SignOutCallback(signout_source, delete_metric, signout_decision))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}

TEST_P(ChromeSigninClientSignoutSourceTest,
       UserSignoutDisallowedAccountManagementAccepted) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  ASSERT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));
  signin_util::SetUserSignoutAllowedForProfile(profile.get(), false);
  ASSERT_FALSE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsSignoutDisallowedByPolicy(profile.get(), signout_source)
          ? SigninClient::SignoutDecision::DISALLOW_SIGNOUT
          : SigninClient::SignoutDecision::ALLOW_SIGNOUT;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(*client_,
              SignOutCallback(signout_source, delete_metric, signout_decision))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}
#endif

const signin_metrics::ProfileSignout kSignoutSources[] = {
    signin_metrics::ProfileSignout::SIGNOUT_PREF_CHANGED,
    signin_metrics::ProfileSignout::GOOGLE_SERVICE_NAME_PATTERN_CHANGED,
    signin_metrics::ProfileSignout::SIGNIN_PREF_CHANGED_DURING_SIGNIN,
    signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS,
    signin_metrics::ProfileSignout::ABORT_SIGNIN,
    signin_metrics::ProfileSignout::SERVER_FORCED_DISABLE,
    signin_metrics::ProfileSignout::TRANSFER_CREDENTIALS,
    signin_metrics::ProfileSignout::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN,
    signin_metrics::ProfileSignout::USER_TUNED_OFF_SYNC_FROM_DICE_UI,
    signin_metrics::ProfileSignout::ACCOUNT_REMOVED_FROM_DEVICE,
    signin_metrics::ProfileSignout::SIGNIN_NOT_ALLOWED_ON_PROFILE_INIT,
    signin_metrics::ProfileSignout::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST,
    signin_metrics::ProfileSignout::USER_DELETED_ACCOUNT_COOKIES,
    signin_metrics::ProfileSignout::MOBILE_IDENTITY_CONSISTENCY_ROLLBACK,
    signin_metrics::ProfileSignout::ACCOUNT_ID_MIGRATION,
    signin_metrics::ProfileSignout::
        IOS_ACCOUNT_REMOVED_FROM_DEVICE_AFTER_RESTORE,
};
static_assert(std::size(kSignoutSources) ==
                  signin_metrics::ProfileSignout::NUM_PROFILE_SIGNOUT_METRICS,
              "kSignoutSources should enumerate all ProfileSignout values");

INSTANTIATE_TEST_SUITE_P(AllSignoutSources,
                         ChromeSigninClientSignoutSourceTest,
                         testing::ValuesIn(kSignoutSources));

#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
