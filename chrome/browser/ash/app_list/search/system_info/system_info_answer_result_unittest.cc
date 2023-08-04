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
using AnswerCardInfo = ash::SystemInfoAnswerCardData;
const char kChromeUIDiagnosticsAppUrl[] = "chrome://diagnostics";

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

TEST_F(SystemInfoAnswerResultTest, version) {
  AnswerCardInfo answer_card_info(
      ash::SystemInfoAnswerCardDisplayType::kTextCard);
  SystemInfoAnswerResult result(
      profile_.get(), u"query", "path", GetTestIcon(), 0.8,
      u"Version 108.0.5359.37 (Official Build) beta (64-bit)",
      u"Click here to check for updates",
      u"Current version 108.0.5359.37 (Official Build) beta (64-bit). Click "
      u"to check for details",
      SystemInfoAnswerResult::SystemInfoCategory::kSettings,
      SystemInfoAnswerResult::SystemInfoCardType::kVersion, answer_card_info);
  EXPECT_EQ(result.id(), "os-settings://path");
  EXPECT_EQ(result.display_type(), DisplayType::kAnswerCard);
  EXPECT_EQ(result.category(), Category::kSettings);
  EXPECT_EQ(result.result_type(), ResultType::kSystemInfo);
  EXPECT_EQ(result.metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(result.icon().dimension, kAppIconDimension);
  EXPECT_EQ(result.icon().shape, ash::SearchResultIconShape::kDefault);
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(*result.icon().icon.Rasterize(nullptr).bitmap(),
                           *GetTestIcon().bitmap()));
  EXPECT_EQ(result.system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kTextCard);
  EXPECT_EQ(result.accessible_name(),
            u"Showing OS version information. Current version 108.0.5359.37 "
            u"(Official "
            u"Build) beta (64-bit). Click "
            u"to check for details. Select to open Settings page.");

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

TEST_F(SystemInfoAnswerResultTest, memory) {
  AnswerCardInfo answer_card_info(54.8);
  SystemInfoAnswerResult result(
      profile_.get(), u"query", "", GetTestIcon(), 0.8, u"",
      u"5.16 GB of 15.52 GB available",
      u"Memory 5.16 GB available out of 15.52 GB total",
      SystemInfoAnswerResult::SystemInfoCategory::kDiagnostics,
      SystemInfoAnswerResult::SystemInfoCardType::kMemory, answer_card_info);
  EXPECT_EQ(result.id(), kChromeUIDiagnosticsAppUrl);
  EXPECT_EQ(result.display_type(), DisplayType::kAnswerCard);
  EXPECT_EQ(result.category(), Category::kSettings);
  EXPECT_EQ(result.result_type(), ResultType::kSystemInfo);
  EXPECT_EQ(result.metrics_type(), ash::SYSTEM_INFO);
  EXPECT_EQ(result.icon().dimension, kAppIconDimension);
  EXPECT_EQ(result.icon().shape, ash::SearchResultIconShape::kDefault);
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(*result.icon().icon.Rasterize(nullptr).bitmap(),
                           *GetTestIcon().bitmap()));
  EXPECT_EQ(result.system_info_answer_card_data()->display_type,
            ash::SystemInfoAnswerCardDisplayType::kBarChart);
  EXPECT_EQ(result.system_info_answer_card_data()->bar_chart_percentage, 54.8);
  EXPECT_EQ(result.accessible_name(),
            u"Showing memory information. Memory 5.16 GB available out of "
            u"15.52 GB total. Select to open Diagnostics app.");

  ASSERT_EQ(result.title_text_vector().size(), 1u);
  const auto& title = result.title_text_vector()[0];
  ASSERT_EQ(title.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(title.GetText(), u"");
  EXPECT_TRUE(title.GetTextTags().empty());

  ASSERT_EQ(result.details_text_vector().size(), 1u);
  const auto& details = result.details_text_vector()[0];
  ASSERT_EQ(details.GetType(), ash::SearchResultTextItemType::kString);
  EXPECT_EQ(details.GetText(), u"5.16 GB of 15.52 GB available");
  EXPECT_TRUE(details.GetTextTags().empty());
}

}  // namespace app_list::test
