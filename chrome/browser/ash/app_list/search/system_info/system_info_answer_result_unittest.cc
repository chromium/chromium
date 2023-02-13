// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/system_info/system_info_answer_result.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace app_list::test {
namespace {

// Creates a 50x50 yellow test icon.
gfx::ImageSkia GetTestIcon() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(50, 50);
  bitmap.eraseColor(SK_ColorYELLOW);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}
}  // namespace

class SystemInfoAnswerResultTest : public testing::Test {
 public:
  SystemInfoAnswerResultTest() {
    profile_ = std::make_unique<TestingProfile>();
  }

  ~SystemInfoAnswerResultTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(SystemInfoAnswerResultTest, Settings) {
  SystemInfoAnswerResult result(
      profile_.get(), u"query", "path", GetTestIcon(), 0.8,
      u"Version 108.0.5359.37 (Official Build) beta (64-bit)",
      u"Click here to check for updates",
      SystemInfoAnswerResult::AnswerCardDisplayType::kTextCard,
      SystemInfoAnswerResult::SystemInfoCategory::kSettings);
  EXPECT_EQ(result.title(),
            u"Version 108.0.5359.37 (Official Build) beta (64-bit)");
  EXPECT_EQ(result.id(), "os-settings://path");
  EXPECT_EQ(result.display_type(), DisplayType::kAnswerCard);
  EXPECT_EQ(result.category(), Category::kSettings);
  EXPECT_EQ(result.result_type(), ResultType::kSystemInfo);
  EXPECT_EQ(result.metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(result.icon().dimension, kAppIconDimension);
  EXPECT_EQ(result.icon().shape, ash::SearchResultIconShape::kDefault);
  EXPECT_TRUE(gfx::BitmapsAreEqual(*result.icon().icon.bitmap(),
                                   *GetTestIcon().bitmap()));

  ASSERT_EQ(result.title_text_vector().size(), 1u);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(),
            u"Version 108.0.5359.37 (Official Build) beta (64-bit)");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1u);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"Click here to check for updates");
  EXPECT_TRUE(details.GetTextTags().empty());
}

}  // namespace app_list::test
