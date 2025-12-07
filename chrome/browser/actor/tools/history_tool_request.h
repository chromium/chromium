// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/tool_request.h"
#include "url/gurl.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Invokes a history back or forward traversal in a specified tab.
class HistoryToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "History";

  enum class Direction {
    kBack,
    kForward,
  };

  HistoryToolRequest(tabs::TabHandle handle, Direction direction);
  ~HistoryToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  bool RequiresUrlCheckInCurrentTab() const override;

  // Whether the navigation is backwards or forwards in session history.
  Direction direction_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_REQUEST_H_
