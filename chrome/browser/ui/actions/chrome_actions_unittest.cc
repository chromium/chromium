// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_actions.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

class ChromeActionsTest : public testing::Test {
 public:
  ChromeActionsTest() = default;
  ChromeActionsTest(const ChromeActionsTest&) = delete;
  ChromeActionsTest& operator=(const ChromeActionsTest&) = delete;
  ~ChromeActionsTest() override = default;

  void TearDown() override { actions::ActionIdMap::ResetMapsForTesting(); }
};

// TODO(crbug.com/40285337): Adding temporarily to unblock the side panel team.
// Should be removed/replaced when general solution to add action id mappings is
// implemented.
TEST_F(ChromeActionsTest, InitializeActionIdStringMappingTest) {
  InitializeActionIdStringMapping();

  auto actual_action_id =
      actions::ActionIdMap::StringToActionId("kActionSidePanelShowFeed");
  EXPECT_THAT(actual_action_id, testing::Optional(kActionSidePanelShowFeed));
}

TEST_F(ChromeActionsTest, ShouldUseActionsForBrowserCommands) {
  // Both disabled by default.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kUseActionsForBrowserCommands,
                               features::kAppMenuGlowUp});
    EXPECT_FALSE(features::ShouldUseActionsForBrowserCommands());
  }

  // Only kUseActionsForBrowserCommands enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kUseActionsForBrowserCommands},
        /*disabled_features=*/{features::kAppMenuGlowUp});
    EXPECT_TRUE(features::ShouldUseActionsForBrowserCommands());
  }

  // Only kAppMenuGlowUp enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kAppMenuGlowUp},
        /*disabled_features=*/{features::kUseActionsForBrowserCommands});
    EXPECT_TRUE(features::ShouldUseActionsForBrowserCommands());
  }

  // Both enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /*enabled_features=*/{features::kUseActionsForBrowserCommands,
                              features::kAppMenuGlowUp},
        /*disabled_features=*/{});
    EXPECT_TRUE(features::ShouldUseActionsForBrowserCommands());
  }
}
