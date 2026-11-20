// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/screenshot_tool_result.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace actor {
namespace {

TEST(ScreenshotToolResultTest, ToDict) {
  const std::vector<uint8_t> image_bytes = {0xFF, 0xD8, 0xFF, 0xD9};
  ScreenshotToolResult result(image_bytes, "image/jpeg", gfx::Size(800, 600));

  EXPECT_EQ(result.ToDict(),
            base::DictValue()
                .Set("mime_type", "image/jpeg")
                .Set("width", 800)
                .Set("height", 600)
                .Set("image_b64", base::Base64Encode(image_bytes)));
}

TEST(ScreenshotToolResultTest, GetImage) {
  const std::vector<uint8_t> image_bytes = {0xFF, 0xD8, 0xFF, 0xD9};
  ScreenshotToolResult result(image_bytes, "image/jpeg", gfx::Size(800, 600));

  std::optional<ToolResultImage> image = result.GetImage();
  ASSERT_TRUE(image.has_value());
  EXPECT_THAT(image->data, testing::ElementsAreArray(image_bytes));
  EXPECT_EQ(image->mime_type, "image/jpeg");
}

TEST(ScreenshotToolResultTest, Clone) {
  const std::vector<uint8_t> image_bytes = {0x01, 0x02, 0x03};
  ScreenshotToolResult original(image_bytes, "image/jpeg",
                                gfx::Size(1024, 768));

  std::unique_ptr<ToolResultData> cloned = original.Clone();
  ASSERT_NE(cloned, nullptr);

  const ScreenshotToolResult* typed = cloned->As<ScreenshotToolResult>();
  ASSERT_NE(typed, nullptr);
  EXPECT_NE(typed, &original);
  EXPECT_EQ(typed->image_data, image_bytes);
  EXPECT_EQ(typed->mime_type, "image/jpeg");
  EXPECT_EQ(typed->dimensions, gfx::Size(1024, 768));
}

TEST(ScreenshotToolResultTest, ToolResultSuccessAndClone) {
  const std::vector<uint8_t> image_bytes = {0xAA, 0xBB, 0xCC};
  ToolResult tool_result = ToolResult::Success(
      ScreenshotToolResult(image_bytes, "image/jpeg", gfx::Size(640, 480)));

  EXPECT_TRUE(tool_result.Ok());
  ASSERT_NE(tool_result.data(), nullptr);

  const ScreenshotToolResult* payload =
      tool_result.data()->As<ScreenshotToolResult>();
  ASSERT_NE(payload, nullptr);
  EXPECT_EQ(payload->image_data, image_bytes);
  EXPECT_EQ(payload->mime_type, "image/jpeg");
  EXPECT_EQ(payload->dimensions, gfx::Size(640, 480));

  ToolResult cloned_result = tool_result.Clone();
  ASSERT_NE(cloned_result.data(), nullptr);
  const ScreenshotToolResult* cloned_payload =
      cloned_result.data()->As<ScreenshotToolResult>();
  ASSERT_NE(cloned_payload, nullptr);
  EXPECT_NE(cloned_payload, payload);
  EXPECT_EQ(cloned_payload->image_data, image_bytes);
  EXPECT_EQ(cloned_payload->mime_type, "image/jpeg");
  EXPECT_EQ(cloned_payload->dimensions, gfx::Size(640, 480));
}

}  // namespace
}  // namespace actor
