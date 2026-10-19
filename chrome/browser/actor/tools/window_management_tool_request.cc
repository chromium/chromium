// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/window_management_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>

#include "build/build_config.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/window_management_tool.h"
#include "chrome/common/actor/action_result.h"

namespace actor {
namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr std::string_view kCreateWindowToolDescription =
    "Opens a new browser window.";

constexpr std::string_view kActivateWindowToolDescription =
    "Brings the specified window to the foreground.";

constexpr std::string_view kWindowIdParamDescription =
    "The ID of the window to activate.";

constexpr std::string_view kCloseWindowToolDescription =
    "Closes the specified window.";

constexpr std::string_view kCloseWindowIdParamDescription =
    "The ID of the window to close.";
#endif  // !BUILDFLAG(IS_ANDROID)

constexpr std::string_view kEnterFullscreenToolDescription =
    "Enter full screen mode for the active browser window.";

constexpr std::string_view kExitFullscreenToolDescription =
    "Exit full screen mode for the active browser window.";

}  // namespace

// TODO(crbug.com/470475787): These requests aren't in ToolRequestVariant on
// Android yet, so the visitor has no overloads for them there.
#if !BUILDFLAG(IS_ANDROID)
// CreateWindowToolRequest
CreateWindowToolRequest::CreateWindowToolRequest() = default;
CreateWindowToolRequest::~CreateWindowToolRequest() = default;

// static
std::optional<ToolDefinition> CreateWindowToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kCreateWindow, kModelFacingName,
                               kCreateWindowToolDescription)
      .Build();
}

ToolRequest::CreateToolResult CreateWindowToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WindowManagementTool>(task_id, tool_delegate),
          MakeOkResult()};
}

void CreateWindowToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CreateWindowToolRequest::Name() const {
  return kName;
}

// ActivateWindowToolRequest
ActivateWindowToolRequest::ActivateWindowToolRequest(int32_t window_id)
    : window_id_(window_id) {}
ActivateWindowToolRequest::~ActivateWindowToolRequest() = default;

// static
std::optional<ToolDefinition> ActivateWindowToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kActivateWindow, kModelFacingName,
                               kActivateWindowToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kWindowIdParam, kWindowIdParamDescription))
      .Build();
}

ToolRequest::CreateToolResult ActivateWindowToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WindowManagementTool>(
              WindowManagementTool::Action::kActivate, task_id, tool_delegate,
              GetWindowId()),
          MakeOkResult()};
}

void ActivateWindowToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ActivateWindowToolRequest::Name() const {
  return kName;
}

// CloseWindowToolRequest
CloseWindowToolRequest::CloseWindowToolRequest(int32_t window_id)
    : window_id_(window_id) {}
CloseWindowToolRequest::~CloseWindowToolRequest() = default;

// static
std::optional<ToolDefinition> CloseWindowToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kCloseWindow, kModelFacingName,
                               kCloseWindowToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kWindowIdParam, kCloseWindowIdParamDescription))
      .Build();
}

ToolRequest::CreateToolResult CloseWindowToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WindowManagementTool>(
              WindowManagementTool::Action::kClose, task_id, tool_delegate,
              GetWindowId()),
          MakeOkResult()};
}

void CloseWindowToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CloseWindowToolRequest::Name() const {
  return kName;
}
#endif  // !BUILDFLAG(IS_ANDROID)

// EnterFullscreenToolRequest
EnterFullscreenToolRequest::EnterFullscreenToolRequest(int32_t window_id)
    : window_id_(window_id) {}
EnterFullscreenToolRequest::~EnterFullscreenToolRequest() = default;

// static
std::optional<ToolDefinition> EnterFullscreenToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kEnterFullscreen, kModelFacingName,
                               kEnterFullscreenToolDescription)
      .Build();
}

ToolRequest::CreateToolResult EnterFullscreenToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WindowManagementTool>(
              WindowManagementTool::Action::kEnterFullscreen, task_id,
              tool_delegate, GetWindowId()),
          MakeOkResult()};
}

void EnterFullscreenToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view EnterFullscreenToolRequest::Name() const {
  return kName;
}

// ExitFullscreenToolRequest
ExitFullscreenToolRequest::ExitFullscreenToolRequest(int32_t window_id)
    : window_id_(window_id) {}
ExitFullscreenToolRequest::~ExitFullscreenToolRequest() = default;

// static
std::optional<ToolDefinition> ExitFullscreenToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kExitFullscreen, kModelFacingName,
                               kExitFullscreenToolDescription)
      .Build();
}

ToolRequest::CreateToolResult ExitFullscreenToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WindowManagementTool>(
              WindowManagementTool::Action::kExitFullscreen, task_id,
              tool_delegate, GetWindowId()),
          MakeOkResult()};
}

void ExitFullscreenToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ExitFullscreenToolRequest::Name() const {
  return kName;
}

}  // namespace actor
