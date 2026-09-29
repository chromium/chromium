// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/open_known_page_tool_request.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_util.h"
#include "chrome/browser/actor/tools/open_known_page_tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor/action_result.h"
#include "components/sessions/core/session_id.h"

namespace actor {

namespace {

// Resolves the profile's most recently active browser window at tool creation
// time, matching SwitchTabToolRequest and LoadAndExtractContentToolRequest.
SessionID GetActiveWindowId(ToolDelegate& tool_delegate) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&tool_delegate.GetProfile());
  if (!collection) {
    return SessionID::InvalidValue();
  }
  BrowserWindowInterface* browser = collection->GetLastActiveBrowser();
  return browser ? browser->GetSessionID() : SessionID::InvalidValue();
}

}  // namespace

OpenKnownPageToolRequest::OpenKnownPageToolRequest(std::string query)
    : query_(std::move(query)) {}

OpenKnownPageToolRequest::~OpenKnownPageToolRequest() = default;

OpenKnownPageToolRequest::OpenKnownPageToolRequest(
    const OpenKnownPageToolRequest&) = default;
OpenKnownPageToolRequest& OpenKnownPageToolRequest::operator=(
    const OpenKnownPageToolRequest&) = default;

ToolRequest::CreateToolResult OpenKnownPageToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  std::string trimmed_query =
      std::string(base::TrimWhitespaceASCII(query_, base::TRIM_ALL));
  if (trimmed_query.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Query cannot be empty.")};
  }
  return {std::make_unique<OpenKnownPageTool>(task_id, tool_delegate,
                                              GetActiveWindowId(tool_delegate),
                                              std::move(trimmed_query)),
          MakeOkResult()};
}

void OpenKnownPageToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view OpenKnownPageToolRequest::Name() const {
  return kName;
}

}  // namespace actor
