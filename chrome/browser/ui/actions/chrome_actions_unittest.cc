// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_actions.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

class ChromeActionsTest : public testing::Test {
 public:
  ChromeActionsTest() = default;
  ChromeActionsTest(const ChromeActionsTest&) = delete;
  ChromeActionsTest& operator=(const ChromeActionsTest&) = delete;
  ~ChromeActionsTest() override = default;
};

// TODO(crbug.com/1491976): Adding temporarily to unblock the side panel team.
// Should be removed/replaced when general solution to add action id mappings is
// implemented.
// TODO(josephjoopark): Rewrite such that static maps are reset to their
// original state.
TEST_F(ChromeActionsTest, InitializeActionIdStringMappingTest) {
  InitializeActionIdStringMapping();

  auto actual_action_id =
      actions::ActionManager::StringToActionId("kActionSidePanelShowFeed");
  EXPECT_THAT(actual_action_id, testing::Optional(kActionSidePanelShowFeed));
}
