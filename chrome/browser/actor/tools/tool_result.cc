// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_result.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/common/actor/action_result.h"

namespace actor {

// static
ToolResult ToolResult::Success() {
  return ToolResult();
}

// static
ToolResult ToolResult::Success(mojom::ActionResultPtr action_result) {
  return ToolResult(std::move(action_result));
}

// static
ToolResult ToolResult::Error(ToolError error) {
  return ToolResult(std::move(error));
}

ToolResult::ToolResult() : action_result_(MakeOkResult()) {}

ToolResult::ToolResult(mojom::ActionResultPtr action_result)
    : action_result_(std::move(action_result)) {}

ToolResult::ToolResult(std::unique_ptr<ToolResultData> data)
    : action_result_(MakeOkResult()), data_(std::move(data)) {}

ToolResult::ToolResult(ToolError error)
    : action_result_(MakeResult(error.code,
                                /*requires_page_stabilization=*/false,
                                error.message.value_or(std::string()))),
      error_(std::move(error)) {}

ToolResult::ToolResult(ToolResult&&) noexcept = default;
ToolResult& ToolResult::operator=(ToolResult&&) noexcept = default;
ToolResult::~ToolResult() = default;

ToolResult ToolResult::Clone() const {
  ToolResult cloned(action_result_ ? action_result_->Clone() : nullptr);
  if (data_) {
    cloned.data_ = data_->Clone();
  }
  cloned.error_ = error_;
  return cloned;
}

bool ToolResult::Ok() const {
  return !error_.has_value();
}

}  // namespace actor
