// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_commands.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "chrome/browser/download/download_item_model.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using download::DownloadItem;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::_;

namespace {

// Default target path for a mock download item in DownloadItemModelTest.
const base::FilePath::CharType kDefaultTargetFilePath[] =
    FILE_PATH_LITERAL("/foo/bar/foo.bar");

// Default URL for a mock download item in DownloadCommandsTest.
const char kDefaultURL[] = "http://example.com/foo.bar";

class DownloadCommandsTest : public testing::Test {
 public:
  DownloadCommandsTest()
      : model_(std::make_unique<DownloadItemModel>(&item_)),
        commands_(model_->GetWeakPtr()) {}

  ~DownloadCommandsTest() override {}

 protected:
  // Sets up defaults for the download item.
  void SetUp() override {
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, CanShowInFolder()).WillByDefault(Return(true));
    ON_CALL(item_, CanOpenDownload()).WillByDefault(Return(true));
    ON_CALL(item_, IsDone()).WillByDefault(Return(false));
    ON_CALL(item_, GetOpenWhenComplete()).WillByDefault(Return(false));
    ON_CALL(item_, GetState())
        .WillByDefault(Return(DownloadItem::IN_PROGRESS));
    ON_CALL(item_, IsPaused()).WillByDefault(Return(false));
    ON_CALL(item_, ShouldOpenFileBasedOnExtension())
        .WillByDefault(Return(false));

    ON_CALL(item_, GetTargetDisposition())
        .WillByDefault(
            Return(DownloadItem::TARGET_DISPOSITION_OVERWRITE));
    ON_CALL(item_, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(item_, GetURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kDefaultURL)));
    ON_CALL(item_, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath(kDefaultTargetFilePath)));
  }

  download::MockDownloadItem& item() { return item_; }

  bool IsCommandEnabled(DownloadCommands::Command command) {
    return commands().IsCommandEnabled(command);
  }

  bool IsCommandChecked(DownloadCommands::Command command) {
    return commands().IsCommandChecked(command);
  }


  DownloadCommands& commands() {
    return commands_;
  }

  void InvalidModel() { model_.reset(); }

 private:
  NiceMock<download::MockDownloadItem> item_;
  std::unique_ptr<DownloadItemModel> model_;
  DownloadCommands commands_;
};

}  // namespace

TEST_F(DownloadCommandsTest, InProgress) {
  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::SHOW_IN_FOLDER));
  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::CANCEL));
  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::PAUSE));
  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::RESUME));
  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::ALWAYS_OPEN_TYPE));
  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::OPEN_WHEN_COMPLETE));

  EXPECT_FALSE(IsCommandChecked(DownloadCommands::OPEN_WHEN_COMPLETE));
  EXPECT_FALSE(IsCommandChecked(DownloadCommands::ALWAYS_OPEN_TYPE));
}

TEST_F(DownloadCommandsTest, OpenWhenCompleteEnabled) {
  ON_CALL(item(), GetOpenWhenComplete()).WillByDefault(Return(true));

  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::OPEN_WHEN_COMPLETE));
  EXPECT_TRUE(IsCommandChecked(DownloadCommands::OPEN_WHEN_COMPLETE));
}

TEST_F(DownloadCommandsTest, Finished) {
  ON_CALL(item(), IsDone()).WillByDefault(Return(true));

  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::CANCEL));
  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::PAUSE));
  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::RESUME));
}

TEST_F(DownloadCommandsTest, PausedResumable) {
  ON_CALL(item(), IsPaused()).WillByDefault(Return(true));
  ON_CALL(item(), CanResume()).WillByDefault(Return(true));

  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::PAUSE));
  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::RESUME));
}

TEST_F(DownloadCommandsTest, PausedUnresumable) {
  ON_CALL(item(), IsPaused()).WillByDefault(Return(true));
  ON_CALL(item(), CanResume()).WillByDefault(Return(false));

  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::PAUSE));
  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::RESUME));
}

TEST_F(DownloadCommandsTest, CantPauseSavePackage) {
  ON_CALL(item(), IsSavePackageDownload()).WillByDefault(Return(true));
  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::PAUSE));
}

TEST_F(DownloadCommandsTest, DoOpenWhenComplete) {
  // Open when complete.
  EXPECT_CALL(item(), OpenDownload()).Times(1);
  commands().ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE);
}

TEST_F(DownloadCommandsTest, DoShowInFolder) {
  // Open when complete.
  EXPECT_CALL(item(), ShowDownloadInShell()).Times(1);
  commands().ExecuteCommand(DownloadCommands::SHOW_IN_FOLDER);
}

TEST_F(DownloadCommandsTest, DoCancel) {
  // Cancel.
  EXPECT_CALL(item(), Cancel(true)).Times(1);
  commands().ExecuteCommand(DownloadCommands::CANCEL);
}

TEST_F(DownloadCommandsTest, DoPause) {
  // Pause.
  EXPECT_CALL(item(), Pause()).Times(1);
  commands().ExecuteCommand(DownloadCommands::PAUSE);
}

TEST_F(DownloadCommandsTest, DoResume) {
  // Resume.
  EXPECT_CALL(item(), Resume(true)).Times(1);
  commands().ExecuteCommand(DownloadCommands::RESUME);
}

TEST_F(DownloadCommandsTest,
       GetLearnMoreURLForInterruptedDownload_ContainsContext) {
  EXPECT_CALL(item(), GetLastReason())
      .WillOnce(
          Return(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED));
  GURL learn_more_url = commands().GetLearnMoreURLForInterruptedDownload();
  std::string name_value_pair = base::StringPrintf(
      "ctx=%d", download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED);
  EXPECT_LT(0u, learn_more_url.query().find(name_value_pair))
      << learn_more_url.spec();
}

TEST_F(DownloadCommandsTest, InvalidDownloadWontCrashDownloadCommands) {
  ON_CALL(item(), GetOpenWhenComplete()).WillByDefault(Return(true));
  // Only 1 out of 2 commands should be executed.
  EXPECT_CALL(item(), OpenDownload()).Times(1);

  EXPECT_TRUE(IsCommandEnabled(DownloadCommands::CANCEL));
  EXPECT_TRUE(IsCommandChecked(DownloadCommands::OPEN_WHEN_COMPLETE));
  EXPECT_TRUE(commands().IsCommandVisible(DownloadCommands::CANCEL));
  commands().ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE);

  InvalidModel();
  EXPECT_FALSE(IsCommandEnabled(DownloadCommands::CANCEL));
  EXPECT_FALSE(IsCommandChecked(DownloadCommands::OPEN_WHEN_COMPLETE));
  EXPECT_FALSE(commands().IsCommandVisible(DownloadCommands::CANCEL));
  commands().ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE);
}
