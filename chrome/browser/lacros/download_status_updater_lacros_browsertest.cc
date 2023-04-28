// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/observer_list.h"
#include "base/uuid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
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
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Pointer;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;

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

// A mock `crosapi::mojom::DownloadStatusUpdater` for testing.
class MockDownloadStatusUpdater : public crosapi::mojom::DownloadStatusUpdater {
 public:
  // DownloadStatusUpdater:
  MOCK_METHOD(void,
              Update,
              (crosapi::mojom::DownloadStatusPtr status),
              (override));
};

// DownloadStatusUpdaterBrowserTest --------------------------------------------

// Base class for tests of `crosapi::mojom::DownloadStatusUpdater`.
class DownloadStatusUpdaterBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // This test suite will no-op if the download status updater interface is
    // not available in this version of Ash Chrome.
    if (!IsInterfaceAvailable()) {
      return;
    }

    // Associate the mock download manager with the Lacros Chrome browser.
    ON_CALL(download_manager_, GetBrowserContext())
        .WillByDefault(Return(browser()->profile()));

    // Register the mock download manager with the download status updater in
    // Lacros Chrome.
    g_browser_process->download_status_updater()->AddManager(
        &download_manager_);

    // Replace the binding for the Ash Chrome download status updater with a
    // mock that can be observed for interactions with Lacros Chrome.
    mojo::Remote<crosapi::mojom::DownloadStatusUpdater>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::DownloadStatusUpdater>();
    remote.reset();
    download_status_updater_receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

  // Runs the current message loop until a no-op message on the download status
  // updater interface's message pipe is received. This effectively ensures that
  // any messages in transit are received before returning.
  void FlushInterfaceForTesting() {
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::DownloadStatusUpdater>()
        .FlushForTesting();
  }

  // Returns whether the download status updater interface is available. It may
  // not be available on earlier versions of Ash Chrome.
  bool IsInterfaceAvailable() const {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    return lacros_service &&
           lacros_service->IsAvailable<crosapi::mojom::DownloadStatusUpdater>();
  }

  // Returns the mock download manager registered with the download status
  // updater in Lacros Chrome.
  testing::NiceMock<MockDownloadManager>& download_manager() {
    return download_manager_;
  }

  // Returns the mock download status updater in Ash Chrome that can be observed
  // for interactions with Lacros Chrome.
  testing::NiceMock<MockDownloadStatusUpdater>& download_status_updater() {
    return download_status_updater_;
  }

 private:
  // The mock download manager registered with the download status updater in
  // Lacros Chrome.
  testing::NiceMock<MockDownloadManager> download_manager_;

  // The mock download status updater in Ash Chrome that can be observed for
  // interactions with Lacros Chrome.
  testing::NiceMock<MockDownloadStatusUpdater> download_status_updater_;
  mojo::Receiver<crosapi::mojom::DownloadStatusUpdater>
      download_status_updater_receiver_{&download_status_updater_};
};

// Tests -----------------------------------------------------------------------

// Verifies that `DownloadStatusUpdater::Update()` events work as intended.
IN_PROC_BROWSER_TEST_F(DownloadStatusUpdaterBrowserTest, Update) {
  // If the download status updater interface is not available in this version
  // of Ash Chrome, this test will no-op.
  if (!IsInterfaceAvailable()) {
    GTEST_SKIP();
  }

  // Create a mock in-progress download `item`.
  testing::NiceMock<download::MockDownloadItem> item;
  ON_CALL(item, GetGuid()).WillByDefault(ReturnRefOfCopy(base::GenerateUuid()));
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` creation.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(AllOf(
          Field(&DownloadStatus::guid, Eq(item.GetGuid())),
          Field(&DownloadStatus::state, Eq(DownloadState::kInProgress))))));

  // Notify the download status updater in Lacros Chrome of `item` creation and
  // verify Ash Chrome expectations.
  download_manager().NotifyDownloadCreated(&item);
  FlushInterfaceForTesting();
  testing::Mock::VerifyAndClearExpectations(&download_status_updater());

  // Expect a `DownloadStatusUpdater::Update()` event in Ash Chrome when the
  // download status updater in Lacros Chrome is notified of `item` updates.
  EXPECT_CALL(
      download_status_updater(),
      Update(Pointer(
          AllOf(Field(&DownloadStatus::guid, Eq(item.GetGuid())),
                Field(&DownloadStatus::state, Eq(DownloadState::kComplete))))));

  // Update `item` state.
  ON_CALL(item, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));

  // Notify the download status updater in Lacros Chrome of `item` update and
  // verify Ash Chrome expectations.
  item.NotifyObserversDownloadUpdated();
  FlushInterfaceForTesting();
  testing::Mock::VerifyAndClearExpectations(&download_status_updater());
}

}  // namespace
