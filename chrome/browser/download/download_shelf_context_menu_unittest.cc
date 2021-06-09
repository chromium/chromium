// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/download/download_shelf_context_menu.h"

#include "chrome/browser/download/download_item_model.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::Return;

class DownloadShelfContextMenuTest : public testing::Test {
 public:
  DownloadShelfContextMenuTest() = default;

  DownloadShelfContextMenu* menu() { return context_menu_.get(); }

  void MakeContextMenu(DownloadUIModel* download) {
    // Don't use std::make_unique because it needs friend.
    context_menu_ =
        base::WrapUnique(new DownloadShelfContextMenu(download->GetWeakPtr()));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<DownloadShelfContextMenu> context_menu_;
};

TEST_F(DownloadShelfContextMenuTest, InvalidDownloadWontCrashContextMenu) {
  std::unique_ptr<download::MockDownloadItem> item =
      std::make_unique<NiceMock<download::MockDownloadItem>>();
  auto download_ui_model = DownloadItemModel::Wrap(item.get());
  auto download_weak_ptr = download_ui_model->GetWeakPtr();
  EXPECT_CALL(*item, IsMixedContent()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item, IsPaused()).WillRepeatedly(Return(true));
  // 2 out of 3 commands should be executed.
  EXPECT_CALL(*item, OpenDownload()).Times(2);

  MakeContextMenu(download_ui_model.get());
  EXPECT_NE(menu()->GetMenuModel(), nullptr);
  EXPECT_TRUE(menu()->IsCommandIdEnabled(DownloadCommands::KEEP));
  EXPECT_TRUE(menu()->IsCommandIdChecked(DownloadCommands::PAUSE));
  EXPECT_TRUE(menu()->IsCommandIdVisible(DownloadCommands::PAUSE));
  menu()->ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE, 0);

  download_ui_model.reset();
  EXPECT_NE(menu()->GetMenuModel(), nullptr);
  EXPECT_TRUE(menu()->IsCommandIdEnabled(DownloadCommands::KEEP));
  EXPECT_TRUE(menu()->IsCommandIdChecked(DownloadCommands::PAUSE));
  EXPECT_TRUE(menu()->IsCommandIdVisible(DownloadCommands::PAUSE));
  menu()->ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE, 0);

  // |download_ui_model| is released by the task runner.
  RunUntilIdle();
  EXPECT_EQ(menu()->GetMenuModel(), nullptr);
  EXPECT_FALSE(menu()->IsCommandIdEnabled(DownloadCommands::KEEP));
  EXPECT_FALSE(menu()->IsCommandIdChecked(DownloadCommands::PAUSE));
  EXPECT_FALSE(menu()->IsCommandIdVisible(DownloadCommands::PAUSE));
  menu()->ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE, 0);
}
