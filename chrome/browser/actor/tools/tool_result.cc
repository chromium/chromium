// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_result.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
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
ToolResult ToolResult::FromDeprecatedActionResult(
    mojom::ActionResultPtr action_result) {
  CHECK(action_result);
  if (IsOk(*action_result)) {
    return ToolResult::Success(std::move(action_result));
  }
  ToolResult tool_result(std::move(action_result));
  tool_result.error_.emplace(tool_result.deprecated_action_result_->code,
                             tool_result.deprecated_action_result_->message);
  return tool_result;
}

// static
ToolResult ToolResult::Error(ToolError error) {
  return ToolResult(std::move(error));
}

ToolResult::ToolResult() : deprecated_action_result_(MakeOkResult()) {}

ToolResult::ToolResult(mojom::ActionResultPtr action_result)
    : deprecated_action_result_(std::move(action_result)) {}

ToolResult::ToolResult(std::unique_ptr<ToolResultData> data)
    : deprecated_action_result_(MakeOkResult()), data_(std::move(data)) {}

ToolResult::ToolResult(ToolError error)
    : deprecated_action_result_(
          MakeResult(error.code,
                     /*requires_page_stabilization=*/false,
                     error.message.value_or(std::string()))),
      error_(std::move(error)) {}

ToolResult::ToolResult(ToolResult&&) noexcept = default;
ToolResult& ToolResult::operator=(ToolResult&&) noexcept = default;
ToolResult::~ToolResult() = default;

ToolResult ToolResult::Clone() const {
  ToolResult cloned(
      deprecated_action_result_ ? deprecated_action_result_->Clone() : nullptr);
  if (data_) {
    cloned.data_ = data_->Clone();
  }
  cloned.error_ = error_;
  return cloned;
}

bool ToolResult::Ok() const {
  return !error_.has_value();
}

ToolResultWithLatencyInfo::ToolResultWithLatencyInfo(base::TimeTicks start_time,
                                                     base::TimeTicks end_time,
                                                     ToolResult tool_result)
    : start_time(start_time),
      end_time(end_time),
      tool_result(std::move(tool_result)) {
  CHECK(!start_time.is_null());
  CHECK(!end_time.is_null());
  CHECK_LE(start_time, end_time);
}

ToolResultWithLatencyInfo::ToolResultWithLatencyInfo(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    mojom::ActionResultPtr result)
    : ToolResultWithLatencyInfo(
          start_time,
          end_time,
          ToolResult::FromDeprecatedActionResult(std::move(result))) {}

ToolResultWithLatencyInfo::ToolResultWithLatencyInfo(
    const ToolResultWithLatencyInfo& other)
    : start_time(other.start_time),
      end_time(other.end_time),
      tool_result(other.tool_result.Clone()) {}
ToolResultWithLatencyInfo::~ToolResultWithLatencyInfo() = default;
ToolResultWithLatencyInfo::ToolResultWithLatencyInfo(
    ToolResultWithLatencyInfo&&) noexcept = default;
ToolResultWithLatencyInfo& ToolResultWithLatencyInfo::operator=(
    ToolResultWithLatencyInfo&&) noexcept = default;

std::vector<ToolResultWithLatencyInfo> MakeResultVector(
    ToolResult tool_result) {
  const auto now = base::TimeTicks::Now();
  std::vector<ToolResultWithLatencyInfo> results;
  results.emplace_back(now, now, std::move(tool_result));
  return results;
}

void ExtractErrorResult(base::span<const ToolResultWithLatencyInfo> results,
                        mojom::ActionResultCode& error_code,
                        std::optional<size_t>& index_of_failed_action) {
  for (size_t i = 0; i < results.size(); ++i) {
    if (!results[i].tool_result.Ok()) {
      error_code = results[i].tool_result.error()->code;
      index_of_failed_action = i;
      return;
    }
  }
}

}  // namespace actor
