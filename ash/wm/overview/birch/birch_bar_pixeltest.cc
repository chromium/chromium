// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_client.h"
#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/stub_birch_client.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

namespace {

std::vector<std::unique_ptr<BirchItem>> CreateItems(BirchItemType type) {
  static const GURL kTestURL("https://www.example.com");
  static const GURL kTestFaviconURL("https://www.favicon.com");

  std::vector<std::unique_ptr<BirchItem>> items;
  switch (type) {
    case BirchItemType::kCalendar:
      items.push_back(std::make_unique<BirchCalendarItem>(
          /*title=*/u"Event",
          /*start_time=*/base::Time(),
          /*end_time=*/base::Time(),
          /*calendar_url=*/kTestURL,
          /*conference_url=*/kTestURL,
          /*event_id=*/"event_id",
          /*all_day_event=*/false));
      items.push_back(std::make_unique<BirchCalendarItem>(
          /*title=*/u"Event",
          /*start_time=*/base::Time(),
          /*end_time=*/base::Time(),
          /*calendar_url=*/GURL(),
          /*conference_url=*/kTestURL,
          /*event_id=*/"event_id",
          /*all_day_event=*/false));
      break;
    case BirchItemType::kAttachment:
      items.push_back(std::make_unique<BirchAttachmentItem>(
          /*title=*/u"Attachment",
          /*file_url=*/kTestURL,
          /*favicon_url=*/kTestFaviconURL,
          /*start_time=*/base::Time(),
          /*end_time=*/base::Time(),
          /*file_id=*/"file_id"));
      break;
    case BirchItemType::kFile:
      items.push_back(std::make_unique<BirchFileItem>(
          /*file_path=*/base::FilePath("test path"), /*title=*/std::nullopt,
          /*justification=*/u"suggestion",
          /*timestamp=*/base::Time(),
          /*file_id=*/"file_id_0",
          /*icon_url=*/"icon_url"));
      break;
    case BirchItemType::kTab:
      items.push_back(std::make_unique<BirchTabItem>(
          /*title=*/u"tab", /*url=*/kTestURL,
          /*timestamp=*/base::Time(),
          /*favicon_url=*/kTestFaviconURL,
          /*session_name=*/"session",
          /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop));
      break;
    case BirchItemType::kWeather:
      items.push_back(std::make_unique<BirchWeatherItem>(
          /*weather_description=*/u"cloudy",
          /*temperature=*/72.f,
          /*icon_url=*/GURL("http://icon.com/")));
      break;
    case BirchItemType::kReleaseNotes:
      items.push_back(std::make_unique<BirchReleaseNotesItem>(
          /*release_notes_title=*/u"note",
          /*release_notes_text=*/u"explore",
          /*url=*/kTestURL,
          /*first_seen=*/base::Time()));
      break;
    case BirchItemType::kSelfShare:
      items.push_back(std::make_unique<BirchSelfShareItem>(
          /*guid=*/u"self share guid", /*title*/ u"self share tab",
          /*url=*/kTestURL,
          /*shared_time=*/base::Time(), /*device_name=*/u"my device",
          /*secondary_icon_type=*/SecondaryIconType::kTabFromDesktop,
          /*activation_callback=*/base::DoNothing()));
      break;
    case BirchItemType::kMostVisited:
      items.push_back(std::make_unique<BirchMostVisitedItem>(
          /*title=*/u"Most Visited",
          /*url=*/kTestURL));
      break;
    case BirchItemType::kLastActive:
      items.push_back(std::make_unique<BirchLastActiveItem>(
          /*title=*/u"Last Active",
          /*url=*/kTestURL,
          /*last_visit=*/base::Time()));
      break;
    case BirchItemType::kLostMedia:
      items.push_back(std::make_unique<BirchLostMediaItem>(
          /*source_url=*/kTestURL,
          /*media_title=*/u"lost media",
          /*backup_icon=*/std::nullopt,
          /*secondary_icon_type=*/SecondaryIconType::kLostMediaVideoConference,
          /*activation_callback=*/base::DoNothing()));
      items.push_back(std::make_unique<BirchLostMediaItem>(
          /*source_url=*/kTestURL,
          /*media_title=*/u"lost media",
          /*backup_icon=*/std::nullopt,
          /*secondary_icon_type=*/SecondaryIconType::kLostMediaVideo,
          /*activation_callback=*/base::DoNothing()));
      break;
    case BirchItemType::kCoral: {
      std::vector<GURL> page_urls;
      page_urls.emplace_back(("https://www.reddit.com/"));
      page_urls.emplace_back(("https://www.figma.com/"));
      page_urls.emplace_back(("https://www.notion.so/"));

      std::vector<std::string> app_ids;
      app_ids.emplace_back("lgnggepjiihbfdbedefdhcffnmhcahbm");

      items.push_back(std::make_unique<BirchCoralItem>(
          /*coral_title=*/u"coral_title",
          /*coral_text=*/u"coral_text",
          /*page_urls=*/page_urls,
          /*app_ids=*/app_ids,
          /*cluster_id=*/0));
      break;
    }
    case BirchItemType::kTest:
      break;
  }
  return items;
}

}  // namespace

struct TestParams {
  std::vector<BirchItemType> types;
  std::string name;
};

class BirchBarPixelTest : public AshTestBase,
                          public testing::WithParamInterface<TestParams> {
 public:
  BirchBarPixelTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kForestFeature, features::kBirchWeather}, {});
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->birch_model()->SetClientAndInit(&stub_birch_client_);
    image_downloader_ = std::make_unique<TestImageDownloader>();
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    AshTestBase::TearDown();
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  StubBirchClient stub_birch_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestImageDownloader> image_downloader_;
};

const TestParams kTestParams[] = {
    {.types = {BirchItemType::kCalendar, BirchItemType::kAttachment,
               BirchItemType::kFile},
     .name = "Calendar_Attachment_File"},
    {.types = {BirchItemType::kTab, BirchItemType::kWeather,
               BirchItemType::kReleaseNotes, BirchItemType::kSelfShare},

     .name = "Tab_Weather_ReleaseNotes_SelfShare"},
    {.types = {BirchItemType::kMostVisited, BirchItemType::kLastActive,
               BirchItemType::kLostMedia},
     .name = "MostVisited_LastActive_LostMedia"}};

INSTANTIATE_TEST_SUITE_P(
    All,
    BirchBarPixelTest,
    testing::ValuesIn(kTestParams),
    [](const testing::TestParamInfo<BirchBarPixelTest::ParamType>& info) {
      return info.param.name;
    });

// TODO(crbug.com/354748639): This test is flaky.
TEST_P(BirchBarPixelTest, DISABLED_VerifyBirchChips) {
  EnterOverview();

  OverviewGridTestApi overview_test_api(Shell::GetPrimaryRootWindow());
  BirchBarView* birch_bar_view = overview_test_api.birch_bar_view();
  ASSERT_TRUE(birch_bar_view);

  const TestParams& param = GetParam();
  std::vector<std::unique_ptr<BirchItem>> items_on_bar;
  for (const BirchItemType& type : param.types) {
    for (auto& item : CreateItems(type)) {
      items_on_bar.emplace_back(std::move(item));
    }
  }

  for (size_t i = 0; i < items_on_bar.size(); i++) {
    birch_bar_view->AddChip(items_on_bar[i].get());
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      param.name, /*revision_number=*/1, birch_bar_view));

  // Manually shut down chips of birch bar to avoid dangling ptrs of the fake
  // birch items.
  birch_bar_view->ShutdownChips();
}

}  // namespace ash
