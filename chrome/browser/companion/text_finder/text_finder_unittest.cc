// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_finder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {

class TextFinderTest : public testing::Test {
 public:
  TextFinderTest() = default;
  ~TextFinderTest() override = default;

  void SetFindingState(std::pair<std::string, bool> text_found) {
    is_found_ = text_found.second;
  }

 protected:
  bool is_found_ = false;
};

TEST_F(TextFinderTest, FoundTextTest) {
  gfx::Rect rect(2, 4);
  TextFinder finder("abc,def");
  finder.SetDidFinishHandler(
      base::BindOnce(&TextFinderTest::SetFindingState, base::Unretained(this)));
  finder.DidFinishAttachment(rect);
  EXPECT_TRUE(is_found_);
}

TEST_F(TextFinderTest, NotFoundTextTest) {
  // Empty
  gfx::Rect rect;
  TextFinder finder("abc,def");
  finder.SetDidFinishHandler(
      base::BindOnce(&TextFinderTest::SetFindingState, base::Unretained(this)));
  finder.DidFinishAttachment(rect);
  EXPECT_FALSE(is_found_);
}

}  // namespace companion
