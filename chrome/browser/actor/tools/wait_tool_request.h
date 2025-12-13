// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {
class ToolRequestVisitorFunctor;

class WaitToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "Wait";

  explicit WaitToolRequest(
      base::TimeDelta wait_duration,
      tabs::TabHandle observe_tab_handle = tabs::TabHandle::Null());
  ~WaitToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;

 private:
  base::TimeDelta wait_duration_;

  // Optional. If provided, an observation from this tab will be included in the
  // result observations. However, this tab is not added to the tab set in
  // ActorTask.
  tabs::TabHandle observe_tab_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WAIT_TOOL_REQUEST_H_
