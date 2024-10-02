// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <memory>
#include <utility>

#include "ash/birch/birch_icon_cache.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/native_theme/native_theme.h"

namespace ash {
namespace {

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

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

class StubBirchClient : public BirchClient {
 public:
  StubBirchClient() = default;
  ~StubBirchClient() override = default;

  // BirchClient:
  BirchDataProvider* GetCalendarProvider() override { return nullptr; }
  BirchDataProvider* GetFileSuggestProvider() override { return nullptr; }
  BirchDataProvider* GetRecentTabsProvider() override { return nullptr; }
  BirchDataProvider* GetLastActiveProvider() override { return nullptr; }
  BirchDataProvider* GetMostVisitedProvider() override { return nullptr; }
  BirchDataProvider* GetSelfShareProvider() override { return nullptr; }
  BirchDataProvider* GetLostMediaProvider() override { return nullptr; }
  BirchDataProvider* GetReleaseNotesProvider() override { return nullptr; }
  void WaitForRefreshTokens(base::OnceClosure callback) override {}
  base::FilePath GetRemovedItemsFilePath() override { return base::FilePath(); }
  void RemoveFileItemFromLauncher(const base::FilePath& path) override {}

  void GetFaviconImage(
      const GURL& url,
      const bool is_page_url,
      base::OnceCallback<void(const ui::ImageModel&)> callback) override {
    did_get_favicon_image_ = true;
    std::move(callback).Run(ui::ImageModel());
  }
  ui::ImageModel GetChromeBackupIcon() override { return ui::ImageModel(); }

  bool did_get_favicon_image_ = false;
};

class BirchItemTest : public testing::Test {
 public:
  BirchItemTest()
      : ash_timezone_(u"America/Los_Angeles"),
        scoped_libc_timezone_("America/Los_Angeles") {
    BirchItem::set_action_count_for_test(0);

    // The mock clock starts with a fixed but arbitrary time. Adjust the time
    // to make the test times more readable (this makes "now" 5 PM).
    mock_clock_override_.Advance(base::Minutes(53));
  }

  ~BirchItemTest() override { BirchItem::set_action_count_for_test(0); }

  TestNewWindowDelegateImpl& new_window_delegate() {
    return new_window_delegate_;
  }

 private:
  TestNewWindowDelegateImpl new_window_delegate_;

  // Use an arbitrary but fixed "now" time for tests.
  base::ScopedMockClockOverride mock_clock_override_;

  // Ensure consistent timezones for testing.
  ash::system::ScopedTimezoneSettings ash_timezone_;
  calendar_test_utils::ScopedLibcTimeZone scoped_libc_timezone_;
};

TEST_F(BirchItemTest, RecordActionMetrics_Basics) {
  base::HistogramTester histograms;
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));
  item.set_ranking(5.f);
  item.RecordActionMetrics();
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kWeather, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.ActivatedRanking", 5, 1);
}

TEST_F(BirchItemTest, RecordActionMetrics_FirstSecondThird) {
  base::HistogramTester histograms;
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));

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
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_,
            GURL("http://calendar.com/"));

  EXPECT_TRUE(item.addon_label());
  item.PerformAddonAction();
  EXPECT_EQ(new_window_delegate().last_opened_url_, GURL("http://meet.com/"));
}

TEST_F(BirchItemTest, Calendar_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL("http://meet.com"),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);
  item.PerformAction(/*is_post_login=*/false);
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kCalendar, 1);

  item.PerformAddonAction();
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
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_,
            GURL("http://calendar.com/"));

  EXPECT_FALSE(item.addon_label());
  item.PerformAddonAction();
  EXPECT_EQ(new_window_delegate().last_opened_url_,
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
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Calendar_ShouldShowAddonAction) {
  base::Time now = base::Time::Now();

  // Create an event with a conference URL, but in the future.
  BirchCalendarItem item0(u"item0", /*start_time=*/now + base::Hours(1),
                          /*end_time=*/now + base::Hours(2),
                          /*calendar_url=*/GURL("http://calendar.com"),
                          /*conference_url=*/GURL("http://meet.com"),
                          /*event_id=*/"000",
                          /*all_day_event=*/false);

  // The meeting is in the future, so don't show the "Join" button.
  EXPECT_FALSE(item0.addon_label().has_value());

  // Create a meeting happening right now.
  BirchCalendarItem item1(u"item1",
                          /*start_time=*/now - base::Minutes(30),
                          /*end_time=*/now + base::Minutes(30),
                          /*calendar_url=*/GURL("http://calendar.com"),
                          /*conference_url=*/GURL("http://meet.com"),
                          /*event_id=*/"001",
                          /*all_day_event=*/false);

  // The meeting is happening now, so show the "Join" button.
  EXPECT_TRUE(item1.addon_label().has_value());

  // Create a meeting starting in the next few minutes.
  BirchCalendarItem item2(u"item2", /*start_time=*/now + base::Minutes(3),
                          /*end_time=*/now + base::Minutes(33),
                          /*calendar_url=*/GURL("http://calendar.com"),
                          /*conference_url=*/GURL("http://meet.com"),
                          /*event_id=*/"002",
                          /*all_day_event=*/false);

  // The meeting is very soon, so show the "Join" button.
  EXPECT_TRUE(item2.addon_label().has_value());
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
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_, GURL("http://file.com/"));
}

TEST_F(BirchItemTest, Attachment_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");
  item.PerformAction(/*is_post_login=*/false);
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
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_, GURL());
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
  BirchFileItem item(base::FilePath("/path/to/file.gdoc"), std::nullopt,
                     u"suggested", base::Time(), "id_1", "icon_url");
  // The title does not contain the ".gdoc" extension.
  EXPECT_EQ(u"file", item.title());
}

TEST_F(BirchItemTest, File_Title) {
  BirchFileItem item(base::FilePath("/path/to/file.gdoc"), "file_title",
                     u"suggested", base::Time(), "id_1", "icon_url");
  // When set, the title will take precedence over the file path.
  EXPECT_EQ(u"file_title", item.title());
}

TEST_F(BirchItemTest, File_PerformAction) {
  BirchFileItem item(base::FilePath("file_path"), "title", u"suggested",
                     base::Time(), "id_1", "icon_url");
  EXPECT_EQ(u"title", item.title());
  EXPECT_EQ(u"suggested", item.subtitle());
  EXPECT_EQ("id_1", item.file_id());

  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_file_path_,
            base::FilePath("file_path"));
}

TEST_F(BirchItemTest, File_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchFileItem item(base::FilePath("file_path"), "title", u"suggested",
                     base::Time(), "id_1", "icon_url");
  item.PerformAction(/*is_post_login=*/false);
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate", BirchItemType::kFile,
                               1);
}

TEST_F(BirchItemTest, Weather_PerformAction) {
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_,
            GURL("https://google.com/search?q=weather"));
}

TEST_F(BirchItemTest, Weather_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));
  item.PerformAction(/*is_post_login=*/false);
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate",
                               BirchItemType::kWeather, 1);
}

// Weather item subtitles require an ash::Shell for the pref service.
using BirchWeatherItemTest = AshTestBase;

TEST_F(BirchWeatherItemTest, AddonLabelInFahrenheit) {
  GetPrefService()->SetBoolean(prefs::kBirchUseCelsius, false);
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));
  EXPECT_EQ(item.addon_label(), u"72");
}

TEST_F(BirchWeatherItemTest, AddonLabelInCelsius) {
  GetPrefService()->SetBoolean(prefs::kBirchUseCelsius, true);
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));
  EXPECT_EQ(item.addon_label(), u"22");
}

TEST_F(BirchItemTest, Tab_Subtitle_Recent) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time::Now() - base::Minutes(5),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"Within 1 hr · From Chromebook");
}

TEST_F(BirchItemTest, Tab_Subtitle_OneHour) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time::Now() - base::Minutes(65),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"1 hr ago · From Chromebook");
}

TEST_F(BirchItemTest, Tab_Subtitle_TwoHours) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time::Now() - base::Minutes(125),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"2 hr ago · From Chromebook");
}

TEST_F(BirchItemTest, Tab_Subtitle_Yesterday) {
  BirchTabItem item(
      u"item", /*url=*/GURL("http://example.com/"),
      /*timestamp=*/base::Time::Now().LocalMidnight() - base::Minutes(5),
      /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
      /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"Yesterday · From Chromebook");
}

TEST_F(BirchItemTest, Tab_PerformAction_ValidUrl) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_,
            GURL("http://example.com/"));
}

TEST_F(BirchItemTest, Tab_PerformAction_EmptyUrl) {
  BirchTabItem item(u"item", /*url=*/GURL(),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Tab_PerformAction_Histograms) {
  base::HistogramTester histograms;
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  item.PerformAction(/*is_post_login=*/false);
  histograms.ExpectBucketCount("Ash.Birch.Bar.Activate", true, 1);
  histograms.ExpectBucketCount("Ash.Birch.Chip.Activate", BirchItemType::kTab,
                               1);
}

TEST_F(BirchItemTest, LastActive_Subtitle_TwoDaysAgo) {
  BirchLastActiveItem item(u"item", GURL("http://example.com/"),
                           base::Time::Now() - base::Days(2));
  EXPECT_EQ(item.subtitle(), u"2 days ago · Continue browsing");
}

TEST_F(BirchItemTest, LastActive_Subtitle_Yesterday) {
  BirchLastActiveItem item(u"item", GURL("http://example.com/"),
                           base::Time::Now() - base::Days(1));
  EXPECT_EQ(item.subtitle(), u"Yesterday · Continue browsing");
}

TEST_F(BirchItemTest, LastActive_Subtitle_OneHourAgo) {
  BirchLastActiveItem item(u"item", GURL("http://example.com/"),
                           base::Time::Now() - base::Hours(1));
  EXPECT_EQ(item.subtitle(), u"1 hr ago · Continue browsing");
}

TEST_F(BirchItemTest, LastActive_PerformAction) {
  BirchLastActiveItem item(u"item", GURL("http://example.com/"), base::Time());
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_,
            GURL("http://example.com/"));
}

TEST_F(BirchItemTest, SelfShare_PerformAction) {
  base::MockCallback<base::RepeatingClosure> activation_callback;
  BirchSelfShareItem item(
      /*guid=*/u"self share guid", /*title*/ u"self share tab",
      /*url=*/GURL("https://www.example.com/"),
      /*shared_time=*/base::Time(), /*device_name=*/u"my device",
      /*secondary_icon_type=*/SecondaryIconType::kTabFromDesktop,
      /*activation_callback=*/activation_callback.Get());
  EXPECT_CALL(activation_callback, Run).Times(1);
  item.PerformAction(/*is_post_login=*/false);
  EXPECT_EQ(new_window_delegate().last_opened_url_,
            GURL("https://www.example.com/"));
}

////////////////////////////////////////////////////////////////////////////////

// The icon downloader requires ash::Shell, so use AshTestBase.
class BirchItemIconTest : public AshTestBase {
 public:
  BirchItemIconTest() {
    feature_list_.InitAndEnableFeature(features::kForestFeature);
  }

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->birch_model()->SetClientAndInit(&stub_birch_client_);
  }

  void TearDown() override {
    Shell::Get()->birch_model()->SetClientAndInit(nullptr);
    AshTestBase::TearDown();
  }

  StubBirchClient stub_birch_client_;
  TestImageDownloader image_downloader_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BirchItemIconTest, Calendar_LoadIcon) {
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL("http://meet.com"),
                         /*event_id=*/"000",
                         /*all_day_event=*/false);

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));
}

TEST_F(BirchItemIconTest, Attachment_LoadIcon) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");

  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type no icon.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kNoIcon);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get("http://attachment.icon/").isNull());
}

TEST_F(BirchItemIconTest, Attachment_LoadIcon_InvalidUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("invalid-url"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time(),
                           /*file_id=*/"");

  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // Secondary icon is of type no icon.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kNoIcon);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 0u);
}

TEST_F(BirchItemIconTest, Tab_LoadIcon) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL("http://icon.com/"),
                    /*session_name=*/"",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // The favicon service was queried.
  EXPECT_TRUE(stub_birch_client_.did_get_favicon_image_);
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type no icon.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kTabFromDesktop);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get("http://icon.com/").isNull());
}

TEST_F(BirchItemIconTest, Tab_LoadIcon_InvalidUrl) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL("invalid-url"),
                    /*session_name=*/"",
                    /*form_factor=*/BirchTabItem::DeviceFormFactor::kDesktop);
  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // Secondary icon is of type no icon.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kTabFromDesktop);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 0u);
}

TEST_F(BirchItemIconTest, Weather_LoadIcon) {
  BirchWeatherItem item(u"item", 72.f, GURL("http://icon.com/"));

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));
}

TEST_F(BirchItemIconTest, Weather_LoadIcon_NoIcon) {
  BirchWeatherItem item(u"Sunny", 72.f, GURL());

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon, SecondaryIconType secondary_icon_type) {
        // In the case where an invalid icon_url is provided, there should still
        // be a valid backup_icon.
        EXPECT_FALSE(icon.IsEmpty());
        EXPECT_EQ(secondary_icon_type, SecondaryIconType::kNoIcon);
      }));
}

TEST_F(BirchItemIconTest, File_LoadIcon) {
  const std::string icon_url =
      "https://drive-thirdparty.googleusercontent.com/32/type/application/"
      "vnd.google-apps.document";

  BirchFileItem item(base::FilePath("/path/to/file.gdoc"), "title",
                     u"suggested", base::Time(), "id_1", icon_url);

  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type no icon.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kNoIcon);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get(icon_url).isNull());
}

TEST_F(BirchItemIconTest, SelfShare_LoadIcon) {
  const GURL page_url = GURL("https://www.example.com/");
  BirchSelfShareItem item(
      u"self share guid", u"self share tab", page_url, base::Time(),
      u"my device", SecondaryIconType::kTabFromDesktop, base::DoNothing());
  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // The favicon service was queried.
  EXPECT_TRUE(stub_birch_client_.did_get_favicon_image_);
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type `kTabFromDesktop`.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kTabFromDesktop);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get(page_url.spec()).isNull());
}

TEST_F(BirchItemTest, LostMedia_VideoConference_Subtitle) {
  BirchLostMediaItem item(GURL(), u"test_title", std::nullopt,
                          SecondaryIconType::kLostMediaVideoConference,
                          base::DoNothing());
  EXPECT_EQ(item.subtitle(), u"Ongoing · Switch to tab");
}

TEST_F(BirchItemTest, LostMedia_MediaTab_Subtitle) {
  BirchLostMediaItem item(GURL(), u"test_title", std::nullopt,
                          SecondaryIconType::kLostMediaVideo,
                          base::DoNothing());
  EXPECT_EQ(item.subtitle(), u"Playing · Switch to tab");
}

TEST_F(BirchItemIconTest, LostMedia_LoadIcon) {
  const GURL page_url = GURL("https://www.example.com/");
  BirchLostMediaItem item(page_url, u"test_title", std::nullopt,
                          SecondaryIconType::kLostMediaVideoConference,
                          base::DoNothing());
  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());
  // The favicon service was queried.
  EXPECT_TRUE(stub_birch_client_.did_get_favicon_image_);
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type `kLostMediaVideoConference`.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kLostMediaVideoConference);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get(page_url.spec()).isNull());
}

TEST_F(BirchItemIconTest, LastActive_LoadIcon) {
  const GURL page_url = GURL("https://www.example.com/");
  BirchLastActiveItem item(u"item", page_url, base::Time());

  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());

  // The favicon service was queried.
  EXPECT_TRUE(stub_birch_client_.did_get_favicon_image_);
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type `kNoIcon`.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kNoIcon);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get(page_url.spec()).isNull());
}

TEST_F(BirchItemIconTest, MostVisited_LoadIcon) {
  const GURL page_url = GURL("https://www.example.com/");
  BirchMostVisitedItem item(u"item", page_url);

  base::test::TestFuture<const ui::ImageModel&, SecondaryIconType> future;
  item.LoadIcon(future.GetCallback());

  // The favicon service was queried.
  EXPECT_TRUE(stub_birch_client_.did_get_favicon_image_);
  // The icon is not empty.
  EXPECT_FALSE(future.Get<0>().IsEmpty());
  // Secondary icon is of type `kNoIcon`.
  EXPECT_EQ(future.Get<1>(), SecondaryIconType::kNoIcon);

  auto* icon_cache = Shell::Get()->birch_model()->icon_cache();
  EXPECT_EQ(icon_cache->size_for_test(), 1u);
  EXPECT_FALSE(icon_cache->Get(page_url.spec()).isNull());
}

}  // namespace
}  // namespace ash
