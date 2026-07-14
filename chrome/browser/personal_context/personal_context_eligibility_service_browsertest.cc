// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/personal_context/core/personal_context_eligibility_service.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/personal_context/personal_context_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_settings/mock_account_setting_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/personal_context/core/country_type.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "components/personal_context/core/personal_context_eligibility_service_impl.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace personal_context {
namespace {
using testing::Return;

constexpr char kAdultUserEmail[] = "adult_user@gmail.com";
constexpr char kUnderagedUserEmail[] = "underaged_user@gmail.com";
constexpr char kCorpUserEmail[] = "corp_user@example.com";

class MockPersonalContextEligibilityServiceObserver
    : public PersonalContextEligibilityService::Observer {
 public:
  MOCK_METHOD(void,
              OnEligibilityStateChanged,
              (PersonalContextEligibilityState),
              (override));
};

std::unique_ptr<KeyedService> BuildMockAccountSettingService(
    content::BrowserContext* /*context*/) {
  auto service = std::make_unique<
      testing::NiceMock<account_settings::MockAccountSettingService>>();
  ON_CALL(*service, GetSyncControllerDelegate()).WillByDefault([]() {
    return std::make_unique<syncer::FakeDataTypeControllerDelegate>(
        syncer::ACCOUNT_SETTING);
  });
  ON_CALL(*service, GetBoolean(testing::_)).WillByDefault(Return(true));
  return service;
}

std::unique_ptr<KeyedService> BuildEligibilityService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<PersonalContextEligibilityServiceImpl>(
      AccountSettingServiceFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
      GeoIpCountryCode("US"), "en-US");
}

class PersonalContextEligibilityServiceImplBrowserTest
    : public InProcessBrowserTest {
 public:
  PersonalContextEligibilityServiceImplBrowserTest() = default;
  ~PersonalContextEligibilityServiceImplBrowserTest() override = default;

  // Configure feature before the main browser process is started
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kPersonalContext},
        /*disabled_features=*/{
            features::debug::kPersonalContextForceEnablementState});
    InProcessBrowserTest::SetUp();
  }

  // Register the service factory callbacks prior to construction of services
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &PersonalContextEligibilityServiceImplBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());

    mock_account_settings_service_ =
        static_cast<account_settings::MockAccountSettingService*>(
            AccountSettingServiceFactory::GetForProfile(GetProfile()));

    pref_service_ = GetProfile()->GetPrefs();
    pref_service_->SetBoolean(
        prefs::kPersonalContextAmbientAutofillNoticeShouldBeShown, false);
    pref_service_->SetBoolean(
        prefs::kPersonalContextInAutofillSettingsToggleStatus, true);

    // Instantiate service locally via factory
    eligibility_service_ = static_cast<PersonalContextEligibilityServiceImpl*>(
        PersonalContextEligibilityServiceFactory::GetForProfile(GetProfile()));
  }

  void TearDownOnMainThread() override {
    eligibility_service_ = nullptr;
    mock_account_settings_service_ = nullptr;
    pref_service_ = nullptr;
    identity_test_env_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    AccountSettingServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockAccountSettingService));
    PersonalContextEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildEligibilityService));
  }

  void SignIn(std::string_view email,
              bool is_underaged = false,
              bool is_managed = false) {
    AccountInfo info = identity_test_env()->MakePrimaryAccountAvailable(
        std::string(email), signin::ConsentLevel::kSignin);
    AccountInfo::Builder builder(info);
    builder.SetHostedDomain(
        is_managed ? "example.com" : signin::constants::kNoHostedDomainFound);

    AccountCapabilities capabilities = info.GetAccountCapabilities();
    AccountCapabilitiesTestMutator mutator(&capabilities);
    mutator.set_can_use_model_execution_features(!is_underaged);
    builder.UpdateAccountCapabilitiesWith(capabilities);

    identity_test_env()->UpdateAccountInfoForAccount(builder.Build());
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<account_settings::MockAccountSettingService>
      mock_account_settings_service_;
  raw_ptr<PersonalContextEligibilityServiceImpl> eligibility_service_;
};

// =============================================================================
// ELIGIBILITY & CONSENT TEST CASES
// =============================================================================

// Verify underaged age capability check gates feature access
IN_PROC_BROWSER_TEST_F(PersonalContextEligibilityServiceImplBrowserTest,
                       ConsentAgeGateDisablesService) {
  SignIn(kUnderagedUserEmail, /*is_underaged=*/true);

  EXPECT_EQ(eligibility_service_->GetEligibilityState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
}

// Ensure managed enterprise accounts disable personal context features
IN_PROC_BROWSER_TEST_F(PersonalContextEligibilityServiceImplBrowserTest,
                       ConsentManagedAccountDisablesService) {
  SignIn(kCorpUserEmail, /*is_underaged=*/false, /*is_managed=*/true);

  EXPECT_EQ(eligibility_service_->GetEligibilityState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
}

// Verify photos and workspace opt-outs disable service status
IN_PROC_BROWSER_TEST_F(PersonalContextEligibilityServiceImplBrowserTest,
                       ConsentCloudPreferencesDeactivate) {
  SignIn(kAdultUserEmail);
  EXPECT_EQ(eligibility_service_->GetEligibilityState(),
            PersonalContextEligibilityState::kEligible);

  // Simulate preferences opt-out
  EXPECT_CALL(*mock_account_settings_service_, GetBoolean(testing::_))
      .WillRepeatedly(Return(false));

  // Enablement check should update after change events
  eligibility_service_->OnAccountSettingDataUpdated("any_setting");

  EXPECT_EQ(eligibility_service_->GetEligibilityState(),
            PersonalContextEligibilityState::kDisabledNotEligible);
}

// =============================================================================
// OBSERVATION MECHANICS TEST CASES
// =============================================================================

// Confirm state observers trigger notifications upon settings shifts
IN_PROC_BROWSER_TEST_F(PersonalContextEligibilityServiceImplBrowserTest,
                       ObserverStateChangeObserverFires) {
  SignIn(kAdultUserEmail);

  testing::StrictMock<MockPersonalContextEligibilityServiceObserver> observer;
  eligibility_service_->AddObserver(&observer);

  // Toggling settings should fire state update notification to the observer
  EXPECT_CALL(observer,
              OnEligibilityStateChanged(
                  PersonalContextEligibilityState::kDisabledNotEligible))
      .Times(1);

  // We simulate preferences opt-out
  EXPECT_CALL(*mock_account_settings_service_, GetBoolean(testing::_))
      .WillRepeatedly(Return(false));

  eligibility_service_->OnAccountSettingDataUpdated("any_setting");

  eligibility_service_->RemoveObserver(&observer);
}
}  // namespace
}  // namespace personal_context
