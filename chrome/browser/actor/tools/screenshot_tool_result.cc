// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/screenshot_tool_result.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "ui/gfx/geometry/size.h"

namespace actor {

ScreenshotToolResult::ScreenshotToolResult(std::vector<uint8_t> image_data,
                                           std::string mime_type,
                                           gfx::Size dimensions)
    : image_data(std::move(image_data)),
      mime_type(std::move(mime_type)),
      dimensions(dimensions) {}

ScreenshotToolResult::ScreenshotToolResult(const ScreenshotToolResult&) =
    default;
ScreenshotToolResult& ScreenshotToolResult::operator=(
    const ScreenshotToolResult&) = default;
ScreenshotToolResult::ScreenshotToolResult(ScreenshotToolResult&&) noexcept =
    default;
ScreenshotToolResult& ScreenshotToolResult::operator=(
    ScreenshotToolResult&&) noexcept = default;
ScreenshotToolResult::~ScreenshotToolResult() = default;

base::DictValue ScreenshotToolResult::ToDict() const {
  return base::DictValue()
      .Set("mime_type", mime_type)
      .Set("width", dimensions.width())
      .Set("height", dimensions.height())
      .Set("image_b64", base::Base64Encode(image_data));
}

std::optional<ToolResultImage> ScreenshotToolResult::GetImage() const {
  return ToolResultImage{.data = image_data, .mime_type = mime_type};
}

std::unique_ptr<ToolResultData> ScreenshotToolResult::Clone() const {
  return std::make_unique<ScreenshotToolResult>(*this);
}

}  // namespace actor
