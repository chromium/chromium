// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/uuid.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/common/mock_simple_download_manager.h"
#include "components/offline_items_collection/core/test_support/scoped_mock_offline_content_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

namespace {
using ScopedMockObserver = offline_items_collection::
    ScopedMockOfflineContentProvider::ScopedMockObserver;

constexpr char kTestDownloadNamespace[] = "TEST_DOWNLOAD_NAMESPACE";
constexpr char kTestUrl[] = "http://www.example.com";
constexpr char kTestOriginalUrl[] = "http://www.exampleoriginalurl.com";
constexpr char kTestReferrerUrl[] = "http://www.examplereferrerurl.com";

}  // namespace

class DownloadOfflineContentProviderTest : public testing::Test {
 public:
  DownloadOfflineContentProviderTest()
      : provider_(&aggregator_, kTestDownloadNamespace),
        coordinator_(base::NullCallback()) {}

  DownloadOfflineContentProviderTest(
      const DownloadOfflineContentProviderTest&) = delete;
  DownloadOfflineContentProviderTest& operator=(
      const DownloadOfflineContentProviderTest&) = delete;

  ~DownloadOfflineContentProviderTest() override = default;

  void InitializeDownloads(bool full_browser) {
    coordinator_.SetSimpleDownloadManager(&mock_manager_, full_browser);
    mock_manager_.NotifyOnDownloadInitialized();
    provider_.SetSimpleDownloadManagerCoordinator(&coordinator_);
  }

  std::unique_ptr<download::MockDownloadItem> CreateMockDownloadItem(
      const std::string& guid) {
    base::FilePath file_path(FILE_PATH_LITERAL("foo.txt"));
    base::Time now = base::Time::Now();

    std::unique_ptr<download::MockDownloadItem> item(
        new ::testing::NiceMock<download::MockDownloadItem>());
    EXPECT_CALL(mock_manager_, GetDownloadByGuid(guid))
        .WillRepeatedly(Return(item.get()));

    ON_CALL(*item, GetURL()).WillByDefault(ReturnRefOfCopy(GURL(kTestUrl)));
    ON_CALL(*item, GetOriginalUrl())
        .WillByDefault(ReturnRefOfCopy(GURL(kTestOriginalUrl)));
    ON_CALL(*item, GetReferrerUrl())
        .WillByDefault(ReturnRefOfCopy(GURL(kTestReferrerUrl)));
    ON_CALL(*item, GetDangerType())
        .WillByDefault(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
    ON_CALL(*item, GetLastReason())
        .WillByDefault(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
    ON_CALL(*item, GetTargetFilePath())
        .WillByDefault(ReturnRefOfCopy(file_path));
    ON_CALL(*item, GetFileNameToReportUser()).WillByDefault(Return(file_path));
    ON_CALL(*item, IsDangerous()).WillByDefault(Return(false));
    ON_CALL(*item, GetGuid()).WillByDefault(ReturnRefOfCopy(guid));
    ON_CALL(*item, GetStartTime()).WillByDefault(Return(now));
    ON_CALL(*item, GetLastAccessTime()).WillByDefault(Return(now));
    ON_CALL(*item, GetReceivedBytes()).WillByDefault(Return(50));
    ON_CALL(*item, GetTotalBytes()).WillByDefault(Return(100));
    ON_CALL(*item, GetMimeType()).WillByDefault(Return("text/html"));
    ON_CALL(*item, CanOpenDownload()).WillByDefault(Return(true));
    ON_CALL(*item, IsDone()).WillByDefault(Return(false));
    ON_CALL(*item, GetState()).WillByDefault(Return(DownloadItem::IN_PROGRESS));
    ON_CALL(*item, IsPaused()).WillByDefault(Return(false));

    return item;
  }

  void RunUntilMainThreadIdle() {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return task_environment_.MainThreadIsIdle(); }));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  OfflineContentAggregator aggregator_;
  DownloadOfflineContentProvider provider_;
  SimpleDownloadManagerCoordinator coordinator_;
  NiceMock<download::MockSimpleDownloadManager> mock_manager_;
};

TEST_F(DownloadOfflineContentProviderTest, PauseDownloadBeforeInit) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, Pause()).Times(0);

  provider_.PauseDownload(id);
  RunUntilMainThreadIdle();

  EXPECT_CALL(*item, Pause()).Times(1);
  InitializeDownloads(false);
  RunUntilMainThreadIdle();
}

TEST_F(DownloadOfflineContentProviderTest, PauseDownloadAfterReducedModeInit) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, Pause()).Times(1);

  InitializeDownloads(false);
  provider_.PauseDownload(id);
  RunUntilMainThreadIdle();
}

TEST_F(DownloadOfflineContentProviderTest, PauseDownloadAfterFullBrowserStart) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, Pause()).Times(1);

  InitializeDownloads(true);
  provider_.PauseDownload(id);
  RunUntilMainThreadIdle();
}

// Tests that updates to a temporary download do not notify.
TEST_F(DownloadOfflineContentProviderTest, DoNotShowTemporaryDownload) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, IsTemporary()).WillRepeatedly(Return(true));

  ScopedMockObserver observer{&provider_};
  EXPECT_CALL(observer, OnItemUpdated(_, _)).Times(0);

  InitializeDownloads(true);
  provider_.OnDownloadUpdated(item.get());
  RunUntilMainThreadIdle();
}

// Tests that updates to a transient download do not notify.
TEST_F(DownloadOfflineContentProviderTest, DoNotShowTransientDownload) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, IsTransient()).WillRepeatedly(Return(true));

  ScopedMockObserver observer{&provider_};
  EXPECT_CALL(observer, OnItemUpdated(_, _)).Times(0);

  InitializeDownloads(true);
  provider_.OnDownloadUpdated(item.get());
  RunUntilMainThreadIdle();
}

// Tests that updates to a download with empty target path do not notify.
TEST_F(DownloadOfflineContentProviderTest, DoNotShowEmptyTargetPathDownload) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(base::FilePath()));

  ScopedMockObserver observer{&provider_};
  EXPECT_CALL(observer, OnItemUpdated(_, _)).Times(0);

  InitializeDownloads(true);
  provider_.OnDownloadUpdated(item.get());
  RunUntilMainThreadIdle();
}

// Tests that updates to a dangerous download does update observers.
TEST_F(DownloadOfflineContentProviderTest, ShowDangerousDownload) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, IsDangerous()).WillRepeatedly(Return(true));

  ScopedMockObserver observer{&provider_};
  EXPECT_CALL(
      observer,
      OnItemUpdated(Field(&OfflineItem::id, Eq(id)),
                    Optional(Field(&UpdateDelta::visuals_changed, true))))
      .Times(1);

  InitializeDownloads(true);
  provider_.OnDownloadUpdated(item.get());
  RunUntilMainThreadIdle();
}

// Tests that updates to a validated download does update observers.
TEST_F(DownloadOfflineContentProviderTest, ShowValidatedDownload) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_USER_VALIDATED));

  ScopedMockObserver observer{&provider_};
  EXPECT_CALL(
      observer,
      OnItemUpdated(Field(&OfflineItem::id, Eq(id)),
                    Optional(Field(&UpdateDelta::visuals_changed, true))))
      .Times(1);

  InitializeDownloads(true);
  provider_.OnDownloadUpdated(item.get());
  RunUntilMainThreadIdle();
}
