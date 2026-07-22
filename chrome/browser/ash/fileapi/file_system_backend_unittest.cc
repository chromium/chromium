// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/file_system_backend.h"

#include <stddef.h>

#include <set>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using storage::ExternalMountPoints;
using storage::FileSystemURL;

namespace {

FileSystemURL CreateFileSystemURL(const std::string& extension,
                                  const char* path,
                                  ExternalMountPoints* mount_points) {
  return mount_points->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://" +
                                                    extension + "/"),
      storage::kFileSystemTypeExternal, base::FilePath::FromUTF8Unsafe(path));
}

TEST(ChromeOSFileSystemBackendTest, DefaultMountPoints) {
  // Make sure no system-level mount points are registered before testing
  // to avoid flakiness.
  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();

  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  ash::FileSystemBackend backend(
      nullptr,  // profile
      nullptr,  // file_system_provider_delegate
      nullptr,  // mtp_delegate
      nullptr,  // arc_content_delegate
      nullptr,  // arc_documents_provider_delegate
      nullptr,  // drivefs_delegate
      nullptr,  // smbfs_delegate
      mount_points.get(), storage::ExternalMountPoints::GetSystemInstance());
  backend.AddSystemMountPoints();

  std::vector<storage::MountPoints::MountPointInfo> system_mount_points;
  storage::ExternalMountPoints::GetSystemInstance()->AddMountPointInfosTo(
      &system_mount_points);

  std::set<base::FilePath> root_dirs_set;
  for (const auto& info : system_mount_points) {
    root_dirs_set.insert(info.path);
  }

  EXPECT_EQ(2u, system_mount_points.size());
  EXPECT_TRUE(
      root_dirs_set.count(ash::CrosDisksClient::GetRemovableDiskMountPoint()));
  EXPECT_TRUE(
      root_dirs_set.count(ash::CrosDisksClient::GetArchiveMountPoint()));
}

TEST(ChromeOSFileSystemBackendTest, AccessPermissions) {
  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<storage::ExternalMountPoints> system_mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  ash::FileSystemBackend backend(nullptr,  // profile
                                 nullptr,  // file_system_provider_delegate
                                 nullptr,  // mtp_delegate
                                 nullptr,  // arc_content_delegate
                                 nullptr,  // arc_documents_provider_delegate
                                 nullptr,  // drivefs_delegate
                                 nullptr,  // smbfs_delegate
                                 mount_points.get(), system_mount_points.get());

  std::string extension("ddammdhioacbehjngdmkjcjbnfginlla");
  url::Origin origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId(extension));

  // Initialize mount points.
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
      "system", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      base::FilePath(FPL("/g/system"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "removable", storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(FPL("/media/removable"))));

  // Backend specific mount point access.
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));

  backend.GrantFileAccessToOrigin(origin, base::FilePath(FPL("removable/foo")));
  EXPECT_TRUE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "removable/foo1", mount_points.get())));

  // System mount point access.
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "system/foo", system_mount_points.get())));

  backend.GrantFileAccessToOrigin(origin, base::FilePath(FPL("system/foo")));
  EXPECT_TRUE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "system/foo", system_mount_points.get())));
  EXPECT_FALSE(
      backend.IsAccessAllowed(ash::BackendFunction::kCreateFileSystemOperation,
                              storage::OperationType::kCopy,
                              CreateFileSystemURL(extension, "system/foo1",
                                                  system_mount_points.get())));

  // The extension cannot access new mount points.
  // TODO(tbarzic): This should probably be changed.
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "test", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      base::FilePath(FPL("/foo/test"))));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "test_/foo", mount_points.get())));

  backend.RevokeAccessForOrigin(origin);
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));

  // ImageLoader has access only to paths that have been explicitly granted.
  const std::string& image_loader = file_manager::kImageLoaderExtensionId;
  url::Origin image_loader_origin =
      url::Origin::Create(file_manager::util::GetImageLoaderBaseURL());
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kGetMetadata,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileStreamReader,
      storage::OperationType::kNone,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileStreamWriter,
      storage::OperationType::kNone,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));

  backend.GrantFileAccessToOrigin(image_loader_origin,
                                  base::FilePath(FPL("removable/foo")));
  EXPECT_TRUE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kGetMetadata,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
  EXPECT_TRUE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileStreamReader,
      storage::OperationType::kNone,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileStreamReader,
      storage::OperationType::kNone,
      CreateFileSystemURL(image_loader, "removable/bar", mount_points.get())));
  EXPECT_FALSE(
      backend.IsAccessAllowed(ash::BackendFunction::kCreateFileStreamReader,
                              storage::OperationType::kNone,
                              CreateFileSystemURL(image_loader, "system/foo",
                                                  system_mount_points.get())));

  // Even after granting file access, non-read operations must be denied for
  // ImageLoader.
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileSystemOperation,
      storage::OperationType::kCopy,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      ash::BackendFunction::kCreateFileStreamWriter,
      storage::OperationType::kNone,
      CreateFileSystemURL(image_loader, "removable/foo", mount_points.get())));
}

TEST(ChromeOSFileSystemBackendTest, GetVirtualPathConflictWithSystemPoints) {
  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<storage::ExternalMountPoints> system_mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  ash::FileSystemBackend backend(nullptr,  // profile
                                 nullptr,  // file_system_provider_delegate
                                 nullptr,  // mtp_delegate
                                 nullptr,  // arc_content_delegate
                                 nullptr,  // arc_documents_provider_delegate
                                 nullptr,  // drivefs_delegate
                                 nullptr,  // smbfs_delegate
                                 mount_points.get(), system_mount_points.get());

  const storage::FileSystemType type = storage::kFileSystemTypeLocal;
  const storage::FileSystemMountOption option =
      storage::FileSystemMountOption();

  // Backend specific mount points.
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "b", type, option, base::FilePath(FPL("/a/b"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "y", type, option, base::FilePath(FPL("/z/y"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "n", type, option, base::FilePath(FPL("/m/n"))));

  // System mount points
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
      "gb", type, option, base::FilePath(FPL("/a/b"))));
  ASSERT_TRUE(
      system_mount_points->RegisterFileSystem(
          "gz", type, option, base::FilePath(FPL("/z"))));
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
       "gp", type, option, base::FilePath(FPL("/m/n/o/p"))));

  struct TestCase {
    const base::FilePath::CharType* const local_path;
    bool success;
    const base::FilePath::CharType* const virtual_path;
  };

  const TestCase kTestCases[] = {
    // Same paths in both mount points.
    { FPL("/a/b/c/d"), true, FPL("b/c/d") },
    // System mount points path more specific.
    { FPL("/m/n/o/p/r/s"), true, FPL("n/o/p/r/s") },
    // System mount points path less specific.
    { FPL("/z/y/x"), true, FPL("y/x") },
    // Only system mount points path matches.
    { FPL("/z/q/r/s"), true, FPL("gz/q/r/s") },
    // No match.
    { FPL("/foo/xxx"), false, FPL("") },
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    // Initialize virtual path with a value.
    base::FilePath virtual_path(FPL("/mount"));
    base::FilePath local_path(UNSAFE_TODO(kTestCases[i]).local_path);
    UNSAFE_TODO(EXPECT_EQ(kTestCases[i].success,
                          backend.GetVirtualPath(local_path, &virtual_path)))
        << "Resolving " << UNSAFE_TODO(kTestCases[i]).local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!UNSAFE_TODO(kTestCases[i]).success) {
      continue;
    }

    base::FilePath expected_virtual_path(
        UNSAFE_TODO(kTestCases[i]).virtual_path);
    EXPECT_EQ(expected_virtual_path, virtual_path)
        << "Resolving " << UNSAFE_TODO(kTestCases[i]).local_path;
  }
}

}  // namespace
