// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_service.h"

#include "base/command_line.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace indigo {

class IndigoServiceTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kIndigo);
    ::indigo::prefs::RegisterProfilePrefs(prefs_.registry());
    if (set_script_switch_in_setup_) {
      scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
          "indigo-script", "/dummy/path");
    }
  }

  void TearDown() override {
    component_updater::ResetIndigoInstallDirForTesting();
  }

  void CreateService() {
    service_ = std::make_unique<IndigoService>(
        &profile_, identity_test_env_.identity_manager(), &prefs_);
    service_->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
        [](IndigoServiceTest* test,
           IndigoService::RemoteEligibilityCallback callback) {
          test->remote_eligibility_fetch_count_++;
          if (test->auto_complete_remote_eligibility_fetch_) {
            std::move(callback).Run(test->mock_remote_eligibility_);
          } else {
            test->pending_remote_eligibility_callback_ = std::move(callback);
          }
        },
        base::Unretained(this)));
  }

  void MakeAccountAvailableAndCapable() {
    AccountInfo info = identity_test_env_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_.UpdateAccountInfoForAccount(info);
  }

  void SetPolicySettings(prefs::Policy value) {
    prefs_.SetInteger(prefs::kIndigoPolicy, value);
  }

  CombinedEligibility GetCombinedEligibility() {
    base::test::TestFuture<CombinedEligibility> future;
    service_->GetCombinedEligibility(
        future.GetCallback<const CombinedEligibility&>());
    return future.Get();
  }

  void CompleteRemoteEligibilityFetch(
      base::expected<RemoteEligibility, std::string> status =
          base::ok(RemoteEligibility{.is_service_supported_for_account = true,
                                     .has_user_image = true})) {
    if (pending_remote_eligibility_callback_) {
      std::move(pending_remote_eligibility_callback_).Run(std::move(status));
    }
  }

  ::testing::AssertionResult LocalEligibilityBecomes(
      ::testing::Matcher<LocalEligibility> matcher) {
    if (matcher.Matches(service_->GetLocalEligibility())) {
      return ::testing::AssertionSuccess();
    }
    base::test::TestFuture<LocalEligibility> future{
        base::test::TestFutureMode::kQueue};
    auto sub = service_->RegisterLocalEligibilityChangedCallback(
        future.GetRepeatingCallback());
    while (future.Wait()) {
      LocalEligibility eligibility = future.Take();
      if (eligibility != service_->GetLocalEligibility()) {
        return ::testing::AssertionFailure()
               << "notification doesn't match the current eligibility";
      }
      if (matcher.Matches(eligibility)) {
        return ::testing::AssertionSuccess();
      }
    }
    return ::testing::AssertionFailure() << "timed out";
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  TestingPrefServiceSimple prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<IndigoService> service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  RemoteEligibility mock_remote_eligibility_ =
      RemoteEligibility{.is_service_supported_for_account = true,
                        .has_user_image = true};
  int remote_eligibility_fetch_count_ = 0;
  IndigoService::RemoteEligibilityCallback pending_remote_eligibility_callback_;
  bool auto_complete_remote_eligibility_fetch_ = true;
  bool set_script_switch_in_setup_ = true;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(IndigoServiceTest, DefaultStateNotSignedIn) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kNotSignedIn);
}

TEST_F(IndigoServiceTest, SignIn) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));
}

TEST_F(IndigoServiceTest, CapabilitiesDisable) {
  CreateService();

  AccountInfo info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&info.capabilities);
  mutator.set_can_use_model_execution_features(false);
  identity_test_env_.UpdateAccountInfoForAccount(info);

  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kMissingCapabilities));
}

TEST_F(IndigoServiceTest, PolicyDisabledFromConstruction) {
  SetPolicySettings(prefs::Policy::kDisallowed);
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kDisabledByPolicy));
}

TEST_F(IndigoServiceTest, PolicyChangeTriggersUpdate) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  SetPolicySettings(prefs::Policy::kDisallowed);
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kDisabledByPolicy));
}

TEST_F(IndigoServiceTest, AnchoredMessageTrigger) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  CreateService();

  EXPECT_TRUE(service_->CanShowAnchoredMessage());
  service_->AnchoredMessageShown();
  EXPECT_FALSE(service_->CanShowAnchoredMessage());

  task_environment_.FastForwardBy(
      features::kIndigoAnchoredMessageResetDuration.Get());
  EXPECT_TRUE(service_->CanShowAnchoredMessage());
}

TEST_F(IndigoServiceTest, RemoteEligibilityUnsupported) {
  mock_remote_eligibility_ = RemoteEligibility{
      .is_service_supported_for_account = false, .has_user_image = false};
  CreateService();

  MakeAccountAvailableAndCapable();

  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  CombinedEligibility combined_eligibility = GetCombinedEligibility();
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_FALSE(combined_eligibility.remote_eligibility
                   ->is_service_supported_for_account);
  EXPECT_FALSE(combined_eligibility.remote_eligibility->has_user_image);
}

TEST_F(IndigoServiceTest, InvalidateRemoteEligibility_NoFetchInProgress) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  CombinedEligibility combined_eligibility = GetCombinedEligibility();
  EXPECT_EQ(remote_eligibility_fetch_count_, 1);
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_TRUE(combined_eligibility.remote_eligibility
                  ->is_service_supported_for_account);

  service_->InvalidateRemoteEligibility();

  // Next call should trigger a new fetch.
  combined_eligibility = GetCombinedEligibility();
  EXPECT_EQ(remote_eligibility_fetch_count_, 2);
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_TRUE(combined_eligibility.remote_eligibility
                  ->is_service_supported_for_account);
}

TEST_F(IndigoServiceTest, InvalidateRemoteEligibility_FetchInProgress) {
  auto_complete_remote_eligibility_fetch_ = false;
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  base::test::TestFuture<CombinedEligibility> future;
  service_->GetCombinedEligibility(
      future.GetCallback<const CombinedEligibility&>());
  EXPECT_EQ(remote_eligibility_fetch_count_, 1);

  service_->InvalidateRemoteEligibility();
  // Invalidation should have cancelled the first fetch's reply (by invalidating
  // weak ptrs) and started a *new* fetch because there was a pending callback.
  EXPECT_EQ(remote_eligibility_fetch_count_, 2);

  // Now complete the *second* fetch.
  CompleteRemoteEligibilityFetch(base::ok(RemoteEligibility{
      .is_service_supported_for_account = true, .has_user_image = true}));

  // The first callback (which was restarted) should now complete.
  CombinedEligibility combined_eligibility = future.Get();
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_TRUE(combined_eligibility.remote_eligibility
                  ->is_service_supported_for_account);
}

TEST_F(IndigoServiceTest, ErrorMessageStored) {
  auto_complete_remote_eligibility_fetch_ = false;
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  base::test::TestFuture<CombinedEligibility> future;
  service_->GetCombinedEligibility(
      future.GetCallback<const CombinedEligibility&>());

  CompleteRemoteEligibilityFetch(base::unexpected("Server down"));

  CombinedEligibility combined_eligibility = future.Get();
  EXPECT_FALSE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_EQ(combined_eligibility.remote_eligibility.error(), "Server down");
}

class IndigoServiceNoScriptTest : public IndigoServiceTest {
 public:
  IndigoServiceNoScriptTest() { set_script_switch_in_setup_ = false; }
};

TEST_F(IndigoServiceNoScriptTest, ScriptNotAvailable) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kMissingScript);
}

TEST_F(IndigoServiceNoScriptTest, DynamicComponentReady) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kMissingScript);

  base::test::TestFuture<LocalEligibility> future;
  auto sub = service_->RegisterLocalEligibilityChangedCallback(
      future.GetRepeatingCallback());

  // Simulate component ready.
  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"),
                        base::FilePath(FILE_PATH_LITERAL("/dummy/path")),
                        base::DictValue());

  // It should transition to kNotSignedIn (since we are not signed in).
  EXPECT_EQ(future.Take(), LocalEligibility::kNotSignedIn);
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kNotSignedIn);
}

}  // namespace indigo
