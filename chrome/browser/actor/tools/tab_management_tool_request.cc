// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool_request.h"

#include <memory>

#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

using tabs::TabInterface;

CreateTabToolRequest::CreateTabToolRequest(int32_t window_id,
                                           WindowOpenDisposition disposition)
    : window_id_(window_id), disposition_(disposition) {}

CreateTabToolRequest::~CreateTabToolRequest() = default;

bool CreateTabToolRequest::AddsTabToObservationSet() const {
  return true;
}

ToolRequest::CreateToolResult CreateTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<TabManagementTool>(task_id, tool_delegate,
                                              window_id_, disposition_),
          MakeOkResult()};
}

void CreateTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CreateTabToolRequest::Name() const {
  return kName;
}

ActivateTabToolRequest::ActivateTabToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

ActivateTabToolRequest::~ActivateTabToolRequest() = default;

ToolRequest::CreateToolResult ActivateTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {
      std::make_unique<TabManagementTool>(
          task_id, tool_delegate, TabManagementTool::kActivate, GetTabHandle()),
      MakeOkResult()};
}

void ActivateTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ActivateTabToolRequest::Name() const {
  return kName;
}

CloseTabToolRequest::CloseTabToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

CloseTabToolRequest::~CloseTabToolRequest() = default;

ToolRequest::CreateToolResult CloseTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {
      std::make_unique<TabManagementTool>(
          task_id, tool_delegate, TabManagementTool::kClose, GetTabHandle()),
      MakeOkResult()};
}

void CloseTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CloseTabToolRequest::Name() const {
  return kName;
}

}  // namespace actor
