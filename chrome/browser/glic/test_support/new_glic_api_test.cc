// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/new_glic_api_test.h"

#include "base/check.h"
#include "base/test/run_until.h"
#include "base/types/expected_macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

WebUIStateListener::WebUIStateListener(Host* host)
    : host_(host ? host->GetWeakPtr() : nullptr) {
  CHECK(host_);
  host_->AddObserver(this);
  states_.push_back(host_->GetPrimaryWebUiState());
}

WebUIStateListener::~WebUIStateListener() {
  if (!host_) {
    return;
  }
  host_->RemoveObserver(this);
}

void WebUIStateListener::WebUiStateChanged(mojom::WebUiState state) {
  states_.push_back(state);
}

void WebUIStateListener::WaitForWebUiState(mojom::WebUiState state) {
  ASSERT_TRUE(host_);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    while (!states_.empty()) {
      if (states_.front() != state) {
        states_.pop_front();
        continue;
      }
      return true;
    }
    return false;
  })) << "Timed out waiting for WebUI state "
      << state << ". State =" << host_->GetPrimaryWebUiState();
}

}  // namespace glic

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

base::expected<tabs::TabHandle, std::string> ParseOptionalTabId(
    const base::DictValue& command) {
  if (!command.FindString("tabId")) {
    return base::ok(tabs::TabHandle());
  }
  return ParseTabId(command);
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
    ASSIGN_OR_RETURN(tabs::TabHandle tab_handle, ParseOptionalTabId(dict));
    return NavigateTabCommand{tab_handle, *url};
  }
  if (*command_field == "parse-actions-result") {
    auto* actions_result = dict.FindString("actionsResult");
    if (!actions_result) {
      return base::unexpected("Missing actionsResult");
    }
    return ParseActionsResultCommand{*actions_result};
  }
  if (*command_field == "make-wait-action") {
    ASSIGN_OR_RETURN(tabs::TabHandle tab_handle, ParseOptionalTabId(dict));
    auto task_id = dict.FindInt("taskId");
    if (!task_id) {
      return base::unexpected("Missing taskId");
    }
    std::optional<base::TimeDelta> duration;
    auto duration_ms = dict.FindInt("durationMs");
    if (duration_ms) {
      duration = base::Milliseconds(*duration_ms);
    }
    return MakeWaitActionCommand{duration, tab_handle, *task_id};
  }
  if (*command_field == "make-navigate-action") {
    ASSIGN_OR_RETURN(tabs::TabHandle tab_handle, ParseOptionalTabId(dict));
    auto* url = dict.FindString("url");
    if (!url) {
      return base::unexpected("Missing url");
    }
    auto task_id = dict.FindInt("taskId");
    if (!task_id) {
      return base::unexpected("Missing taskId");
    }
    return MakeNavigateActionCommand{tab_handle, *url, *task_id};
  }
  return base::unexpected(base::StrCat({"Unknown command: ", *command_field}));
}

}  // namespace glic::internal
