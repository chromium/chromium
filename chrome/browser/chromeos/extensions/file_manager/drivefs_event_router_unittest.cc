// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/drivefs/mojom/drivefs.mojom.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/common/extension.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_manager_private = extensions::api::file_manager_private;

using file_manager_private::DriveConfirmDialogEvent;
using file_manager_private::DriveSyncErrorEvent;
using file_manager_private::FileTransferStatus;
using file_manager_private::FileWatchEvent;
using testing::_;

namespace {

class ValueMatcher : public testing::MatcherInterface<const base::Value&> {
 public:
  explicit ValueMatcher(base::Value expected)
      : expected_(std::move(expected)) {}

  bool MatchAndExplain(const base::Value& actual,
                       testing::MatchResultListener* listener) const override {
    *listener << actual;
    return actual == expected_;
  }

  void DescribeTo(::std::ostream* os) const override { *os << expected_; }

 private:
  base::Value expected_;
};

testing::Matcher<const base::Value&> MatchFileTransferStatus(
    std::string file_url,
    file_manager_private::TransferState transfer_state,
    double processed,
    double total,
    int num_total_jobs) {
  FileTransferStatus status;
  status.file_url = std::move(file_url);
  status.transfer_state = transfer_state;
  status.processed = processed;
  status.total = total;
  status.num_total_jobs = num_total_jobs;
  status.hide_when_zero_jobs = true;
  return testing::MakeMatcher(new ValueMatcher(base::Value(
      file_manager_private::OnFileTransfersUpdated::Create(status))));
}

testing::Matcher<const base::Value&> MatchFileWatchEvent(
    const FileWatchEvent& event) {
  return testing::MakeMatcher(new ValueMatcher(
      base::Value(file_manager_private::OnDirectoryChanged::Create(event))));
}

class TestDriveFsEventRouter : public DriveFsEventRouter {
 public:
  TestDriveFsEventRouter() : DriveFsEventRouter(nullptr) {
    ON_CALL(*this, IsPathWatched).WillByDefault(testing::Return(true));
    ON_CALL(*this, GetEventListenerURLs)
        .WillByDefault(testing::Return(std::set<GURL>{
            extensions::Extension::GetBaseURLFromExtensionId("ext")}));
  }

  TestDriveFsEventRouter(const TestDriveFsEventRouter&) = delete;
  TestDriveFsEventRouter& operator=(const TestDriveFsEventRouter&) = delete;

  void BroadcastEvent(extensions::events::HistogramValue histogram_value,
                      const std::string& event_name,
                      base::Value::List event_args) override {
    BroadcastEventImpl(event_name, base::Value(std::move(event_args)));
  }

  MOCK_METHOD(void,
              BroadcastEventImpl,
              (const std::string& name, const base::Value& event));
  MOCK_METHOD(bool, IsPathWatched, (const base::FilePath&));

  GURL ConvertDrivePathToFileSystemUrl(const base::FilePath& file_path,
                                       const GURL& listener_url) override {
    return GURL(base::StrCat({listener_url.host(), ":", file_path.value()}));
  }

  std::string GetDriveFileSystemName() override { return "drivefs"; }

  MOCK_METHOD(std::set<GURL>,
              GetEventListenerURLs,
              (const std::string& event_name),
              (override));
};

class DriveFsEventRouterTest : public testing::Test {
 protected:
  void SetUp() override {
    event_router_ = std::make_unique<TestDriveFsEventRouter>();
  }

  void Unmount() {
    EXPECT_CALL(
        mock(),
        BroadcastEventImpl(
            file_manager_private::OnFileTransfersUpdated::kEventName,
            MatchFileTransferStatus(
                "", file_manager_private::TRANSFER_STATE_FAILED, 0, 0, 0)));
    EXPECT_CALL(
        mock(),
        BroadcastEventImpl(
            file_manager_private::OnPinTransfersUpdated::kEventName,
            MatchFileTransferStatus(
                "", file_manager_private::TRANSFER_STATE_FAILED, 0, 0, 0)));

    observer().OnUnmounted();
  }

  drivefs::DriveFsHostObserver& observer() { return *event_router_; }
  TestDriveFsEventRouter& mock() { return *event_router_; }

  std::unique_ptr<TestDriveFsEventRouter> event_router_;
};

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_Basic) {
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:a", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      50, 200, 2)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:b", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      50, 200, 2)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnPinTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:c", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      25, 80, 2)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnPinTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:d", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      25, 80, 2)));

  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 3, 4, "c", drivefs::mojom::ItemEvent::State::kInProgress,
      25, 40, drivefs::mojom::ItemEventReason::kPin);
  syncing_status.item_events.emplace_back(
      absl::in_place, 3, 4, "d", drivefs::mojom::ItemEvent::State::kQueued, 0,
      40, drivefs::mojom::ItemEventReason::kPin);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_EmptyStatus) {
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnFileTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));

  drivefs::mojom::SyncingStatus syncing_status;
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_EmptyStatus_ClearsInProgressOrCompleted) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(4);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnPinTransfersUpdated::kEventName, _))
      .Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kInProgress,
      10, 100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnFileTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));

  syncing_status.item_events.clear();
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:c", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      60, 70, 1)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));

  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "c", drivefs::mojom::ItemEvent::State::kInProgress,
      60, 70, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_FailedSync) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kPin);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnFileTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)))
      .Times(2);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnPinTransfersUpdated::kEventName, _))
      .Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      80, 100, drivefs::mojom::ItemEventReason::kPin);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnFileTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnPinTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:a", file_manager_private::TRANSFER_STATE_FAILED, 100,
                      100, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kFailed, -1,
      -1, drivefs::mojom::ItemEventReason::kPin);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_CompletedSync) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(2);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)))
      .Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      80, 100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:a", file_manager_private::TRANSFER_STATE_COMPLETED,
                      100, 100, 0)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_CompletedSync_WithInProgress) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(2);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:a", file_manager_private::TRANSFER_STATE_COMPLETED,
                      110, 200, 1)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:b", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      110, 200, 1)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kInProgress,
      10, 100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_CompletedSync_WithQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(2);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:a", file_manager_private::TRANSFER_STATE_COMPLETED,
                      110, 200, 1)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:b", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      110, 200, 1)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_CompletedSync_OtherQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(1);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:a", file_manager_private::TRANSFER_STATE_COMPLETED,
                      110, 200, 1)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:b", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      110, 200, 1)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_CompletedSync_ThenQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(2);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)))
      .Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnFileTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_CompletedSync_ThenInProgress) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kInProgress,
      50, 100, drivefs::mojom::ItemEventReason::kTransfer);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _))
      .Times(1);
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName, _));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 1, 1, "a", drivefs::mojom::ItemEvent::State::kCompleted,
      -1, -1, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  file_manager_private::OnFileTransfersUpdated::kEventName,
                  MatchFileTransferStatus(
                      "ext:b", file_manager_private::TRANSFER_STATE_IN_PROGRESS,
                      10, 500, 1)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kInProgress,
      10, 500, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_QueuedOnly) {
  drivefs::mojom::SyncingStatus syncing_status;
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 0,
      100, drivefs::mojom::ItemEventReason::kTransfer);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnFileTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnPinTransfersUpdated::kEventName,
          MatchFileTransferStatus(
              "", file_manager_private::TRANSFER_STATE_COMPLETED, 0, 0, 0)));

  syncing_status.item_events.clear();
  syncing_status.item_events.emplace_back(
      absl::in_place, 2, 3, "b", drivefs::mojom::ItemEvent::State::kQueued, 10,
      100, drivefs::mojom::ItemEventReason::kTransfer);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_OnUnmounted) {
  Unmount();
}

TEST_F(DriveFsEventRouterTest, OnFilesChanged_Basic) {
  FileWatchEvent event;
  event.event_type = file_manager_private::FILE_WATCH_EVENT_TYPE_CHANGED;
  event.entry.additional_properties.SetStringKey("fileSystemRoot", "ext:/");
  event.entry.additional_properties.SetStringKey("fileSystemName", "drivefs");
  event.entry.additional_properties.SetStringKey("fileFullPath", "/root");
  event.entry.additional_properties.SetBoolKey("fileIsDirectory", true);
  event.changed_files =
      std::make_unique<std::vector<file_manager_private::FileChange>>();
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/a";
    changed_file.changes.push_back(file_manager_private::CHANGE_TYPE_DELETE);
  }
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/b";
    changed_file.changes.push_back(
        file_manager_private::CHANGE_TYPE_ADD_OR_UPDATE);
  }
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/c";
    changed_file.changes.push_back(
        file_manager_private::CHANGE_TYPE_ADD_OR_UPDATE);
  }

  EXPECT_CALL(mock(), IsPathWatched(base::FilePath("/root")))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock(), IsPathWatched(base::FilePath("/other")))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(mock(), BroadcastEventImpl(
                          file_manager_private::OnDirectoryChanged::kEventName,
                          MatchFileWatchEvent(event)));

  std::vector<drivefs::mojom::FileChange> changes;
  changes.emplace_back(base::FilePath("/root/a"),
                       drivefs::mojom::FileChange::Type::kDelete);
  changes.emplace_back(base::FilePath("/root/b"),
                       drivefs::mojom::FileChange::Type::kCreate);
  changes.emplace_back(base::FilePath("/root/c"),
                       drivefs::mojom::FileChange::Type::kModify);
  changes.emplace_back(base::FilePath("/other/a"),
                       drivefs::mojom::FileChange::Type::kModify);
  observer().OnFilesChanged(changes);
}

TEST_F(DriveFsEventRouterTest, OnFilesChanged_MultipleDirectories) {
  FileWatchEvent event;
  event.event_type = file_manager_private::FILE_WATCH_EVENT_TYPE_CHANGED;
  event.entry.additional_properties.SetStringKey("fileSystemRoot", "ext:/");
  event.entry.additional_properties.SetStringKey("fileSystemName", "drivefs");
  event.entry.additional_properties.SetStringKey("fileFullPath", "/root/a");
  event.entry.additional_properties.SetBoolKey("fileIsDirectory", true);
  event.changed_files =
      std::make_unique<std::vector<file_manager_private::FileChange>>();
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/a/file";
    changed_file.changes.push_back(file_manager_private::CHANGE_TYPE_DELETE);
  }
  EXPECT_CALL(mock(), BroadcastEventImpl(
                          file_manager_private::OnDirectoryChanged::kEventName,
                          MatchFileWatchEvent(event)));

  event.event_type = file_manager_private::FILE_WATCH_EVENT_TYPE_CHANGED;
  event.entry.additional_properties.SetStringKey("fileSystemRoot", "ext:/");
  event.entry.additional_properties.SetStringKey("fileSystemName", "drivefs");
  event.entry.additional_properties.SetStringKey("fileFullPath", "/root/b");
  event.entry.additional_properties.SetBoolKey("fileIsDirectory", true);
  event.changed_files =
      std::make_unique<std::vector<file_manager_private::FileChange>>();
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/b/file";
    changed_file.changes.push_back(
        file_manager_private::CHANGE_TYPE_ADD_OR_UPDATE);
  }
  EXPECT_CALL(mock(), BroadcastEventImpl(
                          file_manager_private::OnDirectoryChanged::kEventName,
                          MatchFileWatchEvent(event)));

  std::vector<drivefs::mojom::FileChange> changes;
  changes.emplace_back(base::FilePath("/root/a/file"),
                       drivefs::mojom::FileChange::Type::kDelete);
  changes.emplace_back(base::FilePath("/root/b/file"),
                       drivefs::mojom::FileChange::Type::kCreate);
  observer().OnFilesChanged(changes);
}

TEST_F(DriveFsEventRouterTest, OnError_CantUploadStorageFull) {
  DriveSyncErrorEvent event;
  event.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE;
  event.file_url = "ext:/a";
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnDriveSyncError::kEventName,
          testing::MakeMatcher(new ValueMatcher(base::Value(
              file_manager_private::OnDriveSyncError::Create(event))))));

  observer().OnError({drivefs::mojom::DriveError::Type::kCantUploadStorageFull,
                      base::FilePath("/a")});
}

TEST_F(DriveFsEventRouterTest, OnError_CantUploadStorageFullOrganization) {
  DriveSyncErrorEvent event;
  event.type =
      file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE_ORGANIZATION;
  event.file_url = "ext:/a";
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnDriveSyncError::kEventName,
          testing::MakeMatcher(new ValueMatcher(base::Value(
              file_manager_private::OnDriveSyncError::Create(event))))));

  observer().OnError(
      {drivefs::mojom::DriveError::Type::kCantUploadStorageFullOrganization,
       base::FilePath("/a")});
}

TEST_F(DriveFsEventRouterTest, OnError_CantPinDiskFull) {
  DriveSyncErrorEvent event;
  event.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_LOCAL_SPACE;
  event.file_url = "ext:a";
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(
          file_manager_private::OnDriveSyncError::kEventName,
          testing::MakeMatcher(new ValueMatcher(base::Value(
              file_manager_private::OnDriveSyncError::Create(event))))));

  observer().OnError({drivefs::mojom::DriveError::Type::kPinningFailedDiskFull,
                      base::FilePath("a")});
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_Display) {
  DriveConfirmDialogEvent expected_event;
  expected_event.type =
      file_manager_private::DRIVE_CONFIRM_DIALOG_TYPE_ENABLE_DOCS_OFFLINE;
  expected_event.file_url = "ext:a";
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(file_manager_private::OnDriveConfirmDialog::kEventName,
                         testing::MakeMatcher(new ValueMatcher(base::Value(
                             file_manager_private::OnDriveConfirmDialog::Create(
                                 expected_event))))));

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  bool called = false;
  event_router_->DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kAccept, result);
      }));
  EXPECT_FALSE(called);
  event_router_->OnDialogResult(drivefs::mojom::DialogResult::kAccept);
  EXPECT_TRUE(called);
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_OneDialogAtATime) {
  DriveConfirmDialogEvent expected_event;
  expected_event.type =
      file_manager_private::DRIVE_CONFIRM_DIALOG_TYPE_ENABLE_DOCS_OFFLINE;
  expected_event.file_url = "ext:a";
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(file_manager_private::OnDriveConfirmDialog::kEventName,
                         testing::MakeMatcher(new ValueMatcher(base::Value(
                             file_manager_private::OnDriveConfirmDialog::Create(
                                 expected_event))))));

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  bool called1 = false;
  event_router_->DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called1 = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kReject, result);
      }));
  EXPECT_FALSE(called1);

  bool called2 = false;
  event_router_->DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called2 = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kNotDisplayed, result);
      }));
  EXPECT_TRUE(called2);
  event_router_->OnDialogResult(drivefs::mojom::DialogResult::kReject);
  EXPECT_TRUE(called1);
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_UnmountBeforeResult) {
  DriveConfirmDialogEvent expected_event;
  expected_event.type =
      file_manager_private::DRIVE_CONFIRM_DIALOG_TYPE_ENABLE_DOCS_OFFLINE;
  expected_event.file_url = "ext:a";
  EXPECT_CALL(
      mock(),
      BroadcastEventImpl(file_manager_private::OnDriveConfirmDialog::kEventName,
                         testing::MakeMatcher(new ValueMatcher(base::Value(
                             file_manager_private::OnDriveConfirmDialog::Create(
                                 expected_event))))))
      .Times(2);

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  event_router_->DisplayConfirmDialog(
      reason, base::BindLambdaForTesting(
                  [&](drivefs::mojom::DialogResult result) { NOTREACHED(); }));
  Unmount();
  event_router_->OnDialogResult(drivefs::mojom::DialogResult::kAccept);

  bool called = false;
  event_router_->DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kDismiss, result);
      }));
  event_router_->OnDialogResult(drivefs::mojom::DialogResult::kDismiss);
  EXPECT_TRUE(called);
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_NoListeners) {
  EXPECT_CALL(mock(), GetEventListenerURLs)
      .WillRepeatedly(testing::Return(std::set<GURL>{}));

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  bool called = false;
  event_router_->DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kNotDisplayed, result);
      }));
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace file_manager
