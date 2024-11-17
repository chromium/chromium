// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_change_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/ash/fileapi/file_change_service_observer.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_blob_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns the file system context associated with the specified `profile`.
storage::FileSystemContext* GetFileSystemContext(Profile* profile) {
  return file_manager::util::GetFileManagerFileSystemContext(profile);
}

// Creates a mojo data pipe with the provided `content`.
mojo::ScopedDataPipeConsumerHandle CreateStream(const std::string& contents) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = 16;
  mojo::CreateDataPipe(&options, producer_handle, consumer_handle);
  CHECK(producer_handle.is_valid());
  auto producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  auto* producer_raw = producer.get();
  producer_raw->Write(
      std::make_unique<mojo::StringDataSource>(
          contents, mojo::StringDataSource::AsyncWritingMode::
                        STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
      base::BindOnce([](std::unique_ptr<mojo::DataPipeProducer>, MojoResult) {},
                     std::move(producer)));
  return consumer_handle;
}

// MockFileChangeServiceObserver -----------------------------------------------

class MockFileChangeServiceObserver : public FileChangeServiceObserver {
 public:
  // FileChangeServiceObserver:
  MOCK_METHOD(void,
              OnFileModified,
              (const storage::FileSystemURL& url),
              (override));
  MOCK_METHOD(void,
              OnFileMoved,
              (const storage::FileSystemURL& src,
               const storage::FileSystemURL& dst),
              (override));
  MOCK_METHOD(void,
              OnFileCreatedFromShowSaveFilePicker,
              (const GURL& file_picker_binding_context,
               const storage::FileSystemURL& url),
              (override));
};

// TempFileSystem --------------------------------------------------------------

// A class which registers a temporary file system and provides convenient APIs
// for interacting with that file system.
class TempFileSystem {
 public:
  explicit TempFileSystem(Profile* profile)
      : profile_(profile), name_(base::UnguessableToken::Create().ToString()) {}

  TempFileSystem(const TempFileSystem&) = delete;
  TempFileSystem& operator=(const TempFileSystem&) = delete;

  ~TempFileSystem() {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
  }

  // Sets up and registers a temporary file system at `temp_dir_`.
  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            name_, storage::kFileSystemTypeLocal,
            storage::FileSystemMountOption(), temp_dir_.GetPath()));

    ash::FileSystemBackend::Get(*GetFileSystemContext(profile_))
        ->GrantFileAccessToOrigin(file_manager::util::GetFilesAppOrigin(),
                                  base::FilePath(name_));
  }

  // Synchronously creates the file specified by `url`.
  base::File::Error CreateFile(const storage::FileSystemURL& url) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::CreateFile(context, url);
  }

  // Returns a file system URL for the specified path relative to `temp_dir_`.
  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return GetFileSystemContext(profile_)->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFirstParty(origin_),
        storage::kFileSystemTypeLocal,
        temp_dir_.GetPath().Append(base::FilePath::FromUTF8Unsafe(path)));
  }

  // Synchronously writes `content` to the file specified by `url`.
  base::File::Error WriteFile(const storage::FileSystemURL& url,
                              const std::string& data) {
    storage::BlobStorageContext blob_storage_context;
    storage::ScopedTextBlob blob(&blob_storage_context, "blob-id:test", data);
    base::File::Error result = base::File::FILE_ERROR_FAILED;
    base::RunLoop run_loop;
    GetFileSystemContext(profile_)->operation_runner()->Write(
        url, blob.GetBlobDataHandle(), 0,
        base::BindLambdaForTesting([&](base::File::Error operation_result,
                                       int64_t bytes, bool complete) {
          if (!complete)
            return;
          result = operation_result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Synchronously writes contents from `stream` to the file specified by `url`.
  base::File::Error WriteStreamToFile(
      const storage::FileSystemURL& url,
      mojo::ScopedDataPipeConsumerHandle stream) {
    base::File::Error result = base::File::FILE_ERROR_FAILED;
    base::RunLoop run_loop;
    GetFileSystemContext(profile_)->operation_runner()->WriteStream(
        url, std::move(stream), 0,
        base::BindLambdaForTesting([&](base::File::Error operation_result,
                                       int64_t bytes, bool complete) {
          if (!complete)
            return;
          result = operation_result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  // Synchronously truncates the file specified by `url` to `size`.
  base::File::Error TruncateFile(const storage::FileSystemURL& url,
                                 size_t size) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::TruncateFile(context, url, size);
  }

  // Synchronously copies the file specified by `src` to `dst`.
  base::File::Error CopyFile(const storage::FileSystemURL& src,
                             const storage::FileSystemURL& dst) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::Copy(context, src, dst);
  }

  // Synchronously copies the file specified by `src` to `dst` locally.
  base::File::Error CopyFileLocal(const storage::FileSystemURL& src,
                                  const storage::FileSystemURL& dst) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::CopyFileLocal(context, src, dst);
  }

  // Synchronously moves the file specified by `src` to `dst`.
  base::File::Error MoveFile(const storage::FileSystemURL& src,
                             const storage::FileSystemURL& dst) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::Move(context, src, dst);
  }

  // Synchronously moves the file specified by `src` to `dst` locally.
  base::File::Error MoveFileLocal(const storage::FileSystemURL& src,
                                  const storage::FileSystemURL& dst) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::MoveFileLocal(context, src, dst);
  }

  // Synchronously removes the file specified by `url`.
  base::File::Error RemoveFile(const storage::FileSystemURL& url,
                               bool recursive = false) {
    storage::FileSystemContext* context = GetFileSystemContext(profile_);
    return storage::AsyncFileTestHelper::Remove(context, url, recursive);
  }

 private:
  const raw_ptr<Profile> profile_;
  const url::Origin origin_;
  const std::string name_;
  base::ScopedTempDir temp_dir_;
};

// FileChangeServiceTest -------------------------------------------------------

class FileChangeServiceTest : public BrowserWithTestWindowTest {
 public:
  FileChangeServiceTest() = default;

  FileChangeServiceTest(const FileChangeServiceTest& other) = delete;
  FileChangeServiceTest& operator=(const FileChangeServiceTest& other) = delete;
  ~FileChangeServiceTest() override = default;

  // Creates and returns a new profile for the specified `name`.
  TestingProfile* CreateLoggedInUserProfile(const std::string& name) {
    LogIn(name);
    return CreateProfile(name);
  }

 private:
  // BrowserWithTestWindowTest:
  std::string GetDefaultProfileName() override {
    return "promary_profile@test";
  }
};

}  // namespace

// Tests -----------------------------------------------------------------------

// Verifies service instances are created on a per-profile basis.
TEST_F(FileChangeServiceTest, CreatesServiceInstancesPerProfile) {
  auto* factory = FileChangeServiceFactory::GetInstance();
  ASSERT_TRUE(factory);

  // `FileChangeService` should exist for the primary profile.
  auto* primary_profile = GetProfile();
  auto* primary_profile_service = factory->GetService(primary_profile);
  ASSERT_TRUE(primary_profile_service);

  // `FileChangeService` should be created as needed for additional profiles.
  constexpr char kSecondaryProfileName[] = "secondary_profile@test";
  auto* secondary_profile = CreateLoggedInUserProfile(kSecondaryProfileName);
  auto* secondary_profile_service = factory->GetService(secondary_profile);
  ASSERT_TRUE(secondary_profile_service);

  // Per-profile services should be unique.
  ASSERT_NE(primary_profile_service, secondary_profile_service);
}

// Verifies service instances are *not* created for OTR profiles.
TEST_F(FileChangeServiceTest, DoesntCreateServiceInstanceForOTRProfile) {
  auto* factory = FileChangeServiceFactory::GetInstance();
  ASSERT_TRUE(factory);

  // `FileChangeService` should be created for non-OTR profile.
  auto* profile = GetProfile();
  ASSERT_TRUE(profile);
  ASSERT_FALSE(profile->IsOffTheRecord());
  ASSERT_TRUE(factory->GetService(profile));

  // `FileChangeService` should *not* be created for OTR profile.
  auto* otr_profile =
      TestingProfile::Builder().BuildIncognito(profile->AsTestingProfile());
  ASSERT_TRUE(otr_profile);
  ASSERT_TRUE(otr_profile->IsOffTheRecord());
  ASSERT_FALSE(factory->GetService(otr_profile));
}

// Verifies service instance *are* created for guest OTR profiles.
TEST_F(FileChangeServiceTest, CreatesServiceInstanceForOTRGuestProfile) {
  auto* factory = FileChangeServiceFactory::GetInstance();
  ASSERT_TRUE(factory);

  // Construct a guest profile.
  TestingProfile::Builder guest_profile_builder;
  guest_profile_builder.SetGuestSession();
  guest_profile_builder.SetProfileName("guest_profile");
  std::unique_ptr<TestingProfile> guest_profile = guest_profile_builder.Build();

  // Service instances should be created for guest profiles.
  ASSERT_TRUE(guest_profile);
  ASSERT_FALSE(guest_profile->IsOffTheRecord());
  FileChangeService* const guest_profile_service =
      factory->GetService(guest_profile.get());
  ASSERT_TRUE(guest_profile_service);

  // Construct an OTR profile from `guest_profile`.
  TestingProfile::Builder otr_guest_profile_builder;
  otr_guest_profile_builder.SetGuestSession();
  otr_guest_profile_builder.SetProfileName(guest_profile->GetProfileUserName());
  Profile* const otr_guest_profile =
      otr_guest_profile_builder.BuildIncognito(guest_profile.get());
  ASSERT_TRUE(otr_guest_profile);
  ASSERT_TRUE(otr_guest_profile->IsOffTheRecord());

  // Service instances *should* be created for OTR guest profiles.
  FileChangeService* const otr_guest_profile_service =
      factory->GetService(otr_guest_profile);
  ASSERT_TRUE(otr_guest_profile_service);

  // OTR service instances should be distinct from non-OTR service instances.
  ASSERT_NE(otr_guest_profile_service, guest_profile_service);
}

// Verifies `OnFileMoved()` events are propagated to observers.
TEST_F(FileChangeServiceTest, PropagatesOnFileMovedEvents) {
  auto* profile = GetProfile();
  auto* service = FileChangeServiceFactory::GetInstance()->GetService(profile);
  ASSERT_TRUE(service);

  testing::NiceMock<MockFileChangeServiceObserver> mock_observer;
  base::ScopedObservation<FileChangeService, FileChangeServiceObserver>
      scoped_observation{&mock_observer};
  scoped_observation.Observe(service);

  TempFileSystem temp_file_system(profile);
  temp_file_system.SetUp();

  storage::FileSystemURL src = temp_file_system.CreateFileSystemURL("src");
  storage::FileSystemURL dst = temp_file_system.CreateFileSystemURL("dst");

  ASSERT_EQ(temp_file_system.CreateFile(src), base::File::FILE_OK);

  {
    base::RunLoop move_run_loop;
    EXPECT_CALL(mock_observer, OnFileMoved)
        // NOTE: `Move()` internally calls `MoveFileLocal()`, so move operation
        // gets reported twice.
        .WillOnce([&](const storage::FileSystemURL& propagated_src,
                      const storage::FileSystemURL& propagated_dst) {
          EXPECT_EQ(src, propagated_src);
          EXPECT_EQ(dst, propagated_dst);
        })
        .WillOnce([&](const storage::FileSystemURL& propagated_src,
                      const storage::FileSystemURL& propagated_dst) {
          EXPECT_EQ(src, propagated_src);
          EXPECT_EQ(dst, propagated_dst);
          move_run_loop.Quit();
        })
        .RetiresOnSaturation();

    EXPECT_CALL(mock_observer, OnFileModified).Times(0);

    ASSERT_EQ(temp_file_system.MoveFile(src, dst), base::File::FILE_OK);
    move_run_loop.Run();
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  {
    base::RunLoop move_run_loop;
    EXPECT_CALL(mock_observer, OnFileMoved)
        .WillOnce([&](const storage::FileSystemURL& propagated_src,
                      const storage::FileSystemURL& propagated_dst) {
          EXPECT_EQ(dst, propagated_src);
          EXPECT_EQ(src, propagated_dst);
          move_run_loop.Quit();
        })
        .RetiresOnSaturation();

    EXPECT_CALL(mock_observer, OnFileModified).Times(0);
    ASSERT_EQ(temp_file_system.MoveFileLocal(dst, src), base::File::FILE_OK);

    move_run_loop.Run();
  }
}

// Verifies `OnFileModified()` events are propagated to observers.
TEST_F(FileChangeServiceTest, PropagatesOnFileModifiedEvents) {
  auto* profile = GetProfile();
  auto* service = FileChangeServiceFactory::GetInstance()->GetService(profile);
  ASSERT_TRUE(service);

  testing::NiceMock<MockFileChangeServiceObserver> mock_observer;
  base::ScopedObservation<FileChangeService, FileChangeServiceObserver>
      scoped_observation{&mock_observer};
  scoped_observation.Observe(service);

  TempFileSystem temp_file_system(profile);
  temp_file_system.SetUp();

  storage::FileSystemURL url =
      temp_file_system.CreateFileSystemURL("test_file");

  ASSERT_EQ(temp_file_system.CreateFile(url), base::File::FILE_OK);

  // Test writing to file.
  {
    base::RunLoop modify_run_loop;
    EXPECT_CALL(mock_observer, OnFileModified)
        .WillOnce([&](const storage::FileSystemURL& propagated_url) {
          EXPECT_EQ(url, propagated_url);
          modify_run_loop.Quit();
        })
        .RetiresOnSaturation();

    ASSERT_EQ(temp_file_system.WriteFile(url, "Test file contents\n"),
              base::File::FILE_OK);
    modify_run_loop.Run();
  }

  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Test truncating file.
  {
    base::RunLoop modify_run_loop;
    EXPECT_CALL(mock_observer, OnFileModified)
        .WillOnce([&](const storage::FileSystemURL& propagated_url) {
          EXPECT_EQ(url, propagated_url);
          modify_run_loop.Quit();
        })
        .RetiresOnSaturation();

    ASSERT_EQ(temp_file_system.TruncateFile(url, 10), base::File::FILE_OK);
    modify_run_loop.Run();
  }

  // Test writing a stream to file.
  {
    base::RunLoop modify_run_loop;
    EXPECT_CALL(mock_observer, OnFileModified)
        .WillOnce([&](const storage::FileSystemURL& propagated_url) {
          EXPECT_EQ(url, propagated_url);
          modify_run_loop.Quit();
        })
        .RetiresOnSaturation();

    ASSERT_EQ(temp_file_system.WriteStreamToFile(
                  url, CreateStream("Test file contents from stream")),
              base::File::FILE_OK);
    modify_run_loop.Run();
  }
}

// Verifies `OnFileCreatedFromShowSaveFilePicker()` events are propagated to
// observers.
TEST_F(FileChangeServiceTest,
       PropagatesOnFileCreatedFromShowSaveFilePickerEvents) {
  auto* profile = GetProfile();
  auto* service = FileChangeServiceFactory::GetInstance()->GetService(profile);
  ASSERT_TRUE(service);

  testing::NiceMock<MockFileChangeServiceObserver> mock_observer;
  base::ScopedObservation<FileChangeService, FileChangeServiceObserver>
      scoped_observation(&mock_observer);
  scoped_observation.Observe(service);

  const GURL file_picker_binding_context;
  const storage::FileSystemURL url;

  EXPECT_CALL(mock_observer, OnFileCreatedFromShowSaveFilePicker(
                                 testing::Ref(file_picker_binding_context),
                                 testing::Ref(url)));

  FileSystemAccessPermissionContextFactory::GetForProfile(profile)
      ->OnFileCreatedFromShowSaveFilePicker(file_picker_binding_context, url);
}

}  // namespace ash
