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

class EditorSwitchTest : public ::testing::Test {
 public:
  EditorSwitchTest() = default;
  ~EditorSwitchTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorSwitchTest,
       FeatureWillNotBeAvailableForUseWithoutReceivingOrcaFlag) {
  EditorSwitch editor_switch;

  EXPECT_FALSE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest, FeatureWillBeAvailableForUseDogfoodFlagIsTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(/*enabled_features=*/{features::kOrcaDogfood},
                                /*disabled_features=*/{});
  EditorSwitch editor_switch;

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
}

TEST_F(EditorSwitchTest, FeatureCanBeTriggeredOnANormalTextField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca},
      /*disabled_features=*/{});
  EditorSwitch editor_switch;
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_TRUE(editor_switch.CanBeTriggered());
}

TEST_F(EditorSwitchTest, FeatureCannotBeTriggeredOnAPasswordField) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{chromeos::features::kOrca},
      /*disabled_features=*/{});
  EditorSwitch editor_switch;
  editor_switch.OnInputContextUpdated(
      TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_PASSWORD));

  EXPECT_TRUE(editor_switch.IsAllowedForUse());
  EXPECT_FALSE(editor_switch.CanBeTriggered());
}

}  // namespace
}  // namespace ash::input_method
