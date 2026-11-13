// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/fake_tool_request.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/fake_tool.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

FakeToolRequest::FakeToolRequest(
    base::OnceCallback<void(DeprecatedToolCallback)> on_invoke,
    base::OnceClosure on_destroy,
    ActorSurfaceHandle actor_surface_handle)
    : on_invoke_(std::move(on_invoke)),
      on_destroy_(std::move(on_destroy)),
      actor_surface_handle_(actor_surface_handle) {}

FakeToolRequest::FakeToolRequest(ToolResult result,
                                 base::OnceClosure on_destroy,
                                 ActorSurfaceHandle actor_surface_handle)
    : on_invoke_result_(base::BindOnce(
          [](ToolResult result, ToolResultCallback callback) {
            std::move(callback).Run(std::move(result));
          },
          std::move(result))),
      on_destroy_(std::move(on_destroy)),
      actor_surface_handle_(actor_surface_handle) {}

FakeToolRequest::~FakeToolRequest() = default;

ToolRequest::CreateToolResult FakeToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (on_invoke_result_) {
    return {std::make_unique<FakeTool>(task_id, tool_delegate,
                                       std::move(on_invoke_result_),
                                       std::move(on_destroy_)),
            MakeOkResult()};
  }
  return {
      std::make_unique<FakeTool>(task_id, tool_delegate, std::move(on_invoke_),
                                 std::move(on_destroy_)),
      MakeOkResult()};
}

void FakeToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(WaitToolRequest(base::Seconds(0)));
}

std::string_view FakeToolRequest::Name() const {
  return "FakeTool";
}

ActorSurfaceHandle FakeToolRequest::GetActorSurfaceHandle() const {
  return actor_surface_handle_;
}

}  // namespace actor
