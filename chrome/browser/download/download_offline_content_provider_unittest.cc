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
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/common/mock_simple_download_manager.h"
#include "components/offline_items_collection/core/test_support/scoped_mock_offline_content_provider.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#endif

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
        coordinator_(base::NullCallback()) {
    provider_.OnProfileCreated(&profile_);
  }

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
  // TestingProfile requires a browser-style task environment.
  content::BrowserTaskEnvironment task_environment_;
  // Some methods under test require a profile.
  TestingProfile profile_;
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

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && BUILDFLAG(IS_ANDROID)
class DownloadOfflineContentProviderWithSafeBrowsingTest
    : public DownloadOfflineContentProviderTest {
 public:
  void SetUp() override {
    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();

    DownloadOfflineContentProviderTest::SetUp();
  }

  void TearDown() override {
    DownloadOfflineContentProviderTest::TearDown();
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    test_safe_browsing_service()->ClearDownloadReport();
  }

  safe_browsing::TestSafeBrowsingService* test_safe_browsing_service() {
    return sb_service_.get();
  }

 protected:
  scoped_refptr<safe_browsing::TestSafeBrowsingService> sb_service_;
};

// Tests that validating a download produces a Safe Browsing warning bypass
// report.
TEST_F(DownloadOfflineContentProviderWithSafeBrowsingTest,
       ValidateSendsSafeBrowsingDownloadReport) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  ContentId id(kTestDownloadNamespace, guid);

  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  EXPECT_CALL(*item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT));
  EXPECT_CALL(*item, IsDangerous()).WillRepeatedly(Return(true));

  InitializeDownloads(true);

  provider_.ValidateDangerousDownload(id);
  RunUntilMainThreadIdle();

  safe_browsing::ClientSafeBrowsingReportRequest sent_report;
  ASSERT_TRUE(sent_report.ParseFromString(
      test_safe_browsing_service()->serialized_download_report()));

  EXPECT_EQ(sent_report.type(), safe_browsing::ClientSafeBrowsingReportRequest::
                                    DANGEROUS_DOWNLOAD_WARNING_ANDROID);

  EXPECT_EQ(item->GetURL().spec(), sent_report.url());
  EXPECT_TRUE(sent_report.did_proceed());
}
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
// Tests that when the UI is hidden by an extension updates do not show.
TEST_F(DownloadOfflineContentProviderTest, DoNotShowHiddenByExtension) {
  DownloadCoreService* service =
      DownloadCoreServiceFactory::GetForBrowserContext(&profile_);
  service->SetDownloadUiEnabledForTest(false);

  const std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);

  ScopedMockObserver observer{&provider_};
  EXPECT_CALL(observer, OnItemUpdated(_, _)).Times(0);

  InitializeDownloads(true);
  provider_.OnDownloadUpdated(item.get());
  RunUntilMainThreadIdle();
}

// Tests that when the UI is hidden by an extension dangerous updates do show.
TEST_F(DownloadOfflineContentProviderTest, DoShowDangerousHiddenByExtension) {
  DownloadCoreService* service =
      DownloadCoreServiceFactory::GetForBrowserContext(&profile_);
  service->SetDownloadUiEnabledForTest(false);

  const std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
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

// Tests that OnDownloadUpdated() does not crash when profile is null.
TEST_F(DownloadOfflineContentProviderTest, OnDownloadUpdatedWithNullProfile) {
  provider_.OnProfileCreated(nullptr);
  const std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::unique_ptr<download::MockDownloadItem> item =
      CreateMockDownloadItem(guid);
  provider_.OnDownloadUpdated(item.get());
  // No crash.
  RunUntilMainThreadIdle();
}
#endif
