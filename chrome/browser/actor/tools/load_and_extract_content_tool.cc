// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/load_and_extract_content_tool.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

namespace actor {

LoadAndExtractContentTool::LoadAndExtractContentTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    std::vector<GURL> urls)
    : Tool(task_id, tool_delegate), urls_(std::move(urls)) {}

LoadAndExtractContentTool::~LoadAndExtractContentTool() = default;

void LoadAndExtractContentTool::Validate(ToolCallback callback) {
  // TODO(b/478281513): Validate the URLs, delegating to the TabManagementTool
  // once it supports opening tabs to specific URLs.
  NOTIMPLEMENTED();
  std::move(callback).Run(mojom::ActionResult::New());
}

void LoadAndExtractContentTool::Invoke(ToolCallback callback) {
  // TODO(b/478282022): Implement this, delegating to the TabManagementTool for
  // opening tabs to the given URLs and then closing them.
  NOTIMPLEMENTED();
  std::move(callback).Run(mojom::ActionResult::New());
}

std::string LoadAndExtractContentTool::DebugString() const {
  return "LoadAndExtractContentTool";
}

std::string LoadAndExtractContentTool::JournalEvent() const {
  return "LoadAndExtractContent";
}

std::unique_ptr<ObservationDelayController>
LoadAndExtractContentTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // TODO(b/478281781): Create an observation delayer that waits for the pages
  // to load.
  NOTIMPLEMENTED();
  return nullptr;
}

tabs::TabHandle LoadAndExtractContentTool::GetTargetTab() const {
  // This tool can operate on multiple tabs, so there's no single target.
  return tabs::TabHandle::Null();
}

}  // namespace actor
