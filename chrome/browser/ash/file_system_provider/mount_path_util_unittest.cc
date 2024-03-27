// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/mount_path_util.h"

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/icon_set.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/isolated_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash::file_system_provider::util {

namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "File/System/Id";
const char kDisplayName[] = "Camera Pictures";
const ProviderId kProviderId = ProviderId::CreateFromExtensionId(kExtensionId);
const ProviderId kNativeProviderId = ProviderId::CreateFromNativeId("native");

// Creates a FileSystemURL for tests.
storage::FileSystemURL CreateFileSystemURL(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& file_path) {
  const std::string origin = std::string("chrome-extension://") + kExtensionId;
  const base::FilePath mount_path = file_system_info.mount_path();
  const storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  DCHECK(file_path.IsAbsolute());
  base::FilePath relative_path(file_path.value().substr(1));
  return mount_points->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFromStringForTesting(origin),
      storage::kFileSystemTypeExternal,
      base::FilePath(mount_path.BaseName().Append(relative_path)));
}

}  // namespace

class FileSystemProviderMountPathUtilTest : public testing::Test {
 protected:
  FileSystemProviderMountPathUtilTest() = default;
  ~FileSystemProviderMountPathUtilTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing-profile");
    user_manager_ = new FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_.get()));
    user_manager_->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    file_system_provider_service_ = Service::Get(profile_);
    file_system_provider_service_->RegisterProvider(
        FakeExtensionProvider::Create(kExtensionId));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;  // Owned by TestingProfileManager.
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  raw_ptr<FakeChromeUserManager> user_manager_;
  raw_ptr<Service> file_system_provider_service_;  // Owned by its factory.
};

TEST_F(FileSystemProviderMountPathUtilTest, GetMountPath) {
  const base::FilePath extension_result =
      GetMountPath(profile_, kProviderId, kFileSystemId);
  const std::string extension_expected =
      "/provided/mbflcebpggnecokmikipoihdbecnjfoj:"
      "File%2FSystem%2FId:testing-profile-hash";
  EXPECT_EQ(extension_expected, extension_result.AsUTF8Unsafe());

  const base::FilePath native_result =
      GetMountPath(profile_, kNativeProviderId, kFileSystemId);
  const std::string native_expected =
      "/provided/@native:"
      "File%2FSystem%2FId:testing-profile-hash";
  EXPECT_EQ(native_expected, native_result.AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, IsFileSystemProviderLocalPath) {
  const base::FilePath mount_path =
      GetMountPath(profile_, kProviderId, kFileSystemId);
  const base::FilePath file_path =
      base::FilePath(FILE_PATH_LITERAL("/hello/world.txt"));
  const base::FilePath local_file_path =
      mount_path.Append(base::FilePath(file_path.value().substr(1)));

  EXPECT_TRUE(IsFileSystemProviderLocalPath(mount_path));
  EXPECT_TRUE(IsFileSystemProviderLocalPath(local_file_path));

  EXPECT_FALSE(IsFileSystemProviderLocalPath(
      base::FilePath(FILE_PATH_LITERAL("provided/hello-world/test.txt"))));
  EXPECT_FALSE(IsFileSystemProviderLocalPath(
      base::FilePath(FILE_PATH_LITERAL("/provided"))));
  EXPECT_FALSE(
      IsFileSystemProviderLocalPath(base::FilePath(FILE_PATH_LITERAL("/"))));
  EXPECT_FALSE(IsFileSystemProviderLocalPath(base::FilePath()));
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser) {
  const base::File::Error result =
      file_system_provider_service_->MountFileSystem(
          kProviderId, MountOptions(kFileSystemId, kDisplayName));
  ASSERT_EQ(base::File::FILE_OK, result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_
          ->GetProvidedFileSystem(kProviderId, kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath =
      base::FilePath(FILE_PATH_LITERAL("/hello/world.txt"));
  const storage::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  EXPECT_TRUE(url.is_valid());

  FileSystemURLParser parser(url);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser_RootPath) {
  const base::File::Error result =
      file_system_provider_service_->MountFileSystem(
          kProviderId, MountOptions(kFileSystemId, kDisplayName));
  ASSERT_EQ(base::File::FILE_OK, result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_
          ->GetProvidedFileSystem(kProviderId, kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath = base::FilePath(FILE_PATH_LITERAL("/"));
  const storage::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  EXPECT_TRUE(url.is_valid());

  FileSystemURLParser parser(url);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser_WrongUrl) {
  const ProvidedFileSystemInfo file_system_info(
      kProviderId, MountOptions(kFileSystemId, kDisplayName),
      GetMountPath(profile_, kProviderId, kFileSystemId),
      /*configurable=*/false, /*watchable=*/true, extensions::SOURCE_FILE,
      IconSet());

  const base::FilePath kFilePath = base::FilePath(FILE_PATH_LITERAL("/hello"));
  const storage::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  // It is impossible to create a cracked URL for a mount point which doesn't
  // exist, therefore is will always be invalid, and empty.
  EXPECT_FALSE(url.is_valid());

  FileSystemURLParser parser(url);
  EXPECT_FALSE(parser.Parse());
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser_IsolatedURL) {
  const base::File::Error result =
      file_system_provider_service_->MountFileSystem(
          kProviderId, MountOptions(kFileSystemId, kDisplayName));
  ASSERT_EQ(base::File::FILE_OK, result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_
          ->GetProvidedFileSystem(kProviderId, kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath =
      base::FilePath(FILE_PATH_LITERAL("/hello/world.txt"));
  const storage::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  EXPECT_TRUE(url.is_valid());

  // Create an isolated URL for the original one.
  storage::IsolatedContext* const isolated_context =
      storage::IsolatedContext::GetInstance();
  const storage::IsolatedContext::ScopedFSHandle isolated_file_system =
      isolated_context->RegisterFileSystemForPath(
          storage::kFileSystemTypeProvided, url.filesystem_id(), url.path(),
          nullptr);

  const base::FilePath isolated_virtual_path =
      isolated_context->CreateVirtualRootPath(isolated_file_system.id())
          .Append(kFilePath.BaseName().value());

  const storage::FileSystemURL isolated_url =
      isolated_context->CreateCrackedFileSystemURL(
          url.storage_key(), storage::kFileSystemTypeIsolated,
          isolated_virtual_path);

  EXPECT_TRUE(isolated_url.is_valid());

  FileSystemURLParser parser(isolated_url);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, LocalPathParser) {
  const base::File::Error result =
      file_system_provider_service_->MountFileSystem(
          kProviderId, MountOptions(kFileSystemId, kDisplayName));
  ASSERT_EQ(base::File::FILE_OK, result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_
          ->GetProvidedFileSystem(kProviderId, kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath =
      base::FilePath(FILE_PATH_LITERAL("/hello/world.txt"));
  const base::FilePath kLocalFilePath = file_system_info.mount_path().Append(
      base::FilePath(kFilePath.value().substr(1)));

  LOG(ERROR) << kLocalFilePath.value();
  LocalPathParser parser(profile_, kLocalFilePath);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, LocalPathParser_RootPath) {
  const base::File::Error result =
      file_system_provider_service_->MountFileSystem(
          kProviderId, MountOptions(kFileSystemId, kDisplayName));
  ASSERT_EQ(base::File::FILE_OK, result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_
          ->GetProvidedFileSystem(kProviderId, kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath = base::FilePath(FILE_PATH_LITERAL("/"));
  const base::FilePath kLocalFilePath = file_system_info.mount_path();

  LocalPathParser parser(profile_, kLocalFilePath);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, LocalPathParser_WrongPath) {
  {
    const base::FilePath kFilePath =
        base::FilePath(FILE_PATH_LITERAL("/hello"));
    LocalPathParser parser(profile_, kFilePath);
    EXPECT_FALSE(parser.Parse());
  }

  {
    const base::FilePath kFilePath =
        base::FilePath(FILE_PATH_LITERAL("/provided"));
    LocalPathParser parser(profile_, kFilePath);
    EXPECT_FALSE(parser.Parse());
  }

  {
    const base::FilePath kFilePath =
        base::FilePath(FILE_PATH_LITERAL("provided/hello/world"));
    LocalPathParser parser(profile_, kFilePath);
    EXPECT_FALSE(parser.Parse());
  }
}

}  // namespace ash::file_system_provider::util
