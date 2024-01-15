// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace tab_groups {
class TabGroupId;
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
  TabGroupsMoveFunction() = default;
  TabGroupsMoveFunction(const TabGroupsMoveFunction&) = delete;
  TabGroupsMoveFunction& operator=(const TabGroupsMoveFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 protected:
  ~TabGroupsMoveFunction() override = default;

 private:
  // Moves the group with ID |group_id| to the |new_index| in the window with ID
  // |window_id|. If |window_id| is not specified, moves the group within its
  // current window. |group| is populated with the group's TabGroupId, and
  // |error| is populated if the group cannot be found or moved.
  bool MoveGroup(int group_id,
                 int new_index,
                 const std::optional<int>& window_id,
                 tab_groups::TabGroupId* group,
                 std::string* error);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_
