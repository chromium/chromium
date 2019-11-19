// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/public/common//mock_simple_download_manager.h"
#include "components/download/public/common/mock_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace {

const char kTestDownloadNamespace[] = "TEST_DOWNLOAD_NAMESPACE";

}  // namespace

class DownloadOfflineContentProviderTest : public testing::Test {
 public:
  DownloadOfflineContentProviderTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        handle_(task_runner_),
        provider_(&aggregator_, kTestDownloadNamespace),
        coordinator_(base::NullCallback(), false) {}

  ~DownloadOfflineContentProviderTest() override {}

  void InitializeDownloads(bool full_browser) {
    coordinator_.SetSimpleDownloadManager(&mock_manager_, full_browser);
    mock_manager_.NotifyOnDownloadInitialized();
    provider_.SetSimpleDownloadManagerCoordinator(&coordinator_);
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle handle_;
  OfflineContentAggregator aggregator_;
  DownloadOfflineContentProvider provider_;
  SimpleDownloadManagerCoordinator coordinator_;
  NiceMock<download::MockSimpleDownloadManager> mock_manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadOfflineContentProviderTest);
};

TEST_F(DownloadOfflineContentProviderTest, PauseDownloadBeforeInit) {
  std::string guid = base::GenerateGUID();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item(
      new ::testing::NiceMock<download::MockDownloadItem>());
  EXPECT_CALL(mock_manager_, GetDownloadByGuid(guid))
      .WillRepeatedly(Return(item.get()));

  EXPECT_CALL(*item, Pause()).Times(0);

  provider_.PauseDownload(id);
  task_runner_->RunUntilIdle();

  EXPECT_CALL(*item, Pause()).Times(1);
  InitializeDownloads(false);
  task_runner_->RunUntilIdle();
}

TEST_F(DownloadOfflineContentProviderTest, PauseDownloadAfterReducedModeInit) {
  std::string guid = base::GenerateGUID();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item(
      new ::testing::NiceMock<download::MockDownloadItem>());
  EXPECT_CALL(mock_manager_, GetDownloadByGuid(guid))
      .WillRepeatedly(Return(item.get()));

  EXPECT_CALL(*item, Pause()).Times(1);

  InitializeDownloads(false);
  provider_.PauseDownload(id);
  task_runner_->RunUntilIdle();
}

TEST_F(DownloadOfflineContentProviderTest, PauseDownloadAfterFullBrowserStart) {
  std::string guid = base::GenerateGUID();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item(
      new ::testing::NiceMock<download::MockDownloadItem>());
  EXPECT_CALL(mock_manager_, GetDownloadByGuid(guid))
      .WillRepeatedly(Return(item.get()));

  EXPECT_CALL(*item, Pause()).Times(1);

  InitializeDownloads(true);
  provider_.PauseDownload(id);
  task_runner_->RunUntilIdle();
}
