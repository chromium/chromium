// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-shared.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
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

constexpr auto kCompleted = drivefs::mojom::ItemEvent::State::kCompleted;
constexpr auto kFailed = drivefs::mojom::ItemEvent::State::kFailed;
constexpr auto kInProgress = drivefs::mojom::ItemEvent::State::kInProgress;
constexpr auto kQueued = drivefs::mojom::ItemEvent::State::kQueued;

constexpr auto kPin = drivefs::mojom::ItemEventReason::kPin;
constexpr auto kTransfer = drivefs::mojom::ItemEventReason::kTransfer;

constexpr auto QUEUED = file_manager_private::TRANSFER_STATE_QUEUED;
constexpr auto IN_PROGRESS = file_manager_private::TRANSFER_STATE_IN_PROGRESS;
constexpr auto FAILED = file_manager_private::TRANSFER_STATE_FAILED;
constexpr auto COMPLETED = file_manager_private::TRANSFER_STATE_COMPLETED;

constexpr auto& kTransferEventName =
    file_manager_private::OnFileTransfersUpdated::kEventName;
constexpr auto& kPinEventName =
    file_manager_private::OnPinTransfersUpdated::kEventName;

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
    int num_total_jobs,
    bool show_notification) {
  FileTransferStatus status;
  status.file_url = std::move(file_url);
  status.transfer_state = transfer_state;
  status.processed = processed;
  status.total = total;
  status.num_total_jobs = num_total_jobs;
  status.show_notification = show_notification;
  status.hide_when_zero_jobs = true;
  return testing::MakeMatcher(new ValueMatcher(base::Value(
      file_manager_private::OnFileTransfersUpdated::Create(status))));
}

struct StatusToMatch {
  std::string file_path;
  file_manager_private::TransferState transfer_state;
  double processed;
  double total;

  bool operator==(const StatusToMatch& s) const {
    return file_path == s.file_path && transfer_state == s.transfer_state &&
           processed == s.processed && total == s.total;
  }
};

// Matches each item of a list of `IndividualFileTransferStatus` against the
// fields in the struct StatusToMatch.
MATCHER_P(MatchesIndividualFileTransferStatuses, matcher, "") {
  std::vector<StatusToMatch> statuses;
  for (const auto& status : arg) {
    const auto& statusDict = status.GetDict();
    statuses.push_back(
        {.file_path = *statusDict.FindStringByDottedPath("entry.fileFullPath"),
         .transfer_state =
             extensions::api::file_manager_private::ParseTransferState(
                 *statusDict.FindString("transferState")),
         .processed = statusDict.FindDouble("processed").value(),
         .total = statusDict.FindDouble("total").value()});
  }
  return testing::ExplainMatchResult(matcher, statuses, result_listener);
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
                      base::Value::List event_args,
                      bool dispatch_to_system_notification = true) override {
    if (dispatch_to_system_notification) {
      BroadcastEventImpl(event_name, base::Value(std::move(event_args)));
    } else {
      BroadcastEventForIndividualFilesImpl(event_name,
                                           std::move(event_args[0].GetList()));
    }
  }

  MOCK_METHOD(void,
              BroadcastEventImpl,
              (const std::string& name, const base::Value& event));
  MOCK_METHOD(void,
              BroadcastEventForIndividualFilesImpl,
              (const std::string& name, const base::Value::List& event));
  MOCK_METHOD(bool, IsPathWatched, (const base::FilePath&));

  GURL ConvertDrivePathToFileSystemUrl(const base::FilePath& file_path,
                                       const GURL& listener_url) override {
    return GURL(base::StrCat({listener_url.host(), ":", file_path.value()}));
  }

  std::vector<GURL> ConvertPathsToFileSystemUrls(
      const std::vector<base::FilePath>& paths,
      const GURL& listener_url) override {
    std::vector<GURL> urls;
    for (const auto& path : paths) {
      const GURL url =
          GURL(base::StrCat({listener_url.host(), ":", path.value()}));
      urls.push_back(url);
    }
    return urls;
  }

  std::string GetDriveFileSystemName() override { return "drivefs"; }

  MOCK_METHOD(std::set<GURL>,
              GetEventListenerURLs,
              (const std::string& event_name),
              (override));
};

class DriveFsEventRouterTest : public testing::Test {
 public:
  DriveFsEventRouterTest() {
    feature_list_.InitWithFeatures({}, {ash::features::kFilesInlineSyncStatus});
  }

 protected:
  void SetUp() override {
    event_router_ = std::make_unique<TestDriveFsEventRouter>();
  }

  void Unmount() {
    EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                           MatchFileTransferStatus(
                                               "", FAILED, 0, 0, 0, true)));
    EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                           MatchFileTransferStatus(
                                               "", FAILED, 0, 0, 0, true)));

    observer().OnUnmounted();
  }

  drivefs::DriveFsHostObserver& observer() { return *event_router_; }
  TestDriveFsEventRouter& mock() { return *event_router_; }

  std::unique_ptr<TestDriveFsEventRouter> event_router_;

  base::test::ScopedFeatureList feature_list_;
};

class DriveFsEventRouterTestInlineSyncStatus : public DriveFsEventRouterTest {
 public:
  DriveFsEventRouterTestInlineSyncStatus() {
    feature_list_.InitWithFeatures({ash::features::kFilesInlineSyncStatus}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

inline void AddEvent(std::vector<drivefs::mojom::ItemEventPtr>& events,
                     drivefs::mojom::ItemEventReason reason,
                     int64_t id,
                     std::string path,
                     drivefs::mojom::ItemEvent::State state,
                     int64_t bytes_transferred,
                     int64_t bytes_to_transfer) {
  events.emplace_back(absl::in_place, id, id, path, state, bytes_transferred,
                      bytes_to_transfer, reason);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_Basic) {
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:a", IN_PROGRESS,
                                                         50, 200, 2, true)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kPinEventName,
                                 MatchFileTransferStatus("ext:c", IN_PROGRESS,
                                                         25, 80, 2, true)));

  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);
  AddEvent(events, kPin, 3, "c", kInProgress, 25, 40);
  AddEvent(events, kPin, 4, "d", kQueued, 0, 40);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_EmptyStatus) {
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  drivefs::mojom::SyncingStatus syncing_status;
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_EmptyStatus_ClearsInProgressOrCompleted) {
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:a", IN_PROGRESS,
                                                         50, 200, 2, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:b", IN_PROGRESS,
                                                         110, 200, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  AddEvent(events, kTransfer, 2, "b", kInProgress, 10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  events.clear();
  observer().OnSyncingStatusUpdate(syncing_status);
  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:c", IN_PROGRESS,
                                                         60, 70, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  AddEvent(events, kTransfer, 3, "c", kInProgress, 60, 70);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_FailedSync) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kPin, 1, "a", kInProgress, 50, 100);
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)))
      .Times(2);
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName, _)).Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  events.clear();
  AddEvent(events, kPin, 1, "a", kInProgress, 80, 100);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(
                          kPinEventName,
                          MatchFileTransferStatus("", FAILED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kPin, 1, "a", kFailed, -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_CompletedSync) {
  // There should be 2 aggregate transfer events (for the progress of 'a'), and
  // 2 "completed" aggregate pin events (because no pin item events were
  // processed).
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName, _)).Times(2);
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)))
      .Times(2);

  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;

  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  observer().OnSyncingStatusUpdate(syncing_status);

  events.clear();
  AddEvent(events, kTransfer, 1, "a", kInProgress, 80, 100);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  // An aggregate transfer completed event is received indicating "a" finished
  // transferring. Still, no pin item events were processed, so the emitted
  // aggregate pin event is again "completed".
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_CompletedSync_WithInProgress) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:a", IN_PROGRESS,
                                                         50, 200, 2, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:b", IN_PROGRESS,
                                                         110, 200, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  AddEvent(events, kTransfer, 2, "b", kInProgress, 10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_CompletedSync_WithQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:a", IN_PROGRESS,
                                                         50, 200, 2, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  kTransferEventName,
                  MatchFileTransferStatus("ext:b", QUEUED, 100, 200, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  AddEvent(events, kTransfer, 2, "b", kQueued, 10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_CompletedSync_OtherQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:a", IN_PROGRESS,
                                                         50, 100, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  kTransferEventName,
                  MatchFileTransferStatus("ext:b", QUEUED, 100, 200, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  AddEvent(events, kTransfer, 2, "b", kQueued, 10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_CompletedSync_ThenQueued) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName, _)).Times(2);
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)))
      .Times(2);
  observer().OnSyncingStatusUpdate(syncing_status);

  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  kTransferEventName,
                  MatchFileTransferStatus("ext:b", QUEUED, 0, 100, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kTransfer, 2, "b", kQueued, 10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest,
       OnSyncingStatusUpdate_CompletedSync_ThenInProgress) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName, _)).Times(1);
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName, _));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kTransfer, 1, "a", kCompleted, -1, -1);
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:b", IN_PROGRESS,
                                                         10, 500, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));
  events.clear();
  AddEvent(events, kTransfer, 2, "b", kInProgress, 10, 500);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_QueuedOnly) {
  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  kTransferEventName,
                  MatchFileTransferStatus("ext:b", QUEUED, 0, 100, 1, true)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, true)));

  events.clear();
  AddEvent(events, kTransfer, 2, "b", kQueued, 10, 100);
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_OnUnmounted) {
  Unmount();
}

TEST_F(DriveFsEventRouterTest, OnSyncingStatusUpdate_SuppressNotifications) {
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kTransferEventName,
                                 MatchFileTransferStatus("ext:a", IN_PROGRESS,
                                                         50, 100, 1, true)));
  // The pin event that is not ignored is in queued state, so it will show up
  // as completed.
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "ext:d", QUEUED, 0, 40, 1, true)));

  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);
  AddEvent(events, kPin, 3, "c", kInProgress, 25, 40);
  AddEvent(events, kPin, 4, "d", kQueued, 0, 40);

  mock().SuppressNotificationsForFilePath(base::FilePath("b"));
  mock().SuppressNotificationsForFilePath(base::FilePath("c"));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  // All syncing file paths are ignored, so `show_notification` is false, and
  // no other meaningful transfer data is broadcasted.
  EXPECT_CALL(mock(), BroadcastEventImpl(kTransferEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, false)));
  EXPECT_CALL(mock(), BroadcastEventImpl(kPinEventName,
                                         MatchFileTransferStatus(
                                             "", COMPLETED, 0, 0, 0, false)));

  mock().SuppressNotificationsForFilePath(base::FilePath("a"));
  mock().SuppressNotificationsForFilePath(base::FilePath("d"));
  observer().OnSyncingStatusUpdate(syncing_status);

  testing::Mock::VerifyAndClearExpectations(&observer());

  EXPECT_CALL(mock(),
              BroadcastEventImpl(
                  kTransferEventName,
                  MatchFileTransferStatus("ext:b", QUEUED, 0, 100, 1, true)));
  EXPECT_CALL(mock(),
              BroadcastEventImpl(kPinEventName,
                                 MatchFileTransferStatus("ext:c", IN_PROGRESS,
                                                         25, 40, 1, true)));

  mock().RestoreNotificationsForFilePath(base::FilePath("b"));
  mock().RestoreNotificationsForFilePath(base::FilePath("c"));
  observer().OnSyncingStatusUpdate(syncing_status);
}

TEST_F(DriveFsEventRouterTest, OnFilesChanged_Basic) {
  FileWatchEvent event;
  event.event_type = file_manager_private::FILE_WATCH_EVENT_TYPE_CHANGED;
  event.entry.additional_properties.Set("fileSystemRoot", "ext:/");
  event.entry.additional_properties.Set("fileSystemName", "drivefs");
  event.entry.additional_properties.Set("fileFullPath", "/root");
  event.entry.additional_properties.Set("fileIsDirectory", true);
  event.changed_files.emplace();
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
  event.entry.additional_properties.Set("fileSystemRoot", "ext:/");
  event.entry.additional_properties.Set("fileSystemName", "drivefs");
  event.entry.additional_properties.Set("fileFullPath", "/root/a");
  event.entry.additional_properties.Set("fileIsDirectory", true);
  event.changed_files.emplace();
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
  event.entry.additional_properties.Set("fileSystemRoot", "ext:/");
  event.entry.additional_properties.Set("fileSystemName", "drivefs");
  event.entry.additional_properties.Set("fileFullPath", "/root/b");
  event.entry.additional_properties.Set("fileIsDirectory", true);
  event.changed_files.emplace();
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

TEST_F(DriveFsEventRouterTestInlineSyncStatus, OnSyncingStatusUpdate_Basic) {
  EXPECT_CALL(mock(), BroadcastEventForIndividualFilesImpl).Times(0);

  drivefs::mojom::SyncingStatus syncing_status;
  auto& events = syncing_status.item_events;
  AddEvent(events, kTransfer, 1, "a", kInProgress, 50, 100);
  AddEvent(events, kTransfer, 2, "b", kQueued, 0, 100);
  AddEvent(events, kPin, 3, "c", kInProgress, 25, 40);
  AddEvent(events, kPin, 4, "d", kQueued, 0, 40);
  observer().OnSyncingStatusUpdate(syncing_status);
}

}  // namespace
}  // namespace file_manager
