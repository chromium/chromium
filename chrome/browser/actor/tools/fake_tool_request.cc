// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/fake_tool_request.h"

#include "chrome/browser/actor/tools/fake_tool.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

FakeToolRequest::FakeToolRequest(
    base::OnceCallback<void(ToolCallback)> on_invoke,
    base::OnceClosure on_destroy)
    : on_invoke_(std::move(on_invoke)), on_destroy_(std::move(on_destroy)) {}

FakeToolRequest::~FakeToolRequest() = default;

ToolRequest::CreateToolResult FakeToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {
      std::make_unique<FakeTool>(task_id, tool_delegate, std::move(on_invoke_),
                                 std::move(on_destroy_)),
      MakeOkResult()};
}

void FakeToolRequest::Apply(ToolRequestVisitorFunctor& f) const {}

std::string_view FakeToolRequest::Name() const {
  return "FakeTool";
}
}  // namespace actor
