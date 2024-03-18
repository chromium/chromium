// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
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
  BirchItemTest() {
    auto new_window_delegate = std::make_unique<TestNewWindowDelegateImpl>();
    new_window_delegate_ = new_window_delegate.get();
    new_window_delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(
            std::move(new_window_delegate));
  }

  std::unique_ptr<TestNewWindowDelegateProvider> new_window_delegate_provider_;
  raw_ptr<TestNewWindowDelegateImpl> new_window_delegate_ = nullptr;
};

// When both conference URL and calendar URL are set, the conference URL is
// preferred.
TEST_F(BirchItemTest, Calendar_PerformAction_BothConferenceAndCalendar) {
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL("http://meet.com"));
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://calendar.com/"));

  EXPECT_TRUE(item.secondary_action());
  item.PerformSecondaryAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL("http://meet.com/"));
}

// If only the calendar URL is set, it is opened.
TEST_F(BirchItemTest, Calendar_PerformAction_CalendarOnly) {
  BirchCalendarItem item(u"item", /*start_time=*/base::Time(),
                         /*end_time=*/base::Time(),
                         /*calendar_url=*/GURL("http://calendar.com"),
                         /*conference_url=*/GURL());
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
                         /*conference_url=*/GURL());
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Attachment_PerformAction_ValidUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time());
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL("http://file.com/"));
}

TEST_F(BirchItemTest, Attachment_PerformAction_EmptyUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL(),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time());
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, File_TitleDoesNotShowFileExtension) {
  BirchFileItem item(base::FilePath("/path/to/file.gdoc"), u"suggested",
                     base::Time());
  // The title does not contain the ".gdoc" extension.
  EXPECT_EQ(u"file", item.title());
}

TEST_F(BirchItemTest, File_PerformAction) {
  BirchFileItem item(base::FilePath("file_path"), u"suggested", base::Time());
  EXPECT_EQ(u"file_path", item.title());
  EXPECT_EQ(u"suggested", item.subtitle());

  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_file_path_,
            base::FilePath("file_path"));
}

TEST_F(BirchItemTest, Weather_PerformAction) {
  BirchWeatherItem item(u"item", u"72 deg", ui::ImageModel());
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("https://google.com/search?q=weather"));
}

TEST_F(BirchItemTest, Tab_SubtitleHasSessionName) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"Chromebook",
                    BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(item.subtitle(), u"From Chromebook");
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
                         /*conference_url=*/GURL("http://meet.com"));

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Attachment_LoadIcon) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("http://attachment.icon"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time());

  item.LoadIcon(base::BindOnce(
      [](const ui::ImageModel& icon) { EXPECT_FALSE(icon.IsEmpty()); }));
}

TEST_F(BirchItemIconTest, Attachment_LoadIcon_InvalidUrl) {
  BirchAttachmentItem item(u"item",
                           /*file_url=*/GURL("http://file.com/"),
                           /*icon_url=*/GURL("invalid-url"),
                           /*start_time=*/base::Time(),
                           /*end_time=*/base::Time());

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
  const base::FilePath excel_path("/my/test/mySheet.xlsx");
  BirchFileItem item(excel_path, u"suggested", base::Time());

  item.LoadIcon(base::BindOnce([](const ui::ImageModel& icon) {
    // Icon was set.
    EXPECT_FALSE(icon.IsEmpty());

    // Color is the one used for MS Excel documents.
    auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
    auto* color_provider = ui::ColorProviderManager::Get().GetColorProviderFor(
        native_theme->GetColorProviderKey(nullptr));
    EXPECT_EQ(icon.GetVectorIcon().color(),
              color_provider->GetColor(cros_tokens::kCrosSysFileMsExcel));
  }));
}

}  // namespace
}  // namespace ash
