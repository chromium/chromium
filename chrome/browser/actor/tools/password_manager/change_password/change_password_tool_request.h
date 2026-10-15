// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_REQUEST_H_

#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/shared_types.h"
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

  ChangePasswordToolRequest(
      ActorSurfaceHandle actor_surface_handle,
      std::optional<PageTarget> current_password_field,
      std::optional<PageTarget> new_password_field,
      std::optional<PageTarget> confirm_new_password_field,
      std::optional<PageTarget> confirm_button);
  ChangePasswordToolRequest(const ChangePasswordToolRequest&);
  ChangePasswordToolRequest& operator=(const ChangePasswordToolRequest&);
  ~ChangePasswordToolRequest() override;

  // TabToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const std::optional<PageTarget>& GetCurrentPasswordFieldForTesting() const {
    return current_password_field_;
  }

  const std::optional<PageTarget>& GetNewPasswordFieldForTesting() const {
    return new_password_field_;
  }

  const std::optional<PageTarget>& GetConfirmNewPasswordFieldForTesting()
      const {
    return confirm_new_password_field_;
  }

  const std::optional<PageTarget>& GetConfirmButtonForTesting() const {
    return confirm_button_;
  }

 private:
  std::optional<PageTarget> current_password_field_;
  std::optional<PageTarget> new_password_field_;
  std::optional<PageTarget> confirm_new_password_field_;
  std::optional<PageTarget> confirm_button_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PASSWORD_MANAGER_CHANGE_PASSWORD_CHANGE_PASSWORD_TOOL_REQUEST_H_
