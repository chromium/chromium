// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_REQUEST_H_

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

// A fake tool request that creates a FakeTool.
class FakeToolRequest : public ToolRequest {
 public:
  // Transitional constructor for tests still using `DeprecatedToolCallback`.
  // New code should use the `ToolResult` constructor below.
  // TODO(crbug.com/574534362): Remove once all tests migrate to `ToolResult`.
  FakeToolRequest(
      base::OnceCallback<void(DeprecatedToolCallback)> on_invoke,
      base::OnceClosure on_destroy,
      ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null());
  explicit FakeToolRequest(
      ToolResult result,
      base::OnceClosure on_destroy = base::OnceClosure(),
      ActorSurfaceHandle actor_surface_handle = ActorSurfaceHandle::Null());
  ~FakeToolRequest() override;
  FakeToolRequest(const FakeToolRequest&) = delete;
  FakeToolRequest& operator=(const FakeToolRequest&) = delete;

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
  ActorSurfaceHandle GetActorSurfaceHandle() const override;

 private:
  mutable base::OnceCallback<void(DeprecatedToolCallback)> on_invoke_;
  mutable base::OnceCallback<void(ToolResultCallback)> on_invoke_result_;
  mutable base::OnceClosure on_destroy_;
  ActorSurfaceHandle actor_surface_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_FAKE_TOOL_REQUEST_H_
