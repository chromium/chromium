// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

class ToolRequestVisitorFunctor;

class AttemptLoginToolRequest : public TabToolRequest {
 public:
  explicit AttemptLoginToolRequest(tabs::TabHandle tab_handle);
  ~AttemptLoginToolRequest() override;

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string JournalEvent() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_REQUEST_H_
