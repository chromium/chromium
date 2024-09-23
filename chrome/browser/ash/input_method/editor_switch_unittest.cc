// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_switch.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "net/base/mock_network_change_notifier.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

using ::testing::ElementsAreArray;
using ::testing::TestWithParam;

const char kAllowedTestCountry[] = "au";
const char kDeniedTestCountry[] = "hk";
const char kUsEngineId[] = "xkb:us::eng";

const char kAllowedTestUrl[] = "https://allowed.testurl.com/allowed/path";

TextFieldContextualInfo CreateFakeTextFieldContextualInfo(
    chromeos::AppType app_type,
    std::string_view url,
    std::string_view app_key) {
  auto text_field_contextual_info = TextFieldContextualInfo();
  text_field_contextual_info.app_type = app_type;
  text_field_contextual_info.tab_url = GURL(url);
  text_field_contextual_info.app_key = app_key;

  return text_field_contextual_info;
}

std::unique_ptr<TestingProfile> CreateTestingProfile(std::string email) {
  std::unique_ptr<TestingProfile> profile = TestingProfile::Builder().Build();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile.get());

  signin::MakePrimaryAccountAvailable(identity_manager, email,
                                      signin::ConsentLevel::kSync);
  return profile;
}

class FakeEditorContextObserver : public EditorContext::Observer {
 public:
  // EditorContext::Observer overrides
  void OnContextUpdated() override {}
  void OnImeChange(std::string_view engine_id) override {}
};

class FakeSystem : public EditorContext::System {
 public:
  FakeSystem() = default;
  ~FakeSystem() override = default;

  // EditorContext::System overrides
  std::optional<ukm::SourceId> GetUkmSourceId() override {
    return std::nullopt;
  }
};

class FakeEditorSwitchObserver : public EditorSwitch::Observer {
 public:
  // EditorSwitch::Observer overrides
  void OnEditorModeChanged(const EditorMode& mode) override {}
};

struct EditorSwitchAvailabilityWithoutPolicyTestCase {
  std::string test_name;

  std::vector<base::test::FeatureRef> enabled_flags;
  std::vector<base::test::FeatureRef> disabled_flags;

  std::string country_code;

  bool expected_availability;
};

using EditorSwitchAvailabilityWithoutPolicyTest =
    TestWithParam<EditorSwitchAvailabilityWithoutPolicyTestCase>;

// TODO: b:329215512: Remove the OrcaUseAccountCapabilities from the disable
// lists of all test cases.
INSTANTIATE_TEST_SUITE_P(
    EditorSwitchAvailabilityWithoutPolicyTests,
    EditorSwitchAvailabilityWithoutPolicyTest,
    testing::ValuesIn<EditorSwitchAvailabilityWithoutPolicyTestCase>({
        {.test_name = "FeatureNotAvailableForUseWithoutReceivingOrcaFlag",
         .enabled_flags = {},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .expected_availability = false},
        {.test_name = "FeatureNotAvailableInACountryNotApprovedYet",
         .enabled_flags = {chromeos::features::kOrca,
                           chromeos::features::kFeatureManagementOrca},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kDeniedTestCountry,
         .expected_availability = false},
        {.test_name = "FeatureNotAvailableWithoutFeatureManagementFlag",
         .enabled_flags = {chromeos::features::kOrca},
         .disabled_flags = {chromeos::features::kFeatureManagementOrca,
                            ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .expected_availability = false},
        {.test_name = "FeatureAvailableWhenReceivingDogfoodFlag",
         .enabled_flags = {chromeos::features::kOrcaDogfood},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .expected_availability = true},
        {.test_name = "FeatureAvailableInApprovedCountryWithFe"
                      "atureManagementFlag",
         .enabled_flags = {chromeos::features::kOrca,
                           chromeos::features::kFeatureManagementOrca},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .expected_availability = true},
    }),
    [](const testing::TestParamInfo<
        EditorSwitchAvailabilityWithoutPolicyTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(EditorSwitchAvailabilityWithoutPolicyTest,
       TestEditorAvailabilityWhenNoPolicyIsApplied) {
  const EditorSwitchAvailabilityWithoutPolicyTestCase& test_case = GetParam();
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/test_case.enabled_flags,
                                /*disabled_features=*/test_case.disabled_flags);

  TestingProfile profile;
  FakeSystem system;
  FakeEditorContextObserver context_observer;
  FakeEditorSwitchObserver switch_observer;
  EditorGeolocationMockProvider geolocation_provider(test_case.country_code);
  EditorContext context(&context_observer, &system, &geolocation_provider);
  EditorSwitch editor_switch(/*observer=*/&switch_observer,
                             /*profile=*/&profile,
                             /*context=*/&context);

  EXPECT_EQ(editor_switch.IsAllowedForUse(), test_case.expected_availability);
}

struct EditorSwitchAvailabilityWithPolicyTestCase {
  std::string test_name;

  std::vector<base::test::FeatureRef> enabled_flags;
  std::vector<base::test::FeatureRef> disabled_flags;

  std::string country_code;
  bool enabled_by_policy;

  bool expected_availability;
};

using EditorSwitchAvailabilityWithPolicyTest =
    TestWithParam<EditorSwitchAvailabilityWithPolicyTestCase>;

// TODO: b:329215512: Remove the OrcaUseAccountCapabilities from the disable
// lists of all test cases.
INSTANTIATE_TEST_SUITE_P(
    EditorSwitchAvailabilityWithPolicyTests,
    EditorSwitchAvailabilityWithPolicyTest,
    testing::ValuesIn<EditorSwitchAvailabilityWithPolicyTestCase>(
        {{.test_name = "FeatureAvailableIfAllowedByPolicy",
          .enabled_flags = {chromeos::features::kOrca,
                            chromeos::features::kFeatureManagementOrca},
          .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
          .country_code = kAllowedTestCountry,
          .enabled_by_policy = true,
          .expected_availability = true},
         {.test_name = "FeatureAvailableEvenIfPolicyValueIsDisabled",
          .enabled_flags = {chromeos::features::kOrca,
                            chromeos::features::kFeatureManagementOrca},
          .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
          .country_code = kAllowedTestCountry,
          .enabled_by_policy = false,
          .expected_availability = true},
         {.test_name = "FeatureUnavailableIfOrcaForManagedUsersFlagIsDisabled",
          .enabled_flags = {chromeos::features::kOrca,
                            chromeos::features::kFeatureManagementOrca},
          .disabled_flags = {ash::features::kOrcaUseAccountCapabilities,
                             ash::features::kOrcaForManagedUsers},
          .country_code = kAllowedTestCountry,
          .enabled_by_policy = true,
          .expected_availability = false}}),
    [](const testing::TestParamInfo<
        EditorSwitchAvailabilityWithPolicyTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(EditorSwitchAvailabilityWithPolicyTest,
       TestEditorAvailabilityWhenPolicyIsApplied) {
  const EditorSwitchAvailabilityWithPolicyTestCase& test_case = GetParam();
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/test_case.enabled_flags,
                                /*disabled_features=*/test_case.disabled_flags);

  TestingProfile profile;
  profile.GetTestingPrefService()->SetManagedPref(
      prefs::kOrcaEnabled, base::Value(test_case.enabled_by_policy));
  FakeSystem system;
  FakeEditorContextObserver context_observer;
  FakeEditorSwitchObserver switch_observer;
  EditorGeolocationMockProvider geolocation_provider(test_case.country_code);
  EditorContext context(&context_observer, &system, &geolocation_provider);
  EditorSwitch editor_switch(/*observer=*/&switch_observer,
                             /*profile=*/&profile,
                             /*context=*/&context);

  EXPECT_EQ(editor_switch.IsAllowedForUse(), test_case.expected_availability);
}

struct EditorSwitchTriggerTestCase {
  std::string test_name;

  std::vector<base::test::FeatureRef> additional_enabled_flags;
  std::string email;

  std::string active_engine_id;
  std::string locale;
  std::string url;
  std::string app_id;
  ui::TextInputType input_type;
  chromeos::AppType app_type;
  bool is_in_tablet_mode;
  net::NetworkChangeNotifier::ConnectionType network_status;
  bool user_pref;
  ConsentStatus consent_status;
  size_t num_chars_selected;

  EditorMode expected_editor_mode;
  EditorOpportunityMode expected_editor_opportunity_mode;
  std::vector<EditorBlockedReason> expected_blocked_reasons;
};

using EditorSwitchTriggerTest = TestWithParam<EditorSwitchTriggerTestCase>;

INSTANTIATE_TEST_SUITE_P(
    EditorSwitchTriggerTests,
    EditorSwitchTriggerTest,
    testing::ValuesIn<EditorSwitchTriggerTestCase>({
        {
            .test_name = "DoNotTriggerFeatureIfConsentDeclined",
            .additional_enabled_flags = {},
            .email = "testuser@gmail.com",
            .active_engine_id = "xkb:us::eng",
            .locale = "en-us",
            .url = kAllowedTestUrl,
            .input_type = ui::TEXT_INPUT_TYPE_TEXT,
            .app_type = chromeos::AppType::BROWSER,
            .is_in_tablet_mode = false,
            .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            .user_pref = true,
            .consent_status = ConsentStatus::kDeclined,
            .num_chars_selected = 0,
            .expected_editor_mode = EditorMode::kSoftBlocked,
            .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
            .expected_blocked_reasons =
                {EditorBlockedReason::kBlockedByConsent},
        },
        {
            .test_name = "DoNotTriggerFeatureOnAPasswordField",
            .additional_enabled_flags = {},
            .email = "testuser@gmail.com",
            .active_engine_id = "xkb:us::eng",
            .locale = "en-us",
            .url = kAllowedTestUrl,
            .input_type = ui::TEXT_INPUT_TYPE_PASSWORD,
            .app_type = chromeos::AppType::BROWSER,
            .is_in_tablet_mode = false,
            .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            .user_pref = true,
            .consent_status = ConsentStatus::kApproved,
            .num_chars_selected = 0,
            .expected_editor_mode = EditorMode::kSoftBlocked,
            .expected_editor_opportunity_mode =
                EditorOpportunityMode::kInvalidInput,
            .expected_blocked_reasons =
                {EditorBlockedReason::kBlockedByInputType},
        },
        {.test_name = "TriggersFeatureOnWorkspaceForNonGooglerAccount",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "https://mail.google.com/mail",
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kWrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {}},
        {.test_name = "TriggerFeatureOnWorkspaceForGooglerAccount",
         .additional_enabled_flags = {},
         .email = "testuser@google.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "https://mail.google.com/mail",
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kWrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {}},
        {.test_name = "TriggerFeatureOnDemoWorkspaceAppsForNonGooglerAccount",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "",
         .app_id = extension_misc::kGoogleDocsDemoAppId,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kWrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {}},
        {.test_name = "DoNotTriggerFeatureWithNonSupportedInputMethod",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         // Greek keyboard
         .active_engine_id = "xkb:gr::gre",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kSoftBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons =
             {EditorBlockedReason::kBlockedByInputMethod}},
        {.test_name = "TriggersFeatureOnArcApps",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::ARC_APP,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kWrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {}},
        {.test_name = "DoNotTriggerFeatureIfSettingToggleIsOff",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = false,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kSoftBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {EditorBlockedReason::kBlockedBySetting}},
        {.test_name = "DoNotTriggerFeatureOnTabletMode",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = true,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kSoftBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons =
             {EditorBlockedReason::kBlockedByInvalidFormFactor}},
        {.test_name = "DoNotTriggerFeatureWhenOffline",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_NONE,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kSoftBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons =
             {EditorBlockedReason::kBlockedByNetworkStatus}},
        {.test_name = "DoNotTriggerFeatureWhenSelectingTooLongText",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 10001,
         .expected_editor_mode = EditorMode::kSoftBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kRewrite,
         .expected_blocked_reasons =
             {EditorBlockedReason::kBlockedByTextLength}},
        {.test_name =
             "TriggersConsentIfSettingToggleIsOnAndUserHasNotGivenConsent",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kPending,
         .num_chars_selected = 100,
         .expected_editor_mode = EditorMode::kConsentNeeded,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kRewrite,
         .expected_blocked_reasons = {}},
        {.test_name = "TriggersWriteModeForNoTextSelection",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kWrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {}},
        {.test_name = "TriggersRewriteModeWhenSomeTextIsSelected",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 100,
         .expected_editor_mode = EditorMode::kRewrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kRewrite,
         .expected_blocked_reasons = {}},
        {.test_name = "TriggersRewriteModeWhenOrcaRestrictedInEnglishLocalesFla"
                      "gIsSetAsDefault",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "fr",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 100,
         .expected_editor_mode = EditorMode::kRewrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kRewrite,
         .expected_blocked_reasons = {}},
        {.test_name = "DoNotTriggerFeatureInEnUsLocaleWhenOrcaOnlyInEnglishLoca"
                      "lesFlagIsEnabled",
         .additional_enabled_flags = {features::kOrcaOnlyInEnglishLocales},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 100,
         .expected_editor_mode = EditorMode::kRewrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kRewrite,
         .expected_blocked_reasons = {}},
        {.test_name = "DoNotTriggerFeatureInFrenchLocaleWhenOrcaOnlyInEnglishLo"
                      "calesFlagIsEnabled",
         .additional_enabled_flags = {features::kOrcaOnlyInEnglishLocales},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "fr",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = chromeos::AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 100,
         .expected_editor_mode = EditorMode::kSoftBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kRewrite,
         .expected_blocked_reasons = {}},
    }),
    [](const testing::TestParamInfo<EditorSwitchTriggerTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(EditorSwitchTriggerTest, TestEditorMode) {
  const EditorSwitchTriggerTestCase& test_case = GetParam();
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRef> base_enabled_features = {
      chromeos::features::kOrca, chromeos::features::kFeatureManagementOrca};
  base_enabled_features.insert(base_enabled_features.end(),
                               test_case.additional_enabled_flags.begin(),
                               test_case.additional_enabled_flags.end());
  // TODO: b:329215512: Remove the OrcaUseAccountCapabilities from the disable
  // list.
  feature_list.InitWithFeatures(
      /*enabled_features=*/base_enabled_features,
      /*disabled_features=*/{ash::features::kOrcaUseAccountCapabilities});
  ScopedBrowserLocale browser_locale(test_case.locale);

  std::unique_ptr<TestingProfile> profile =
      CreateTestingProfile(test_case.email);
  FakeSystem system;
  FakeEditorContextObserver context_observer;
  FakeEditorSwitchObserver switch_observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedTestCountry);
  EditorContext context(&context_observer, &system, &geolocation_provider);
  EditorSwitch editor_switch(/*observer=*/&switch_observer,
                             /*profile=*/profile.get(),
                             /*context=*/&context);

  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);

  mock_notifier->SetConnectionType(test_case.network_status);

  profile->GetPrefs()->SetBoolean(prefs::kOrcaEnabled, test_case.user_pref);
  profile->GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(test_case.consent_status));
  context.OnTabletModeUpdated(test_case.is_in_tablet_mode);
  context.OnActivateIme(test_case.active_engine_id);
  context.OnInputContextUpdated(
      TextInputMethod::InputContext(test_case.input_type),
      CreateFakeTextFieldContextualInfo(test_case.app_type, test_case.url,
                                        test_case.app_id));
  context.OnTextSelectionLengthChanged(test_case.num_chars_selected);

  ASSERT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), test_case.expected_editor_mode);
  EXPECT_EQ(editor_switch.GetEditorOpportunityMode(),
            test_case.expected_editor_opportunity_mode);

  EXPECT_THAT(editor_switch.GetBlockedReasons(),
              testing::ElementsAreArray(test_case.expected_blocked_reasons));
}

using DenylistTestCase = std::pair<std::string, EditorMode>;

using EditorSwitchDenylistTest = TestWithParam<DenylistTestCase>;

INSTANTIATE_TEST_SUITE_P(
    EditorSwitchDenylist,
    EditorSwitchDenylistTest,
    testing::ValuesIn<DenylistTestCase>({
        {"https://calendar.google.com", EditorMode::kSoftBlocked},
        {"https://calendar.google.com/c/1234", EditorMode::kSoftBlocked},
        {"https://docs.google.com", EditorMode::kSoftBlocked},
        {"https://docs.google.com/drawings/1234", EditorMode::kSoftBlocked},
        {"https://docs.google.com/document/1234", EditorMode::kSoftBlocked},
        {"https://docs.google.com/forms/1234", EditorMode::kSoftBlocked},
        {"https://docs.google.com/presentation/1234", EditorMode::kSoftBlocked},
        {"https://docs.google.com/spreadsheet/1234", EditorMode::kSoftBlocked},
        {"https://docs.google.com/videos/1234", EditorMode::kSoftBlocked},
        {"https://drive.google.com", EditorMode::kSoftBlocked},
        {"https://drive.google.com/1234", EditorMode::kSoftBlocked},
        {"https://keep.google.com", EditorMode::kSoftBlocked},
        {"https://keep.google.com/1234", EditorMode::kSoftBlocked},
        {"https://mail.google.com/chat", EditorMode::kSoftBlocked},
        {"https://mail.google.com/mail", EditorMode::kSoftBlocked},
        {"https://meet.google.com", EditorMode::kSoftBlocked},
        {"https://meet.google.com/1234", EditorMode::kSoftBlocked},
        {"https://script.google.com", EditorMode::kSoftBlocked},
        {"https://script.google.com/1234", EditorMode::kSoftBlocked},
        {"https://sites.google.com", EditorMode::kSoftBlocked},
        {"https://sites.google.com/view/test-page", EditorMode::kSoftBlocked},
        {"https://sites.google.com/1234", EditorMode::kSoftBlocked},
        {"https://outlook.com", EditorMode::kRewrite},
        {"https://whatsapp.com", EditorMode::kRewrite},
        {"https://x.com", EditorMode::kRewrite},
        {"https://linkedin.com", EditorMode::kRewrite},
    }));

TEST_P(EditorSwitchDenylistTest, IsBlockedWhenVisitingUrlInDenylist) {
  const DenylistTestCase& test_case = GetParam();
  const std::string& test_url = std::get<0>(test_case);
  const EditorMode& expected_mode = std::get<1>(test_case);
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            chromeos::features::kFeatureManagementOrca,
                            chromeos::features::kOrcaInternationalize},
      /*disabled_features=*/{ash::features::kOrcaUseAccountCapabilities,
                             ash::features::kOrcaOnWorkspace});
  ScopedBrowserLocale browser_locale("en");

  std::unique_ptr<TestingProfile> profile =
      CreateTestingProfile("testuser@gmail.com");
  FakeSystem system;
  FakeEditorContextObserver context_observer;
  FakeEditorSwitchObserver switch_observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedTestCountry);
  EditorContext context(&context_observer, &system, &geolocation_provider);
  EditorSwitch editor_switch(/*observer=*/&switch_observer,
                             /*profile=*/profile.get(),
                             /*context=*/&context);

  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  mock_notifier->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  profile->GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile->GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  context.OnTabletModeUpdated(false);
  context.OnActivateIme(kUsEngineId);
  context.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(chromeos::AppType::BROWSER, test_url,
                                        ""));
  context.OnTextSelectionLengthChanged(10);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), expected_mode);
}

using InputMethodTestCase = std::pair<std::string, EditorMode>;

class InternationalizeTestSuite : public TestWithParam<InputMethodTestCase> {
 public:
  InternationalizeTestSuite()
      : browser_locale_("en"),
        profile_(CreateTestingProfile("testuser@gmail.com")),
        geolocation_provider_(kAllowedTestCountry),
        context_(&context_observer_, &system_, &geolocation_provider_) {
    auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
    profile_->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
    mock_notifier->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_WIFI);

    profile_->GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
    profile_->GetPrefs()->SetInteger(
        prefs::kOrcaConsentStatus,
        base::to_underlying(ConsentStatus::kApproved));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ScopedBrowserLocale browser_locale_;
  std::unique_ptr<TestingProfile> profile_;
  FakeSystem system_;
  FakeEditorContextObserver context_observer_;
  FakeEditorSwitchObserver switch_observer_;
  EditorGeolocationMockProvider geolocation_provider_;
  EditorContext context_;
};

class EditorSwitchDefaultFlagsTest : public InternationalizeTestSuite {};

INSTANTIATE_TEST_SUITE_P(EditorSwitchDefaultFlags,
                         EditorSwitchDefaultFlagsTest,
                         testing::ValuesIn<InputMethodTestCase>({
                             // English
                             {"xkb:ca:eng:eng", EditorMode::kWrite},
                             {"xkb:gb::eng", EditorMode::kWrite},
                             {"xkb:gb:extd:eng", EditorMode::kWrite},
                             {"xkb:gb:dvorak:eng", EditorMode::kWrite},
                             {"xkb:in::eng", EditorMode::kWrite},
                             {"xkb:pk::eng", EditorMode::kWrite},
                             {"xkb:us:altgr-intl:eng", EditorMode::kWrite},
                             {"xkb:us:colemak:eng", EditorMode::kWrite},
                             {"xkb:us:dvorak:eng", EditorMode::kWrite},
                             {"xkb:us:dvp:eng", EditorMode::kWrite},
                             {"xkb:us:intl_pc:eng", EditorMode::kWrite},
                             {"xkb:us:intl:eng", EditorMode::kWrite},
                             {"xkb:us:workman-intl:eng", EditorMode::kWrite},
                             {"xkb:us:workman:eng", EditorMode::kWrite},
                             {"xkb:us::eng", EditorMode::kWrite},
                             {"xkb:za:gb:eng", EditorMode::kWrite},
                             // French
                             {"xkb:be::fra", EditorMode::kWrite},
                             {"xkb:ca::fra", EditorMode::kWrite},
                             {"xkb:ca:multix:fra", EditorMode::kWrite},
                             {"xkb:fr::fra", EditorMode::kWrite},
                             {"xkb:fr:bepo:fra", EditorMode::kWrite},
                             {"xkb:ch:fr:fra", EditorMode::kWrite},
                             // German
                             {"xkb:be::ger", EditorMode::kWrite},
                             {"xkb:de::ger", EditorMode::kWrite},
                             {"xkb:de:neo:ger", EditorMode::kWrite},
                             {"xkb:ch::ger", EditorMode::kWrite},
                             // Japanese
                             {"xkb:jp::jpn", EditorMode::kWrite},
                             {"nacl_mozc_us", EditorMode::kWrite},
                             {"nacl_mozc_jp", EditorMode::kWrite},
                             // Danish
                             {"xkb:dk::dan", EditorMode::kWrite},
                             // Dutch
                             {"xkb:be::nld", EditorMode::kWrite},
                             {"xkb:us:intl_pc:nld", EditorMode::kWrite},
                             {"xkb:us:intl:nld", EditorMode::kWrite},
                             // Finnish
                             {"xkb:fi::fin", EditorMode::kWrite},
                             // Italian
                             {"xkb:it::ita", EditorMode::kWrite},
                             // Norwegian
                             {"xkb:no::nob", EditorMode::kWrite},
                             // Polish
                             {"xkb:pl::pol", EditorMode::kWrite},
                             // Portugese
                             {"xkb:br::por", EditorMode::kWrite},
                             {"xkb:pt::por", EditorMode::kWrite},
                             {"xkb:us:intl_pc:por", EditorMode::kWrite},
                             {"xkb:us:intl:por", EditorMode::kWrite},
                             // Spanish
                             {"xkb:latam::spa", EditorMode::kWrite},
                             {"xkb:es::spa", EditorMode::kWrite},
                             // Swedish
                             {"xkb:se::swe", EditorMode::kWrite},
                             // Turkish (example case where always disabled)
                             {"xkb:tr::tur", EditorMode::kSoftBlocked},
                             {"xkb:tr:f:tur", EditorMode::kSoftBlocked},
                         }));

TEST_P(EditorSwitchDefaultFlagsTest, EditorModeHasCorrectState) {
  const InputMethodTestCase& test_case = GetParam();
  const std::string& engine_id = std::get<0>(test_case);
  const EditorMode& expected_mode = std::get<1>(test_case);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            chromeos::features::kFeatureManagementOrca},
      /*disabled_features=*/{ash::features::kOrcaUseAccountCapabilities});

  EditorSwitch editor_switch(&switch_observer_, profile_.get(), &context_);
  context_.OnTabletModeUpdated(false);
  context_.OnActivateIme(engine_id);
  context_.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(chromeos::AppType::BROWSER,
                                        kAllowedTestUrl, ""));
  context_.OnTextSelectionLengthChanged(0);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), expected_mode);
}

class EditorSwitchAllFlagsEnabledTest : public InternationalizeTestSuite {};

INSTANTIATE_TEST_SUITE_P(EditorSwitchAllFlagsEnabled,
                         EditorSwitchAllFlagsEnabledTest,
                         testing::ValuesIn<InputMethodTestCase>({
                             // English
                             {"xkb:ca:eng:eng", EditorMode::kWrite},
                             {"xkb:gb::eng", EditorMode::kWrite},
                             {"xkb:gb:extd:eng", EditorMode::kWrite},
                             {"xkb:gb:dvorak:eng", EditorMode::kWrite},
                             {"xkb:in::eng", EditorMode::kWrite},
                             {"xkb:pk::eng", EditorMode::kWrite},
                             {"xkb:us:altgr-intl:eng", EditorMode::kWrite},
                             {"xkb:us:colemak:eng", EditorMode::kWrite},
                             {"xkb:us:dvorak:eng", EditorMode::kWrite},
                             {"xkb:us:dvp:eng", EditorMode::kWrite},
                             {"xkb:us:intl_pc:eng", EditorMode::kWrite},
                             {"xkb:us:intl:eng", EditorMode::kWrite},
                             {"xkb:us:workman-intl:eng", EditorMode::kWrite},
                             {"xkb:us:workman:eng", EditorMode::kWrite},
                             {"xkb:us::eng", EditorMode::kWrite},
                             {"xkb:za:gb:eng", EditorMode::kWrite},
                             // French
                             {"xkb:be::fra", EditorMode::kWrite},
                             {"xkb:ca::fra", EditorMode::kWrite},
                             {"xkb:ca:multix:fra", EditorMode::kWrite},
                             {"xkb:fr::fra", EditorMode::kWrite},
                             {"xkb:fr:bepo:fra", EditorMode::kWrite},
                             {"xkb:ch:fr:fra", EditorMode::kWrite},
                             // German
                             {"xkb:be::ger", EditorMode::kWrite},
                             {"xkb:de::ger", EditorMode::kWrite},
                             {"xkb:de:neo:ger", EditorMode::kWrite},
                             {"xkb:ch::ger", EditorMode::kWrite},
                             // Japanese
                             {"xkb:jp::jpn", EditorMode::kWrite},
                             {"nacl_mozc_us", EditorMode::kWrite},
                             {"nacl_mozc_jp", EditorMode::kWrite},
                             // Danish
                             {"xkb:dk::dan", EditorMode::kWrite},
                             // Dutch
                             {"xkb:be::nld", EditorMode::kWrite},
                             {"xkb:us:intl_pc:nld", EditorMode::kWrite},
                             {"xkb:us:intl:nld", EditorMode::kWrite},
                             // Finnish
                             {"xkb:fi::fin", EditorMode::kWrite},
                             // Italian
                             {"xkb:it::ita", EditorMode::kWrite},
                             // Norwegian
                             {"xkb:no::nob", EditorMode::kWrite},
                             // Polish
                             {"xkb:pl::pol", EditorMode::kWrite},
                             // Portugese
                             {"xkb:br::por", EditorMode::kWrite},
                             {"xkb:pt::por", EditorMode::kWrite},
                             {"xkb:us:intl_pc:por", EditorMode::kWrite},
                             {"xkb:us:intl:por", EditorMode::kWrite},
                             // Spanish
                             {"xkb:latam::spa", EditorMode::kWrite},
                             {"xkb:es::spa", EditorMode::kWrite},
                             // Swedish
                             {"xkb:se::swe", EditorMode::kWrite},
                             // Turkish (example case where always disabled)
                             {"xkb:tr::tur", EditorMode::kSoftBlocked},
                             {"xkb:tr:f:tur", EditorMode::kSoftBlocked},
                         }));

TEST_P(EditorSwitchAllFlagsEnabledTest, EditorModeHasCorrectState) {
  const InputMethodTestCase& test_case = GetParam();
  const std::string& engine_id = std::get<0>(test_case);
  const EditorMode& expected_mode = std::get<1>(test_case);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            chromeos::features::kFeatureManagementOrca,
                            features::kOrcaDanish, features::kOrcaDutch,
                            features::kOrcaFinnish, features::kOrcaItalian,
                            features::kOrcaNorwegian, features::kOrcaPortugese,
                            features::kOrcaSpanish, features::kOrcaSwedish},
      /*disabled_features=*/{ash::features::kOrcaUseAccountCapabilities});

  EditorSwitch editor_switch(&switch_observer_, profile_.get(), &context_);
  context_.OnTabletModeUpdated(false);
  context_.OnActivateIme(engine_id);
  context_.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(chromeos::AppType::BROWSER,
                                        kAllowedTestUrl, ""));
  context_.OnTextSelectionLengthChanged(0);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), expected_mode);
}

}  // namespace
}  // namespace ash::input_method
