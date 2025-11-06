// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/tool.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace actor {

using ::content::WebContents;
using ::tabs::TabHandle;
using ::tabs::TabInterface;

ToolRequest::CreateToolResult::CreateToolResult(std::unique_ptr<Tool> tool,
                                                mojom::ActionResultPtr result)
    : tool(std::move(tool)), result(std::move(result)) {}
ToolRequest::CreateToolResult::~CreateToolResult() = default;

ToolRequest::ToolRequest() = default;
ToolRequest::~ToolRequest() = default;
ToolRequest::ToolRequest(const ToolRequest& other) = default;
ToolRequest& ToolRequest::operator=(const ToolRequest& other) = default;

bool ToolRequest::IsTabScoped() const {
  return GetTabHandle() != tabs::TabHandle::Null();
}

bool ToolRequest::AddsTabToObservationSet() const {
  return IsTabScoped();
}

GURL ToolRequest::GetURLForJournal() const {
  return GURL::EmptyGURL();
}

tabs::TabHandle ToolRequest::GetTabHandle() const {
  return tabs::TabHandle();
}

std::string ToolRequest::JournalEvent() const {
  return std::string(Name());
}

bool ToolRequest::RequiresUrlCheckInCurrentTab() const {
  // By default, tab scoped tools require current tab URL checks but individual
  // tools can override this.
  return IsTabScoped();
}

std::optional<url::Origin> ToolRequest::AssociatedOriginGrant() const {
  return std::nullopt;
}

ObservationDelayController::PageStabilityConfig
ToolRequest::GetObservationPageStabilityConfig() const {
  return ObservationDelayController::PageStabilityConfig();
}

TabToolRequest::TabToolRequest(const tabs::TabHandle tab_handle)
    : tab_handle_(tab_handle) {
  // The given handle need not be valid - the handle is validated at time of
  // dereferencing when instantiating a tool. However, it must be a non-null
  // value.
  CHECK_NE(tab_handle.raw_value(), TabHandle::Null().raw_value());
}
TabToolRequest::~TabToolRequest() = default;
TabToolRequest::TabToolRequest(const TabToolRequest& other) = default;
TabToolRequest& TabToolRequest::operator=(const TabToolRequest& other) =
    default;

GURL TabToolRequest::GetURLForJournal() const {
  if (TabInterface* tab = tab_handle_.Get()) {
    return tab->GetContents()->GetLastCommittedURL();
  }
  return ToolRequest::GetURLForJournal();
}

tabs::TabHandle TabToolRequest::GetTabHandle() const {
  return tab_handle_;
}

}  // namespace actor
