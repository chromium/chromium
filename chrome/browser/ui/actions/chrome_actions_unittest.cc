// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_actions.h"

#include <cstddef>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/command_id_to_action_id.h"
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

#define MAP_ACTION_E1(action) 0 +
#define MAP_ACTION_E2(action, idc) 1 +
#define MAP_ACTION_E3(action, idc, scope) 1 +
#define MAP_ACTION_E4(action, idc, val, scope) 1 +

#define GET_MAP_ACTION_E(_1, _2, _3, _4, macro_name, ...) macro_name
#define E(...)                                                               \
  GET_MAP_ACTION_E(__VA_ARGS__, MAP_ACTION_E4, MAP_ACTION_E3, MAP_ACTION_E2, \
                   MAP_ACTION_E1)(__VA_ARGS__)

TEST_F(ChromeActionsTest, NoDuplicateCommandIds) {
  constexpr size_t kExpectedMapSize =
      (CHROME_ACTION_IDS SIDE_PANEL_ACTION_IDS TOOLBAR_PINNABLE_ACTION_IDS
           SUBMENU_ACTION_IDS 0);
  EXPECT_EQ(chrome::GetCommandIdToActionIdMappings().size(), kExpectedMapSize);
}

#undef E
#undef GET_MAP_ACTION_E
#undef MAP_ACTION_E1
#undef MAP_ACTION_E2
#undef MAP_ACTION_E3
#undef MAP_ACTION_E4
