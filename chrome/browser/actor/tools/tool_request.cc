// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_request.h"

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

GURL ToolRequest::GetURLForJournal() const {
  return GURL::EmptyGURL();
}

TabToolRequest::TabToolRequest(const tabs::TabInterface::Handle tab_handle)
    : tab_handle_(tab_handle) {
  // The given handle need not be valid - the handle is validated at time of
  // dereferencing when instantiating a tool. However, it must be a non-null
  // value.
  CHECK_NE(tab_handle.raw_value(), TabHandle::Null().raw_value());
}
TabToolRequest::~TabToolRequest() = default;

GURL TabToolRequest::GetURLForJournal() const {
  if (TabInterface* tab = tab_handle_.Get()) {
    return tab->GetContents()->GetLastCommittedURL();
  }
  return ToolRequest::GetURLForJournal();
}

tabs::TabInterface::Handle TabToolRequest::GetTabHandle() const {
  return tab_handle_;
}

}  // namespace actor
