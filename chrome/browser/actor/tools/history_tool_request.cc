// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/history_tool_request.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

using ::tabs::TabHandle;
using ::tabs::TabInterface;

namespace {

ToolRequest::CreateToolResult CreateHistoryTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    TabHandle tab_handle,
    HistoryTool::Direction direction) {
  TabInterface* tab = tab_handle.Get();

  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  CHECK(tab->GetContents());
  return {
      std::make_unique<HistoryTool>(task_id, tool_delegate, *tab, direction),
      MakeOkResult()};
}

}  // namespace

HistoryBackToolRequest::HistoryBackToolRequest(TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}
HistoryBackToolRequest::~HistoryBackToolRequest() = default;

ToolRequest::CreateToolResult HistoryBackToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateHistoryTool(task_id, tool_delegate, GetTabHandle(),
                           HistoryTool::Direction::kBack);
}

void HistoryBackToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view HistoryBackToolRequest::Name() const {
  return kName;
}

bool HistoryBackToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A history tool is tab scoped but navigates *away* from the current URL --
  // the destination URL is checked in HistoryTool::Validate().
  return false;
}

HistoryForwardToolRequest::HistoryForwardToolRequest(TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}
HistoryForwardToolRequest::~HistoryForwardToolRequest() = default;

ToolRequest::CreateToolResult HistoryForwardToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateHistoryTool(task_id, tool_delegate, GetTabHandle(),
                           HistoryTool::Direction::kForward);
}

void HistoryForwardToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view HistoryForwardToolRequest::Name() const {
  return kName;
}

bool HistoryForwardToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A history tool is tab scoped but navigates *away* from the current URL --
  // the destination URL is checked in HistoryTool::Validate().
  return false;
}

ReloadPageToolRequest::ReloadPageToolRequest(TabHandle tab_handle,
                                             bool bypass_cache)
    : TabToolRequest(tab_handle), bypass_cache_(bypass_cache) {}
ReloadPageToolRequest::~ReloadPageToolRequest() = default;

ToolRequest::CreateToolResult ReloadPageToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateHistoryTool(task_id, tool_delegate, GetTabHandle(),
                           bypass_cache_
                               ? HistoryTool::Direction::kReloadBypassingCache
                               : HistoryTool::Direction::kReload);
}

void ReloadPageToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ReloadPageToolRequest::Name() const {
  return kName;
}

bool ReloadPageToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A reload tool checks the target entry URL in HistoryTool::Validate().
  return false;
}

}  // namespace actor
