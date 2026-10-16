// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tool_result_data.h"

#include <optional>
#include <string>
#include <utility>

namespace actor {

ToolResultData::~ToolResultData() = default;

ToolError::ToolError(mojom::ActionResultCode code,
                     std::optional<std::string> message)
    : code(code),
      message(message && !message->empty() ? std::move(message)
                                           : std::nullopt) {}

ToolError::ToolError(const ToolError&) = default;
ToolError& ToolError::operator=(const ToolError&) = default;
ToolError::ToolError(ToolError&&) noexcept = default;
ToolError& ToolError::operator=(ToolError&&) noexcept = default;
ToolError::~ToolError() = default;

}  // namespace actor
