// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_RESULT_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_RESULT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/actor/tool_result_data.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class DictValue;
}  // namespace base

namespace actor {

// Result data of `GetScreenshotTool`.
struct GetScreenshotToolResult : public ToolResultData {
  GetScreenshotToolResult(std::vector<uint8_t> image_data,
                          std::string mime_type,
                          gfx::Size dimensions);
  GetScreenshotToolResult(const GetScreenshotToolResult&);
  GetScreenshotToolResult& operator=(const GetScreenshotToolResult&);
  GetScreenshotToolResult(GetScreenshotToolResult&&) noexcept;
  GetScreenshotToolResult& operator=(GetScreenshotToolResult&&) noexcept;
  ~GetScreenshotToolResult() override;

  // ToolResultData:
  base::DictValue ToDict() const override;
  std::optional<ToolResultImage> GetImage() const override;
  std::unique_ptr<ToolResultData> Clone() const override;

  // Encoded screenshot bytes.
  std::vector<uint8_t> image_data;

  // MIME type of `image_data`.
  std::string mime_type;

  // Dimensions of the captured screenshot.
  gfx::Size dimensions;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_RESULT_H_
