// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_ui_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/tls_deprecation_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/history/core/browser/download_row.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::MockDownloadManager;
using download::MockDownloadItem;
using history::HistoryService;
using testing::_;
using testing::AnyNumber;
using testing::Assign;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using testing::SaveArg;

namespace {

// A DownloadUIController::Delegate that stores the DownloadItem* for the last
// download that was sent to the UI.
class TestDelegate : public DownloadUIController::Delegate {
 public:
  explicit TestDelegate(base::WeakPtr<download::DownloadItem*> receiver);
  ~TestDelegate() override {}

 private:
  void OnNewDownloadReady(download::DownloadItem* item) override;

  base::WeakPtr<download::DownloadItem*> receiver_;
};

TestDelegate::TestDelegate(base::WeakPtr<download::DownloadItem*> receiver)
    : receiver_(receiver) {}

void TestDelegate::OnNewDownloadReady(download::DownloadItem* item) {
  if (receiver_.get())
    *receiver_ = item;
}

// A DownloadCoreService that returns a custom DownloadHistory.
class TestDownloadCoreService : public DownloadCoreServiceImpl {
 public:
  explicit TestDownloadCoreService(Profile* profile);
  ~TestDownloadCoreService() override;

  void set_download_history(std::unique_ptr<DownloadHistory> download_history) {
    download_history_.swap(download_history);
  }
  DownloadHistory* GetDownloadHistory() override;

 private:
  std::unique_ptr<DownloadHistory> download_history_;
};

TestDownloadCoreService::TestDownloadCoreService(Profile* profile)
    : DownloadCoreServiceImpl(profile) {}

TestDownloadCoreService::~TestDownloadCoreService() {}

DownloadHistory* TestDownloadCoreService::GetDownloadHistory() {
  return download_history_.get();
}

// The test fixture:
class DownloadUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  DownloadUIControllerTest();

 protected:
  // testing::Test
  void SetUp() override;

  // Returns a TestDelegate. Invoking OnNewDownloadReady on the returned
  // delegate results in the DownloadItem* being stored in |notified_item_|.
  std::unique_ptr<DownloadUIController::Delegate> GetTestDelegate();

  MockDownloadManager* manager() { return manager_.get(); }

  // Returns the DownloadManager::Observer registered by a test case. This is
  // the DownloadUIController's observer for all current test cases.
  content::DownloadManager::Observer* manager_observer() {
    return manager_observer_;
  }

  // The most recent DownloadItem that was passed into OnNewDownloadReady().
  download::DownloadItem* notified_item() { return notified_item_; }

  // DownloadHistory performs a query of existing downloads when it is first
  // instantiated. This method returns a pointer to the completion callback
  // for that query. It can be used to inject history downloads.
  HistoryService::DownloadQueryCallback* history_query_callback() {
    return &(history_adapter_->download_query_callback_);
  }

  // DownloadManager::Observer registered by DownloadHistory.
  content::DownloadManager::Observer* download_history_manager_observer() {
    return download_history_manager_observer_;
  }

  std::unique_ptr<MockDownloadItem> CreateMockInProgressDownload();

 private:
  // A private history adapter that stores the DownloadQueryCallback when
  // QueryDownloads is called.
  class HistoryAdapter : public DownloadHistory::HistoryAdapter {
   public:
    HistoryAdapter() : DownloadHistory::HistoryAdapter(NULL) {}
    HistoryService::DownloadQueryCallback download_query_callback_;

   private:
    void QueryDownloads(
        HistoryService::DownloadQueryCallback callback) override {
      download_query_callback_ = std::move(callback);
    }

    void UpdateDownload(const history::DownloadRow& data,
                        bool should_commit_immediately) override {}
  };

  // Constructs and returns a TestDownloadCoreService.
  static std::unique_ptr<KeyedService> TestingDownloadCoreServiceFactory(
      content::BrowserContext* browser_context);

  std::unique_ptr<MockDownloadManager> manager_;
  content::DownloadManager::Observer* download_history_manager_observer_;
  content::DownloadManager::Observer* manager_observer_;
  download::DownloadItem* notified_item_;
  base::WeakPtrFactory<download::DownloadItem*> notified_item_receiver_factory_;

  HistoryAdapter* history_adapter_;
};

// static
std::unique_ptr<KeyedService>
DownloadUIControllerTest::TestingDownloadCoreServiceFactory(
    content::BrowserContext* browser_context) {
  return std::make_unique<TestDownloadCoreService>(
      Profile::FromBrowserContext(browser_context));
}

DownloadUIControllerTest::DownloadUIControllerTest()
    : download_history_manager_observer_(NULL),
      manager_observer_(NULL),
      notified_item_(NULL),
      notified_item_receiver_factory_(&notified_item_) {
}

void DownloadUIControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  manager_.reset(new testing::StrictMock<MockDownloadManager>());
  EXPECT_CALL(*manager_, IsManagerInitialized()).Times(AnyNumber());
  EXPECT_CALL(*manager_, AddObserver(_))
      .WillOnce(SaveArg<0>(&download_history_manager_observer_));
  EXPECT_CALL(*manager_,
              RemoveObserver(testing::Eq(
                  testing::ByRef(download_history_manager_observer_))))
      .WillOnce(testing::Assign(
          &download_history_manager_observer_,
          static_cast<content::DownloadManager::Observer*>(NULL)));
  EXPECT_CALL(*manager_, GetAllDownloads(_)).Times(AnyNumber());

  std::unique_ptr<HistoryAdapter> history_adapter(new HistoryAdapter);
  history_adapter_ = history_adapter.get();
  std::unique_ptr<DownloadHistory> download_history(
      new DownloadHistory(manager_.get(), std::move(history_adapter)));
  ASSERT_TRUE(download_history_manager_observer_);

  EXPECT_CALL(*manager_, AddObserver(_))
      .WillOnce(SaveArg<0>(&manager_observer_));
  EXPECT_CALL(*manager_,
              RemoveObserver(testing::Eq(testing::ByRef(manager_observer_))))
      .WillOnce(testing::Assign(
          &manager_observer_,
          static_cast<content::DownloadManager::Observer*>(NULL)));
  TestDownloadCoreService* download_core_service =
      static_cast<TestDownloadCoreService*>(
          DownloadCoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              browser_context(),
              base::BindRepeating(&TestingDownloadCoreServiceFactory)));
  ASSERT_TRUE(download_core_service);
  download_core_service->set_download_history(std::move(download_history));
}

std::unique_ptr<MockDownloadItem>
DownloadUIControllerTest::CreateMockInProgressDownload() {
  std::unique_ptr<MockDownloadItem> item(
      new testing::StrictMock<MockDownloadItem>());
  EXPECT_CALL(*item, GetId()).WillRepeatedly(Return(1));
  EXPECT_CALL(*item, GetGuid())
      .WillRepeatedly(
          ReturnRefOfCopy(std::string("14CA04AF-ECEC-4B13-8829-817477EFAB83")));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(
          ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*item, GetFullPath()).WillRepeatedly(
      ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item, GetUrlChain())
      .WillRepeatedly(ReturnRefOfCopy(std::vector<GURL>()));
  EXPECT_CALL(*item, GetReferrerUrl()).WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, GetSiteUrl()).WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, GetTabUrl()).WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, GetTabReferrerUrl())
      .WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, GetStartTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*item, GetEndTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*item, GetETag()).WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*item, GetLastModifiedTime())
      .WillRepeatedly(ReturnRefOfCopy(std::string()));
  EXPECT_CALL(*item, GetDangerType())
      .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(*item, GetLastReason())
      .WillRepeatedly(Return(download::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_CALL(*item, GetReceivedBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(*item, GetReceivedSlices())
      .WillRepeatedly(ReturnRefOfCopy(
          std::vector<download::DownloadItem::ReceivedSlice>()));
  EXPECT_CALL(*item, GetTotalBytes()).WillRepeatedly(Return(0));
  EXPECT_CALL(*item, GetTargetDisposition())
      .WillRepeatedly(
          Return(download::DownloadItem::TARGET_DISPOSITION_OVERWRITE));
  EXPECT_CALL(*item, GetOpened()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item, GetLastAccessTime()).WillRepeatedly(Return(base::Time()));
  EXPECT_CALL(*item, IsTransient()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item, GetMimeType()).WillRepeatedly(Return(std::string()));
  EXPECT_CALL(*item, GetURL()).WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, IsTemporary()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item, GetDownloadCreationType())
      .WillRepeatedly(Return(download::DownloadItem::TYPE_ACTIVE_DOWNLOAD));
  EXPECT_CALL(*item, IsSavePackageDownload()).WillRepeatedly(Return(false));
  EXPECT_CALL(*item, GetOriginalMimeType())
      .WillRepeatedly(Return(std::string()));
  content::DownloadItemUtils::AttachInfo(item.get(), browser_context(),
                                         nullptr);

  return item;
}

std::unique_ptr<DownloadUIController::Delegate>
DownloadUIControllerTest::GetTestDelegate() {
  std::unique_ptr<DownloadUIController::Delegate> delegate(
      new TestDelegate(notified_item_receiver_factory_.GetWeakPtr()));
  return delegate;
}

// New downloads should be presented to the UI when GetTargetFilePath() returns
// a non-empty path.  I.e. once the download target has been determined.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyBasic) {
  std::unique_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  // The destination for the download hasn't been determined yet. It should not
  // be displayed.
  EXPECT_FALSE(notified_item());

  // Once the destination has been determined, then it should be displayed.
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillOnce(ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));
  item->NotifyObserversDownloadUpdated();

  EXPECT_EQ(static_cast<download::DownloadItem*>(item.get()), notified_item());
}

// A download that's created in an interrupted state should also be displayed.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyBasic_Interrupted) {
  std::unique_ptr<MockDownloadItem> item = CreateMockInProgressDownload();
  DownloadUIController controller(manager(), GetTestDelegate());
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::INTERRUPTED));

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());
  EXPECT_EQ(static_cast<download::DownloadItem*>(item.get()), notified_item());
}

// Downloads that have a target path on creation and are in the IN_PROGRESS
// state should be displayed in the UI immediately without requiring an
// additional OnDownloadUpdated() notification.
TEST_F(DownloadUIControllerTest, DownloadUIController_NotifyReadyOnCreate) {
  std::unique_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  DownloadUIController controller(manager(), GetTestDelegate());

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());
  EXPECT_EQ(static_cast<download::DownloadItem*>(item.get()), notified_item());
}

// The UI shouldn't be notified of downloads that were restored from history.
TEST_F(DownloadUIControllerTest, DownloadUIController_HistoryDownload) {
  DownloadUIController controller(manager(), GetTestDelegate());
  // DownloadHistory should already have been created. It performs a query of
  // existing downloads upon creation. We'll use the callback to inject a
  // history download.
  ASSERT_FALSE(history_query_callback()->is_null());

  // download_history_manager_observer is the DownloadManager::Observer
  // registered by the DownloadHistory. DownloadHistory relies on the
  // OnDownloadCreated notification to mark a download as having been restored
  // from history.
  ASSERT_TRUE(download_history_manager_observer());

  std::vector<history::DownloadRow> history_downloads;
  history_downloads.push_back(history::DownloadRow());
  history_downloads.front().id = 1;

  std::vector<GURL> url_chain;
  GURL url;
  std::unique_ptr<MockDownloadItem> item = CreateMockInProgressDownload();

  EXPECT_CALL(*item, GetDownloadCreationType())
      .WillRepeatedly(Return(download::DownloadItem::TYPE_HISTORY_IMPORT));
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::INTERRUPTED));
  EXPECT_CALL(*item, IsDone()).WillRepeatedly(Return(false));
  EXPECT_CALL(
      *manager(),
      PostInitialization(content::DownloadManager::
                             DOWNLOAD_INITIALIZATION_DEPENDENCY_HISTORY_DB));

  {
    testing::InSequence s;
    testing::MockFunction<void()> mock_function;
    // DownloadHistory will immediately try to create a download using the info
    // we push through the query callback. When DownloadManager::CreateDownload
    // is called, we need to first invoke the OnDownloadCreated callback for
    // DownloadHistory before returning the DownloadItem since that's the
    // sequence of events expected by DownloadHistory.
    content::DownloadManager::Observer* observer =
        download_history_manager_observer();
    MockDownloadManager* download_manager = manager();
    MockDownloadItem* download_item = item.get();
    EXPECT_CALL(*manager(), MockCreateDownloadItem(_))
        .WillOnce(testing::DoAll(
            testing::InvokeWithoutArgs(
                [observer, download_manager, download_item]() {
                  observer->OnDownloadCreated(download_manager, download_item);
                }),
            Return(item.get())));
    EXPECT_CALL(mock_function, Call());

    std::move(*history_query_callback()).Run(std::move(history_downloads));
    mock_function.Call();
  }

  // Now pass along the notification to the OnDownloadCreated observer from
  // DownloadUIController. It should ignore the download since it's marked as
  // being restored from history.
  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  // Finally, the expectation we've been waiting for:
  EXPECT_FALSE(notified_item());

  // Resume the download, and it should update the UI.
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(download::DownloadItem::IN_PROGRESS));
  item->NotifyObserversDownloadUpdated();
  EXPECT_EQ(static_cast<download::DownloadItem*>(item.get()), notified_item());
}

TEST_F(DownloadUIControllerTest, LegacyTLSMetrics) {
  base::HistogramTester histograms;
  SecurityStateTabHelper::CreateForWebContents(web_contents());
  InitializeEmptyLegacyTLSConfig();

  auto navigation =
      CreateLegacyTLSNavigation(GURL(kLegacyTLSDefaultURL), web_contents());
  navigation->Commit();

  // Start a download from the same page, setting up the mock item to correctly
  // associate with the WebContents of the previous navigation.
  std::unique_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  GURL download_url("https://download.test/file.bin");
  EXPECT_CALL(*item, GetURL()).WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*item, GetOriginalUrl()).WillRepeatedly(ReturnRef(download_url));
  content::DownloadItemUtils::AttachInfo(item.get(), browser_context(),
                                         web_contents());

  DownloadUIController controller(manager(), GetTestDelegate());

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  histograms.ExpectUniqueSample("Security.LegacyTLS.DownloadStarted", true, 1);
}

TEST_F(DownloadUIControllerTest, LegacyTLSControlSiteMetrics) {
  base::HistogramTester histograms;
  SecurityStateTabHelper::CreateForWebContents(web_contents());
  InitializeLegacyTLSConfigWithControl();

  auto navigation =
      CreateLegacyTLSNavigation(GURL(kLegacyTLSControlURL), web_contents());
  navigation->Commit();

  // Start a download from the same page, setting up the mock item to correctly
  // associate with the WebContents of the previous navigation.
  std::unique_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  GURL download_url("https://download.test/file.bin");
  EXPECT_CALL(*item, GetURL()).WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*item, GetOriginalUrl()).WillRepeatedly(ReturnRef(download_url));
  content::DownloadItemUtils::AttachInfo(item.get(), browser_context(),
                                         web_contents());

  DownloadUIController controller(manager(), GetTestDelegate());

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  histograms.ExpectUniqueSample("Security.LegacyTLS.DownloadStarted", false, 1);
}

TEST_F(DownloadUIControllerTest, LegacyTLSGoodSiteMetrics) {
  base::HistogramTester histograms;
  SecurityStateTabHelper::CreateForWebContents(web_contents());
  InitializeEmptyLegacyTLSConfig();

  auto navigation =
      CreateNonlegacyTLSNavigation(GURL("https://good.test"), web_contents());
  navigation->Commit();

  // Start a download from the same page, setting up the mock item to correctly
  // associate with the WebContents of the previous navigation.
  std::unique_ptr<MockDownloadItem> item(CreateMockInProgressDownload());
  GURL download_url("https://download.test/file.bin");
  EXPECT_CALL(*item, GetURL()).WillRepeatedly(ReturnRef(download_url));
  EXPECT_CALL(*item, GetOriginalUrl()).WillRepeatedly(ReturnRef(download_url));
  content::DownloadItemUtils::AttachInfo(item.get(), browser_context(),
                                         web_contents());

  DownloadUIController controller(manager(), GetTestDelegate());

  ASSERT_TRUE(manager_observer());
  manager_observer()->OnDownloadCreated(manager(), item.get());

  histograms.ExpectUniqueSample("Security.LegacyTLS.DownloadStarted", false, 1);
}

} // namespace
