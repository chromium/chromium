// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/get_pointer_tool_result.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/values.h"
#include "chrome/browser/actor/tool_result_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

TEST(GetPointerToolResultTest, DictAndImage) {
  const std::vector<uint8_t> jpeg = {0xFF, 0xD8, 0xFF, 0xD9};
  const GetPointerToolResult result(base::DictValue().Set("status", "ok"),
                                    jpeg);

  EXPECT_EQ(result.ToDict(), base::DictValue().Set("status", "ok"));
  const std::optional<ToolResultImage> image = result.GetImage();
  ASSERT_TRUE(image);
  EXPECT_THAT(image->data, testing::ElementsAreArray(jpeg));
  EXPECT_EQ(image->mime_type, "image/jpeg");
}

// Without image data there is no image, as when the user is not pointing.
TEST(GetPointerToolResultTest, NoImage) {
  const GetPointerToolResult result(
      base::DictValue().Set("status", "no_active_pointer"), {});
  EXPECT_FALSE(result.GetImage());
}

TEST(GetPointerToolResultTest, Clone) {
  const std::vector<uint8_t> jpeg = {1, 2, 3};
  const GetPointerToolResult original(base::DictValue().Set("status", "ok"),
                                      jpeg);

  const std::unique_ptr<ToolResultData> clone = original.Clone();
  ASSERT_TRUE(clone);
  const GetPointerToolResult* typed = clone->As<GetPointerToolResult>();
  EXPECT_NE(typed, &original);
  EXPECT_EQ(typed->dict, original.dict);
  EXPECT_EQ(typed->image_data, jpeg);
}

}  // namespace
}  // namespace actor
