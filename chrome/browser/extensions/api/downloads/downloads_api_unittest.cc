// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/downloads/downloads_api.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using HistoryService = history::HistoryService;
using MockDownloadManager = content::MockDownloadManager;

namespace extensions {

namespace {

// A DownloadCoreService that returns a custom DownloadHistory.
class TestDownloadCoreService : public DownloadCoreServiceImpl {
 public:
  explicit TestDownloadCoreService(Profile* profile)
      : DownloadCoreServiceImpl(profile), profile_(profile) {}

  TestDownloadCoreService(const TestDownloadCoreService&) = delete;
  TestDownloadCoreService& operator=(const TestDownloadCoreService&) = delete;

  ~TestDownloadCoreService() override {}

  void Shutdown() override {
    DownloadCoreServiceImpl::Shutdown();
    download_history_.reset();
    router_.reset();
  }

  void set_download_history(std::unique_ptr<DownloadHistory> download_history) {
    download_history_.swap(download_history);
  }

  DownloadHistory* GetDownloadHistory() override {
    return download_history_.get();
  }

  ExtensionDownloadsEventRouter* GetExtensionEventRouter() override {
    if (!router_.get()) {
      router_ = std::make_unique<ExtensionDownloadsEventRouter>(
          profile_, profile_->GetDownloadManager());
    }
    return router_.get();
  }

 private:
  std::unique_ptr<DownloadHistory> download_history_;
  std::unique_ptr<ExtensionDownloadsEventRouter> router_;
  raw_ptr<Profile> profile_;
};

}  // namespace

class DownloadsApiUnitTest : public ExtensionApiUnittest {
 public:
  DownloadsApiUnitTest() {}

  DownloadsApiUnitTest(const DownloadsApiUnitTest&) = delete;
  DownloadsApiUnitTest& operator=(const DownloadsApiUnitTest&) = delete;

  ~DownloadsApiUnitTest() override {}
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    manager_ = std::make_unique<testing::StrictMock<MockDownloadManager>>();
    EXPECT_CALL(*manager_, IsManagerInitialized());
    EXPECT_CALL(*manager_, AddObserver(testing::_))
        .WillOnce(testing::SaveArg<0>(&download_history_manager_observer_));
    EXPECT_CALL(*manager_, RemoveObserver(testing::Eq(testing::ByRef(
                               download_history_manager_observer_))))
        .WillOnce(testing::Assign(
            &download_history_manager_observer_,
            static_cast<content::DownloadManager::Observer*>(nullptr)));
    EXPECT_CALL(*manager_, GetAllDownloads(testing::_))
        .Times(testing::AnyNumber());

    std::unique_ptr<HistoryAdapter> history_adapter(new HistoryAdapter);
    std::unique_ptr<DownloadHistory> download_history(
        new DownloadHistory(manager_.get(), std::move(history_adapter)));
    TestDownloadCoreService* download_core_service =
        static_cast<TestDownloadCoreService*>(
            DownloadCoreServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating(&TestingDownloadCoreServiceFactory)));
    ASSERT_TRUE(download_core_service);
    download_core_service->set_download_history(std::move(download_history));
  }

 private:
  // A private empty history adapter that does nothing on QueryDownloads().
  class HistoryAdapter : public DownloadHistory::HistoryAdapter {
   public:
    HistoryAdapter() : DownloadHistory::HistoryAdapter(nullptr) {}

   private:
    void QueryDownloads(
        HistoryService::DownloadQueryCallback callback) override {}
  };

  // Constructs and returns a TestDownloadCoreService.
  static std::unique_ptr<KeyedService> TestingDownloadCoreServiceFactory(
      content::BrowserContext* browser_context);

  std::unique_ptr<MockDownloadManager> manager_;
  raw_ptr<content::DownloadManager::Observer>
      download_history_manager_observer_;
};

// static
std::unique_ptr<KeyedService>
DownloadsApiUnitTest::TestingDownloadCoreServiceFactory(
    content::BrowserContext* browser_context) {
  return std::make_unique<TestDownloadCoreService>(
      Profile::FromBrowserContext(browser_context));
}

// Tests that Number/double properties in query are parsed correctly.
// Regression test for https://crbug.com/617435.
TEST_F(DownloadsApiUnitTest, ParseSearchQuery) {
  RunFunction(new DownloadsSearchFunction, "[{\"totalBytesLess\":1}]");
  RunFunction(new DownloadsSearchFunction, "[{\"totalBytesGreater\":2}]");
}

}  // namespace extensions
