// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_browser_test_base.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/scalable_iph/customizable_test_env_browser_test_base.h"
#include "chrome/browser/ash/scalable_iph/mock_scalable_iph_delegate.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_delegate_impl.h"
#include "chrome/browser/ash/scalable_iph/scalable_iph_factory_impl.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "chromeos/ash/services/network_config/in_process_instance.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

namespace {

using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkType;
using Observer = ::scalable_iph::ScalableIphDelegate::Observer;

std::set<std::string> mock_delegate_created_;

constexpr char kTestWiFiId[] = "test-wifi-id";

constexpr auto kEligileUserSessionTypesForMantaService = base::MakeFixedFlatSet<
    CustomizableTestEnvBrowserTestBase::UserSessionType>(
    {CustomizableTestEnvBrowserTestBase::UserSessionType::kRegular,
     CustomizableTestEnvBrowserTestBase::UserSessionType::kRegularNonOwner,
     CustomizableTestEnvBrowserTestBase::UserSessionType::kManaged,
     CustomizableTestEnvBrowserTestBase::UserSessionType::kRegularWithOobe});

BASE_FEATURE(kScalableIphTest,
             "ScalableIphTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

ScalableIphBrowserTestBase::ScalableIphBrowserTestBase() {
  scalable_iph::ScalableIph::ForceEnableIphFeatureForTesting();
}

ScalableIphBrowserTestBase::~ScalableIphBrowserTestBase() = default;

void ScalableIphBrowserTestBase::SetUp() {
  InitializeScopedFeatureList();

  network_config::OverrideInProcessInstanceForTesting(
      &fake_cros_network_config_);

  // Keyed service is a service which is tied to an object. For our use cases,
  // the object is `BrowserContext` (e.g. `Profile`). See
  // //components/keyed_service/README.md for details on keyed service.
  //
  // We set a testing factory to inject a mock. A testing factory must be set
  // early enough as a service is not created before that, e.g. a `Tracker` must
  // not be created before we set `CreateMockTracker`. If a keyed service is
  // created before we set our testing factory, `SetTestingFactory` will
  // destruct already created keyed services at a time we set our testing
  // factory. It destructs a keyed service at an unusual timing. It can trigger
  // a dangling pointer issue, etc.
  //
  // `SetUpOnMainThread` below is too late to set a testing factory. Note that
  // `InProcessBrowserTest::SetUp` is called at the very early stage, e.g.
  // before command lines are set, etc.
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &ScalableIphBrowserTestBase::SetTestingFactories,
              enable_mock_tracker_));

  CustomizableTestEnvBrowserTestBase::SetUp();
}

// `SetUpOnMainThread` is called just before a test body. Do the mock set up in
// this function as `browser()` is not available in `SetUp` above.
void ScalableIphBrowserTestBase::SetUpOnMainThread() {
  if (kEligileUserSessionTypesForMantaService.contains(
          test_environment().user_session_type()) &&
      !force_disable_manta_service_) {
    ScalableIphFactory::GetInstance()
        ->SetOnBuildingServiceInstanceForTestingCallback(base::BindRepeating(
            &ScalableIphBrowserTestBase::SetCanUseMantaService));
  }

  // `CustomizableTestEnvBrowserTestBase::SetUpOnMainThread` must be called
  // before our `SetUpOnMainThread` as login happens in the method, i.e. profile
  // is not available before it.
  CustomizableTestEnvBrowserTestBase::SetUpOnMainThread();

  // If user session type is `kRegularWithOobe`, Chrome enters post login OOBE
  // screens after a login. It means that there won't be `ScalableIph` as
  // `ScalableIph` starts after post login OOBE screens. We have to wait the
  // initialization of `ScalableIph` before setting up mocks.
  if (test_environment().user_session_type() ==
      CustomizableTestEnvBrowserTestBase::UserSessionType::kRegularWithOobe) {
    return;
  }

  if (enable_multi_user_) {
    // Add a secondary user.
    LoginManagerMixin* login_manager_mixin = GetLoginManagerMixin();
    CHECK(login_manager_mixin);
    login_manager_mixin->AppendRegularUsers(1);
    CHECK_EQ(login_manager_mixin->users().size(), 2ul);

    // By default, `MultiUserWindowManager` is created with multi profile off.
    // Re-create for multi profile tests. This has to be done after
    // `SetUpOnMainThread` of a base class as the original multi-profile-off
    // `MultiUserWindowManager` is created there.
    MultiUserWindowManagerHelper::CreateInstanceForTest(
        GetPrimaryUserContext().GetAccountId());
  }

  // If we don't intend to enforce ScalableIph setup (i.e. the user profile
  // doesn't qualify for ScalableIph), do not set up mocks as ScalableIph
  // should not be available for the profile.
  if (!setup_scalable_iph_) {
    return;
  }

  CHECK(enable_scalable_iph_)
      << "ScalableIph feature flag must be intended to be enabled to set up "
         "fakes and mocks of ScalableIph";

  SetUpMocks();
}

void ScalableIphBrowserTestBase::TearDownOnMainThread() {
  // We are going to release references to mock objects below. Verify the
  // expectations in advance to have a predictable behavior.
  testing::Mock::VerifyAndClearExpectations(mock_tracker_);
  mock_tracker_ = nullptr;
  testing::Mock::VerifyAndClearExpectations(mock_delegate_);
  mock_delegate_ = nullptr;

  InProcessBrowserTest::TearDownOnMainThread();
}

void ScalableIphBrowserTestBase::SetUpMocks() {
  CHECK(!mock_delegate_) << "Mocks have already been set up.";

  // Do not access profile via `browser()` as a browser might not be created if
  // session type is WithOobe.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  CHECK(IsMockDelegateCreatedFor(profile))
      << "ScalableIph service has a timer inside. The service must be created "
         "at a login time. We check the behavior by confirming creation of a "
         "delegate.";

  if (enable_mock_tracker_) {
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(profile));
    CHECK(mock_tracker_)
        << "mock_tracker_ must be non-nullptr. GetForBrowserContext should "
           "create one via CreateMockTracker if it does not exist.";

    ON_CALL(*mock_tracker_, AddOnInitializedCallback)
        .WillByDefault(
            [](feature_engagement::Tracker::OnInitializedCallback callback) {
              std::move(callback).Run(true);
            });

    ON_CALL(*mock_tracker_, IsInitialized).WillByDefault(testing::Return(true));
  }

  // The static cast is necessary to access the delegate functions declared in
  // the `ScalableIphFactoryImpl` class.
  ScalableIphFactoryImpl* scalable_iph_factory =
      static_cast<ScalableIphFactoryImpl*>(ScalableIphFactory::GetInstance());
  CHECK(scalable_iph_factory);
  CHECK(scalable_iph_factory->has_delegate_factory_for_testing())
      << "This test uses MockScalableIphDelegate. A factory for testing must "
         "be set.";
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(profile);
  CHECK(scalable_iph);

  // `ScalableIph` for the profile is initialzied in
  // `CustomizableTestEnvBrowserTestBase::SetUpOnMainThread` above. We cannot
  // simply use `TestMockTimeTaskRunner::ScopedContext` as `RunLoop` is used
  // there and it's not supported by `ScopedContext`. We override a task runner
  // after a timer has created and started.
  task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  scalable_iph->OverrideTaskRunnerForTesting(task_runner());

  mock_delegate_ = static_cast<test::MockScalableIphDelegate*>(
      scalable_iph->delegate_for_testing());
  CHECK(mock_delegate_);
}

void ScalableIphBrowserTestBase::InitializeScopedFeatureList() {
  base::FieldTrialParams params;
  AppendVersionNumber(params);
  AppendUiParams(params);
  base::test::FeatureRefAndParams test_config(kScalableIphTest, params);

  std::vector<base::test::FeatureRefAndParams> enabled_features({test_config});
  std::vector<base::test::FeatureRef> disabled_features;

  AppendTestSpecificFeatures(enabled_features, disabled_features);

  if (enable_scalable_iph_) {
    enabled_features.push_back(
        base::test::FeatureRefAndParams(ash::features::kScalableIph, {}));
  } else {
    disabled_features.push_back(
        base::test::FeatureRef(ash::features::kScalableIph));
  }

  if (enable_scalable_iph_debug_) {
    enabled_features.push_back(
        base::test::FeatureRefAndParams(ash::features::kScalableIphDebug, {}));
  } else {
    disabled_features.push_back(
        base::test::FeatureRef(ash::features::kScalableIphDebug));
  }

  scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                     disabled_features);
}

void ScalableIphBrowserTestBase::AppendUiParams(
    base::FieldTrialParams& params) {
  AppendFakeUiParamsNotification(params, /*has_body_text=*/true,
                                 kScalableIphTest);
}

void ScalableIphBrowserTestBase::AppendVersionNumber(
    base::FieldTrialParams& params,
    const base::Feature& feature,
    const std::string& version_number) {
  params[FullyQualified(feature,
                        scalable_iph::kCustomParamsVersionNumberParamName)] =
      version_number;
}

void ScalableIphBrowserTestBase::AppendVersionNumber(
    base::FieldTrialParams& params,
    const base::Feature& feature) {
  AppendVersionNumber(
      params, feature,
      base::NumberToString(scalable_iph::kCurrentVersionNumber));
}

void ScalableIphBrowserTestBase::AppendVersionNumber(
    base::FieldTrialParams& params) {
  AppendVersionNumber(params, kScalableIphTest);
}

void ScalableIphBrowserTestBase::AppendFakeUiParamsNotification(
    base::FieldTrialParams& params,
    bool has_body_text,
    const base::Feature& feature) {
  params[FullyQualified(feature, scalable_iph::kCustomUiTypeParamName)] =
      scalable_iph::kCustomUiTypeValueNotification;
  params[FullyQualified(feature,
                        scalable_iph::kCustomNotificationIdParamName)] =
      kTestNotificationId;
  params[FullyQualified(feature,
                        scalable_iph::kCustomNotificationTitleParamName)] =
      kTestNotificationTitle;

  if (has_body_text) {
    params[FullyQualified(feature,
                          scalable_iph::kCustomNotificationBodyTextParamName)] =
        kTestNotificationBodyText;
  }

  params[FullyQualified(feature,
                        scalable_iph::kCustomNotificationButtonTextParamName)] =
      kTestNotificationButtonText;
  params[FullyQualified(feature,
                        scalable_iph::kCustomButtonActionTypeParamName)] =
      kTestButtonActionTypeOpenChrome;
  params[FullyQualified(feature,
                        scalable_iph::kCustomButtonActionEventParamName)] =
      kTestActionEventName;
}

void ScalableIphBrowserTestBase::AppendFakeUiParamsBubble(
    base::FieldTrialParams& params) {
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomUiTypeParamName)] =
      scalable_iph::kCustomUiTypeValueBubble;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomBubbleIdParamName)] =
      kTestBubbleId;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomBubbleTitleParamName)] =
      kTestBubbleTitle;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomBubbleTextParamName)] =
      kTestBubbleText;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomBubbleButtonTextParamName)] =
      kTestBubbleButtonText;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomButtonActionTypeParamName)] =
      kTestButtonActionTypeOpenGoogleDocs;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomButtonActionEventParamName)] =
      kTestActionEventName;
  params[FullyQualified(kScalableIphTest,
                        scalable_iph::kCustomBubbleIconParamName)] =
      kTestBubbleIconString;
}

// static
std::string ScalableIphBrowserTestBase::FullyQualified(
    const base::Feature& feature,
    const std::string& param_name) {
  return base::StrCat({feature.name, "_", param_name});
}

bool ScalableIphBrowserTestBase::IsMockDelegateCreatedFor(Profile* profile) {
  return mock_delegate_created_.contains(profile->GetProfileUserName());
}

void ScalableIphBrowserTestBase::EnableTestIphFeatures(
    const std::vector<raw_ptr<const base::Feature, VectorExperimental>>
        test_iph_features) {
  CHECK(mock_delegate_)
      << "To enable a test iph feature, mocks have to be set up.";

  const base::flat_set<const base::Feature*> test_iph_features_set(
      test_iph_features.begin(), test_iph_features.end());
  ON_CALL(*mock_tracker(), ShouldTriggerHelpUI)
      .WillByDefault([test_iph_features_set](const base::Feature& feature) {
        return test_iph_features_set.contains(&feature);
      });

  // Do not access profile via `browser()` as this method can be called before a
  // browser is created.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  // `OverrideFeatureListForTesting` prohibits calling it twice and it has a
  // check. We don't need to do another check for `EnableTestIphFeature` being
  // called twice.
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(profile);
  scalable_iph->OverrideFeatureListForTesting(test_iph_features);
}

void ScalableIphBrowserTestBase::EnableTestIphFeature() {
  EnableTestIphFeatures({&kScalableIphTest});
}

const base::Feature& ScalableIphBrowserTestBase::TestIphFeature() const {
  return kScalableIphTest;
}

void ScalableIphBrowserTestBase::TriggerConditionsCheckWithAFakeEvent(
    scalable_iph::ScalableIph::Event event) {
  // Do not access profile via `browser()` as this method can be called before a
  // browser is created.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(profile);
  scalable_iph->RecordEvent(event);
}

ash::UserContext ScalableIphBrowserTestBase::GetPrimaryUserContext() {
  return ash::LoginManagerMixin::CreateDefaultUserContext(
      GetLoginManagerMixin()->users()[0]);
}

ash::UserContext ScalableIphBrowserTestBase::GetSecondaryUserContext() {
  CHECK(enable_multi_user_);
  return ash::LoginManagerMixin::CreateDefaultUserContext(
      GetLoginManagerMixin()->users()[1]);
}

void ScalableIphBrowserTestBase::ShutdownScalableIph() {
  scalable_iph::ScalableIph* scalable_iph =
      ScalableIphFactory::GetForBrowserContext(browser()->profile());
  CHECK(scalable_iph) << "ScalableIph does not exist for a current profile";

  // `ScalableIph::Shutdown` destructs a delegate. Release the pointer to the
  // mock delegate to avoid having a dangling pointer. We can retain a pointer
  // to the mock tracker as a tracker is not destructed by the
  // `ScalableIph::Shutdown`.
  mock_delegate_ = nullptr;

  scalable_iph->Shutdown();
}

void ScalableIphBrowserTestBase::AddOnlineNetwork() {
  fake_cros_network_config_.AddNetworkAndDevice(
      network_config::CrosNetworkConfigTestHelper::
          CreateStandaloneNetworkProperties(kTestWiFiId, NetworkType::kWiFi,
                                            ConnectionStateType::kOnline,
                                            /*signal_strength=*/0));
}

// static
void ScalableIphBrowserTestBase::SetTestingFactories(
    bool enable_mock_tracker,
    content::BrowserContext* browser_context) {
  if (enable_mock_tracker) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        browser_context,
        base::BindRepeating(&ScalableIphBrowserTestBase::CreateMockTracker));
  }

  // The static cast is necessary to access the delegate functions declared in
  // the `ScalableIphFactoryImpl` class.
  ScalableIphFactoryImpl* scalable_iph_factory =
      static_cast<ScalableIphFactoryImpl*>(ScalableIphFactory::GetInstance());
  CHECK(scalable_iph_factory);

  // This method can be called more than once for a single browser context.
  if (scalable_iph_factory->has_delegate_factory_for_testing()) {
    return;
  }

  // This is NOT a testing factory of a keyed service factory .But the delegate
  // factory is called from the factory of `ScalableIphFactory`. Set this at the
  // same time.
  scalable_iph_factory->SetDelegateFactoryForTesting(
      base::BindRepeating(&ScalableIphBrowserTestBase::CreateMockDelegate));
}

// static
std::unique_ptr<KeyedService> ScalableIphBrowserTestBase::CreateMockTracker(
    content::BrowserContext* browser_context) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

// static
std::unique_ptr<scalable_iph::ScalableIphDelegate>
ScalableIphBrowserTestBase::CreateMockDelegate(Profile* profile,
                                               scalable_iph::Logger* logger) {
  std::pair<std::set<std::string>::iterator, bool> result =
      mock_delegate_created_.insert(profile->GetProfileUserName());
  CHECK(result.second) << "Delegate is created twice for a profile";

  std::unique_ptr<test::MockScalableIphDelegate> delegate =
      std::make_unique<test::MockScalableIphDelegate>();
  delegate->SetDelegate(
      std::make_unique<ScalableIphDelegateImpl>(profile, logger));

  // Fake behaviors of observers must be set at an early stage as those methods
  // are called from constructors, i.e. Set up phases of test fixtures.
  delegate->FakeObservers();

  return delegate;
}

// static
void ScalableIphBrowserTestBase::SetCanUseMantaService(
    content::BrowserContext* browser_context) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  CHECK(identity_manager);

  const user_manager::User* user =
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          browser_context);
  CHECK(user);
  AccountInfo account_info = identity_manager->FindExtendedAccountInfoByGaiaId(
      user->GetAccountId().GetGaiaId());
  AccountCapabilitiesTestMutator test_mutator(&account_info.capabilities);
  test_mutator.set_can_use_manta_service(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

}  // namespace ash
