// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_screenshot_tool_result.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "ui/gfx/geometry/size.h"

namespace actor {

GetScreenshotToolResult::GetScreenshotToolResult(
    std::vector<uint8_t> image_data,
    std::string mime_type,
    gfx::Size dimensions)
    : image_data(std::move(image_data)),
      mime_type(std::move(mime_type)),
      dimensions(dimensions) {}

GetScreenshotToolResult::GetScreenshotToolResult(
    const GetScreenshotToolResult&) = default;
GetScreenshotToolResult& GetScreenshotToolResult::operator=(
    const GetScreenshotToolResult&) = default;
GetScreenshotToolResult::GetScreenshotToolResult(
    GetScreenshotToolResult&&) noexcept = default;
GetScreenshotToolResult& GetScreenshotToolResult::operator=(
    GetScreenshotToolResult&&) noexcept = default;
GetScreenshotToolResult::~GetScreenshotToolResult() = default;

base::DictValue GetScreenshotToolResult::ToDict() const {
  return base::DictValue()
      .Set("width", dimensions.width())
      .Set("height", dimensions.height());
}

std::optional<ToolResultImage> GetScreenshotToolResult::GetImage() const {
  return ToolResultImage{.data = image_data, .mime_type = mime_type};
}

std::unique_ptr<ToolResultData> GetScreenshotToolResult::Clone() const {
  return std::make_unique<GetScreenshotToolResult>(*this);
}

}  // namespace actor
