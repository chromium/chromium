// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_CHANGE_PASSWORD_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_CHANGE_PASSWORD_TOOL_REQUEST_H_

#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Tool request for the Change Password tool.
//
// This tool must NOT be used directly from the side panel
// because that would not work and could lose passwords.
// Only the Automated Password Change flow can safely use this tool.
class ChangePasswordToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "ChangePassword";

  explicit ChangePasswordToolRequest(
      tabs::TabHandle tab_handle = tabs::TabHandle::Null());
  ChangePasswordToolRequest(const ChangePasswordToolRequest&);
  ChangePasswordToolRequest& operator=(const ChangePasswordToolRequest&);
  ~ChangePasswordToolRequest() override;

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_CHANGE_PASSWORD_TOOL_REQUEST_H_
