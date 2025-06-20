// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool_request.h"

#include <memory>

#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

using tabs::TabInterface;

CreateTabToolRequest::CreateTabToolRequest(int32_t window_id,
                                           WindowOpenDisposition disposition)
    : window_id_(window_id), disposition_(disposition) {}

CreateTabToolRequest::~CreateTabToolRequest() = default;

ToolRequest::CreateToolResult CreateTabToolRequest::CreateTool(
    TaskId task_id,
    AggregatedJournal& journal) const {
  return {std::make_unique<TabManagementTool>(task_id, journal, window_id_,
                                              disposition_),
          MakeOkResult()};
}

std::string CreateTabToolRequest::JournalEvent() const {
  return "CreateTab";
}

ActivateTabToolRequest::ActivateTabToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

ActivateTabToolRequest::~ActivateTabToolRequest() = default;

ToolRequest::CreateToolResult ActivateTabToolRequest::CreateTool(
    TaskId task_id,
    AggregatedJournal& journal) const {
  TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<TabManagementTool>(
              task_id, journal, TabManagementTool::kActivate, GetTabHandle()),
          MakeOkResult()};
}

std::string ActivateTabToolRequest::JournalEvent() const {
  return "ActivateTab";
}

CloseTabToolRequest::CloseTabToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

CloseTabToolRequest::~CloseTabToolRequest() = default;

ToolRequest::CreateToolResult CloseTabToolRequest::CreateTool(
    TaskId task_id,
    AggregatedJournal& journal) const {
  TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<TabManagementTool>(
              task_id, journal, TabManagementTool::kClose, GetTabHandle()),
          MakeOkResult()};
}

std::string CloseTabToolRequest::JournalEvent() const {
  return "CloseTab";
}

}  // namespace actor
