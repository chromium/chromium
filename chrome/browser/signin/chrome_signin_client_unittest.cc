// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
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
                        source_metric,
                        /*has_sync_account=*/false);
  }

  signin_util::ScopedForceSigninSetterForTesting forced_signin_setter_;
  std::unique_ptr<MockChromeSigninClient> client_;
};

TEST_F(ChromeSigninClientSignoutTest, SignOut) {
  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(1);
  EXPECT_CALL(*client_, SignOutCallback(source_metric, delete_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);

  PreSignOut(source_metric, delete_metric);
}

TEST_F(ChromeSigninClientSignoutTest, SignOutWithoutForceSignin) {
  signin_util::ScopedForceSigninSetterForTesting signin_setter(false);
  CreateClient(browser()->profile());

  signin_metrics::ProfileSignout source_metric =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;

  EXPECT_CALL(*client_, ShowUserManager(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, LockForceSigninProfile(browser()->profile()->GetPath()))
      .Times(0);
  EXPECT_CALL(*client_, SignOutCallback(source_metric, delete_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);
  PreSignOut(source_metric, delete_metric);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ChromeSigninClientSignoutTest, MainProfile) {
  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  EXPECT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
}
#endif

TEST_F(ChromeSigninClientSignoutTest, AllAllowed) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_FALSE(profile->IsMainProfile());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  EXPECT_FALSE(profile->IsChild());
#endif

  CreateClient(profile.get());

  EXPECT_TRUE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
#endif
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(ChromeSigninClientSignoutTest, ChildProfile) {
  TestingProfile::Builder builder;
  builder.SetIsSupervisedProfile();
  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_TRUE(profile->IsChild());

  CreateClient(profile.get());
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#else
  EXPECT_TRUE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
#endif
  EXPECT_TRUE(client_->IsRevokeSyncConsentAllowed());
}
#endif

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
bool IsAlwaysAllowedSignoutSources(
    signin_metrics::ProfileSignout signout_source) {
  switch (signout_source) {
    // NOTE: SIGNOUT_TEST == SIGNOUT_PREF_CHANGED.
    case signin_metrics::ProfileSignout::kPrefChanged:
    case signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged:
    case signin_metrics::ProfileSignout::kUserClickedSignoutSettings:
    case signin_metrics::ProfileSignout::kServerForcedDisable:
    case signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin:
    case signin_metrics::ProfileSignout::kSigninNotAllowedOnProfileInit:
    case signin_metrics::ProfileSignout::kSigninRetriggeredFromWebSignin:
    case signin_metrics::ProfileSignout::
        kUserClickedSignoutFromClearBrowsingDataPage:
    case signin_metrics::ProfileSignout::
        kIosAccountRemovedFromDeviceAfterRestore:
    case signin_metrics::ProfileSignout::kUserDeletedAccountCookies:
    case signin_metrics::ProfileSignout::kGaiaCookieUpdated:
    case signin_metrics::ProfileSignout::kAccountReconcilorReconcile:
    case signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu:
    case signin_metrics::ProfileSignout::kAccountEmailUpdated:
      return false;

    case signin_metrics::ProfileSignout::kAccountRemovedFromDevice:
    // Allow signout because data has not been synced yet.
    case signin_metrics::ProfileSignout::kAbortSignin:
    // Allow signout for tests that want to force it.
    case signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest:
    case signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings:
    case signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog:
      return true;
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutMainProfile) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetIsMainProfile(true);
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  ASSERT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));

  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(*client_,
              SignOutCallback(signout_source, delete_metric, signout_decision))
      .Times(1);
  PreSignOut(signout_source, delete_metric);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutAllowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());
  ASSERT_TRUE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
  ASSERT_TRUE(client_->IsRevokeSyncConsentAllowed());

  // Verify IdentityManager gets callback indicating sign-out is always allowed.
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(*client_, SignOutCallback(signout_source, delete_metric,
                                        SigninClient::SignoutDecision::ALLOW))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}

// TODO(crbug.com/1369588): Enable |ChromeSigninClientSignoutSourceTest| test
// suite on Android.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
TEST_P(ChromeSigninClientSignoutSourceTest, UserSignoutDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  client_->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED);
  ASSERT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(*client_,
              SignOutCallback(signout_source, delete_metric, signout_decision))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}

TEST_P(ChromeSigninClientSignoutSourceTest, RevokeSyncDisallowed) {
  signin_metrics::ProfileSignout signout_source = GetParam();

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  std::unique_ptr<TestingProfile> profile = builder.Build();

  CreateClient(profile.get());

  client_->set_is_clear_primary_account_allowed_for_testing(
      SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED);
  ASSERT_FALSE(
      client_->IsClearPrimaryAccountAllowed(/*has_sync_account=*/false));
  ASSERT_FALSE(client_->IsRevokeSyncConsentAllowed());

  // Verify IdentityManager gets callback indicating sign-out is disallowed iff
  // the source of the sign-out is a user-action.
  SigninClient::SignoutDecision signout_decision =
      IsAlwaysAllowedSignoutSources(signout_source)
          ? SigninClient::SignoutDecision::ALLOW
          : SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED;
  signin_metrics::SignoutDelete delete_metric =
      signin_metrics::SignoutDelete::kIgnoreMetric;
  EXPECT_CALL(*client_,
              SignOutCallback(signout_source, delete_metric, signout_decision))
      .Times(1);

  PreSignOut(signout_source, delete_metric);
}
#endif

const signin_metrics::ProfileSignout kSignoutSources[] = {
    signin_metrics::ProfileSignout::kPrefChanged,
    signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged,
    signin_metrics::ProfileSignout::kUserClickedSignoutSettings,
    signin_metrics::ProfileSignout::kAbortSignin,
    signin_metrics::ProfileSignout::kServerForcedDisable,
    signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin,
    signin_metrics::ProfileSignout::kAccountRemovedFromDevice,
    signin_metrics::ProfileSignout::kSigninNotAllowedOnProfileInit,
    signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest,
    signin_metrics::ProfileSignout::kUserDeletedAccountCookies,
    signin_metrics::ProfileSignout::kIosAccountRemovedFromDeviceAfterRestore,
    signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings,
    signin_metrics::ProfileSignout::kUserClickedSignoutProfileMenu,
    signin_metrics::ProfileSignout::kSigninRetriggeredFromWebSignin,
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog,
    signin_metrics::ProfileSignout::kAccountEmailUpdated,
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromClearBrowsingDataPage,
    signin_metrics::ProfileSignout::kGaiaCookieUpdated,
    signin_metrics::ProfileSignout::kAccountReconcilorReconcile,
};
// kNumberOfObsoleteSignoutSources should be updated when a ProfileSignout
// value is deprecated.
const int kNumberOfObsoleteSignoutSources = 5;
static_assert(std::size(kSignoutSources) + kNumberOfObsoleteSignoutSources ==
                  static_cast<int>(signin_metrics::ProfileSignout::kMaxValue) +
                      1,
              "kSignoutSources should enumerate all ProfileSignout values that "
              "are not obsolete");

INSTANTIATE_TEST_SUITE_P(AllSignoutSources,
                         ChromeSigninClientSignoutSourceTest,
                         testing::ValuesIn(kSignoutSources));

#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
