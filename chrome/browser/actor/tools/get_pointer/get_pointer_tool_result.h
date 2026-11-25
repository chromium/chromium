// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_TOOL_RESULT_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_TOOL_RESULT_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"

namespace actor {

// Result data of get_pointer: a description, for the model, of what the user
// pointed at or selected on the page, and an image of the page around it.
// ToDict() returns the description alone. Unlike in ScreenshotToolResult, the
// image is not in it; GetImage() returns it.
struct GetPointerToolResult : public ToolResultData {
  // `image_data` is a JPEG, or is empty if there is no image.
  GetPointerToolResult(base::DictValue dict, std::vector<uint8_t> image_data);
  GetPointerToolResult(GetPointerToolResult&&) noexcept;
  GetPointerToolResult& operator=(GetPointerToolResult&&) noexcept;
  ~GetPointerToolResult() override;

  // ToolResultData:
  base::DictValue ToDict() const override;
  std::optional<ToolResultImage> GetImage() const override;
  std::unique_ptr<ToolResultData> Clone() const override;

  // The description. BuildGetPointerResult() documents what is in it.
  base::DictValue dict;

  // A JPEG of the part of the page that `dict` describes. Empty if `dict`
  // says that there is no active pointer.
  std::vector<uint8_t> image_data;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_TOOL_RESULT_H_
