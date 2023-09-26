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
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

constexpr std::string_view kAllowedTestCountry = "allowed_country";
constexpr std::string_view kDeniedTestCountry = "denied_country";

TextFieldContextualInfo CreateFakeTextFieldContextualInfo(
    ash::AppType app_type) {
  auto text_field_contextual_info = TextFieldContextualInfo();
  text_field_contextual_info.app_type = app_type;
  return text_field_contextual_info;
}

class EditorSwitchTest : public ::testing::Test {
 public:
  EditorSwitchTest() = default;
  ~EditorSwitchTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorSwitchTest,
       FeatureWillNotBeAvailableForUseWithoutReceivingOrcaFlag) {
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  EXPECT_FALSE(chromeos::features::IsOrcaEnabled());
  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest,
       FeatureWillNotBeAvailableForManagedAccountOnNonDogfoodDevices) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest,
       FeatureWillBeAvailableForUseWhenReceivingOrcaDogfoodFlag) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kOrcaDogfood);
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  EXPECT_TRUE(chromeos::features::IsOrcaEnabled());
  EXPECT_TRUE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest, FeatureWillNotBeAvailableForACountryNotApprovedYet) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kDeniedTestCountry);

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest,
       FeatureWillNotBeAvailableWithoutFeatureManagementFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca},
      /*disabled_features=*/{features::kFeatureManagementOrca});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kDeniedTestCountry);

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredIfConsentDeclined) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kDeclined));
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredOnAPasswordField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnActivateIme("xkb:us::eng");

  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("nacl_mozc_jp");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_PASSWORD),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredWithNonEnglishInputMethod) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("nacl_mozc_jp");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest, FeatureCanNotBeTriggeredOnArcApps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::ARC_APP));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest,
       FeatureCanNotBeTriggeredIfUserSwitchesOffSettingToggle) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, false);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::ARC_APP));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest, FeatureCanNotBeTriggeredOnTabletMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(true);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredWhenOffline) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(EditorSwitchTest, FeatureCanNotBeTriggeredWithTooLongTextSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(true);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));
  editor_switch.OnTextSelectionLengthChanged(10000);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kBlocked);
}

TEST_F(
    EditorSwitchTest,
    FeatureCanBeTriggeredIfUserSwitchesOnSettingToggleAndHasNotGivenConsent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(prefs::kOrcaConsentStatus,
                                  base::to_underlying(ConsentStatus::kPending));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kConsentNeeded);
}

TEST_F(EditorSwitchTest, TriggersRewriteModeForNoTextSelection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kWrite);
}

TEST_F(EditorSwitchTest, TriggersRewriteModeWhenSomeTextIsSelected) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca,
                            features::kFeatureManagementOrca},
      /*disabled_features=*/{});
  TestingProfile profile_;
  profile_.GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  auto mock_notifier = net::test::MockNetworkChangeNotifier::Create();
  mock_notifier->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN);
  EditorSwitch editor_switch(/*profile=*/&profile_,
                             /*country_code=*/kAllowedTestCountry);

  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  profile_.GetPrefs()->SetInteger(
      prefs::kOrcaConsentStatus, base::to_underlying(ConsentStatus::kApproved));
  editor_switch.OnTabletModeUpdated(false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));
  editor_switch.OnTextSelectionLengthChanged(100);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_EQ(editor_switch.GetEditorMode(), EditorMode::kRewrite);
}

}  // namespace
}  // namespace ash::input_method
