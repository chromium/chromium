// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOL_RESULT_DATA_H_
#define CHROME_BROWSER_ACTOR_TOOL_RESULT_DATA_H_

#include <concepts>
#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"

namespace actor {

// Base struct for tool result data.
struct ToolResultData {
  virtual ~ToolResultData();

  // Converts this data to a dictionary.
  virtual base::DictValue ToDict() const = 0;

  // Clones this data.
  virtual std::unique_ptr<ToolResultData> Clone() const = 0;

  // Casts this data to T.
  template <typename T>
    requires(std::derived_from<T, ToolResultData> &&
             !std::same_as<T, ToolResultData>)
  const T* As() const {
    return static_cast<const T*>(this);
  }
};

// Error from a failed tool execution.
struct ToolError {
  explicit ToolError(mojom::ActionResultCode code,
                     std::optional<std::string> message = std::nullopt);
  ToolError(const ToolError&);
  ToolError& operator=(const ToolError&);
  ToolError(ToolError&&) noexcept;
  ToolError& operator=(ToolError&&) noexcept;
  ~ToolError();

  // Error code.
  mojom::ActionResultCode code;

  // Error message.
  std::optional<std::string> message;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOL_RESULT_DATA_H_
