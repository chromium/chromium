// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_display_controller.h"
#include "chrome/browser/download/bubble/download_display.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace {
using StrictMockDownloadItem = testing::StrictMock<download::MockDownloadItem>;

class FakeDownloadDisplay : public DownloadDisplay {
 public:
  FakeDownloadDisplay() = default;
  FakeDownloadDisplay(const FakeDownloadDisplay&) = delete;
  FakeDownloadDisplay& operator=(const FakeDownloadDisplay&) = delete;

  void Show() override { shown_ = true; }

  void Hide() override { shown_ = false; }

  bool IsShowing() override { return shown_; }

  void Enable() override { enabled_ = true; }

  void Disable() override { enabled_ = false; }

  void UpdateDownloadIcon(download::DownloadIconState state) override {}

 private:
  bool shown_ = false;
  bool enabled_ = false;
};

class DownloadDisplayControllerTest : public testing::Test {
 public:
  DownloadDisplayControllerTest()
      : manager_(std::make_unique<NiceMock<content::MockDownloadManager>>()) {}
  DownloadDisplayControllerTest(const DownloadDisplayControllerTest&) = delete;
  DownloadDisplayControllerTest& operator=(
      const DownloadDisplayControllerTest&) = delete;

 protected:
  NiceMock<content::MockDownloadManager>& manager() { return *manager_.get(); }
  download::MockDownloadItem& item(size_t index) { return *items_[index]; }

  void InitDownloadItem(const base::FilePath::CharType* path,
                        download::DownloadItem::DownloadState state) {
    size_t index = items_.size();
    items_.push_back(std::make_unique<StrictMockDownloadItem>());
    EXPECT_CALL(item(index), GetId())
        .WillRepeatedly(Return(static_cast<uint32_t>(items_.size() + 1)));
    EXPECT_CALL(item(index), GetState()).WillRepeatedly(Return(state));
    int received_bytes =
        state == download::DownloadItem::IN_PROGRESS ? 50 : 100;
    EXPECT_CALL(item(index), GetReceivedBytes())
        .WillRepeatedly(Return(received_bytes));
    EXPECT_CALL(item(index), GetTotalBytes()).WillRepeatedly(Return(100));

    std::vector<download::DownloadItem*> items;
    for (size_t i = 0; i < items_.size(); ++i) {
      items.push_back(&item(i));
    }
    EXPECT_CALL(*manager_.get(), GetAllDownloads(_))
        .WillRepeatedly(SetArgPointee<0>(items));
  }

 private:
  std::vector<std::unique_ptr<StrictMockDownloadItem>> items_;
  std::unique_ptr<NiceMock<content::MockDownloadManager>> manager_;
};

TEST_F(DownloadDisplayControllerTest, GetProgressItemsInProgress) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar4.pdf"),
                   download::DownloadItem::IN_PROGRESS);
  FakeDownloadDisplay display;
  DownloadDisplayController controller(&display, &manager());
  DownloadDisplayController::ProgressInfo progress = controller.GetProgress();

  EXPECT_EQ(progress.download_count, 2);
  EXPECT_EQ(progress.progress_percentage, 50);
}

TEST_F(DownloadDisplayControllerTest, GetProgressItemsAllComplete) {
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar.pdf"),
                   download::DownloadItem::COMPLETE);
  InitDownloadItem(FILE_PATH_LITERAL("/foo/bar2.pdf"),
                   download::DownloadItem::COMPLETE);
  FakeDownloadDisplay display;
  DownloadDisplayController controller(&display, &manager());
  DownloadDisplayController::ProgressInfo progress = controller.GetProgress();

  EXPECT_EQ(progress.download_count, 0);
  EXPECT_EQ(progress.progress_percentage, 0);
}
}  // namespace
