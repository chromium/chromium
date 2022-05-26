// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/ui/suggestion_view.h"

#include <stddef.h>

#include <string>

#include "chrome/browser/ash/input_method/ui/suggestion_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

namespace ui {
namespace ime {
namespace {

class SuggestionViewTest : public views::ViewsTestBase {
 public:
  SuggestionViewTest() = default;
};

TEST_F(SuggestionViewTest, AnchorOriginIsPadding) {
  SuggestionView suggestion({});
  suggestion.SetView({
      .text = u"good",
  });

  EXPECT_EQ(suggestion.GetAnchorOrigin(), gfx::Point(kPadding, 0));
}

}  // namespace
}  // namespace ime
}  // namespace ui
