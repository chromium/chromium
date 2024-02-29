// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_item.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"

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

  GURL last_opened_url_;
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
  BirchCalendarItem item(u"item");
  item.conference_url = GURL("http://meet.com/");
  item.calendar_url = GURL("http://calendar.com/");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL("http://meet.com/"));
}

// If only the calendar URL is set, it is opened.
TEST_F(BirchItemTest, Calendar_PerformAction_OnlyCalendar) {
  BirchCalendarItem item(u"item");
  item.calendar_url = GURL("http://calendar.com/");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://calendar.com/"));
}

// If neither the conference URL nor the calendar URL is set, nothing opens.
TEST_F(BirchItemTest, Calendar_PerformAction_NoURL) {
  BirchCalendarItem item(u"item");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Attachment_PerformAction_ValidUrl) {
  BirchAttachmentItem item(u"item");
  item.file_url = GURL("http://file.com/");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL("http://file.com/"));
}

TEST_F(BirchItemTest, Attachment_PerformAction_EmptyUrl) {
  BirchAttachmentItem item(u"item");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

TEST_F(BirchItemTest, Weather_PerformAction) {
  BirchWeatherItem item(u"item", u"72 deg", ui::ImageModel());
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("https://google.com/search?q=weather"));
}

TEST_F(BirchItemTest, Tab_PerformAction_ValidUrl) {
  BirchTabItem item(u"item", /*url=*/GURL("http://example.com/"),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_,
            GURL("http://example.com/"));
}

TEST_F(BirchItemTest, Tab_PerformAction_EmptyUrl) {
  BirchTabItem item(u"item", /*url=*/GURL(),
                    /*timestamp=*/base::Time(),
                    /*favicon_url=*/GURL(), /*session_name=*/"");
  item.PerformAction();
  EXPECT_EQ(new_window_delegate_->last_opened_url_, GURL());
}

}  // namespace
}  // namespace ash
