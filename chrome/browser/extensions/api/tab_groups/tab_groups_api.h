// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_

#include "extensions/browser/extension_function.h"

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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_API_H_
