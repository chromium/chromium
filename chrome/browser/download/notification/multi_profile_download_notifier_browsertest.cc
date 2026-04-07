// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/multi_profile_download_notifier.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_download_item.h"
#include "content/public/test/mock_download_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

class MockNotifierClient : public MultiProfileDownloadNotifier::Client {
 public:
  MockNotifierClient() = default;
  ~MockNotifierClient() override = default;
  MockNotifierClient(const MockNotifierClient&) = delete;
  MockNotifierClient& operator=(const MockNotifierClient&) = delete;

  MOCK_METHOD(void,
              OnManagerInitialized,
              (content::DownloadManager * manager),
              (override));
  MOCK_METHOD(void,
              OnManagerGoingDown,
              (content::DownloadManager * manager),
              (override));
  MOCK_METHOD(void,
              OnDownloadCreated,
              (content::DownloadManager * manager,
               download::DownloadItem* item),
              (override));
  MOCK_METHOD(void,
              OnDownloadUpdated,
              (content::DownloadManager * manager,
               download::DownloadItem* item),
              (override));
  MOCK_METHOD(void,
              OnDownloadDestroyed,
              (content::DownloadManager * manager,
               download::DownloadItem* item),
              (override));
  MOCK_METHOD(bool, ShouldObserveProfile, (Profile * profile), (override));
};

// A mock `content::DownloadManager` which can notify observers of events.
class MockDownloadManager : public content::MockDownloadManager {
 public:
  // content::MockDownloadManager:
  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void Shutdown() override {
    for (auto& observer : observers_) {
      observer.ManagerGoingDown(this);
    }
  }

 private:
  base::ObserverList<content::DownloadManager::Observer>::Unchecked observers_;
};

}  // namespace

class MultiProfileDownloadNotifierBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    notifier_ = std::make_unique<MultiProfileDownloadNotifier>(
        &client(), RequireManagerInitialization());
    ConfigureMockManagerForProfile(browser()->profile());
  }

  void ConfigureMockManagerForProfile(Profile* profile) {
    auto manager = std::make_unique<testing::NiceMock<MockDownloadManager>>();
    ON_CALL(*manager, IsManagerInitialized())
        .WillByDefault(testing::Return(ShouldInitializeManager()));
    ON_CALL(*manager, GetBrowserContext())
        .WillByDefault(testing::Return(profile));
    manager_ = manager.get();
    profile->SetDownloadManagerForTesting(std::move(manager));
  }

  virtual bool ShouldInitializeManager() { return true; }

  virtual bool RequireManagerInitialization() { return true; }

  MockNotifierClient& client() { return client_; }

  MockDownloadManager* manager() { return manager_; }

  download::MockDownloadItem* item() { return &item_; }

  MultiProfileDownloadNotifier* notifier() { return notifier_.get(); }

  download::AllDownloadItemNotifier::Observer*
  notifier_as_all_download_item_notifier_observer() const {
    return notifier_.get();
  }

 private:
  testing::NiceMock<MockNotifierClient> client_;
  raw_ptr<testing::NiceMock<MockDownloadManager>, AcrossTasksDanglingUntriaged>
      manager_;
  testing::NiceMock<download::MockDownloadItem> item_;
  std::unique_ptr<MultiProfileDownloadNotifier> notifier_;
};

IN_PROC_BROWSER_TEST_F(MultiProfileDownloadNotifierBrowserTest,
                       ProfileObservation) {
  // Make sure that profiles are not observed when they are filtered by
  // `ShouldObserveProfile()`.
  Profile* profile = browser()->profile();
  ON_CALL(client(), ShouldObserveProfile(profile))
      .WillByDefault(testing::Return(false));
  // Since `manager()`'s `IsManagerInitialized()` is mocked to return true,
  // `client()`'s `OnManagerInitialized()` would be called if the profile's
  // addition were not filtered.
  EXPECT_CALL(client(), OnManagerInitialized(manager())).Times(0);
  notifier()->AddProfile(profile);

  // Make sure that profiles are observed when they are not filtered by
  // `ShouldObserveProfile()`.
  ON_CALL(client(), ShouldObserveProfile(_))
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(client(), OnManagerInitialized(manager()));
  notifier()->AddProfile(profile);

  // Make sure that off-the-record profiles are observed when spawned from an
  // observed profile.
  // In a browser test, we can just use the real OTR profile.
  EXPECT_CALL(client(), OnManagerInitialized(_));
  Profile* const incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // If `profile` and `incognito_profile` are being observed, `client()`'s
  // `OnManagerGoingDown()` should be called with each profile's download
  // manager when the profile goes out of scope.
  EXPECT_CALL(client(), OnManagerGoingDown(manager()));
  EXPECT_CALL(client(),
              OnManagerGoingDown(incognito_profile->GetDownloadManager()));
}

class MultiProfileDownloadNotifierManagerInitializationBrowserTest
    : public MultiProfileDownloadNotifierBrowserTest,
      public testing::WithParamInterface<
          std::tuple<bool /* should_initialize_manager */,
                     bool /* require_manager_initialization */>> {
 public:
  bool ShouldInitializeManager() override { return std::get<0>(GetParam()); }
  bool RequireManagerInitialization() override {
    return std::get<1>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiProfileDownloadNotifierManagerInitializationBrowserTest,
    testing::Combine(/*should_initialize_manager=*/testing::Bool(),
                     /*require_manager_initialization=*/testing::Bool()));

IN_PROC_BROWSER_TEST_P(
    MultiProfileDownloadNotifierManagerInitializationBrowserTest,
    ClientCalls) {
  Profile* profile = browser()->profile();
  ON_CALL(client(), ShouldObserveProfile(profile))
      .WillByDefault(testing::Return(true));
  notifier()->AddProfile(profile);

  EXPECT_CALL(client(), OnManagerInitialized(manager()));
  notifier_as_all_download_item_notifier_observer()->OnManagerInitialized(
      manager());

  // Make sure the OnDownload...() functions are not called when we require the
  // download manager to be initialized, but it is not.
  int num_calls_expected =
      !RequireManagerInitialization() || ShouldInitializeManager();

  EXPECT_CALL(client(), OnDownloadCreated(manager(), item()))
      .Times(num_calls_expected);
  notifier_as_all_download_item_notifier_observer()->OnDownloadCreated(
      manager(), item());

  EXPECT_CALL(client(), OnDownloadUpdated(manager(), item()))
      .Times(num_calls_expected);
  notifier_as_all_download_item_notifier_observer()->OnDownloadUpdated(
      manager(), item());

  EXPECT_CALL(client(), OnDownloadDestroyed(manager(), item()))
      .Times(num_calls_expected);
  notifier_as_all_download_item_notifier_observer()->OnDownloadDestroyed(
      manager(), item());

  EXPECT_CALL(client(), OnManagerGoingDown(manager()));
  notifier_as_all_download_item_notifier_observer()->OnManagerGoingDown(
      manager());
}

IN_PROC_BROWSER_TEST_P(
    MultiProfileDownloadNotifierManagerInitializationBrowserTest,
    DownloadRetrieval) {
  Profile* profile = browser()->profile();
  ON_CALL(client(), ShouldObserveProfile(profile))
      .WillByDefault(testing::Return(true));
  notifier()->AddProfile(profile);

  // For each `MultiProfileDownloadNotifier` download retrieval function, mock
  // the `DownloadManager` function to which it ultimately forwards.
  std::vector<std::unique_ptr<content::FakeDownloadItem>> downloads;
  downloads.emplace_back(std::make_unique<content::FakeDownloadItem>());
  downloads.emplace_back(std::make_unique<content::FakeDownloadItem>());

  ON_CALL(*manager(), GetDownloadByGuid)
      .WillByDefault([&downloads](const std::string& guid) {
        return downloads.front().get();
      });
  auto* item = notifier()->GetDownloadByGuid("");
  // `GetDownloadByGuid()` ignores manager initialization status.
  EXPECT_EQ(item, downloads.front().get());

  ON_CALL(*manager(), GetAllDownloads)
      .WillByDefault(
          [&downloads](
              std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>*
                  download_ptrs) {
            for (auto& download : downloads) {
              download_ptrs->push_back(download.get());
            }
          });
  auto retrieved_downloads = notifier()->GetAllDownloads();
  // `GetAllDownloads()` honors `client()`'s manager initialization requirement.
  EXPECT_EQ(retrieved_downloads.size(),
            !RequireManagerInitialization() || ShouldInitializeManager()
                ? downloads.size()
                : 0u);
}
