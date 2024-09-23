// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/download_metadata_manager.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::_;

namespace safe_browsing {

namespace {

const uint32_t kTestDownloadId = 47;
const uint32_t kOtherDownloadId = 48;
const uint32_t kCrazyDowloadId = 655;
const uint32_t kUninitializedDowloadId = 963;
const int64_t kTestDownloadTimeMsec = 84;
const char kTestUrl[] = "http://test.test/foo";
const uint64_t kTestDownloadLength = 1000;
const double kTestDownloadEndTimeMs = 1413514824057;

// A utility class suitable for mocking that exposes a
// GetDownloadDetailsCallback.
class DownloadDetailsGetter {
 public:
  virtual ~DownloadDetailsGetter() {}
  virtual void OnDownloadDetails(
      ClientIncidentReport_DownloadDetails* details) = 0;
  DownloadMetadataManager::GetDownloadDetailsCallback GetCallback() {
    return base::BindOnce(&DownloadDetailsGetter::DownloadDetailsCallback,
                          base::Unretained(this));
  }

 private:
  void DownloadDetailsCallback(
      std::unique_ptr<ClientIncidentReport_DownloadDetails> details) {
    OnDownloadDetails(details.get());
  }
};

// A mock DownloadDetailsGetter.
class MockDownloadDetailsGetter : public DownloadDetailsGetter {
 public:
  MOCK_METHOD1(OnDownloadDetails, void(ClientIncidentReport_DownloadDetails*));
};

// A mock DownloadMetadataManager that can be used to map a BrowserContext to
// a DownloadManager.
class MockDownloadMetadataManager : public DownloadMetadataManager {
 public:
  MockDownloadMetadataManager() = default;

  MOCK_METHOD(download::SimpleDownloadManagerCoordinator*,
              GetCoordinatorForBrowserContext,
              (content::BrowserContext*),
              (override));
};

// A helper function that returns the download URL from a DownloadDetails.
const std::string& GetDetailsDownloadUrl(
    const ClientIncidentReport_DownloadDetails* details) {
  return details->download().url();
}

// A helper function that returns the open time from a DownloadDetails.
int64_t GetDetailsOpenTime(
    const ClientIncidentReport_DownloadDetails* details) {
  return details->open_time_msec();
}

class MockDownloadManager : public content::MockDownloadManager {
 public:
  // Protected methods of SimpleDownloadManager called by the tests.
  using download::SimpleDownloadManager::OnInitialized;
  using download::SimpleDownloadManager::OnNewDownloadCreated;
};

}  // namespace

// The basis upon which unit tests of the DownloadMetadataManager are built.
class DownloadMetadataManagerTestBase : public ::testing::Test {
 protected:
  // Sets up a DownloadMetadataManager that will run tasks on the main test
  // thread.
  DownloadMetadataManagerTestBase() = default;

  // Returns the path to the test profile's DownloadMetadata file.
  base::FilePath GetMetadataPath() const {
    return profile_.GetPath().Append(FILE_PATH_LITERAL("DownloadMetadata"));
  }

  // Returns a new ClientDownloadRequest for the given download URL.
  static std::unique_ptr<ClientDownloadRequest> MakeTestRequest(
      const char* url) {
    std::unique_ptr<ClientDownloadRequest> request(new ClientDownloadRequest());
    request->set_url(url);
    request->mutable_digests();
    request->set_length(kTestDownloadLength);
    return request;
  }

  // Returns a new DownloadMetdata for the given download id.
  static std::unique_ptr<DownloadMetadata> GetTestMetadata(
      uint32_t download_id) {
    std::unique_ptr<DownloadMetadata> metadata(new DownloadMetadata());
    metadata->set_download_id(download_id);
    ClientIncidentReport_DownloadDetails* details =
        metadata->mutable_download();
    details->set_download_time_msec(kTestDownloadTimeMsec);
    details->set_allocated_download(MakeTestRequest(kTestUrl).release());
    return metadata;
  }

  // Writes a test DownloadMetadata file for the given download id to the
  // test profile directory.
  void WriteTestMetadataFileForItem(uint32_t download_id) {
    std::string data;
    ASSERT_TRUE(GetTestMetadata(download_id)->SerializeToString(&data));
    ASSERT_TRUE(base::WriteFile(GetMetadataPath(), data));
  }

  // Writes a test DownloadMetadata file for kTestDownloadId to the test profile
  // directory.
  void WriteTestMetadataFile() {
    WriteTestMetadataFileForItem(kTestDownloadId);
  }

  // Reads the DownloadMetadata from the test profile's directory into
  // |metadata|.
  void ReadTestMetadataFile(std::unique_ptr<DownloadMetadata>* metadata) const {
    std::string data;
    ASSERT_TRUE(base::ReadFileToString(GetMetadataPath(), &data));
    *metadata = std::make_unique<DownloadMetadata>();
    ASSERT_TRUE((*metadata)->ParseFromString(data));
  }

  // Runs all tasks posted to the test thread's message loop.
  void RunAllTasks() { content::RunAllTasksUntilIdle(); }

  // Adds a DownloadManager for the test profile. The DownloadMetadataManager's
  // observer is stashed for later use. Only call once per call to
  // ShutdownDownloadManager.
  void AddDownloadManager() {
    // Tell the MockDownloadManager that it is in the initialized state.
    download_manager_.OnInitialized();
    // Connect the TestingProfile to the MockDownloadManager.
    ON_CALL(download_manager_, GetBrowserContext())
        .WillByDefault(Return(&profile_));
    // Create the SimpleDownloadManagerCoordinator.
    coordinator_.emplace(base::NullCallback());
    // Connect the coordinator to the MockDownloadMetadataManager.
    ON_CALL(manager_, GetCoordinatorForBrowserContext(Eq(&profile_)))
        .WillByDefault(Return(&*coordinator_));
    // Connect the MockDownloadManager to the coordinator.
    coordinator_->SetSimpleDownloadManager(
        &download_manager_, /*manages_all_history_downloads=*/false);
    // Add the MockDownloadManager to the MockDownloadMetadataManager.
    manager_.AddDownloadManager(&download_manager_);
  }

  // Shuts down the DownloadManager. Safe to call any number of times.
  void ShutdownDownloadManager() {
    // Note: these calls may result in "Uninteresting mock function call"
    // warnings as a result of MockDownloadItem invoking observers in its
    // dtor. This happens after the NiceMock wrapper has removed its niceness
    // hook. These can safely be ignored, as they are entirely expected. The
    // values specified by ON_CALL invocations in AddDownloadItems are
    // returned as desired.
    other_item_.reset();
    test_item_.reset();
    zero_item_.reset();
    uninitialized_item_.reset();
    coordinator_.reset();
  }

  // Adds two test DownloadItems to the DownloadManager.
  void AddDownloadItems() {
    // Add the item under test.
    test_item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*test_item_, GetId())
        .WillByDefault(Return(kTestDownloadId));
    ON_CALL(*test_item_, GetEndTime())
        .WillByDefault(Return(base::Time::FromMillisecondsSinceUnixEpoch(
            kTestDownloadEndTimeMs)));
    ON_CALL(*test_item_, GetState())
        .WillByDefault(Return(download::DownloadItem::COMPLETE));
    content::DownloadItemUtils::AttachInfoForTesting(test_item_.get(),
                                                     &profile_, nullptr);
    download_manager_.OnNewDownloadCreated(test_item_.get());

    // Add another item.
    other_item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*other_item_, GetId())
        .WillByDefault(Return(kOtherDownloadId));
    ON_CALL(*other_item_, GetEndTime())
        .WillByDefault(Return(base::Time::FromMillisecondsSinceUnixEpoch(
            kTestDownloadEndTimeMs)));
    content::DownloadItemUtils::AttachInfoForTesting(other_item_.get(),
                                                     &profile_, nullptr);
    download_manager_.OnNewDownloadCreated(other_item_.get());

    // Add an item with an id of zero.
    zero_item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*zero_item_, GetId())
        .WillByDefault(Return(0));
    ON_CALL(*zero_item_, GetEndTime())
        .WillByDefault(Return(base::Time::FromMillisecondsSinceUnixEpoch(
            kTestDownloadEndTimeMs)));
    ON_CALL(*zero_item_, GetState())
        .WillByDefault(Return(download::DownloadItem::COMPLETE));
    content::DownloadItemUtils::AttachInfoForTesting(zero_item_.get(),
                                                     &profile_, nullptr);
    download_manager_.OnNewDownloadCreated(zero_item_.get());

    ON_CALL(download_manager_, GetAllDownloads(_))
        .WillByDefault(
            Invoke(this, &DownloadMetadataManagerTestBase::GetAllDownloads));
  }

  // Adds an uninitialized active download.
  void AddUninitializedActiveItem() {
    // Add the item under test.
    uninitialized_item_ =
        std::make_unique<NiceMock<download::MockDownloadItem>>();
    ON_CALL(*uninitialized_item_, GetId())
        .WillByDefault(Return(kUninitializedDowloadId));
    ON_CALL(*uninitialized_item_, GetEndTime())
        .WillByDefault(Return(base::Time::FromMillisecondsSinceUnixEpoch(
            kTestDownloadEndTimeMs)));
    ON_CALL(*uninitialized_item_, GetState())
        .WillByDefault(Return(download::DownloadItem::COMPLETE));
    content::DownloadItemUtils::AttachInfoForTesting(uninitialized_item_.get(),
                                                     &profile_, nullptr);
    download_manager_.OnNewDownloadCreated(uninitialized_item_.get());

    ON_CALL(download_manager_, GetUninitializedActiveDownloadsIfAny(_))
        .WillByDefault(Invoke(this, &DownloadMetadataManagerTestBase::
                                        GetUninitializedActiveDownloadsIfAny));
  }

  // An implementation of the MockDownloadManager's
  // DownloadManager::GetAllDownloads method that returns all items.
  void GetAllDownloads(content::DownloadManager::DownloadVector* downloads) {
    if (test_item_)
      downloads->push_back(test_item_.get());
    if (other_item_)
      downloads->push_back(other_item_.get());
    if (zero_item_)
      downloads->push_back(zero_item_.get());
  }

  // An implementation of the MockDownloadManager's
  // DownloadManager::GetUninitializedActiveDownloadsIfAny method that returns
  // the uninitialized item.
  void GetUninitializedActiveDownloadsIfAny(
      content::DownloadManager::DownloadVector* downloads) {
    if (uninitialized_item_) {
      downloads->push_back(uninitialized_item_.get());
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  std::optional<download::SimpleDownloadManagerCoordinator> coordinator_;
  NiceMock<MockDownloadMetadataManager> manager_;
  TestingProfile profile_;
  NiceMock<MockDownloadManager> download_manager_;
  std::unique_ptr<download::MockDownloadItem> test_item_;
  std::unique_ptr<download::MockDownloadItem> other_item_;
  std::unique_ptr<download::MockDownloadItem> zero_item_;
  std::unique_ptr<download::MockDownloadItem> uninitialized_item_;
};

// A parameterized test that exercises GetDownloadDetails. The parameters
// dictate the exact state of affairs leading up to the call as follows:
// 0: if "present", the profile has a pre-existing DownloadMetadata file.
// 1: if "managed", the profile's DownloadManager has been created.
// 2: the state of the DownloadItem prior to the call:
//    "not_created": the DownloadItem has not been created.
//    "created": the DownloadItem has been created.
//    "opened": the DownloadItem has been opened.
//    "removed": the DownloadItem has been removed.
// 3: if "loaded", the task to load the DownloadMetadata file is allowed to
//    complete.
// 4: if "early_shutdown", the DownloadManager is shut down before the callback
//    is allowed to complete.
class GetDetailsTest
    : public DownloadMetadataManagerTestBase,
      public ::testing::WithParamInterface<testing::tuple<const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*>> {
 protected:
  enum DownloadItemAction {
    NOT_CREATED,
    CREATED,
    OPENED,
    REMOVED,
  };
  GetDetailsTest()
      : metadata_file_present_(),
        manager_added_(),
        item_action_(NOT_CREATED),
        details_loaded_(),
        early_shutdown_() {}

  void SetUp() override {
    DownloadMetadataManagerTestBase::SetUp();
    metadata_file_present_ =
        (std::string(testing::get<0>(GetParam())) == "present");
    manager_added_ = (std::string(testing::get<1>(GetParam())) == "managed");
    const std::string item_action(testing::get<2>(GetParam()));
    item_action_ = (item_action == "not_created" ? NOT_CREATED :
                    (item_action == "created" ? CREATED :
                     (item_action == "opened" ? OPENED : REMOVED)));
    details_loaded_ = (std::string(testing::get<3>(GetParam())) == "loaded");
    early_shutdown_ =
        (std::string(testing::get<4>(GetParam())) == "early_shutdown");

    // Fixup combinations that don't make sense.
    if (!manager_added_)
      item_action_ = NOT_CREATED;
  }

  bool metadata_file_present_;
  bool manager_added_;
  DownloadItemAction item_action_;
  bool details_loaded_;
  bool early_shutdown_;
};

// Tests that DownloadMetadataManager::GetDownloadDetails works for all
// combinations of states.
TEST_P(GetDetailsTest, GetDownloadDetails) {
  // Optionally put a metadata file in the profile directory.
  if (metadata_file_present_)
    ASSERT_NO_FATAL_FAILURE(WriteTestMetadataFile());

  // Optionally add a download manager for the profile.
  if (manager_added_)
    ASSERT_NO_FATAL_FAILURE(AddDownloadManager());

  // Optionally create download items and perform actions on the one under test.
  if (item_action_ != NOT_CREATED)
    ASSERT_NO_FATAL_FAILURE(AddDownloadItems());
  if (item_action_ == OPENED)
    test_item_->NotifyObserversDownloadOpened();
  else if (item_action_ == REMOVED)
    test_item_->NotifyObserversDownloadRemoved();

  // Optionally allow the task to read the file to complete.
  if (details_loaded_)
    RunAllTasks();

  // In http://crbug.com/433928, open after removal during load caused a crash.
  if (item_action_ == REMOVED)
    test_item_->NotifyObserversDownloadOpened();

  MockDownloadDetailsGetter details_getter;
  if (metadata_file_present_ && item_action_ != REMOVED) {
    // The file is present, so expect that the callback is invoked with the
    // details of the test download data written by WriteTestMetadataFile.
    if (item_action_ == OPENED) {
      EXPECT_CALL(details_getter,
                  OnDownloadDetails(
                      AllOf(ResultOf(GetDetailsDownloadUrl, StrEq(kTestUrl)),
                            ResultOf(GetDetailsOpenTime, Ne(0)))));
    } else {
      EXPECT_CALL(details_getter,
                  OnDownloadDetails(
                      AllOf(ResultOf(GetDetailsDownloadUrl, StrEq(kTestUrl)),
                            ResultOf(GetDetailsOpenTime, Eq(0)))));
    }
  } else {
    // No file on disk, so expect that the callback is invoked with null.
    EXPECT_CALL(details_getter, OnDownloadDetails(IsNull()));
  }

  // Fire in the hole!
  manager_.GetDownloadDetails(&profile_, details_getter.GetCallback());

  // Shutdown the download manager, if relevant.
  if (early_shutdown_)
    ShutdownDownloadManager();

  // Allow the read task and the response callback to run.
  RunAllTasks();

  // Shutdown the download manager, if relevant.
  ShutdownDownloadManager();
}

INSTANTIATE_TEST_SUITE_P(
    DownloadMetadataManager,
    GetDetailsTest,
    testing::Combine(
        testing::Values("absent", "present"),
        testing::Values("not_managed", "managed"),
        testing::Values("not_created", "created", "opened", "removed"),
        testing::Values("waiting", "loaded"),
        testing::Values("normal_shutdown", "early_shutdown")));

// A parameterized test that exercises SetRequest. The parameters dictate the
// exact state of affairs leading up to the call as follows:
// 0: the state of the DownloadMetadata file for the test profile:
//    "absent": no file is present.
//    "this": the file corresponds to the item being updated.
//    "other": the file correponds to a different item.
//    "unknown": the file corresponds to an item that has not been created.
// 1: if "pending", an operation is applied to the item being updated prior to
//    the call.
// 2: if "pending", an operation is applied to a different item prior to the
//    call.
// 3: if "loaded", the task to load the DownloadMetadata file is allowed to
//    complete.
// 4: if "set", the call to SetRequest contains a new request; otherwise it
//    does not, leading to removal of metadata.
class SetRequestTest
    : public DownloadMetadataManagerTestBase,
      public ::testing::WithParamInterface<testing::tuple<const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*,
                                                          const char*>> {
 protected:
  enum MetadataFilePresent {
    ABSENT,
    PRESENT_FOR_THIS_ITEM,
    PRESENT_FOR_OTHER_ITEM,
    PRESENT_FOR_UNKNOWN_ITEM,
  };
  SetRequestTest()
      : metadata_file_present_(ABSENT),
        same_ops_(),
        other_ops_(),
        details_loaded_(),
        set_request_() {}

  void SetUp() override {
    DownloadMetadataManagerTestBase::SetUp();
    const std::string present(testing::get<0>(GetParam()));
    metadata_file_present_ = (present == "absent" ? ABSENT :
                              (present == "this" ? PRESENT_FOR_THIS_ITEM :
                               (present == "other" ? PRESENT_FOR_OTHER_ITEM :
                                PRESENT_FOR_UNKNOWN_ITEM)));
    same_ops_ = (std::string(testing::get<1>(GetParam())) == "pending");
    other_ops_ = (std::string(testing::get<2>(GetParam())) == "pending");
    details_loaded_ = (std::string(testing::get<3>(GetParam())) == "loaded");
    set_request_ = (std::string(testing::get<4>(GetParam())) == "set");
  }

  MetadataFilePresent metadata_file_present_;
  bool same_ops_;
  bool other_ops_;
  bool details_loaded_;
  bool set_request_;
};

// Tests that DownloadMetadataManager::SetRequest works for all combinations of
// states.
TEST_P(SetRequestTest, SetRequest) {
  // Optionally put a metadata file in the profile directory.
  switch (metadata_file_present_) {
    case ABSENT:
      break;
    case PRESENT_FOR_THIS_ITEM:
      ASSERT_NO_FATAL_FAILURE(WriteTestMetadataFile());
      break;
    case PRESENT_FOR_OTHER_ITEM:
      ASSERT_NO_FATAL_FAILURE(WriteTestMetadataFileForItem(kOtherDownloadId));
      break;
    case PRESENT_FOR_UNKNOWN_ITEM:
      ASSERT_NO_FATAL_FAILURE(WriteTestMetadataFileForItem(kCrazyDowloadId));
      break;
  }

  ASSERT_NO_FATAL_FAILURE(AddDownloadManager());
  ASSERT_NO_FATAL_FAILURE(AddDownloadItems());

  // Optionally allow the task to read the file to complete.
  if (details_loaded_) {
    RunAllTasks();
  } else {
    // Optionally add pending operations if the load is outstanding.
    if (same_ops_)
      test_item_->NotifyObserversDownloadOpened();
    if (other_ops_)
      other_item_->NotifyObserversDownloadOpened();
  }

  static const char kNewUrl[] = "http://blorf";
  if (set_request_)
    manager_.SetRequest(test_item_.get(), MakeTestRequest(kNewUrl).get());

  // Allow the write or remove task to run.
  RunAllTasks();

  if (set_request_) {
    MockDownloadDetailsGetter details_getter;
    // Expect that the callback is invoked with details for this item.
    EXPECT_CALL(
        details_getter,
        OnDownloadDetails(ResultOf(GetDetailsDownloadUrl, StrEq(kNewUrl))));
    manager_.GetDownloadDetails(&profile_, details_getter.GetCallback());
  }

  // In http://crbug.com/433928, open after SetRequest(nullpr) caused a crash.
  test_item_->NotifyObserversDownloadOpened();

  ShutdownDownloadManager();

  RunAllTasks();

  if (set_request_) {
    // Expect that the file contains metadata for the download.
    std::unique_ptr<DownloadMetadata> metadata;
    ASSERT_NO_FATAL_FAILURE(ReadTestMetadataFile(&metadata))
        << GetMetadataPath().value();
    EXPECT_EQ(kTestDownloadId, metadata->download_id());
    EXPECT_STREQ(kNewUrl, metadata->download().download().url().c_str());
  } else if (metadata_file_present_ != ABSENT) {
    // Expect that the metadata file has not been overwritten.
    std::unique_ptr<DownloadMetadata> metadata;
    ASSERT_NO_FATAL_FAILURE(ReadTestMetadataFile(&metadata))
        << GetMetadataPath().value();
  } else {
    // Expect that the file is not present.
    ASSERT_FALSE(base::PathExists(GetMetadataPath()));
  }
}

INSTANTIATE_TEST_SUITE_P(
    DownloadMetadataManager,
    SetRequestTest,
    testing::Combine(testing::Values("absent", "this", "other", "unknown"),
                     testing::Values("none", "pending"),
                     testing::Values("none", "pending"),
                     testing::Values("waiting", "loaded"),
                     testing::Values("clear", "set")));

TEST_F(DownloadMetadataManagerTestBase, ActiveDownloadNoRequest) {
  // Put some metadata on disk from a previous download.
  ASSERT_NO_FATAL_FAILURE(WriteTestMetadataFileForItem(kOtherDownloadId));

  ASSERT_NO_FATAL_FAILURE(AddDownloadManager());
  ASSERT_NO_FATAL_FAILURE(AddDownloadItems());

  // Allow everything to load into steady-state.
  RunAllTasks();

  // The test item is in progress.
  ON_CALL(*test_item_, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  test_item_->NotifyObserversDownloadUpdated();
  test_item_->NotifyObserversDownloadUpdated();

  // The test item completes.
  ON_CALL(*test_item_, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  test_item_->NotifyObserversDownloadUpdated();

  RunAllTasks();
  ShutdownDownloadManager();

  // Expect that the metadata file is still present.
  std::unique_ptr<DownloadMetadata> metadata;
  ASSERT_NO_FATAL_FAILURE(ReadTestMetadataFile(&metadata));
  EXPECT_EQ(kOtherDownloadId, metadata->download_id());
}

TEST_F(DownloadMetadataManagerTestBase, ActiveDownloadWithRequest) {
  // Put some metadata on disk from a previous download.
  ASSERT_NO_FATAL_FAILURE(WriteTestMetadataFileForItem(kOtherDownloadId));

  ASSERT_NO_FATAL_FAILURE(AddDownloadManager());
  ASSERT_NO_FATAL_FAILURE(AddDownloadItems());

  // Allow everything to load into steady-state.
  RunAllTasks();

  // The test item is in progress.
  ON_CALL(*test_item_, GetState())
      .WillByDefault(Return(download::DownloadItem::IN_PROGRESS));
  test_item_->NotifyObserversDownloadUpdated();

  // A request is set for it.
  static const char kNewUrl[] = "http://blorf";
  manager_.SetRequest(test_item_.get(), MakeTestRequest(kNewUrl).get());

  test_item_->NotifyObserversDownloadUpdated();

  // The test item completes.
  ON_CALL(*test_item_, GetState())
      .WillByDefault(Return(download::DownloadItem::COMPLETE));
  test_item_->NotifyObserversDownloadUpdated();

  RunAllTasks();

  MockDownloadDetailsGetter details_getter;
  // Expect that the callback is invoked with details for this item.
  EXPECT_CALL(
      details_getter,
      OnDownloadDetails(ResultOf(GetDetailsDownloadUrl, StrEq(kNewUrl))));
  manager_.GetDownloadDetails(&profile_, details_getter.GetCallback());

  ShutdownDownloadManager();

  // Expect that the file contains metadata for the download.
  std::unique_ptr<DownloadMetadata> metadata;
  ASSERT_NO_FATAL_FAILURE(ReadTestMetadataFile(&metadata));
  EXPECT_EQ(kTestDownloadId, metadata->download_id());
  EXPECT_STREQ(kNewUrl, metadata->download().download().url().c_str());
}

// Regression test for http://crbug.com/504092: open an item with id==0 when
// there is no metadata loaded.
TEST_F(DownloadMetadataManagerTestBase, OpenItemWithZeroId) {
  ASSERT_NO_FATAL_FAILURE(AddDownloadManager());
  ASSERT_NO_FATAL_FAILURE(AddDownloadItems());

  // Allow everything to load into steady-state.
  RunAllTasks();

  // Open the zero-id item.
  zero_item_->NotifyObserversDownloadOpened();

  ShutdownDownloadManager();
}

// Regression test for https://crbug.com/40072145, where observers weren't being
// removed at shutdown from uninitialized active downloads.
TEST_F(DownloadMetadataManagerTestBase, UninitializedActiveDownload) {
  ASSERT_NO_FATAL_FAILURE(AddDownloadManager());
  ASSERT_NO_FATAL_FAILURE(AddUninitializedActiveItem());

  // Allow everything to load into steady-state.
  RunAllTasks();

  // There should be no crash when destroying the ManagerContext from it still
  // observing a DownloadItem.
  ShutdownDownloadManager();
}

}  // namespace safe_browsing
