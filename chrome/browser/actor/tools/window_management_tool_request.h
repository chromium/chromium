// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_REQUEST_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Creates a new blank window.
class CreateWindowToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "CreateWindow";
  static constexpr std::string_view kModelFacingName = "create_window";

  CreateWindowToolRequest();
  ~CreateWindowToolRequest() override;

  // Returns the `ToolId::kCreateWindow` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  std::string_view Name() const override;
};

// Brings the specified window to the foreground.
class ActivateWindowToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "ActivateWindow";
  static constexpr std::string_view kModelFacingName = "activate_window";
  static constexpr std::string_view kWindowIdParam = "window_id";

  explicit ActivateWindowToolRequest(int32_t window_id);
  ~ActivateWindowToolRequest() override;

  int32_t GetWindowId() const { return window_id_; }

  // Returns the `ToolId::kActivateWindow` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

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
  static constexpr std::string_view kModelFacingName = "close_window";
  static constexpr std::string_view kWindowIdParam = "window_id";

  explicit CloseWindowToolRequest(int32_t window_id);
  ~CloseWindowToolRequest() override;

  int32_t GetWindowId() const { return window_id_; }

  // Returns the `ToolId::kCloseWindow` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  int32_t window_id_;
};

// Enters fullscreen mode for a window.
class EnterFullscreenToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "EnterFullscreen";
  static constexpr std::string_view kModelFacingName = "enter_fullscreen";

  explicit EnterFullscreenToolRequest(int32_t window_id);
  ~EnterFullscreenToolRequest() override;

  // Returns the `ToolId::kEnterFullscreen` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  int32_t GetWindowId() const { return window_id_; }

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  int32_t window_id_;
};

// Exits fullscreen mode for a window.
class ExitFullscreenToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "ExitFullscreen";
  static constexpr std::string_view kModelFacingName = "exit_fullscreen";

  explicit ExitFullscreenToolRequest(int32_t window_id);
  ~ExitFullscreenToolRequest() override;

  // Returns the `ToolId::kExitFullscreen` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

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
