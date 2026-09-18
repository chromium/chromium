// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_TOOL_TYPES_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_TOOL_TYPES_H_

#include <optional>
#include <string>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"

namespace ttc {

// Defines a tool registered with the model execution backend.
struct ToolDefinition {
  // Behavior specifies whether model execution should block waiting for tool
  // results or continue asynchronously.
  enum class Behavior {
    // Model execution blocks until tool execution completes and returns
    // results.
    kBlocking,
    // Model execution continues without blocking on tool execution results.
    kNonBlocking,
  };

  // Verbalization specifies whether the model should speak or describe its
  // action to the user while/after executing the tool.
  enum class Verbalization {
    // Standard verbalization where the model can speak or acknowledge tool
    // actions.
    kStandard,
    // Silent action where the tool executes without spoken verbalization from
    // the model.
    kSilentAction,
  };

  ToolDefinition();
  ~ToolDefinition();
  ToolDefinition(ToolDefinition&&);
  ToolDefinition& operator=(ToolDefinition&&);
  ToolDefinition(const ToolDefinition&) = delete;
  ToolDefinition& operator=(const ToolDefinition&) = delete;

  ToolDefinition Clone() const;

  std::string name;
  std::string description;
  base::DictValue parameters_json_schema;
  Behavior behavior = Behavior::kBlocking;
  Verbalization verbalization = Verbalization::kStandard;
};

// Parameters for a tool call.
struct ToolRequest {
  std::string name;
  base::DictValue arguments;
};

// Details of a failed tool call. At least one of the fields is always set.
struct ToolError {
  // The actor error code, if the failure came from an actor tool. Never kOk.
  std::optional<actor::mojom::ActionResultCode> code;

  // An English language description of the failure. If absent, `code` is set
  // and callers should describe the failure using it.
  std::optional<std::string> message;
};

// The result of executing a tool: either a success, which may carry a
// tool-specific result dict, or a failure, which carries an actor error code
// and/or a message. Instances can only be created through the factories below.
class ToolResponse {
 public:
  // Returns a response for a tool call that succeeded. `result` holds
  // tool-specific values to report back to the model, and may be empty.
  static ToolResponse Success(base::DictValue result = base::DictValue());

  // Returns a response for a tool call that failed with actor error `code`,
  // which must not be kOk. An empty `message` is treated as absent, leaving
  // the failure described by `code` alone.
  static ToolResponse Error(actor::mojom::ActionResultCode code,
                            std::string message = std::string());

  // Returns a response for a tool call that failed for a reason that has no
  // corresponding actor error code. `message` must not be empty.
  static ToolResponse Error(std::string message);

  ToolResponse(ToolResponse&&);
  ToolResponse& operator=(ToolResponse&&);
  ~ToolResponse();

  ToolResponse(const ToolResponse&) = delete;
  ToolResponse& operator=(const ToolResponse&) = delete;

  // Whether the tool call succeeded.
  bool Ok() const;

  // The tool-specific result values, which may be empty. Valid only if Ok().
  const base::DictValue& GetResult() const;
  base::DictValue TakeResult() &&;

  // Details of the failure. Valid only if !Ok().
  const ToolError& error() const;

 private:
  explicit ToolResponse(std::variant<base::DictValue, ToolError> value);

  std::variant<base::DictValue, ToolError> value_;
};

using ToolResponseCallback = base::OnceCallback<void(ToolResponse)>;

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_TOOL_TYPES_H_
