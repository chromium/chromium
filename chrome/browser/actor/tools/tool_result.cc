// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_result.h"

#include "base/notimplemented.h"

namespace actor {

// static
ToolResult ToolResult::Success() {
  // TODO(crbug.com/574090562): Implement status-only success result.
  NOTIMPLEMENTED();
  return ToolResult();
}

// static
ToolResult ToolResult::Success(mojom::ActionResultPtr action_result) {
  // TODO(crbug.com/574090562): Store ActionResult on ToolResult.
  NOTIMPLEMENTED();
  return ToolResult();
}

// static
ToolResult ToolResult::Error(ToolError error) {
  // TODO(crbug.com/574090562): Store ToolError on ToolResult.
  NOTIMPLEMENTED();
  return ToolResult();
}

ToolResult::ToolResult() = default;
ToolResult::ToolResult(ToolResult&&) noexcept = default;
ToolResult& ToolResult::operator=(ToolResult&&) noexcept = default;
ToolResult::~ToolResult() = default;

ToolResult ToolResult::Clone() const {
  // TODO(crbug.com/574090562): Clone ToolResult.
  NOTIMPLEMENTED();
  return ToolResult();
}

bool ToolResult::Ok() const {
  // TODO(crbug.com/574090562): Return whether the result succeeded.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace actor
