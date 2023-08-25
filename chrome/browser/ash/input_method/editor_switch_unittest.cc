// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_switch.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace ash::input_method {
namespace {

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
  EditorSwitch editor_switch(/*is_managed=*/false);

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest,
       FeatureWillNotBeAvailableForUnmanagedAccountOnDogfoodDevices) {
  base::test::ScopedFeatureList feature_list(features::kOrcaDogfood);
  EditorSwitch editor_switch(/*is_managed=*/false);

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest,
       FeatureWillNotBeAvailableForManagedAccountOnNonDogfoodDevices) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kOrca);
  EditorSwitch editor_switch(/*is_managed=*/true);

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest,
       FeatureWillBeAvailableForUseForManagedAccountOnDogfoodDevices) {
  base::test::ScopedFeatureList feature_list(features::kOrcaDogfood);
  EditorSwitch editor_switch(/*is_managed=*/true);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredIfConsentDeclined) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kOrca);
  EditorSwitch editor_switch(/*is_managed=*/false);

  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));
  editor_switch.OnConsentStatusUpdated(ConsentStatus::kDeclined);

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_FALSE(editor_switch.CanBeTriggered());
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredOnAPasswordField) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kOrca);
  EditorSwitch editor_switch(/*is_managed=*/false);

  editor_switch.OnActivateIme("nacl_mozc_jp");
  editor_switch.OnConsentStatusUpdated(ConsentStatus::kApproved);
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_PASSWORD),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_FALSE(editor_switch.CanBeTriggered());
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredWithNonEnglishInputMethod) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kOrca);
  EditorSwitch editor_switch(/*is_managed=*/false);

  editor_switch.OnActivateIme("nacl_mozc_jp");
  editor_switch.OnConsentStatusUpdated(ConsentStatus::kApproved);
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_FALSE(editor_switch.CanBeTriggered());
}

TEST_F(EditorSwitchTest, FeatureCanNotBeTriggeredOnArcApps) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca},
      /*disabled_features=*/{});

  EditorSwitch editor_switch(/*is_managed=*/false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnConsentStatusUpdated(ConsentStatus::kApproved);
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::ARC_APP));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_FALSE(editor_switch.CanBeTriggered());
}

TEST_F(
    EditorSwitchTest,
    FeatureCanBeTriggeredOnANormalTextFieldOnABrowserWindowAndWithEnglishInputMethod) {
  base::test::ScopedFeatureList feature_list(chromeos::features::kOrca);

  EditorSwitch editor_switch(/*is_managed=*/false);
  editor_switch.OnActivateIme("xkb:us::eng");
  editor_switch.OnConsentStatusUpdated(ConsentStatus::kApproved);
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT),
      CreateFakeTextFieldContextualInfo(AppType::BROWSER));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_TRUE(editor_switch.CanBeTriggered());
}

}  // namespace
}  // namespace ash::input_method
