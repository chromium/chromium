// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// ChromeOS has its own network delay logic.
#if !defined(OS_CHROMEOS)

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
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<Profile> profile_;
  SigninClient* signin_client_;
};

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsImmediatelyWithNetwork) {
  SetUpNetworkConnection(true, network::mojom::ConnectionType::CONNECTION_3G);
  CallbackTester tester;
  signin_client()->DelayNetworkCall(
      base::Bind(&CallbackTester::Increment, base::Unretained(&tester)));
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

TEST_F(ChromeSigninClientTest, DelayNetworkCallRunsAfterGetConnectionType) {
  SetUpNetworkConnection(false, network::mojom::ConnectionType::CONNECTION_3G);

  base::RunLoop run_loop;
  CallbackTester tester;
  signin_client()->DelayNetworkCall(
      base::Bind(&CallbackTester::IncrementAndUnblock,
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
      base::Bind(&CallbackTester::IncrementAndUnblock,
                 base::Unretained(&tester), &run_loop));

  ASSERT_FALSE(tester.WasCalledExactlyOnce());
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_3G);
  run_loop.Run();  // Wait for IncrementAndUnblock().
  ASSERT_TRUE(tester.WasCalledExactlyOnce());
}

#if !defined(OS_ANDROID)

class MockChromeSigninClient : public ChromeSigninClient {
 public:
  MockChromeSigninClient(Profile* profile, SigninErrorController* controller)
      : ChromeSigninClient(profile, controller) {}

  MOCK_METHOD1(ShowUserManager, void(const base::FilePath&));
  MOCK_METHOD1(LockForceSigninProfile, void(const base::FilePath&));
};

class MockSigninManager : public SigninManager {
 public:
  explicit MockSigninManager(SigninClient* client,
                             SigninErrorController* signin_error_controller)
      : SigninManager(client,
                      nullptr,
                      &fake_service_,
                      nullptr,
                      signin_error_controller,
                      signin::AccountConsistencyMethod::kDisabled) {
    DCHECK(signin_error_controller);
  }

  MOCK_METHOD4(OnSignoutDecisionReached,
               void(signin_metrics::ProfileSignout,
                    signin_metrics::SignoutDelete,
                    RemoveAccountsOption remove_option,
                    SigninClient::SignoutDecision signout_decision));

  AccountTrackerService fake_service_;
};

class ChromeSigninClientSignoutTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    signin_util::SetForceSigninForTesting(true);
    CreateClient(browser()->profile());
    manager_ = std::make_unique<MockSigninManager>(client_.get(),
                                                   fake_controller_.get());
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  void CreateClient(Profile* profile) {
    SigninErrorController* controller = new SigninErrorController(
        SigninErrorController::AccountMode::ANY_ACCOUNT);
    client_.reset(new MockChromeSigninClient(profile, controller));
    fake_controller_.reset(controller);
  }

  std::unique_ptr<SigninErrorController> fake_controller_;
  std::unique_ptr<MockChromeSigninClient> client_;
  std::unique_ptr<MockSigninManager> manager_;
};

TEST_F(ChromeSigninClientSignoutTest, SignOut) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  source_metric, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);

  manager_->SignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutManager) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  MockSigninManager other_manager(client_.get(), fake_controller_.get());
  other_manager.CopyCredentialsFrom(*manager_.get());

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  source_metric, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);
  manager_->SignOut(source_metric, delete_metric);

  ::testing::Mock::VerifyAndClearExpectations(manager_.get());

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  source_metric, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);
  manager_->SignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutForceSignin) {
  signin_util::SetForceSigninForTesting(false);
  CreateClient(browser()->profile());
  manager_ = std::make_unique<MockSigninManager>(client_.get(),
                                                 fake_controller_.get());

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  source_metric, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);
  manager_->SignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutGuestSession) {
  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  manager_ = std::make_unique<MockSigninManager>(client_.get(),
                                                 fake_controller_.get());

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  source_metric, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);
  manager_->SignOut(source_metric, delete_metric);
}

class ChromeSigninClientSignoutSourceTest
    : public ::testing::WithParamInterface<signin_metrics::ProfileSignout>,
      public ChromeSigninClientSignoutTest {};

// Returns true if signout can be disallowed by policy for the given source.
bool IsSignoutDisallowedByPolicy(
    signin_metrics::ProfileSignout signout_source) {
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
    case signin_metrics::ProfileSignout::USER_TUNED_OFF_SYNC_FROM_DICE_UI:
      return true;
    case signin_metrics::ProfileSignout::ACCOUNT_REMOVED_FROM_DEVICE:
      // TODO(msarda): Add more of the above cases to this "false" branch.
      // For now only ACCOUNT_REMOVED_FROM_DEVICE is here to preserve the status
      // quo. Additional internal sources of sign-out will be moved here in a
      // follow up CL.
      return false;
    case signin_metrics::ProfileSignout::ABORT_SIGNIN:
      // Allow signout because data has not been synced yet.
      return false;
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
  manager_ = std::make_unique<MockSigninManager>(client_.get(),
                                                 fake_controller_.get());

  // User sign-out is allowed for this test.
  ASSERT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));

  // Verify SigninManager gets callback indicating sign-out is always allowed.
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  signout_source, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  SigninClient::SignoutDecision::ALLOW_SIGNOUT))
      .Times(1);

  manager_->SignOut(signout_source, delete_metric);
}

TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  manager_ = std::make_unique<MockSigninManager>(client_.get(),
                                                 fake_controller_.get());

  // Disallow user sign-out.
  ASSERT_TRUE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));
  signin_util::SetUserSignoutAllowedForProfile(profile.get(), false);
  ASSERT_FALSE(signin_util::IsUserSignoutAllowedForProfile(profile.get()));

  // Verify SigninManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsSignoutDisallowedByPolicy(signout_source)
          ? SigninClient::SignoutDecision::DISALLOW_SIGNOUT
          : SigninClient::SignoutDecision::ALLOW_SIGNOUT;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::IGNORE_METRIC;
  EXPECT_CALL(*manager_,
              OnSignoutDecisionReached(
                  signout_source, delete_metric,
                  SigninManager::RemoveAccountsOption::kRemoveAllAccounts,
                  signout_decision))
      .Times(1);

  manager_->SignOut(signout_source, delete_metric);
}

const signin_metrics::ProfileSignout kSignoutSources[] = {
    // NOTE: SIGNOUT_TEST == SIGNOUT_PREF_CHANGED.
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
};
static_assert(base::size(kSignoutSources) ==
                  signin_metrics::ProfileSignout::NUM_PROFILE_SIGNOUT_METRICS,
              "kSignoutSources should enumerate all ProfileSignout values");

INSTANTIATE_TEST_CASE_P(AllSignoutSources,
                        ChromeSigninClientSignoutSourceTest,
                        testing::ValuesIn(kSignoutSources));

#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_CHROMEOS)
