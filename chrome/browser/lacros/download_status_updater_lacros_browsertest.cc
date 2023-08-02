// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/mock_download_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Aliases.
using ::crosapi::mojom::DownloadState;
using ::crosapi::mojom::DownloadStatus;
using ::crosapi::mojom::DownloadStatusPtr;
using ::crosapi::mojom::DownloadStatusUpdater;
using ::crosapi::mojom::DownloadStatusUpdaterClient;
using ::crosapi::mojom::DownloadStatusUpdaterClientAsyncWaiter;
using ::testing::Action;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Pointer;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

// Actions ---------------------------------------------------------------------

ACTION_P3(InvokeEq, invokee, invokable, expected) {
  return (invokee->*invokable)() == expected;
}

ACTION_P2(ReturnAllOf, a, b) {
  Action<bool()> action_a = a;
  Action<bool()> action_b = b;
  return action_a.Perform(std::forward_as_tuple()) &&
         action_b.Perform(std::forward_as_tuple());
}

// MockDownloadManager ---------------------------------------------------------

// A mock `content::DownloadManager` for testing which supports realistic
// observer behaviors and propagation of download creation events.
class MockDownloadManager : public content::MockDownloadManager {
 public:
  // content::MockDownloadManager:
  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void Shutdown() override {
    for (auto& observer : observer_list_) {
      observer.ManagerGoingDown(this);
    }
  }

  void NotifyDownloadCreated(download::DownloadItem* item) {
    for (auto& observer : observer_list_) {
      observer.OnDownloadCreated(this, item);
    }
  }

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

// MockDownloadStatusUpdater ---------------------------------------------------

// A mock `DownloadStatusUpdater` for testing.
class MockDownloadStatusUpdater : public DownloadStatusUpdater {
 public:
  // DownloadStatusUpdater:
  MOCK_METHOD(void,
              BindClient,
              (mojo::PendingRemote<DownloadStatusUpdaterClient> client),
              (override));
  MOCK_METHOD(void, Update, (DownloadStatusPtr status), (override));
};

// DownloadStatusUpdaterBrowserTest --------------------------------------------

// Base class for tests of `DownloadStatusUpdater`.
class DownloadStatusUpdaterBrowserTest : public DownloadTestBase {
 public:
  // DownloadTestBase:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    DownloadTestBase::CreatedBrowserMainParts(browser_main_parts);

    // Replace the binding for the Ash Chrome download status updater with a
    // mock that can be observed for interactions with Lacros Chrome.
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        download_status_updater_receiver_
            .BindNewPipeAndPassRemoteWithVersion());

    // When the Lacros Chrome download status updater is initiated, it will
    // attempt to bind the client for the Ash Chrome download status updater.
    // Bind the client ourselves so we can verify it is working as intended.
    EXPECT_CALL(download_status_updater_, BindClient)
        .WillOnce(Invoke(
            [&](mojo::PendingRemote<DownloadStatusUpdaterClient> client) {
              download_status_updater_client_.Bind(std::move(client));
            }));
  }

  void SetUpOnMainThread() override {
    DownloadTestBase::SetUpOnMainThread();

    // Associate the mock download manager with the Lacros Chrome browser.
    ON_CALL(download_manager_, GetBrowserContext())
        .WillByDefault(Return(browser()->profile()));

    // Register the mock download manager with the download status updater in
    // Lacros Chrome.
    g_browser_process->download_status_updater()->AddManager(
        &download_manager_);
  }

  // Runs the current message loop until a no-op message on the download status
  // updater interface's message pipe is received. This effectively ensures that
  // any messages in transit are received before returning.
  void FlushInterfaceForTesting() {
    chromeos::LacrosService::Get()
        ->GetRemote<DownloadStatusUpdater>()
        .FlushForTesting();
  }

  // Returns the mock download manager registered with the download status
  // updater in Lacros Chrome.
  NiceMock<MockDownloadManager>& download_manager() {
    return download_manager_;
  }

  // Returns the mock download status updater in Ash Chrome that can be observed
  // for interactions with Lacros Chrome.
  NiceMock<MockDownloadStatusUpdater>& download_status_updater() {
    return download_status_updater_;
  }

  // Returns the client for the download status updater in Ash Chrome,
  // implemented by the delegate of the download status updater in Lacros
  // Chrome.
  DownloadStatusUpdaterClient* download_status_updater_client() {
    return download_status_updater_client_.get();
  }

 private:
  // The mock download manager registered with the download status updater in
  // Lacros Chrome.
  NiceMock<MockDownloadManager> download_manager_;

  // The mock download status updater in Ash Chrome that can be observed for
  // interactions with Lacros Chrome.
  NiceMock<MockDownloadStatusUpdater> download_status_updater_;
  mojo::Receiver<DownloadStatusUpdater> download_status_updater_receiver_{
      &download_status_updater_};

  // The client for the download status updater in Ash Chrome, implemented by
  // the delegate of the download status updater in Lacros Chrome.
  mojo::Remote<DownloadStatusUpdaterClient> download_status_updater_client_;
};

// Tests -----------------------------------------------------------------------

// Verifies that `DownloadStatusUpdaterClient::Cancel()` works as intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, Cancel) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  EXPECT_TRUE(client.Cancel(item->GetGuid()));
  EXPECT_EQ(item->GetState(), download::DownloadItem::CANCELLED);
}

// Verifies that `DownloadStatusUpdaterClient::Pause()` and `Resume()` work as
// intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, PauseAndResume) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  ASSERT_FALSE(item->IsPaused());

  EXPECT_TRUE(client.Pause(item->GetGuid()));
  EXPECT_TRUE(item->IsPaused());

  EXPECT_TRUE(client.Resume(item->GetGuid()));
  EXPECT_FALSE(item->IsPaused());
  EXPECT_NE(item->GetState(), download::DownloadItem::CANCELLED);

  // Clean up: cancel the item to allow the test to exit.
  item->Cancel(false);
}

// Tests the case where `Pause()` is called on an already-paused item.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, PauseNoOp) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_FALSE(item->IsPaused());
  item->Pause();
  ASSERT_TRUE(item->IsPaused());

  // Handled because item was found (despite being a no-op).
  EXPECT_TRUE(client.Pause(item->GetGuid()));
  EXPECT_TRUE(item->IsPaused());

  // Clean up: cancel the item to allow the test to exit.
  item->Cancel(false);
}

// Tests the case where `Resume()` is called on a not-paused item.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, ResumeNoOp) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  download::DownloadItem* item = CreateSlowTestDownload();
  ASSERT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  ASSERT_FALSE(item->CanResume());

  // Handled because item was found (despite being a no-op).
  EXPECT_TRUE(client.Resume(item->GetGuid()));
  EXPECT_NE(item->GetState(), download::DownloadItem::CANCELLED);
  EXPECT_FALSE(item->CanResume());

  // Clean up: cancel the item to allow the test to exit.
  item->Cancel(false);
}

IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, NoItem) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());

  // Create an item and then remove it so it no longer can be found.
  download::DownloadItem* item = CreateSlowTestDownload();
  std::string guid = item->GetGuid();
  item->Cancel(false);
  item->Remove();

  EXPECT_FALSE(client.Pause(guid));
  EXPECT_FALSE(client.Resume(guid));
  EXPECT_FALSE(client.Cancel(guid));
}

// Verifies that `DownloadStatusUpdaterClient::ShowInBrowser()` works as
// intended. Note that this API is currently hard-coded to no-op and return
// `false`.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, ShowInBrowser) {
  DownloadStatusUpdaterClientAsyncWaiter client(
      download_status_updater_client());
  EXPECT_FALSE(client.ShowInBrowser(/*guid=*/base::EmptyString()));
}

// Verifies that `DownloadStatusUpdater::Update()` events work as intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, Update) {
  // Create a mock in-progress download `item`.
  NiceMock<download::MockDownloadItem> item;
  ON_CALL(item, GetGuid())
      .WillByDefault(
          ReturnRefOfCopy(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  ON_CALL(item, GetReceivedBytes()).WillByDefault(Return(10));
  ON_CALL(item, GetTotalBytes()).WillByDefault(Return(100));
  ON_CALL(item, GetTargetFilePath())
      .WillByDefault(ReturnRefOfCopy(base::FilePath("target_file_path")));

  // Fulfill `CanResume()` dynamically based on `item` state and paused status.
  ON_CALL(item, CanResume())
      .WillByDefault(
          ReturnAllOf(Invoke(&item, &download::DownloadItem::IsPaused),
                      InvokeEq(&item, &download::DownloadItem::GetState,
                               download::DownloadItem::IN_PROGRESS)));

  // Fulfill `IsDone()` dynamically based on `item` state.
  ON_CALL(item, IsDone())
      .WillByDefault(InvokeEq(&item, &download::DownloadItem::GetState,
                              download::DownloadItem::COMPLETE));

  // Associate the download `item` with the browser `profile()`.
  content::DownloadItemUtils::AttachInfoForTesting(&item, browser()->profile(),
                                                   /*web_contents=*/nullptr);

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` creation.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress)),
          Field(&DownloadStatus::received_bytes, Eq(item.GetReceivedBytes())),
          Field(&DownloadStatus::total_bytes, Eq(item.GetTotalBytes())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath())),
          Field(&DownloadStatus::cancellable, Eq(true)),
          Field(&DownloadStatus::pausable, Eq(true)),
          Field(&DownloadStatus::resumable, Eq(false))))));

  // Notify the download status updater in Lacros Chrome of `item` creation and
  // verify Ash Chrome expectations.
  download_manager().NotifyDownloadCreated(&item);
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Pause `item`.
  ON_CALL(item, IsPaused()).WillByDefault(Return(true));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress)),
          Field(&DownloadStatus::received_bytes, Eq(item.GetReceivedBytes())),
          Field(&DownloadStatus::total_bytes, Eq(item.GetTotalBytes())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath())),
          Field(&DownloadStatus::cancellable, Eq(true)),
          Field(&DownloadStatus::pausable, Eq(false)),
          Field(&DownloadStatus::resumable, Eq(true))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());

  // Complete `item`.
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  ON_CALL(item, GetReceivedBytes())
      .WillByDefault(Invoke(&item, &download::DownloadItem::GetTotalBytes));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kComplete)),
          Field(&DownloadStatus::received_bytes, Eq(item.GetReceivedBytes())),
          Field(&DownloadStatus::total_bytes, Eq(item.GetTotalBytes())),
          Field(&DownloadStatus::target_file_path,
                Eq(item.GetTargetFilePath())),
          Field(&DownloadStatus::cancellable, Eq(false)),
          Field(&DownloadStatus::pausable, Eq(false)),
          Field(&DownloadStatus::resumable, Eq(false))))));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  Mock::VerifyAndClearExpectations(&download_status_updater());
}

}  // namespace
