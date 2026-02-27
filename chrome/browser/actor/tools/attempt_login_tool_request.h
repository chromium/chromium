// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

class ToolRequestVisitorFunctor;

class AttemptLoginToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "AttemptLogin";

  explicit AttemptLoginToolRequest(
      tabs::TabHandle tab_handle,
      std::optional<PageTarget> password_button,
      std::optional<PageTarget> sign_in_with_google_button);
  ~AttemptLoginToolRequest() override;
  AttemptLoginToolRequest(const AttemptLoginToolRequest&);
  AttemptLoginToolRequest& operator=(const AttemptLoginToolRequest&);

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
  bool RequiresOpeningWebContents() const override;

  std::optional<PageTarget> GetPasswordButtonForTesting() const {
    return password_button_;
  }

  std::optional<PageTarget> GetSignInWithGoogleButtonForTesting() const {
    return sign_in_with_google_button_;
  }

 private:
  std::optional<PageTarget> password_button_;
  std::optional<PageTarget> sign_in_with_google_button_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_LOGIN_TOOL_REQUEST_H_
