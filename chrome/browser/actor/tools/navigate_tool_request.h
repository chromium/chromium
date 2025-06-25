// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"
#include "url/gurl.h"

namespace actor {

// Navigates a specified tab to a specified URL.
class NavigateToolRequest : public TabToolRequest {
 public:
  NavigateToolRequest(tabs::TabHandle tab_handle, GURL url);
  ~NavigateToolRequest() override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              AggregatedJournal& journal) const override;
  std::string JournalEvent() const override;

 private:
  GURL url_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_
