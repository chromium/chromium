// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_change_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_observer.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns the file system context associated with the specified `profile`.
storage::FileSystemContext* GetFileSystemContext(Profile* profile) {
  return file_manager::util::GetFileSystemContextForExtensionId(
      profile, file_manager::kFileManagerAppId);
}

// Returns the file system operation runner associated with the specified
// `profile`.
storage::FileSystemOperationRunner* GetFileSystemOperationRunner(
    Profile* profile) {
  return GetFileSystemContext(profile)->operation_runner();
}

// MockFileChangeServiceObserver -----------------------------------------------

class MockFileChangeServiceObserver : public FileChangeServiceObserver {
 public:
  // FileChangeServiceObserver:
  MOCK_METHOD(void,
              OnFileCopied,
              (const storage::FileSystemURL& src,
               const storage::FileSystemURL& dst),
              (override));
  MOCK_METHOD(void,
              OnFileMoved,
              (const storage::FileSystemURL& src,
               const storage::FileSystemURL& dst),
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
            name_, storage::kFileSystemTypeNativeLocal,
            storage::FileSystemMountOption(), temp_dir_.GetPath()));

    GetFileSystemContext(profile_)
        ->external_backend()
        ->GrantFileAccessToExtension(file_manager::kFileManagerAppId,
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
        origin_, storage::kFileSystemTypeNativeLocal,
        temp_dir_.GetPath().Append(base::FilePath::FromUTF8Unsafe(path)));
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
  Profile* const profile_;
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
  TestingProfile* CreateProfileWithName(const std::string& name) {
    return profile_manager()->CreateTestingProfile(name);
  }

 private:
  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    constexpr char kPrimaryProfileName[] = "primary_profile";
    return CreateProfileWithName(kPrimaryProfileName);
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
  constexpr char kSecondaryProfileName[] = "secondary_profile";
  auto* secondary_profile = CreateProfileWithName(kSecondaryProfileName);
  auto* secondary_profile_service = factory->GetService(secondary_profile);
  ASSERT_TRUE(secondary_profile_service);

  // Per-profile services should be unique.
  ASSERT_NE(primary_profile_service, secondary_profile_service);
}

// Verifies `OnFileCopied` events are propagated to observers.
TEST_F(FileChangeServiceTest, PropagatesOnFileCopiedEvents) {
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

  EXPECT_CALL(mock_observer, OnFileCopied)
      .WillRepeatedly([&](const storage::FileSystemURL& propagated_src,
                          const storage::FileSystemURL& propagated_dst) {
        EXPECT_EQ(src, propagated_src);
        EXPECT_EQ(dst, propagated_dst);
      });

  ASSERT_EQ(temp_file_system.CopyFile(src, dst), base::File::FILE_OK);
  ASSERT_EQ(temp_file_system.RemoveFile(dst), base::File::FILE_OK);
  ASSERT_EQ(temp_file_system.CopyFileLocal(src, dst), base::File::FILE_OK);
}

// Verifies `OnFileMoved` events are propagated to observers.
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

  EXPECT_CALL(mock_observer, OnFileMoved)
      .WillRepeatedly([&](const storage::FileSystemURL& propagated_src,
                          const storage::FileSystemURL& propagated_dst) {
        EXPECT_EQ(src, propagated_src);
        EXPECT_EQ(dst, propagated_dst);
      });

  ASSERT_EQ(temp_file_system.MoveFile(src, dst), base::File::FILE_OK);
  std::swap(dst, src);
  ASSERT_EQ(temp_file_system.MoveFileLocal(src, dst), base::File::FILE_OK);
}

}  // namespace chromeos
