// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/load_and_extract_content_tool_request.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/actor/tools/load_and_extract_content_tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor/action_result.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace actor {

namespace {
// TODO(b/478282022): This is a temporary hack to get the active window's ID.
// Remove this once we have a way to specify the window in the action.
SessionID GetActiveWindowId(ToolDelegate& tool_delegate) {
  SessionID active_window_id = SessionID::InvalidValue();
  ProfileBrowserCollection::GetForProfile(&tool_delegate.GetProfile())
      ->ForEach(
          [&active_window_id](BrowserWindowInterface* browser) {
            if (browser->IsActive()) {
              active_window_id = browser->GetSessionID();
              return false;
            }
            return true;
          },
          BrowserCollection::Order::kActivation);
  return active_window_id;
}

}  // namespace

LoadAndExtractContentToolRequest::LoadAndExtractContentToolRequest(
    std::vector<GURL> urls)
    : urls_(std::move(urls)) {}

LoadAndExtractContentToolRequest::~LoadAndExtractContentToolRequest() = default;

LoadAndExtractContentToolRequest::LoadAndExtractContentToolRequest(
    const LoadAndExtractContentToolRequest&) = default;
LoadAndExtractContentToolRequest& LoadAndExtractContentToolRequest::operator=(
    const LoadAndExtractContentToolRequest&) = default;

ToolRequest::CreateToolResult LoadAndExtractContentToolRequest::CreateTool(
    TaskId task_id,
    actor::ToolDelegate& tool_delegate) const {
  SessionID window_id = GetActiveWindowId(tool_delegate);

  // TODO(b/443954134): Could we avoid the copy of the URLs?
  return {std::make_unique<LoadAndExtractContentTool>(task_id, tool_delegate,
                                                      window_id, urls_),
          MakeOkResult()};
}

void LoadAndExtractContentToolRequest::Apply(
    ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view LoadAndExtractContentToolRequest::Name() const {
  return kName;
}

}  // namespace actor
