// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/window_management_tool_request.h"

#include <memory>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/window_management_tool.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

// CreateWindowToolRequest
CreateWindowToolRequest::CreateWindowToolRequest() = default;
CreateWindowToolRequest::~CreateWindowToolRequest() = default;

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

// EnterFullscreenToolRequest
EnterFullscreenToolRequest::EnterFullscreenToolRequest(int32_t window_id)
    : window_id_(window_id) {}
EnterFullscreenToolRequest::~EnterFullscreenToolRequest() = default;

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
