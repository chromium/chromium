// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
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
  TestDriveFsEventRouter()
      : DriveFsEventRouter(/*profile=*/nullptr,
                           /*notification_manager=*/nullptr) {
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
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  void Unmount() { event_router_.OnUnmounted(); }
  TestDriveFsEventRouter event_router_;
};

TEST_F(DriveFsEventRouterTest, OnFilesChanged_Basic) {
  FileWatchEvent event;
  event.event_type = file_manager_private::FileWatchEventType::kChanged;
  event.entry.additional_properties.Set("fileSystemRoot", "ext:/");
  event.entry.additional_properties.Set("fileSystemName", "drivefs");
  event.entry.additional_properties.Set("fileFullPath", "/root");
  event.entry.additional_properties.Set("fileIsDirectory", true);
  event.changed_files.emplace();
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/a";
    changed_file.changes.push_back(file_manager_private::ChangeType::kDelete);
  }
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/b";
    changed_file.changes.push_back(
        file_manager_private::ChangeType::kAddOrUpdate);
  }
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/c";
    changed_file.changes.push_back(
        file_manager_private::ChangeType::kAddOrUpdate);
  }

  EXPECT_CALL(event_router_, IsPathWatched(base::FilePath("/root")))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(event_router_, IsPathWatched(base::FilePath("/other")))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(file_manager_private::OnDirectoryChanged::kEventName,
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
  event_router_.OnFilesChanged(changes);
}

TEST_F(DriveFsEventRouterTest, OnFilesChanged_MultipleDirectories) {
  FileWatchEvent event;
  event.event_type = file_manager_private::FileWatchEventType::kChanged;
  event.entry.additional_properties.Set("fileSystemRoot", "ext:/");
  event.entry.additional_properties.Set("fileSystemName", "drivefs");
  event.entry.additional_properties.Set("fileFullPath", "/root/a");
  event.entry.additional_properties.Set("fileIsDirectory", true);
  event.changed_files.emplace();
  event.changed_files->emplace_back();
  {
    auto& changed_file = event.changed_files->back();
    changed_file.url = "ext:/root/a/file";
    changed_file.changes.push_back(file_manager_private::ChangeType::kDelete);
  }
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(file_manager_private::OnDirectoryChanged::kEventName,
                         MatchFileWatchEvent(event)));

  event.event_type = file_manager_private::FileWatchEventType::kChanged;
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
        file_manager_private::ChangeType::kAddOrUpdate);
  }
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(file_manager_private::OnDirectoryChanged::kEventName,
                         MatchFileWatchEvent(event)));

  std::vector<drivefs::mojom::FileChange> changes;
  changes.emplace_back(base::FilePath("/root/a/file"),
                       drivefs::mojom::FileChange::Type::kDelete);
  changes.emplace_back(base::FilePath("/root/b/file"),
                       drivefs::mojom::FileChange::Type::kCreate);
  event_router_.OnFilesChanged(changes);
}

TEST_F(DriveFsEventRouterTest, OnError_CantUploadStorageFull) {
  DriveSyncErrorEvent event;
  event.type = file_manager_private::DriveSyncErrorType::kNoServerSpace;
  event.file_url = "ext:/a";
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(
          file_manager_private::OnDriveSyncError::kEventName,
          testing::MakeMatcher(new ValueMatcher(base::Value(
              file_manager_private::OnDriveSyncError::Create(event))))));

  event_router_.OnError(
      {drivefs::mojom::DriveError::Type::kCantUploadStorageFull,
       base::FilePath("/a")});
}

TEST_F(DriveFsEventRouterTest, OnError_CantUploadStorageFullOrganization) {
  DriveSyncErrorEvent event;
  event.type =
      file_manager_private::DriveSyncErrorType::kNoServerSpaceOrganization;
  event.file_url = "ext:/a";
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(
          file_manager_private::OnDriveSyncError::kEventName,
          testing::MakeMatcher(new ValueMatcher(base::Value(
              file_manager_private::OnDriveSyncError::Create(event))))));

  event_router_.OnError(
      {drivefs::mojom::DriveError::Type::kCantUploadStorageFullOrganization,
       base::FilePath("/a")});
}

TEST_F(DriveFsEventRouterTest, OnError_CantPinDiskFull) {
  DriveSyncErrorEvent event;
  event.type = file_manager_private::DriveSyncErrorType::kNoLocalSpace;
  event.file_url = "ext:a";
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(
          file_manager_private::OnDriveSyncError::kEventName,
          testing::MakeMatcher(new ValueMatcher(base::Value(
              file_manager_private::OnDriveSyncError::Create(event))))));

  event_router_.OnError(
      {drivefs::mojom::DriveError::Type::kPinningFailedDiskFull,
       base::FilePath("a")});
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_Display) {
  DriveConfirmDialogEvent expected_event;
  expected_event.type =
      file_manager_private::DriveConfirmDialogType::kEnableDocsOffline;
  expected_event.file_url = "ext:a";
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(file_manager_private::OnDriveConfirmDialog::kEventName,
                         testing::MakeMatcher(new ValueMatcher(base::Value(
                             file_manager_private::OnDriveConfirmDialog::Create(
                                 expected_event))))));

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  bool called = false;
  event_router_.DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kAccept, result);
      }));
  EXPECT_FALSE(called);
  event_router_.OnDialogResult(drivefs::mojom::DialogResult::kAccept);
  EXPECT_TRUE(called);
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_OneDialogAtATime) {
  DriveConfirmDialogEvent expected_event;
  expected_event.type =
      file_manager_private::DriveConfirmDialogType::kEnableDocsOffline;
  expected_event.file_url = "ext:a";
  EXPECT_CALL(
      event_router_,
      BroadcastEventImpl(file_manager_private::OnDriveConfirmDialog::kEventName,
                         testing::MakeMatcher(new ValueMatcher(base::Value(
                             file_manager_private::OnDriveConfirmDialog::Create(
                                 expected_event))))));

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  bool called1 = false;
  event_router_.DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called1 = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kReject, result);
      }));
  EXPECT_FALSE(called1);

  bool called2 = false;
  event_router_.DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called2 = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kNotDisplayed, result);
      }));
  EXPECT_TRUE(called2);
  event_router_.OnDialogResult(drivefs::mojom::DialogResult::kReject);
  EXPECT_TRUE(called1);
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_UnmountBeforeResult) {
  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  event_router_.DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        NOTREACHED_IN_MIGRATION();
      }));
  Unmount();
  event_router_.OnDialogResult(drivefs::mojom::DialogResult::kAccept);

  bool called = false;
  event_router_.DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kDismiss, result);
      }));
  event_router_.OnDialogResult(drivefs::mojom::DialogResult::kDismiss);
  EXPECT_TRUE(called);
}

TEST_F(DriveFsEventRouterTest, DisplayConfirmDialog_NoListeners) {
  EXPECT_CALL(event_router_, GetEventListenerURLs)
      .WillRepeatedly(testing::Return(std::set<GURL>{}));

  drivefs::mojom::DialogReason reason;
  reason.type = drivefs::mojom::DialogReason::Type::kEnableDocsOffline;
  reason.path = base::FilePath("a");
  bool called = false;
  event_router_.DisplayConfirmDialog(
      reason,
      base::BindLambdaForTesting([&](drivefs::mojom::DialogResult result) {
        called = true;
        EXPECT_EQ(drivefs::mojom::DialogResult::kNotDisplayed, result);
      }));
  EXPECT_TRUE(called);
}

TEST_F(DriveFsEventRouterTest, StaleSyncStatusesCleaned) {
  const base::FilePath path("/test");

  drivefs::mojom::ProgressEvent syncing_status;
  syncing_status.progress = 50;
  syncing_status.file_path = path;
  event_router_.OnItemProgress(syncing_status);

  auto sync_status = event_router_.GetDriveSyncStateForPath(path);
  EXPECT_EQ(sync_status.status, drivefs::SyncStatus::kInProgress);

  // 60s is less than the threshold of 90s where entries are considered stale.
  task_environment.FastForwardBy(base::Seconds(50));

  sync_status = event_router_.GetDriveSyncStateForPath(path);
  EXPECT_EQ(sync_status.status, drivefs::SyncStatus::kInProgress);

  // 60s + 60s = 120s is enough time for the entry to be considered stale.
  task_environment.FastForwardBy(base::Seconds(50));

  sync_status = event_router_.GetDriveSyncStateForPath(path);
  EXPECT_EQ(sync_status.status, drivefs::SyncStatus::kNotFound);
}

}  // namespace
}  // namespace file_manager
