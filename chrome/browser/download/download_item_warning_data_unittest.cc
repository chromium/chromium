// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_warning_data.h"

#include <vector>

#include "base/test/task_environment.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gtest/include/gtest/gtest.h"

using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;
using DeepScanTrigger = DownloadItemWarningData::DeepScanTrigger;

class DownloadItemWarningDataTest : public testing::Test {
 public:
  DownloadItemWarningDataTest() = default;

 protected:
  void FastForwardAndAddEvent(base::TimeDelta time_delta,
                              WarningSurface surface,
                              WarningAction action) {
    task_environment_.FastForwardBy(time_delta);
    DownloadItemWarningData::AddWarningActionEvent(&download_, surface, action);
  }

  std::vector<WarningActionEvent> GetEvents() {
    return DownloadItemWarningData::GetWarningActionEvents(&download_);
  }

  bool VerifyEventValue(const WarningActionEvent& actual_event,
                        WarningSurface expected_surface,
                        WarningAction expected_action,
                        int64_t expected_latency,
                        bool expected_is_terminal_action) {
    bool success = true;
    if (actual_event.surface != expected_surface) {
      success = false;
      ADD_FAILURE() << "Warning action event should have surface "
                    << static_cast<int>(expected_surface) << ", but found "
                    << static_cast<int>(actual_event.surface);
    }
    if (actual_event.action != expected_action) {
      success = false;
      ADD_FAILURE() << "Warning action event should have action "
                    << static_cast<int>(expected_action) << ", but found "
                    << static_cast<int>(actual_event.action);
    }
    if (actual_event.action_latency_msec != expected_latency) {
      success = false;
      ADD_FAILURE() << "Warning action event should have latency "
                    << expected_latency << ", but found "
                    << actual_event.action_latency_msec;
    }
    if (actual_event.is_terminal_action != expected_is_terminal_action) {
      success = false;
      ADD_FAILURE() << "Warning action event should have is_terminal_action "
                    << expected_is_terminal_action << ", but found "
                    << actual_event.is_terminal_action;
    }
    return success;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  download::MockDownloadItem download_;
};

TEST_F(DownloadItemWarningDataTest, GetEvents) {
  FastForwardAndAddEvent(base::Seconds(0), WarningSurface::BUBBLE_MAINPAGE,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_SUBPAGE,
                         WarningAction::CLOSE);
  FastForwardAndAddEvent(base::Seconds(10), WarningSurface::DOWNLOAD_PROMPT,
                         WarningAction::CANCEL);
  FastForwardAndAddEvent(base::Seconds(15), WarningSurface::DOWNLOADS_PAGE,
                         WarningAction::DISCARD);

  std::vector<WarningActionEvent> events = GetEvents();

  ASSERT_EQ(3u, events.size());
  EXPECT_TRUE(VerifyEventValue(events[0], WarningSurface::BUBBLE_SUBPAGE,
                               WarningAction::CLOSE, /*expected_latency=*/5000,
                               /*expected_is_terminal_action=*/false));
  EXPECT_TRUE(VerifyEventValue(
      events[1], WarningSurface::DOWNLOAD_PROMPT, WarningAction::CANCEL,
      /*expected_latency=*/15000, /*expected_is_terminal_action=*/false));
  EXPECT_TRUE(VerifyEventValue(
      events[2], WarningSurface::DOWNLOADS_PAGE, WarningAction::DISCARD,
      /*expected_latency=*/30000, /*expected_is_terminal_action=*/true));
}

TEST_F(DownloadItemWarningDataTest, GetEvents_Notification) {
  FastForwardAndAddEvent(base::Seconds(0),
                         WarningSurface::DOWNLOAD_NOTIFICATION,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5),
                         WarningSurface::DOWNLOAD_NOTIFICATION,
                         WarningAction::OPEN_SUBPAGE);

  std::vector<WarningActionEvent> events = GetEvents();

  ASSERT_EQ(1u, events.size());
  EXPECT_TRUE(VerifyEventValue(events[0], WarningSurface::DOWNLOAD_NOTIFICATION,
                               WarningAction::OPEN_SUBPAGE,
                               /*expected_latency=*/5000,
                               /*expected_is_terminal_action=*/false));
}

TEST_F(DownloadItemWarningDataTest, GetEvents_WarningShownNotLogged) {
  FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_SUBPAGE,
                         WarningAction::PROCEED);

  std::vector<WarningActionEvent> events = GetEvents();

  // The events are not logged because the SHOWN event is not logged, so there
  // is no anchor event to calculate the latency.
  EXPECT_EQ(0u, events.size());
}

TEST_F(DownloadItemWarningDataTest, GetEvents_MultipleWarningShownLogged) {
  FastForwardAndAddEvent(base::Seconds(0), WarningSurface::BUBBLE_MAINPAGE,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_MAINPAGE,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5),
                         WarningSurface::DOWNLOAD_NOTIFICATION,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_SUBPAGE,
                         WarningAction::DISCARD);

  std::vector<WarningActionEvent> events = GetEvents();

  ASSERT_EQ(1u, events.size());
  // The expected latency should be 15000 instead of 5000 because the latency
  // should be anchored to the first shown event.
  EXPECT_TRUE(VerifyEventValue(events[0], WarningSurface::BUBBLE_SUBPAGE,
                               WarningAction::DISCARD,
                               /*expected_latency=*/15000,
                               /*expected_is_terminal_action=*/true));
}

TEST_F(DownloadItemWarningDataTest, GetEvents_ExceedEventMaxLength) {
  FastForwardAndAddEvent(base::Seconds(0), WarningSurface::BUBBLE_MAINPAGE,
                         WarningAction::SHOWN);
  for (int i = 0; i <= 30; i++) {
    FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_SUBPAGE,
                           WarningAction::CLOSE);
  }

  std::vector<WarningActionEvent> events = GetEvents();

  ASSERT_EQ(20u, events.size());
  // The newer events should be ignored, so the expected latency of the last
  // event should be the 20th event.
  EXPECT_TRUE(VerifyEventValue(events[19], WarningSurface::BUBBLE_SUBPAGE,
                               WarningAction::CLOSE,
                               /*expected_latency=*/20 * 5000,
                               /*expected_is_terminal_action=*/false));
}

TEST_F(DownloadItemWarningDataTest, IsEncryptedArchive) {
  EXPECT_FALSE(DownloadItemWarningData::IsTopLevelEncryptedArchive(&download_));
  DownloadItemWarningData::SetIsTopLevelEncryptedArchive(&download_, true);
  EXPECT_TRUE(DownloadItemWarningData::IsTopLevelEncryptedArchive(&download_));
}

TEST_F(DownloadItemWarningDataTest, HasIncorrectPassword) {
  EXPECT_FALSE(DownloadItemWarningData::HasIncorrectPassword(&download_));
  DownloadItemWarningData::SetHasIncorrectPassword(&download_, true);
  EXPECT_TRUE(DownloadItemWarningData::HasIncorrectPassword(&download_));
}

TEST_F(DownloadItemWarningDataTest, HasShownLocalDecryptionPrompt) {
  EXPECT_FALSE(
      DownloadItemWarningData::HasShownLocalDecryptionPrompt(&download_));
  DownloadItemWarningData::SetHasShownLocalDecryptionPrompt(&download_, true);
  EXPECT_TRUE(
      DownloadItemWarningData::HasShownLocalDecryptionPrompt(&download_));
}

TEST_F(DownloadItemWarningDataTest, DeepScanTrigger) {
  EXPECT_EQ(DownloadItemWarningData::DownloadDeepScanTrigger(&download_),
            DeepScanTrigger::TRIGGER_UNKNOWN);
  DownloadItemWarningData::SetDeepScanTrigger(
      &download_, DeepScanTrigger::TRIGGER_CONSUMER_PROMPT);
  EXPECT_EQ(DownloadItemWarningData::DownloadDeepScanTrigger(&download_),
            DeepScanTrigger::TRIGGER_CONSUMER_PROMPT);
}

TEST_F(DownloadItemWarningDataTest, FirstShownTimeAndSurface) {
  EXPECT_EQ(DownloadItemWarningData::WarningFirstShownSurface(&download_),
            std::nullopt);
  EXPECT_TRUE(
      DownloadItemWarningData::WarningFirstShownTime(&download_).is_null());
  base::Time now = base::Time::Now();
  FastForwardAndAddEvent(base::Seconds(0),
                         WarningSurface::DOWNLOAD_NOTIFICATION,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_MAINPAGE,
                         WarningAction::SHOWN);

  EXPECT_EQ(*DownloadItemWarningData::WarningFirstShownSurface(&download_),
            WarningSurface::DOWNLOAD_NOTIFICATION);
  EXPECT_EQ(DownloadItemWarningData::WarningFirstShownTime(&download_), now);
}

TEST_F(DownloadItemWarningDataTest, EventToString) {
  FastForwardAndAddEvent(base::Seconds(0), WarningSurface::BUBBLE_MAINPAGE,
                         WarningAction::SHOWN);
  FastForwardAndAddEvent(base::Seconds(5), WarningSurface::BUBBLE_SUBPAGE,
                         WarningAction::CLOSE);
  FastForwardAndAddEvent(base::Seconds(10), WarningSurface::DOWNLOAD_PROMPT,
                         WarningAction::CANCEL);
  FastForwardAndAddEvent(base::Seconds(15), WarningSurface::DOWNLOADS_PAGE,
                         WarningAction::DISCARD);

  std::vector<WarningActionEvent> events = GetEvents();

  // The initial SHOWN event is not included.
  EXPECT_EQ(events[0].ToString(), "BUBBLE_SUBPAGE:CLOSE:5000");
  EXPECT_EQ(events[1].ToString(), "DOWNLOAD_PROMPT:CANCEL:15000");
  EXPECT_EQ(events[2].ToString(), "DOWNLOADS_PAGE:DISCARD:30000");
}
