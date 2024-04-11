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
#include "chrome/browser/ash/input_method/editor_identity_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

using ::testing::ElementsAreArray;
using ::testing::TestWithParam;

const char kAllowedTestCountry[] = "au";
const char kDeniedTestCountry[] = "br";

const char kAllowedTestUrl[] = "https://allowed.testurl.com/allowed/path";

class FakeEditorSwitchDelegate : public EditorSwitch::Delegate {
 public:
  // EditorSwitch::Delegate overrides
  void OnEditorModeChanged(const EditorMode& mode) override {}
};

struct EditorSwitchAvailabilityTestCase {
  std::string test_name;

  std::vector<base::test::FeatureRef> enabled_flags;
  std::vector<base::test::FeatureRef> disabled_flags;

  std::string country_code;
  bool is_managed;

  bool expected_availability;
};

struct EditorSwitchTriggerTestCase {
  std::string test_name;

  std::vector<base::test::FeatureRef> additional_enabled_flags;
  std::string email;

  std::string active_engine_id;
  std::string locale;
  std::string url;
  std::string app_id;
  ui::TextInputType input_type;
  ash::AppType app_type;
  bool is_in_tablet_mode;
  net::NetworkChangeNotifier::ConnectionType network_status;
  bool user_pref;
  ConsentStatus consent_status;
  size_t num_chars_selected;

  EditorMode expected_editor_mode;
  EditorOpportunityMode expected_editor_opportunity_mode;
  std::vector<EditorBlockedReason> expected_blocked_reasons;
};

using EditorSwitchAvailabilityTest =
    TestWithParam<EditorSwitchAvailabilityTestCase>;

using EditorSwitchTriggerTest = TestWithParam<EditorSwitchTriggerTestCase>;

TextFieldContextualInfo CreateFakeTextFieldContextualInfo(
    ash::AppType app_type,
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

// TODO: b:329215512: Remove the OrcaUseAccountCapabilities from the disable
// lists of all test cases.
INSTANTIATE_TEST_SUITE_P(
    EditorSwitchAvailabilityTests,
    EditorSwitchAvailabilityTest,
    testing::ValuesIn<EditorSwitchAvailabilityTestCase>({
        {.test_name = "FeatureNotAvailableForUseWithoutReceivingOrcaFlag",
         .enabled_flags = {},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .is_managed = false,
         .expected_availability = false},
        {.test_name = "FeatureNotAvailableForManagedAccountOnNonDogfoodDevices",
         .enabled_flags = {chromeos::features::kOrca,
                           chromeos::features::kFeatureManagementOrca},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .is_managed = true,
         .expected_availability = false},
        {.test_name = "FeatureNotAvailableInACountryNotApprovedYet",
         .enabled_flags = {chromeos::features::kOrca,
                           chromeos::features::kFeatureManagementOrca},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kDeniedTestCountry,
         .is_managed = false,
         .expected_availability = false},
        {.test_name = "FeatureNotAvailableWithoutFeatureManagementFlag",
         .enabled_flags = {chromeos::features::kOrca},
         .disabled_flags = {chromeos::features::kFeatureManagementOrca,
                            ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .is_managed = false,
         .expected_availability = false},
        {.test_name = "FeatureAvailableWhenReceivingDogfoodFlag",
         .enabled_flags = {chromeos::features::kOrcaDogfood},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .is_managed = true,
         .expected_availability = true},
        {.test_name = "FeatureAvailableOnUnmanagedDeviceInApprovedCountryWithFe"
                      "atureManagementFlag",
         .enabled_flags = {chromeos::features::kOrca,
                           chromeos::features::kFeatureManagementOrca},
         .disabled_flags = {ash::features::kOrcaUseAccountCapabilities},
         .country_code = kAllowedTestCountry,
         .is_managed = false,
         .expected_availability = true},
    }),
    [](const testing::TestParamInfo<EditorSwitchAvailabilityTest::ParamType>&
           info) { return info.param.test_name; });

TEST_P(EditorSwitchAvailabilityTest, TestEditorAvailability) {
  const EditorSwitchAvailabilityTestCase& test_case = GetParam();
  content::BrowserTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/test_case.enabled_flags,
                                /*disabled_features=*/test_case.disabled_flags);

  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      test_case.is_managed);
  FakeEditorSwitchDelegate delegate;
  EditorSwitch editor_switch(/*delegate=*/&delegate,
                             /*profile=*/&profile_,
                             /*country_code=*/test_case.country_code);

  EXPECT_EQ(editor_switch.IsAllowedForUse(), test_case.expected_availability);
}

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
            .app_type = AppType::BROWSER,
            .is_in_tablet_mode = false,
            .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            .user_pref = true,
            .consent_status = ConsentStatus::kDeclined,
            .num_chars_selected = 0,
            .expected_editor_mode = EditorMode::kBlocked,
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
            .app_type = AppType::BROWSER,
            .is_in_tablet_mode = false,
            .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
            .user_pref = true,
            .consent_status = ConsentStatus::kApproved,
            .num_chars_selected = 0,
            .expected_editor_mode = EditorMode::kBlocked,
            .expected_editor_opportunity_mode = EditorOpportunityMode::kNone,
            .expected_blocked_reasons =
                {EditorBlockedReason::kBlockedByInputType},
        },
        {.test_name = "DoNotTriggerFeatureOnWorkspaceForNonGooglerAccount",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "https://mail.google.com/mail",
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {EditorBlockedReason::kBlockedByUrl}},
        {.test_name = "DoNotTriggerFeatureOnWorkspaceForGooglerAccountWithou"
                      "tOrcaOnWorkspaceFlag",
         .additional_enabled_flags = {},
         .email = "testuser@google.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "https://mail.google.com/mail",
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {EditorBlockedReason::kBlockedByUrl}},
        {.test_name =
             "DoNotTriggerFeatureOnDemoWorkspaceAppsForNonGooglerAccount",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "",
         .app_id = extension_misc::kGoogleDocsDemoAppId,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {EditorBlockedReason::kBlockedByApp}},
        {.test_name = "TriggerFeatureOnWorkspaceForGooglerAccountWithOrcaOnW"
                      "orkspaceFlag",
         .additional_enabled_flags = {features::kOrcaOnWorkspace},
         .email = "testuser@google.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = "https://mail.google.com/mail",
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kWrite,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {}},
        {.test_name = "DoNotTriggerFeatureWithNonEnglishInputMethod",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "nacl_mozc_jp",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons =
             {EditorBlockedReason::kBlockedByInputMethod}},
        {.test_name = "DoNotTriggerFeatureOnArcApps",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::ARC_APP,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {EditorBlockedReason::kBlockedByAppType}},
        {.test_name = "DoNotTriggerFeatureIfSettingToggleIsOff",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = false,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
         .expected_editor_opportunity_mode = EditorOpportunityMode::kWrite,
         .expected_blocked_reasons = {EditorBlockedReason::kBlockedBySetting}},
        {.test_name = "DoNotTriggerFeatureOnTabletMode",
         .additional_enabled_flags = {},
         .email = "testuser@gmail.com",
         .active_engine_id = "xkb:us::eng",
         .locale = "en-us",
         .url = kAllowedTestUrl,
         .input_type = ui::TEXT_INPUT_TYPE_TEXT,
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = true,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
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
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_NONE,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 0,
         .expected_editor_mode = EditorMode::kBlocked,
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
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 10001,
         .expected_editor_mode = EditorMode::kBlocked,
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
         .app_type = AppType::BROWSER,
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
         .app_type = AppType::BROWSER,
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
         .app_type = AppType::BROWSER,
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
         .app_type = AppType::BROWSER,
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
         .app_type = AppType::BROWSER,
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
         .app_type = AppType::BROWSER,
         .is_in_tablet_mode = false,
         .network_status = net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
         .user_pref = true,
         .consent_status = ConsentStatus::kApproved,
         .num_chars_selected = 100,
         .expected_editor_mode = EditorMode::kBlocked,
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
  FakeEditorSwitchDelegate delegate;
  EditorSwitch editor_switch(/*delegate=*/&delegate,
                             /*profile=*/profile.get(),
                             /*country_code=*/kAllowedTestCountry);

  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);

  mock_notifier->SetConnectionType(test_case.network_status);

  profile->GetPrefs()->SetBoolean(prefs::kOrcaEnabled, test_case.user_pref);
  profile->GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(test_case.consent_status));
  editor_switch.OnTabletModeUpdated(test_case.is_in_tablet_mode);
  editor_switch.OnActivateIme(test_case.active_engine_id);
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(test_case.input_type),
      CreateFakeTextFieldContextualInfo(test_case.app_type, test_case.url,
                                        test_case.app_id));
  editor_switch.OnTextSelectionLengthChanged(test_case.num_chars_selected);

  ASSERT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), test_case.expected_editor_mode);
  EXPECT_EQ(editor_switch.GetEditorOpportunityMode(),
            test_case.expected_editor_opportunity_mode);

  EXPECT_THAT(editor_switch.GetBlockedReasons(),
              testing::ElementsAreArray(test_case.expected_blocked_reasons));
}

}  // namespace
}  // namespace ash::input_method
