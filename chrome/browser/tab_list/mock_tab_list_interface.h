// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_LIST_MOCK_TAB_LIST_INTERFACE_H_
#define CHROME_BROWSER_TAB_LIST_MOCK_TAB_LIST_INTERFACE_H_

#include <set>
#include <vector>

#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

// A mock implementation of TabListInterface for use in tests.
// To use this in a unit test where the production code calls
// TabListInterface::From(window), you must register the mock in the window's
// UnownedUserDataHost:
//
//   tab_list_ = std::make_unique<NiceMock<MockTabListInterface>>();
//   tab_list_registration_ =
//       std::make_unique<ui::ScopedUnownedUserData<TabListInterface>>(
//           browser_window_->GetUnownedUserDataHost(), *tab_list_);
class MockTabListInterface : public TabListInterface {
 public:
  MockTabListInterface();
  ~MockTabListInterface() override;

  MOCK_METHOD(void,
              AddTabListInterfaceObserver,
              (TabListInterfaceObserver*),
              (override));
  MOCK_METHOD(void,
              RemoveTabListInterfaceObserver,
              (TabListInterfaceObserver*),
              (override));
  MOCK_METHOD(int, GetTabCount, (), (const, override));
  MOCK_METHOD(int, GetActiveIndex, (), (const, override));
  MOCK_METHOD(tabs::TabInterface*, GetActiveTab, (), (override));
  MOCK_METHOD(void, ActivateTab, (tabs::TabHandle), (override));
  MOCK_METHOD(tabs::TabInterface*, OpenTab, (const GURL&, int), (override));
  MOCK_METHOD(void,
              SetOpenerForTab,
              (tabs::TabHandle, tabs::TabHandle),
              (override));
  MOCK_METHOD(tabs::TabInterface*,
              GetOpenerForTab,
              (tabs::TabHandle),
              (override));
  MOCK_METHOD(tabs::TabInterface*,
              InsertWebContentsAt,
              (int,
               std::unique_ptr<content::WebContents>,
               bool,
               std::optional<tab_groups::TabGroupId>),
              (override));
  MOCK_METHOD(content::WebContents*, DiscardTab, (tabs::TabHandle), (override));
  MOCK_METHOD(tabs::TabInterface*, DuplicateTab, (tabs::TabHandle), (override));
  MOCK_METHOD(tabs::TabInterface*, GetTab, (int), (override));
  MOCK_METHOD(int, GetIndexOfTab, (tabs::TabHandle), (override));
  MOCK_METHOD(void,
              HighlightTabs,
              (tabs::TabHandle, const std::set<tabs::TabHandle>&),
              (override));
  MOCK_METHOD(void, MoveTab, (tabs::TabHandle, int), (override));
  MOCK_METHOD(void, CloseTab, (tabs::TabHandle), (override));
  MOCK_METHOD(std::vector<tabs::TabInterface*>, GetAllTabs, (), (override));
  MOCK_METHOD(void, PinTab, (tabs::TabHandle), (override));
  MOCK_METHOD(void, UnpinTab, (tabs::TabHandle), (override));
  MOCK_METHOD(bool, ContainsTabGroup, (tab_groups::TabGroupId), (override));
  MOCK_METHOD(std::vector<tab_groups::TabGroupId>,
              ListTabGroups,
              (),
              (override));
  MOCK_METHOD(std::optional<tab_groups::TabGroupVisualData>,
              GetTabGroupVisualData,
              (tab_groups::TabGroupId),
              (override));
  MOCK_METHOD(gfx::Range,
              GetTabGroupTabIndices,
              (tab_groups::TabGroupId),
              (override));
  MOCK_METHOD(std::optional<tab_groups::TabGroupId>,
              CreateTabGroup,
              (const std::vector<tabs::TabHandle>&),
              (override));
  MOCK_METHOD(void,
              SetTabGroupVisualData,
              (tab_groups::TabGroupId, const tab_groups::TabGroupVisualData&),
              (override));
  MOCK_METHOD(std::optional<tab_groups::TabGroupId>,
              AddTabsToGroup,
              (std::optional<tab_groups::TabGroupId>,
               const std::set<tabs::TabHandle>&),
              (override));
  MOCK_METHOD(void, Ungroup, (const std::set<tabs::TabHandle>&), (override));
  MOCK_METHOD(void, MoveGroupTo, (tab_groups::TabGroupId, int), (override));
  MOCK_METHOD(void,
              MoveTabToWindow,
              (tabs::TabHandle, SessionID, int),
              (override));
  MOCK_METHOD(void,
              MoveTabGroupToWindow,
              (tab_groups::TabGroupId, SessionID, int),
              (override));
  MOCK_METHOD(bool, IsThisTabListEditable, (), (override));
  MOCK_METHOD(bool, IsClosingAllTabs, (), (override));
};

#endif  // CHROME_BROWSER_TAB_LIST_MOCK_TAB_LIST_INTERFACE_H_
