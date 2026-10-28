// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_context_menu.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"

using testing::NiceMock;
using testing::Return;
using testing::ReturnRefOfCopy;

class DownloadUiContextMenuTest : public testing::Test {
 public:
  DownloadUiContextMenuTest() = default;

  DownloadUiContextMenu* menu() { return context_menu_.get(); }

  void MakeContextMenu(DownloadUIModel* download) {
    // Don't use std::make_unique because it needs friend.
    context_menu_ =
        base::WrapUnique(new DownloadUiContextMenu(download->GetWeakPtr()));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<DownloadUiContextMenu> context_menu_;
};

TEST_F(DownloadUiContextMenuTest, InvalidDownloadWontCrashContextMenu) {
  std::unique_ptr<download::MockDownloadItem> item =
      std::make_unique<NiceMock<download::MockDownloadItem>>();
  auto download_ui_model = DownloadItemModel::Wrap(item.get());
  auto download_weak_ptr = download_ui_model->GetWeakPtr();
  EXPECT_CALL(*item, IsInsecure()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item, IsPaused()).WillRepeatedly(Return(true));
  // 1 out of 2 commands should be executed.
  EXPECT_CALL(*item, OpenDownload()).Times(1);

  MakeContextMenu(download_ui_model.get());
  EXPECT_NE(menu()->GetMenuModel(), nullptr);
  EXPECT_TRUE(menu()->IsCommandIdEnabled(DownloadCommands::KEEP));
  EXPECT_TRUE(menu()->IsCommandIdChecked(DownloadCommands::PAUSE));
  EXPECT_TRUE(menu()->IsCommandIdVisible(DownloadCommands::PAUSE));
  menu()->ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE, 0);

  // Weakptr should be invalidated when `download_ui_model` is released.
  download_ui_model.reset();
  EXPECT_EQ(menu()->GetMenuModel(), nullptr);
  EXPECT_FALSE(menu()->IsCommandIdEnabled(DownloadCommands::KEEP));
  EXPECT_FALSE(menu()->IsCommandIdChecked(DownloadCommands::PAUSE));
  EXPECT_FALSE(menu()->IsCommandIdVisible(DownloadCommands::PAUSE));
  menu()->ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE, 0);
}

TEST_F(DownloadUiContextMenuTest, RecordCommandsEnabled) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<download::MockDownloadItem> item =
      std::make_unique<NiceMock<download::MockDownloadItem>>();
  auto download_ui_model = DownloadItemModel::Wrap(item.get());
  auto download_weak_ptr = download_ui_model->GetWeakPtr();
  EXPECT_CALL(*item, IsInsecure()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item, IsPaused()).WillRepeatedly(Return(true));

  MakeContextMenu(download_ui_model.get());
  EXPECT_NE(menu()->GetMenuModel(), nullptr);
  EXPECT_TRUE(menu()->IsCommandIdEnabled(DownloadCommands::KEEP));

  menu()->RecordCommandsEnabled(menu()->GetMenuModel());
  histogram_tester.ExpectBucketCount("Download.ContextMenuAction",
                                     DownloadUiContextMenuAction::kKeepEnabled,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Download.ContextMenuAction",
      DownloadUiContextMenuAction::kLearnMoreInsecureDownloadEnabled, 1);
  histogram_tester.ExpectTotalCount("Download.ContextMenuAction", 2);
}

// "Open with..." is only offered for finished downloads on Windows when
// `kEnableDownloadOpenWithContextMenu` is enabled.
TEST_F(DownloadUiContextMenuTest, OpenWith) {
  std::unique_ptr<download::MockDownloadItem> item =
      std::make_unique<NiceMock<download::MockDownloadItem>>();
  auto download_ui_model = DownloadItemModel::Wrap(item.get());
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(*item, IsDone()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(
          base::FilePath(FILE_PATH_LITERAL("/foo/bar/foo.bar"))));
  EXPECT_CALL(*item, CanOpenDownload()).WillRepeatedly(Return(true));
  EXPECT_CALL(*item, CanShowInFolder()).WillRepeatedly(Return(true));

  // Disabled by default.
  MakeContextMenu(download_ui_model.get());
  ui::SimpleMenuModel* model = menu()->GetMenuModel();
  ASSERT_NE(model, nullptr);
  EXPECT_FALSE(
      model->GetIndexOfCommandId(DownloadCommands::OPEN_WITH).has_value());
  EXPECT_FALSE(menu()->IsCommandIdEnabled(DownloadCommands::OPEN_WITH));

#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list(
      download::features::kEnableDownloadOpenWithContextMenu);
  MakeContextMenu(download_ui_model.get());
  model = menu()->GetMenuModel();
  ASSERT_NE(model, nullptr);
  const std::optional<size_t> index =
      model->GetIndexOfCommandId(DownloadCommands::OPEN_WITH);
  ASSERT_TRUE(index.has_value());
  EXPECT_EQ(model->GetLabelAt(*index),
            l10n_util::GetStringUTF16(IDS_DOWNLOAD_MENU_OPEN_WITH));
  EXPECT_TRUE(menu()->IsCommandIdEnabled(DownloadCommands::OPEN_WITH));
#endif
  EXPECT_FALSE(menu()->IsCommandIdChecked(DownloadCommands::OPEN_WITH));
}
