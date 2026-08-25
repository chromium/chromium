// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_REQUEST_H_

#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Highlights matching text in a tab and scrolls it into view.
class FindAndHighlightToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "FindAndHighlight";

  FindAndHighlightToolRequest(tabs::TabHandle tab_handle, std::string query);
  ~FindAndHighlightToolRequest() override;

  FindAndHighlightToolRequest(const FindAndHighlightToolRequest&);
  FindAndHighlightToolRequest& operator=(const FindAndHighlightToolRequest&);

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const std::string& query() const { return query_; }

 private:
  std::string query_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FIND_AND_HIGHLIGHT_TOOL_REQUEST_H_
