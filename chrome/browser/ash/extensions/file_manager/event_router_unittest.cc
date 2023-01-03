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
#include "chrome/browser/ash/file_manager/fake_disk_mount_manager.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/test_event_router.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace file_manager {

TEST(EventRouterTest, PopulateCrostiniEvent) {
  extensions::api::file_manager_private::CrostiniEvent ext_event;
  url::Origin ext_origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId("extensionid"));
  EventRouter::PopulateCrostiniEvent(
      ext_event,
      extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE,
      "vmname", ext_origin, "mountname", "filesystemname", "/full/path");

  EXPECT_EQ(ext_event.event_type,
            extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_UNSHARE);
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
      extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_SHARE,
      "vmname", swa_origin, "mountname", "filesystemname", "/full/path");

  EXPECT_EQ(swa_event.event_type,
            extensions::api::file_manager_private::CROSTINI_EVENT_TYPE_SHARE);
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
  FileManagerEventRouterTest() = default;
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

    file_manager::util::GetFileSystemContextForSourceURL(
        profile_.get(), GURL("chrome-extension://abc"))
        ->external_backend()
        ->GrantFileAccessToOrigin(
            url::Origin::Create(GURL("chrome-extension://abc")),
            base::FilePath(file_manager::util::GetDownloadsMountPointName(
                profile_.get())));
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

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  file_manager::FakeDiskMountManager disk_mount_manager_;
};

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

}  // namespace
}  // namespace file_manager
