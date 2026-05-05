// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/new_glic_api_test.h"

namespace glic::internal {
namespace {
base::expected<tabs::TabHandle, std::string> ParseTabId(
    const base::DictValue& command,
    bool must_exist = true) {
  auto* tab_id = command.FindString("tabId");
  if (!tab_id) {
    return base::unexpected("Missing tabId");
  }
  int tab_id_val;
  if (!base::StringToInt(*tab_id, &tab_id_val)) {
    return base::unexpected("Invalid tabId");
  }
  auto handle = tabs::TabHandle(tab_id_val);
  if (must_exist && !handle.Get()) {
    return base::unexpected("Tab not found");
  }
  return base::ok(handle);
}
}  // namespace

base::expected<Command, std::string> DeserializeCommand(
    const base::DictValue& dict) {
  auto* command_field = dict.FindString("command");
  if (!command_field) {
    return base::unexpected("Missing command");
  }

  if (*command_field == "close-tab") {
    ASSIGN_OR_RETURN(tabs::TabHandle tab_handle, ParseTabId(dict));
    return CloseTabCommand{tab_handle};
  }
  if (*command_field == "exec-js-in-tab") {
    auto* script = dict.FindString("script");
    if (!script) {
      return base::unexpected("Missing script");
    }
    ASSIGN_OR_RETURN(tabs::TabHandle tab_handle, ParseTabId(dict));
    return ExecJsCommand{tab_handle, *script};
  }
  if (*command_field == "navigate-tab") {
    auto* url = dict.FindString("url");
    if (!url) {
      return base::unexpected("Missing url");
    }
    ASSIGN_OR_RETURN(tabs::TabHandle tab_handle, ParseTabId(dict));
    return NavigateTabCommand{tab_handle, *url};
  }
  return base::unexpected(base::StrCat({"Unknown command: ", *command_field}));
}

}  // namespace glic::internal
