// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TOOL_RESULT_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TOOL_RESULT_H_

#include <concepts>
#include <memory>
#include <optional>
#include <utility>

#include "chrome/browser/actor/tool_result_data.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

// Result of a tool execution.
class ToolResult {
 public:
  // Creates a successful result with no data.
  static ToolResult Success();

  // Creates a successful result with an ActionResult.
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  static ToolResult Success(mojom::ActionResultPtr action_result);

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
  const mojom::ActionResultPtr& action_result() const { return action_result_; }

  // Sets the ActionResult.
  // TODO(crbug.com/574534362): Remove once all tools migrate off
  // mojom::ActionResult.
  void set_action_result(mojom::ActionResultPtr action_result) {
    action_result_ = std::move(action_result);
  }

 private:
  ToolResult();
  explicit ToolResult(mojom::ActionResultPtr action_result);
  explicit ToolResult(std::unique_ptr<ToolResultData> data);
  explicit ToolResult(ToolError error);

  mojom::ActionResultPtr action_result_;
  std::unique_ptr<ToolResultData> data_;
  std::optional<ToolError> error_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TOOL_RESULT_H_
