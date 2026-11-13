// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_RESULT_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_RESULT_H_

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

// Result of a `Tool::Invoke` execution.
// TODO(crbug.com/574534362): Work in progress to replace
// `mojom::ActionResultPtr` as the return type of `Tool::Invoke`.
class ToolResult {
 public:
  // Creates a successful result with no data.
  static ToolResult Success();

  // Creates a successful result with an ActionResult.
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  static ToolResult Success(mojom::ActionResultPtr action_result);

  // Creates a result from an ActionResult.
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  static ToolResult FromDeprecatedActionResult(
      mojom::ActionResultPtr action_result);

  // Creates a successful result with data.
  template <typename T>
    requires(std::derived_from<T, ToolResultData> &&
             !std::same_as<T, ToolResultData>)
  static ToolResult Success(T data) {
    return ToolResult(std::make_unique<T>(std::move(data)));
  }

  // Creates an error result.
  static ToolResult Error(ToolError error);

  ToolResult(ToolResult&&) noexcept;
  ToolResult& operator=(ToolResult&&) noexcept;
  ToolResult(const ToolResult&) = delete;
  ToolResult& operator=(const ToolResult&) = delete;
  ~ToolResult();

  // Clones this result.
  ToolResult Clone() const;

  // Returns whether the tool succeeded.
  bool Ok() const;

  // Returns the error, or nullopt if the tool succeeded.
  const std::optional<ToolError>& error() const { return error_; }

  // Returns the result data, or nullptr if none.
  const ToolResultData* data() const { return data_.get(); }

  // Returns the ActionResult.
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  const mojom::ActionResultPtr& deprecated_action_result() const {
    return deprecated_action_result_;
  }
  mojom::ActionResultPtr& deprecated_action_result() {
    return deprecated_action_result_;
  }

  // Sets the ActionResult.
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  void set_deprecated_action_result(mojom::ActionResultPtr action_result) {
    deprecated_action_result_ = std::move(action_result);
  }

 private:
  ToolResult();
  explicit ToolResult(mojom::ActionResultPtr action_result);
  explicit ToolResult(std::unique_ptr<ToolResultData> data);
  explicit ToolResult(ToolError error);

  mojom::ActionResultPtr deprecated_action_result_;
  std::unique_ptr<ToolResultData> data_;
  std::optional<ToolError> error_;
};

// The result of each action with latency information.
// TODO(crbug.com/584876714): Replace `ToolResultWithLatencyInfo` and
// `std::vector<ToolResultWithLatencyInfo>` with the new browser-side
// `actor::ActionResult` and `actor::ActionsResult` types.
struct ToolResultWithLatencyInfo {
  base::TimeTicks start_time;
  base::TimeTicks end_time;
  ToolResult tool_result;

  ToolResultWithLatencyInfo(base::TimeTicks start_time,
                            base::TimeTicks end_time,
                            ToolResult tool_result);
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  ToolResultWithLatencyInfo(base::TimeTicks start_time,
                            base::TimeTicks end_time,
                            mojom::ActionResultPtr result);
  ToolResultWithLatencyInfo(ToolResultWithLatencyInfo&&) noexcept;
  ToolResultWithLatencyInfo& operator=(ToolResultWithLatencyInfo&&) noexcept;
  ToolResultWithLatencyInfo(const ToolResultWithLatencyInfo&);
  ToolResultWithLatencyInfo& operator=(const ToolResultWithLatencyInfo&) =
      delete;
  ~ToolResultWithLatencyInfo();
};

// Returns a singleton result vector using the given `tool_result`.
// TODO(crbug.com/584876714): Remove once `ToolResultWithLatencyInfo` is
// replaced with `actor::ActionResult` and `actor::ActionsResult`.
std::vector<ToolResultWithLatencyInfo> MakeResultVector(ToolResult tool_result);

// Assigns `error_code` and `index_of_failed_action` to the first member of
// `results` with a non-Ok result, if any.
// TODO(crbug.com/584876714): Remove once `ToolResultWithLatencyInfo` is
// replaced with `actor::ActionResult` and `actor::ActionsResult`.
void ExtractErrorResult(base::span<const ToolResultWithLatencyInfo> results,
                        mojom::ActionResultCode& error_code,
                        std::optional<size_t>& index_of_failed_action);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_RESULT_H_
