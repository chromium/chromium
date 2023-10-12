// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/test_download_shelf.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace {

class DownloadShelfTest : public testing::Test {
 public:
  DownloadShelfTest();

 protected:
  download::MockDownloadItem* download_item() { return download_item_.get(); }
  TestDownloadShelf* shelf() { return &shelf_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  std::unique_ptr<download::MockDownloadItem> download_item_ =
      std::make_unique<::testing::NiceMock<download::MockDownloadItem>>();
  TestDownloadShelf shelf_{profile_.get()};
};

DownloadShelfTest::DownloadShelfTest() {
  ON_CALL(*download_item(), GetGuid())
      .WillByDefault(::testing::ReturnRefOfCopy(std::string("TEST_GUID")));
  ON_CALL(*download_item(), GetAutoOpened()).WillByDefault(Return(false));
  ON_CALL(*download_item(), GetMimeType()).WillByDefault(Return("text/plain"));
  ON_CALL(*download_item(), GetOpenWhenComplete()).WillByDefault(Return(false));
  ON_CALL(*download_item(), GetTargetDisposition())
      .WillByDefault(
          Return(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  ON_CALL(*download_item(), GetURL())
      .WillByDefault(
          ::testing::ReturnRefOfCopy(GURL("http://example.com/foo")));
  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  ON_CALL(*download_item(), IsTemporary()).WillByDefault(Return(false));
  ON_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillByDefault(Return(false));
  content::DownloadItemUtils::AttachInfoForTesting(download_item(),
                                                   profile_.get(), nullptr);

  auto download_manager =
      std::make_unique<::testing::NiceMock<content::MockDownloadManager>>();
  ON_CALL(*download_manager, GetDownloadByGuid(::testing::_))
      .WillByDefault(Return(download_item()));
  ON_CALL(*download_manager, GetBrowserContext())
      .WillByDefault(Return(profile_.get()));

  profile_->SetDownloadManagerForTesting(std::move(download_manager));
}

} // namespace

TEST_F(DownloadShelfTest, ClosesShelfWhenHidden) {
  shelf()->Open();
  EXPECT_TRUE(shelf()->IsShowing());
  shelf()->Hide();
  EXPECT_FALSE(shelf()->IsShowing());
  shelf()->Unhide();
  EXPECT_TRUE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, CloseWhileHiddenPreventsShowOnUnhide) {
  shelf()->Open();
  shelf()->Hide();
  shelf()->Close();
  shelf()->Unhide();
  EXPECT_FALSE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, UnhideDoesntShowIfNotShownOnHide) {
  shelf()->Hide();
  shelf()->Unhide();
  EXPECT_FALSE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, AddDownloadWhileHiddenUnhides) {
  shelf()->Open();
  shelf()->Hide();
  shelf()->AddDownload(DownloadItemModel::Wrap(download_item()));
  EXPECT_TRUE(shelf()->IsShowing());
}

TEST_F(DownloadShelfTest, AddDownloadWhileHiddenUnhidesAndShows) {
  shelf()->Hide();
  shelf()->AddDownload(DownloadItemModel::Wrap(download_item()));
  EXPECT_TRUE(shelf()->IsShowing());
}

// Normal downloads should be added synchronously and cause the shelf to show.
TEST_F(DownloadShelfTest, AddNormalDownload) {
  EXPECT_FALSE(shelf()->IsShowing());
  shelf()->AddDownload(DownloadItemModel::Wrap(download_item()));
  EXPECT_TRUE(shelf()->did_add_download());
  EXPECT_TRUE(shelf()->IsShowing());
}

// Add a transient download. It should not be added immediately. Instead it
// should be added after a delay. For testing, the delay is set to 0 seconds. So
// the download should be added once the message loop is flushed.
TEST_F(DownloadShelfTest, AddDelayedDownload) {
  ON_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillByDefault(Return(true));
  DownloadItemModel::DownloadUIModelPtr download =
      DownloadItemModel::Wrap(download_item());
  ASSERT_TRUE(download->ShouldRemoveFromShelfWhenComplete());
  shelf()->AddDownload(std::move(download));

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());

  base::RunLoop().RunUntilIdle();
}

// Add a transient download that completes before the delay. It should not be
// displayed on the shelf.
TEST_F(DownloadShelfTest, AddDelayedCompletedDownload) {
  ON_CALL(*download_item(), ShouldOpenFileBasedOnExtension())
      .WillByDefault(Return(true));
  DownloadItemModel::DownloadUIModelPtr download =
      DownloadItemModel::Wrap(download_item());
  ASSERT_TRUE(download->ShouldRemoveFromShelfWhenComplete());
  ON_CALL(*download_item(), IsTemporary()).WillByDefault(Return(true));
  shelf()->AddDownload(std::move(download));

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());

  ON_CALL(*download_item(), GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(*download_item(), GetAutoOpened()).WillByDefault(Return(true));

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(shelf()->did_add_download());
  EXPECT_FALSE(shelf()->IsShowing());
}
