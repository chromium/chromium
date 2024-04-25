// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_core_service_impl.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using DownloadState = download::DownloadItem::DownloadState;
using CancelDownloadsTrigger = DownloadCoreService::CancelDownloadsTrigger;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;

class TestDownloadManagerDelegate : public ChromeDownloadManagerDelegate {
 public:
  explicit TestDownloadManagerDelegate(Profile* profile)
      : ChromeDownloadManagerDelegate(profile) {}
  ~TestDownloadManagerDelegate() override = default;

  void OnDownloadCanceledAtShutdown(download::DownloadItem* item) override {
    canceled_at_shutdown_called_count_++;
  }

  int OnDownloadCanceledAtShutdownCalledCount() {
    return canceled_at_shutdown_called_count_;
  }

 private:
  int canceled_at_shutdown_called_count_ = 0;
};

class DownloadCoreServiceImplTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    auto download_manager =
        std::make_unique<NiceMock<content::MockDownloadManager>>();
    download_manager_ = download_manager.get();
    profile_->SetDownloadManagerForTesting(std::move(download_manager));
    download_core_service_ =
        std::make_unique<DownloadCoreServiceImpl>(profile_.get());
    auto delegate =
        std::make_unique<TestDownloadManagerDelegate>(profile_.get());
    delegate_ = delegate.get();
    download_core_service_->SetDownloadManagerDelegateForTesting(
        std::move(delegate));
  }

  void TearDown() override {
    download_core_service_->GetDownloadManagerDelegate()->Shutdown();
    delegate_ = nullptr;
    download_manager_ = nullptr;
    download_core_service_ = nullptr;
    profile_ = nullptr;
  }

 protected:
  std::unique_ptr<download::MockDownloadItem> CreateDownloadItem(
      DownloadState state) {
    auto item = std::make_unique<NiceMock<download::MockDownloadItem>>();
    EXPECT_CALL(*item, GetState()).WillRepeatedly(Return(state));
    return item;
  }

  void RunCancelDownloadsTest(
      CancelDownloadsTrigger trigger,
      int expected_download_canceled_at_shutdown_called_count) {
    auto completed_item = CreateDownloadItem(DownloadState::COMPLETE);
    auto in_progress_item1 = CreateDownloadItem(DownloadState::IN_PROGRESS);
    auto in_progress_item2 = CreateDownloadItem(DownloadState::IN_PROGRESS);
    std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
    items.push_back(completed_item.get());
    items.push_back(in_progress_item1.get());
    items.push_back(in_progress_item2.get());
    EXPECT_CALL(*download_manager_, GetAllDownloads)
        .WillRepeatedly(SetArgPointee<0>(items));

    // Only in progress items should be canceled.
    EXPECT_CALL(*completed_item, Cancel(_)).Times(0);
    EXPECT_CALL(*in_progress_item1, Cancel(/*user_cancel=*/false)).Times(1);
    EXPECT_CALL(*in_progress_item2, Cancel(/*user_cancel=*/false)).Times(1);

    download_core_service_->CancelDownloads(trigger);

    EXPECT_EQ(delegate_->OnDownloadCanceledAtShutdownCalledCount(),
              expected_download_canceled_at_shutdown_called_count);
  }

  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<NiceMock<content::MockDownloadManager>> download_manager_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<TestDownloadManagerDelegate> delegate_;
  std::unique_ptr<DownloadCoreServiceImpl> download_core_service_;
};

TEST_F(DownloadCoreServiceImplTest, CancelDownloadsAtShutdown) {
  RunCancelDownloadsTest(
      DownloadCoreService::CancelDownloadsTrigger::kShutdown,
      /*expected_download_canceled_at_shutdown_called_count=*/2);
}

TEST_F(DownloadCoreServiceImplTest, CancelDownloadsAtProfileDeletion) {
  RunCancelDownloadsTest(
      DownloadCoreService::CancelDownloadsTrigger::kProfileDeletion,
      /*expected_download_canceled_at_shutdown_called_count=*/0);
}

}  // namespace
