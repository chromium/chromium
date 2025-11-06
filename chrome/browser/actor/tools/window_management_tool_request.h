// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_REQUEST_H_

#include <string>

#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Creates a new blank window.
class CreateWindowToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "CreateWindow";

  CreateWindowToolRequest();
  ~CreateWindowToolRequest() override;

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  std::string_view Name() const override;
};

// Brings the specified window to the foreground.
class ActivateWindowToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "ActivateWindow";

  explicit ActivateWindowToolRequest(int32_t window_id);
  ~ActivateWindowToolRequest() override;

  int32_t GetWindowId() const { return window_id_; }

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  int32_t window_id_;
};

// Closes the specified window.
class CloseWindowToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "CloseWindow";

  explicit CloseWindowToolRequest(int32_t window_id);
  ~CloseWindowToolRequest() override;

  int32_t GetWindowId() const { return window_id_; }

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  int32_t window_id_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_REQUEST_H_
