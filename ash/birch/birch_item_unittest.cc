// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/native_theme/native_theme.h"

namespace ash {
namespace {

class TestNewWindowDelegateImpl : public TestNewWindowDelegate {
 public:
  // TestNewWindowDelegate:
  void OpenUrl(const GURL& url,
               OpenUrlFrom from,
               Disposition disposition) override {
    last_opened_url_ = url;
  }

  void OpenFile(const base::FilePath& file_path) override {
    last_opened_file_path_ = file_path;
  }

  GURL last_opened_url_;
  base::FilePath last_opened_file_path_;
};

class BirchItemTest : public testing::Test {
 public:
  BirchItemTest()
      : ash_timezone_(u"America/Los_Angeles"),
        scoped_libc_timezone_("America/Los_Angeles") {
    auto new_window_delegate = std::make_unique<TestNewWindowDelegateImpl>();
    new_window_delegate_ = new_window_delegate.get();
    new_window_delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(
            std::move(new_window_delegate));
    BirchItem::set_action_count_for_test(0);

    // The mock clock starts with a fixed but arbitrary time. Adjust the time
    // to make the test times more readable (this makes "now" 5 PM).
    mock_clock_override_.Advance(base::Minutes(53));
  }

  ~BirchItemTest() override { BirchItem::set_action_count_for_test(0); }

  std::unique_ptr<TestNewWindowDelegateProvider> new_window_delegate_provider_;
  raw_ptr<TestNewWindowDelegateImpl> new_window_delegate_ = nullptr;
  // Use an arbitrary but fixed "now" time for tests.
  base::ScopedMockClockOverride mock_clock_override_;

  // Ensure consistent timezones for testing.
  ash::system::ScopedTimezoneSettings ash_timezone_;
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone_;
};

TEST_F(BirchItemTest, RecordActionMetrics_Basics) {
  base::HistogramTester histograms;
  BirchWeatherItem item(u"item", u"72 deg", ui::ImageModel());
  item.set_ranking(5.f);
  item.RecordActionMetrics();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivatedRanking", 5, 1);
}

TEST_F(BirchItemTest, RecordActionMetrics_FirstSecondThird) {
  base::HistogramTester histograms;
  BirchWeatherItem item(u"item", u"72 deg", ui::ImageModel());

  // First action records in "ActivateFirst" metric.
  item.RecordActionMetrics();
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateFirst",
                               BirchItemType::kWeather, 1);
  histograms.ExpectTotalCount("Ash.Birch.Chip.ActivateSecond", 0);
  histograms.ExpectTotalCount("Ash.Birch.Chip.ActivateThird", 0);

  // Second action records in "ActivateSecond" metric.
  item.RecordActionMetrics();
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateFirst",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateSecond",
                               BirchItemType::kWeather, 1);
  histograms.ExpectTotalCount("Ash.Birch.Chip.ActivateThird", 0);

  // Third action records in "ActivateThird" metric.
  item.RecordActionMetrics();
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateFirst",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateSecond",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateThird",
                               BirchItemType::kWeather, 1);

  // Fourth action doesn't change the metrics.
  item.RecordActionMetrics();
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateFirst",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateSecond",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivateThird",
                               BirchItemType::kWeather, 1);
}

// When both conference URL and calendar URL are set, the conference URL is
// preferred.
TEST_F(BirchItemTest, Calendar_PerformAction_BothConferenceAndCalendar) {
  // Create an event that is happening now so the "Join" action is enabled.
  base::Time now = base::Time::Now();
  BirchCalendarItem item(u"item", /*start_time=*/now - base::Minutes(30),
                         /*end_time=*/now + base::Minutes(30),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL("http://meet.com"),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://calendar.com/"));

  EXPECT_TRUE(item.secondary_action());
  item.PerformSecondaryAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL("http://meet.com/"));
}

TEST_F(BirchItemTest, Calendar_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL("http://meet.com"),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  item.PerformAction();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kCalendar, 1);

  item.PerformSecondaryAction();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 2);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kCalendar, 2);
}

// If only the calendar URL is set, it is opened.
TEST_F(BirchItemTest, Calendar_PerformAction_CalendarOnly) {
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://calendar.com/"));

  EXPECT_FALSE(item.secondary_action());
  item.PerformSecondaryAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://calendar.com/"));
}

// If neither the conference URL nor the calendar URL is set, nothing opens.
TEST_F(BirchItemTest, Calendar_PerformAction_NoURL) {
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL(),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Calendar_ShouldShowSecondaryAction) {
  base::Time now = base::Time::Now();

  // Create an event with a conference URL, but in the future.
  BirchCalendarItem item0(u"item0", /*start_time=*/now + base::Hours(1),
                          /*end_time=*/now + base::Hours(2),
                          /*calendar_url=*/GURL("http://calendar.com"),
                          /*conference_url=*/GURL("http://meet.com"),
                          /*event_id=*/"000",
                          /*all_day_event=*/false);

  // The meeting is in the future, so don't show the "Join" button.
  EXPECT_FALSE(item0.secondary_action().has_value());

  // Create a meeting happening right now.
  BirchCalendarItem item1(u"item1",
                          /*start_time=*/now - base::Minutes(30),
                          /*end_time=*/now + base::Minutes(30),
                          /*calendar_url=*/GURL("http://calendar.com"),
                          /*conference_url=*/GURL("http://meet.com"),
                          /*event_id=*/"001",
                          /*all_day_event=*/false);

  // The meeting is happening now, so show the "Join" button.
  EXPECT_TRUE(item1.secondary_action().has_value());

  // Create a meeting starting in the next few minutes.
  BirchCalendarItem item2(u"item2", /*start_time=*/now + base::Minutes(3),
                          /*end_time=*/now + base::Minutes(33),
                          /*calendar_url=*/GURL("http://calendar.com"),
                          /*conference_url=*/GURL("http://meet.com"),
                          /*event_id=*/"002",
                          /*all_day_event=*/false);

  // The meeting is very soon, so show the "Join" button.
  EXPECT_TRUE(item2.secondary_action().has_value());
}

TEST_F(BirchItemTest, Calendar_Subtitle_Ongoing) {
  BirchCalendarItem item(u"item",
                         /*start_time=*/base::Time::Now() - base::Minutes(30),
                         /*end_time=*/base::Time::Now() + base::Minutes(30),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  EXPECT_EQ(item.subtitle(), u"Now · Ends 5:30 PM");
}

TEST_F(BirchItemTest, Calendar_Subtitle_Soon) {
  BirchCalendarItem item(u"item",
                         /*start_time=*/base::Time::Now() + base::Minutes(15),
                         /*end_time=*/base::Time::Now() + base::Hours(1),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  EXPECT_EQ(item.subtitle(), u"In 15 mins · 5:15 PM - 6:00 PM");
}

TEST_F(BirchItemTest, Calendar_Subtitle_NotSoon) {
  BirchCalendarItem item(u"item",
                         /*start_time=*/base::Time::Now() + base::Hours(1),
                         /*end_time=*/base::Time::Now() + base::Hours(2),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  EXPECT_EQ(item.subtitle(), u"6:00 PM - 7:00 PM");
}

TEST_F(BirchItemTest, Calendar_Subtitle_Tomorrow) {
  base::Time next_midnight = base::Time::Now().LocalMidnight() + base::Days(1);
  BirchCalendarItem item(u"item",
                         /*start_time=*/next_midnight + base::Hours(1),
                         /*end_time=*/next_midnight + base::Hours(2),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  EXPECT_EQ(item.subtitle(), u"Tomorrow · 1:00 AM - 2:00 AM");
}

TEST_F(BirchItemTest, Calendar_Subtitle_AllDay) {
  base::Time next_midnight = base::Time::Now().LocalMidnight() + base::Days(1);
  BirchCalendarItem item(u"item",
                         /*start_time=*/next_midnight - base::Days(1),
                         /*end_time=*/next_midnight,
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL(),
                         /*event_id=*/"000",
                         /*all_day_event=*/true);
  EXPECT_EQ(item.subtitle(), u"All Day");
}

TEST_F(BirchItemTest, Attachment_PerformAction_ValidUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL("http://file.com/"));
}

TEST_F(BirchItemTest, Attachment_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");
  item.PerformAction();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kAttachment, 1);
}

TEST_F(BirchItemTest, Attachment_PerformAction_EmptyUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL(),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Attachment_Subtitle_Now) {
  base::Time now = base::Time::Now();
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/now - base::Minutes(30),
                           /*end_time=*/now + base::Minutes(30),
                           /*file_id=*/"");
  EXPECT_EQ(item.subtitle(), u"From event happening now");
}

TEST_F(BirchItemTest, Attachment_Subtitle_Upcoming) {
  base::Time now = base::Time::Now();
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/now + base::Hours(1),
                           /*end_time=*/now + base::Hours(2),
                           /*file_id=*/"");
  EXPECT_EQ(item.subtitle(), u"From upcoming calendar event");
}

TEST_F(BirchItemTest, File_TitleDoesNotShowFileExtension) {
  BirchFileItem item(base::FilePath("/path/to/file.gdoc"), u"suggested",
                     base::Time(), "id_1", "icon_url");
  // The title does not contain the ".gdoc" extension.
  EXPECT_EQ(u"file", item.title());
}

TEST_F(BirchItemTest, File_PerformAction) {
  BirchFileItem item(base::FilePath("file_path"), u"suggested", base::Time(),
                     "id_1", "icon_url");
  EXPECT_EQ(u"file_path", item.title());
  EXPECT_EQ(u"suggested", item.subtitle());
  EXPECT_EQ("id_1", item.file_id());

  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_file_path_,
            base::FilePath("file_path"));
}

TEST_F(BirchItemTest, File_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchFileItem item(base::FilePath("file_path"), u"suggested", base::Time(),
                     "id_1", "icon_url");
  item.PerformAction();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate", BirchItemType::kFile,
                               1);
}

TEST_F(BirchItemTest, Weather_PerformAction) {
  BirchWeatherItem item(u"item", u"72 deg", ui::ImageModel());
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("https://google.com/search?q=weather"));
}

TEST_F(BirchItemTest, Weather_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchWeatherItem item(u"item", u"72 deg", ui::ImageModel());
  item.PerformAction();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kWeather, 1);
}

TEST_F(BirchItemTest, Tab_Subtitle_Recent) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time::Now() - base::Minutes(5),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"< 1 hour ago · From Chromebook");
}

TEST_F(BirchItemTest, Tab_Subtitle_OneHour) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time::Now() - base::Minutes(65),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"1 hour ago · From Chromebook");
}

TEST_F(BirchItemTest, Tab_Subtitle_TwoHours) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time::Now() - base::Minutes(125),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"2 hours ago · From Chromebook");
}

TEST_F(BirchItemTest, Tab_Subtitle_Yesterday) {
  BirchTabItem item(
      u"item", /*url=*/GURL("http://example.com/"),
      /*timestamp=*/base::Time::Now().LocalMidnight() - base::Minutes(5),
      /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
      BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"Yesterday · From Chromebook");
}

TEST_F(BirchItemTest, Tab_PerformAction_ValidUrl) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://example.com/"));
}

TEST_F(BirchItemTest, Tab_PerformAction_EmptyUrl) {
  BirchTabItem item(u"item", /*url=*/GURL(),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Tab_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  item.PerformAction();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate", BirchItemType::kTab,
                               1);
}

////////////////////////////////////////////////////////////////////////////////

// The icon downloader requires ash::Shell, so use AshTestBase.
class BirchItemIconTest : public AshTestBase {
 public:
  TestImageDownloader image_downloader_;
};

TEST_F(BirchItemIconTest, Calendar_LoadIcon) {
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL("http://meet.com"),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Attachment_LoadIcon) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Attachment_LoadIcon_InvalidUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("invalid-url"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_TRUE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Tab_LoadIcon) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL("http://icon.com/"),
                    /*session_name=*/"",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Tab_LoadIcon_InvalidUrl) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL("invalid-url"),
                    /*session_name=*/"",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_TRUE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Weather_LoadIcon) {
  gfx::ImageSkia image = gfx::test::CreateImageSkia(10);
  BirchWeatherItem item(u"Sunny", u"72 deg",
                        ui::ImageModel::FromImageSkia(image));

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Weather_LoadIcon_NoIcon) {
  BirchWeatherItem item(u"Sunny", u"72 deg", ui::ImageModel());

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_TRUE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, File_LoadIcon) {
  const std::string icon_url =
      "https://drive-thirdparty.googleusercontent.com/32/type/application/"
      "vnd.google-apps.document";

  BirchFileItem item(base::FilePath("/path/to/file.gdoc"), u"suggested",
                     base::Time(), "id_1", icon_url);

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

}  // namespace
}  // namespace ash
