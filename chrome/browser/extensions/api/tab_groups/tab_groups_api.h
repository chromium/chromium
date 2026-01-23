// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_

#include <optional>
#include <string>

#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/gfx/range/range.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;

namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}

namespace extensions {

class TabGroupsGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.get", TAB_GROUPS_GET)
  TabGroupsGetFunction() = default;
  TabGroupsGetFunction(const TabGroupsGetFunction&) = delete;
  TabGroupsGetFunction& operator=(const TabGroupsGetFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~TabGroupsGetFunction() override = default;
};

class TabGroupsQueryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.query", TAB_GROUPS_QUERY)
  TabGroupsQueryFunction() = default;
  TabGroupsQueryFunction(const TabGroupsQueryFunction&) = delete;
  TabGroupsQueryFunction& operator=(const TabGroupsQueryFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~TabGroupsQueryFunction() override = default;
};

class TabGroupsUpdateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.update", TAB_GROUPS_UPDATE)
  TabGroupsUpdateFunction() = default;
  TabGroupsUpdateFunction(const TabGroupsUpdateFunction&) = delete;
  TabGroupsUpdateFunction& operator=(const TabGroupsUpdateFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~TabGroupsUpdateFunction() override = default;
};

class TabGroupsMoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("tabGroups.move", TAB_GROUPS_MOVE)
  TabGroupsMoveFunction();
  TabGroupsMoveFunction(const TabGroupsMoveFunction&) = delete;
  TabGroupsMoveFunction& operator=(const TabGroupsMoveFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~TabGroupsMoveFunction() override;

 private:
  // Moves the group with ID `group_id` to the `new_index` in the window with ID
  // `window_id`. If `window_id` is not specified, moves the group within its
  // current window. `group` is populated with the group's TabGroupId,
  // `cross_window` is set if the move is to a different window, and `error` is
  // populated if the group cannot be found or moved.
  bool MoveGroup(int group_id,
                 int new_index,
                 const std::optional<int>& window_id,
                 tab_groups::TabGroupId* group,
                 bool* cross_window,
                 std::string* error);

  // Moves a tab group between browser windows.
  bool MoveTabGroupBetweenBrowsers(
      BrowserWindowInterface* source_browser,
      BrowserWindowInterface* target_browser,
      const tab_groups::TabGroupId& group,
      const tab_groups::TabGroupVisualData& visual_data,
      const gfx::Range& tabs,
      int new_index,
      std::string* error);

#if BUILDFLAG(IS_ANDROID)
  // Called when a tab group is created in the target window for a cross-window
  // move.
  void OnTabGroupCreated(tab_groups::TabGroupId group_id);

  // Tab group moves between windows are asynchronous on Android, so we must
  // observe for the group being created in the target window.
  class ObserverHelper;
  std::unique_ptr<ObserverHelper> observer_helper_;
#endif  // BUILDFLAG(IS_ANDROID)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_
