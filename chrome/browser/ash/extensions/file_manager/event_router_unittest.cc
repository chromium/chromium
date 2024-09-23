// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/event_router.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/display/test/test_screen.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace file_manager {

TEST(EventRouterTest, PopulateCrostiniEvent) {
  extensions::api::file_manager_private::CrostiniEvent ext_event;
  url::Origin ext_origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId("extensionid"));
  EventRouter::PopulateCrostiniEvent(
      ext_event,
      extensions::api::file_manager_private::CrostiniEventType::kUnshare,
      "vmname", ext_origin, "mountname", "filesystemname", "/full/path");

  EXPECT_EQ(ext_event.event_type,
            extensions::api::file_manager_private::CrostiniEventType::kUnshare);
  EXPECT_EQ(ext_event.vm_name, "vmname");
  EXPECT_EQ(ext_event.entries.size(), 1u);
  base::Value::Dict ext_props;
  ext_props.Set(
      "fileSystemRoot",
      "filesystem:chrome-extension://extensionid/external/mountname/");
  ext_props.Set("fileSystemName", "filesystemname");
  ext_props.Set("fileFullPath", "/full/path");
  ext_props.Set("fileIsDirectory", true);
  EXPECT_EQ(ext_event.entries[0].additional_properties, ext_props);

  extensions::api::file_manager_private::CrostiniEvent swa_event;
  url::Origin swa_origin = url::Origin::Create(
      GURL("chrome://file-manager/this-part-should-not-be-in?the=event"));
  EventRouter::PopulateCrostiniEvent(
      swa_event,
      extensions::api::file_manager_private::CrostiniEventType::kShare,
      "vmname", swa_origin, "mountname", "filesystemname", "/full/path");

  EXPECT_EQ(swa_event.event_type,
            extensions::api::file_manager_private::CrostiniEventType::kShare);
  EXPECT_EQ(swa_event.vm_name, "vmname");
  EXPECT_EQ(swa_event.entries.size(), 1u);
  base::Value::Dict swa_props;
  swa_props.Set("fileSystemRoot",
                "filesystem:chrome://file-manager/external/mountname/");
  swa_props.Set("fileSystemName", "filesystemname");
  swa_props.Set("fileFullPath", "/full/path");
  swa_props.Set("fileIsDirectory", true);
  EXPECT_EQ(swa_event.entries[0].additional_properties, swa_props);
}

namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;

// Observes the `BroadcastEvent` operation that is emitted by the event router.
// The mock methods are used to assert expectations on the return results.
class TestEventRouterObserver
    : public extensions::TestEventRouter::EventObserver {
 public:
  explicit TestEventRouterObserver(extensions::TestEventRouter* event_router)
      : event_router_(event_router) {
    event_router_->AddEventObserver(this);
  }
  ~TestEventRouterObserver() override {
    event_router_->RemoveEventObserver(this);
  }
  TestEventRouterObserver(const TestEventRouterObserver&) = delete;
  TestEventRouterObserver& operator=(const TestEventRouterObserver&) = delete;

  // TestEventRouter::EventObserver:
  MOCK_METHOD(void, OnBroadcastEvent, (const extensions::Event&), (override));
  MOCK_METHOD(void,
              OnDispatchEventToExtension,
              (const std::string&, const extensions::Event&),
              (override));

 private:
  raw_ptr<extensions::TestEventRouter> event_router_;
};

class FileManagerEventRouterTest : public testing::Test {
 public:
  FileManagerEventRouterTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  FileManagerEventRouterTest(const FileManagerEventRouterTest&) = delete;
  FileManagerEventRouterTest& operator=(const FileManagerEventRouterTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    profile_ =
        std::make_unique<TestingProfile>(base::FilePath(temp_dir_.GetPath()));
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());

    VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([this](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(std::make_unique<VolumeManager>(
              Profile::FromBrowserContext(context), nullptr, nullptr,
              &disk_mount_manager_, nullptr,
              VolumeManager::GetMtpStorageInfoCallback()));
        }));

    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        temp_dir_.GetPath());

    auto* context = file_manager::util::GetFileSystemContextForSourceURL(
        profile_.get(), GURL("chrome-extension://abc"));
    ash::FileSystemBackend::Get(*context)->GrantFileAccessToOrigin(
        url::Origin::Create(GURL("chrome-extension://abc")),
        base::FilePath(
            file_manager::util::GetDownloadsMountPointName(profile_.get())));
  }

  const io_task::EntryStatus CreateSuccessfulEntryStatusForFileName(
      const std::string& file_name) {
    const base::FilePath file_path = temp_dir_.GetPath().Append(file_name);
    EXPECT_TRUE(base::WriteFile(file_path, base::RandBytesAsString(32)));

    storage::FileSystemURL url =
        file_system_context_->CreateCrackedFileSystemURL(
            kTestStorageKey, storage::kFileSystemTypeTest, file_path);

    return io_task::EntryStatus(std::move(url), base::File::FILE_OK);
  }

  ScopedTestingLocalState scoped_testing_local_state_;
  content::BrowserTaskEnvironment task_environment_;
  display::test::TestScreen test_screen_{/*create_display=*/true,
                                         /*register_screen=*/true};
  base::ScopedTempDir temp_dir_;
  ash::disks::FakeDiskMountManager disk_mount_manager_;
  std::unique_ptr<TestingProfile> profile_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

MATCHER(ExpectNoArgs, "") {
  return arg.size() == 0;
}

// A matcher that matches an `extensions::Event::event_args` and attempts to
// extract the "outputs" key. It then looks at the output at `index` and matches
// the `field` against the `expected_value`.
MATCHER_P3(ExpectEventArgString, index, field, expected_value, "") {
  EXPECT_GE(arg.size(), 1u);
  const base::Value::List* outputs = arg[0].GetDict().FindList("outputs");
  EXPECT_TRUE(outputs) << "The outputs field is not available on the event";
  EXPECT_GT(outputs->size(), index)
      << "The supplied index on outputs is not available, size: "
      << outputs->size() << ", index: " << index;
  const std::string* actual_value =
      (*outputs)[index].GetDict().FindString(field);
  EXPECT_TRUE(actual_value) << "Could not find the string with key: " << field;
  return testing::ExplainMatchResult(expected_value, *actual_value,
                                     result_listener);
}

// A matcher that matches an `extensions::Event::event_args` and attempts to
// extract the "conflictParams" and "pauseParams" keys. It expects
// "conflictParams" to be empty, and then matches the "policyParams" values
// against the expected ones.
MATCHER_P4(ExpectEventArgPauseParams,
           expected_type,
           expected_count,
           expected_file_name,
           expected_always_show_review,
           "") {
  EXPECT_GE(arg.size(), 1u);
  const base::Value::Dict* pause_params =
      arg[0].GetDict().FindDict("pauseParams");
  EXPECT_TRUE(pause_params)
      << "The pause_params field is not available on the event";

  const base::Value::Dict* conflict_pause_params =
      pause_params->FindDict("conflictParams");
  EXPECT_FALSE(conflict_pause_params)
      << "The conflictParams field should not be available on the event";

  const base::Value::Dict* policy_pause_params =
      pause_params->FindDict("policyParams");
  EXPECT_TRUE(policy_pause_params)
      << "The policyParams field is not available on the event";
  const std::string* actual_type = policy_pause_params->FindString("type");
  EXPECT_TRUE(actual_type) << "Could not find the string with key: type";
  const std::optional<int> actual_count =
      policy_pause_params->FindInt("policyFileCount");
  EXPECT_TRUE(actual_count.has_value())
      << "Could not find the number with key: type";
  const std::string* actual_file_name =
      policy_pause_params->FindString("fileName");
  EXPECT_TRUE(actual_file_name)
      << "Could not find the string with key: fileName";
  const std::optional<bool> actual_always_show_review =
      policy_pause_params->FindBool("alwaysShowReview");
  EXPECT_TRUE(actual_always_show_review.has_value())
      << "Could not find the string with key: alwaysShowReview";
  return testing::ExplainMatchResult(expected_type, *actual_type,
                                     result_listener) &&
         testing::ExplainMatchResult(expected_count, actual_count.value(),
                                     result_listener) &&
         testing::ExplainMatchResult(expected_file_name, *actual_file_name,
                                     result_listener) &&
         testing::ExplainMatchResult(expected_always_show_review,
                                     actual_always_show_review.value(),
                                     result_listener);
}

// A matcher that matches an `extensions::Event::event_args` and attempts to
// extract the "policyError" key. It then matches the "policyError" values
// against the expected ones.
MATCHER_P4(ExpectEventArgPolicyError,
           expected_type,
           expected_count,
           expected_file_name,
           expected_always_show_review,
           "") {
  EXPECT_GE(arg.size(), 1u);
  const base::Value::Dict* policy_error =
      arg[0].GetDict().FindDict("policyError");
  EXPECT_TRUE(policy_error)
      << "The policyError field is not available on the event";

  const std::string* actual_type = policy_error->FindString("type");
  EXPECT_TRUE(actual_type) << "Could not find the string with key: type";
  const std::optional<int> actual_count =
      policy_error->FindInt("policyFileCount");
  EXPECT_TRUE(actual_count.has_value())
      << "Could not find the string with key: type";
  const std::string* actual_file_name = policy_error->FindString("fileName");
  EXPECT_TRUE(actual_file_name)
      << "Could not find the string with key: fileName";
  const std::optional<bool> actual_always_show_review =
      policy_error->FindBool("alwaysShowReview");
  EXPECT_TRUE(actual_always_show_review.has_value())
      << "Could not find the string with key: alwaysShowReview";
  return testing::ExplainMatchResult(expected_type, *actual_type,
                                     result_listener) &&
         testing::ExplainMatchResult(expected_count, actual_count.value(),
                                     result_listener) &&
         testing::ExplainMatchResult(expected_file_name, *actual_file_name,
                                     result_listener) &&
         testing::ExplainMatchResult(expected_always_show_review,
                                     actual_always_show_review.value(),
                                     result_listener);
}

TEST_F(FileManagerEventRouterTest, OnIOTaskStatusForTrash) {
  // Setup event routers.
  extensions::TestEventRouter* test_event_router =
      extensions::CreateAndUseTestEventRouter(profile_.get());
  TestEventRouterObserver observer(test_event_router);
  auto event_router = std::make_unique<EventRouter>(profile_.get());
  event_router->ForceBroadcastingForTesting(true);

  io_task::EntryStatus source_entry =
      CreateSuccessfulEntryStatusForFileName("foo.txt");
  io_task::EntryStatus output_entry =
      CreateSuccessfulEntryStatusForFileName("bar.txt");

  std::vector<io_task::EntryStatus> source_entries;
  source_entries.push_back(std::move(source_entry));
  std::vector<io_task::EntryStatus> output_entries;
  output_entries.push_back(std::move(output_entry));

  // Setup the ProgressStatus event that expects
  file_manager::io_task::ProgressStatus status;
  status.type = file_manager::io_task::OperationType::kTrash;
  status.state = file_manager::io_task::State::kSuccess;
  status.sources = std::move(source_entries);
  status.outputs = std::move(output_entries);

  base::RunLoop run_loop;
  EXPECT_CALL(
      observer,
      OnBroadcastEvent(Field(
          &extensions::Event::event_args,
          AllOf(ExpectEventArgString(0u, "fileFullPath", "/bar.txt"),
                ExpectEventArgString(0u, "fileSystemName", "Downloads"),
                ExpectEventArgString(
                    0u, "fileSystemRoot",
                    "filesystem:chrome-extension://abc/external/Downloads/")))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  event_router->OnIOTaskStatus(status);
  run_loop.Run();
}

TEST_F(FileManagerEventRouterTest, OnIOTaskStatusForCopyPause) {
  // Setup event routers.
  extensions::TestEventRouter* test_event_router =
      extensions::CreateAndUseTestEventRouter(profile_.get());
  TestEventRouterObserver observer(test_event_router);
  auto event_router = std::make_unique<EventRouter>(profile_.get());
  event_router->ForceBroadcastingForTesting(true);

  io_task::EntryStatus source_entry =
      CreateSuccessfulEntryStatusForFileName("foo.txt");

  std::vector<io_task::EntryStatus> source_entries;
  source_entries.push_back(std::move(source_entry));

  // Setup the ProgressStatus event.
  file_manager::io_task::ProgressStatus status;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.state = file_manager::io_task::State::kPaused;
  status.sources = std::move(source_entries);
  status.pause_params.policy_params = io_task::PolicyPauseParams(
      policy::Policy::kDlp, /*warning_files_count*/ 2u, "foo.txt",
      /*always_show_review=*/false);

  // Expect the event to have dlp as policy pause params.
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnBroadcastEvent(Field(&extensions::Event::event_args,
                                     AllOf(ExpectEventArgPauseParams(
                                         "dlp", 2, "foo.txt", false)))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  event_router->OnIOTaskStatus(status);
  run_loop.Run();
}

TEST_F(FileManagerEventRouterTest, OnIOTaskStatusForPolicyError) {
  // Setup event routers.
  extensions::TestEventRouter* test_event_router =
      extensions::CreateAndUseTestEventRouter(profile_.get());
  TestEventRouterObserver observer(test_event_router);
  auto event_router = std::make_unique<EventRouter>(profile_.get());
  event_router->ForceBroadcastingForTesting(true);

  io_task::EntryStatus source_entry =
      CreateSuccessfulEntryStatusForFileName("foo.txt");

  std::vector<io_task::EntryStatus> source_entries;
  source_entries.push_back(std::move(source_entry));

  // Setup the ProgressStatus event.
  file_manager::io_task::ProgressStatus status;
  status.type = file_manager::io_task::OperationType::kCopy;
  status.state = file_manager::io_task::State::kError;
  status.sources = std::move(source_entries);
  status.policy_error.emplace(io_task::PolicyErrorType::kDlp,
                              /*blocked_files=*/1, "foo.txt",
                              /*always_show_review=*/true);

  // Expect the event to have dlp as policy error.
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnBroadcastEvent(Field(
                  &extensions::Event::event_args,
                  AllOf(ExpectEventArgPolicyError("dlp", 1, "foo.txt", true)))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  event_router->OnIOTaskStatus(status);
  run_loop.Run();
}

class FileManagerEventRouterLocalFilesTest : public FileManagerEventRouterTest {
 public:
  FileManagerEventRouterLocalFilesTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kSkyVault);
  }
  ~FileManagerEventRouterLocalFilesTest() override = default;

  void SetLocalUserFilesPolicy(bool allowed) {
    scoped_testing_local_state_.Get()->SetBoolean(prefs::kLocalUserFilesAllowed,
                                                  allowed);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FileManagerEventRouterLocalFilesTest, OnLocalUserFilesPolicyChanged) {
  // Set up event routers.
  extensions::TestEventRouter* test_event_router =
      extensions::CreateAndUseTestEventRouter(profile_.get());
  TestEventRouterObserver observer(test_event_router);
  auto event_router = std::make_unique<EventRouter>(profile_.get());
  event_router->ForceBroadcastingForTesting(true);

  // Expect the preferences changed event.
  base::Value::List event_args =
      extensions::api::file_manager_private::OnPreferencesChanged::Create();
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnBroadcastEvent(Field(&extensions::Event::event_args,
                                               AllOf(ExpectNoArgs()))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  SetLocalUserFilesPolicy(/*allowed=*/false);
  run_loop.Run();
}

}  // namespace
}  // namespace file_manager
