// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/switch_tab_tool_request.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/tools/switch_tab_tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor/action_result.h"
#include "components/sessions/core/session_id.h"

namespace actor {

namespace {

// A tab switch acts on the window the user is looking at, and the request has
// no way to name a window yet, so resolve the profile's most recently active
// one here. This mirrors LoadAndExtractContentToolRequest, which also picks
// its window at CreateTool() time and hands the tool a SessionID.
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

SwitchTabToolRequest::SwitchTabToolRequest(std::string query)
    : query_(std::move(query)) {}

SwitchTabToolRequest::~SwitchTabToolRequest() = default;

SwitchTabToolRequest::SwitchTabToolRequest(const SwitchTabToolRequest&) =
    default;
SwitchTabToolRequest& SwitchTabToolRequest::operator=(
    const SwitchTabToolRequest&) = default;

ToolRequest::CreateToolResult SwitchTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (query_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Query cannot be empty.")};
  }
  return {std::make_unique<SwitchTabTool>(
              task_id, tool_delegate, GetActiveWindowId(tool_delegate), query_),
          MakeOkResult()};
}

void SwitchTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view SwitchTabToolRequest::Name() const {
  return kName;
}

}  // namespace actor
