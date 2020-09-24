// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"

#include "ash/assistant/ui/test_support/mock_assistant_view_delegate.h"
#include "ash/assistant/util/test_support/macros.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

namespace {

using chromeos::assistant::AssistantSuggestion;

// Helpers ---------------------------------------------------------------------

AssistantSuggestion CreateSuggestionWithIconUrl(const std::string& icon_url) {
  AssistantSuggestion suggestion;
  suggestion.icon_url = GURL(icon_url);
  return suggestion;
}

}  // namespace

// Tests -----------------------------------------------------------------------

using SuggestionChipViewTest = AshTestBase;

TEST_F(SuggestionChipViewTest, ShouldHandleLocalIcons) {
  SuggestionChipView suggestion_chip_view(
      /*delegate=*/nullptr,
      CreateSuggestionWithIconUrl(
          "googleassistant://resource?type=icon&name=assistant"),
      /*listener=*/nullptr);

  const auto& actual = suggestion_chip_view.GetIcon();
  gfx::ImageSkia expected = gfx::CreateVectorIcon(
      gfx::IconDescription(chromeos::kAssistantIcon, /*size=*/16));

  ASSERT_PIXELS_EQ(actual, expected);
}

TEST_F(SuggestionChipViewTest, ShouldHandleRemoteIcons) {
  const gfx::ImageSkia expected =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);

  MockAssistantViewDelegate delegate;
  EXPECT_CALL(delegate, DownloadImage)
      .WillOnce(testing::Invoke(
          [&](const GURL& url, ImageDownloader::DownloadCallback callback) {
            std::move(callback).Run(expected);
          }));

  SuggestionChipView suggestion_chip_view(
      &delegate,
      CreateSuggestionWithIconUrl("https://www.gstatic.com/images/branding/"
                                  "product/2x/googleg_48dp.png"),
      /*listener=*/nullptr);

  const auto& actual = suggestion_chip_view.GetIcon();
  EXPECT_TRUE(actual.BackedBySameObjectAs(expected));
}

}  // namespace ash
