// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/fake_tool.h"

#include "chrome/common/actor/action_result.h"

namespace actor {

FakeTool::FakeTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   base::OnceCallback<void(ToolCallback)> on_invoke,
                   base::OnceClosure on_destroy)
    : Tool(task_id, tool_delegate),
      on_invoke_(std::move(on_invoke)),
      on_destroy_(std::move(on_destroy)) {}

FakeTool::~FakeTool() {
  if (on_destroy_) {
    std::move(on_destroy_).Run();
  }
}

void FakeTool::Validate(ToolCallback callback) {
  std::move(callback).Run(MakeOkResult());
}

void FakeTool::Invoke(ToolCallback callback) {
  if (on_invoke_) {
    std::move(on_invoke_).Run(std::move(callback));
  }
}

std::string FakeTool::DebugString() const {
  return "FakeTool";
}

std::string FakeTool::JournalEvent() const {
  return "FakeTool";
}

std::unique_ptr<ObservationDelayController> FakeTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return nullptr;
}

tabs::TabHandle FakeTool::GetTargetTab() const {
  return tabs::TabHandle::Null();
}

}  // namespace actor
