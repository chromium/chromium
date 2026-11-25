// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/get_pointer_tool_result.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"

namespace actor {

GetPointerToolResult::GetPointerToolResult(base::DictValue dict,
                                           std::vector<uint8_t> image_data)
    : dict(std::move(dict)), image_data(std::move(image_data)) {}

GetPointerToolResult::GetPointerToolResult(GetPointerToolResult&&) noexcept =
    default;
GetPointerToolResult& GetPointerToolResult::operator=(
    GetPointerToolResult&&) noexcept = default;
GetPointerToolResult::~GetPointerToolResult() = default;

base::DictValue GetPointerToolResult::ToDict() const {
  return dict.Clone();
}

std::optional<ToolResultImage> GetPointerToolResult::GetImage() const {
  if (image_data.empty()) {
    return std::nullopt;
  }
  return ToolResultImage{.data = image_data, .mime_type = "image/jpeg"};
}

std::unique_ptr<ToolResultData> GetPointerToolResult::Clone() const {
  return std::make_unique<GetPointerToolResult>(dict.Clone(), image_data);
}

}  // namespace actor
