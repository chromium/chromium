// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_system_backend.h"

#include <stddef.h>

#include <set>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "extensions/common/constants.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using storage::ExternalMountPoints;
using storage::FileSystemURL;

namespace {

FileSystemURL CreateFileSystemURL(const std::string& extension,
                                  const char* path,
                                  ExternalMountPoints* mount_points) {
  return mount_points->CreateCrackedFileSystemURL(
      url::Origin::Create(GURL("chrome-extension://" + extension + "/")),
      storage::kFileSystemTypeExternal, base::FilePath::FromUTF8Unsafe(path));
}

TEST(ChromeOSFileSystemBackendTest, DefaultMountPoints) {
  // Make sure no system-level mount points are registered before testing
  // to avoid flakiness.
  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();

  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  chromeos::FileSystemBackend backend(
      nullptr,  // file_system_provider_delegate
      nullptr,  // mtp_delegate
      nullptr,  // arc_content_delegate
      nullptr,  // arc_documents_provider_delegate
      nullptr,  // drivefs_delegate
      mount_points.get(), storage::ExternalMountPoints::GetSystemInstance());
  backend.AddSystemMountPoints();
  std::vector<base::FilePath> root_dirs = backend.GetRootDirectories();
  std::set<base::FilePath> root_dirs_set(root_dirs.begin(), root_dirs.end());

  // By default there should be 3 mount points (in system mount points):
  EXPECT_EQ(3u, root_dirs.size());

  EXPECT_TRUE(root_dirs_set.count(
      chromeos::CrosDisksClient::GetRemovableDiskMountPoint()));
  EXPECT_TRUE(root_dirs_set.count(
      chromeos::CrosDisksClient::GetArchiveMountPoint()));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/usr/share/oem"))));
}

TEST(ChromeOSFileSystemBackendTest, GetRootDirectories) {
  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());

  scoped_refptr<storage::ExternalMountPoints> system_mount_points(
      storage::ExternalMountPoints::CreateRefCounted());

  chromeos::FileSystemBackend backend(
      nullptr,  // file_system_provider_delegate
      nullptr,  // mtp_delegate
      nullptr,  // arc_content_delegate
      nullptr,  // arc_documents_provider_delegate
      nullptr,  // drivefs_delegate
      mount_points.get(), system_mount_points.get());

  const size_t initial_root_dirs_size = backend.GetRootDirectories().size();

  // Register 'local' test mount points.
  mount_points->RegisterFileSystem("c",
                                   storage::kFileSystemTypeNativeLocal,
                                   storage::FileSystemMountOption(),
                                   base::FilePath(FPL("/a/b/c")));
  mount_points->RegisterFileSystem("d",
                                   storage::kFileSystemTypeNativeLocal,
                                   storage::FileSystemMountOption(),
                                   base::FilePath(FPL("/b/c/d")));

  // Register system test mount points.
  system_mount_points->RegisterFileSystem("d",
                                          storage::kFileSystemTypeNativeLocal,
                                          storage::FileSystemMountOption(),
                                          base::FilePath(FPL("/g/c/d")));
  system_mount_points->RegisterFileSystem("e",
                                          storage::kFileSystemTypeNativeLocal,
                                          storage::FileSystemMountOption(),
                                          base::FilePath(FPL("/g/d/e")));

  std::vector<base::FilePath> root_dirs = backend.GetRootDirectories();
  std::set<base::FilePath> root_dirs_set(root_dirs.begin(), root_dirs.end());
  EXPECT_EQ(initial_root_dirs_size + 4, root_dirs.size());
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/a/b/c"))));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/b/c/d"))));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/g/c/d"))));
  EXPECT_TRUE(root_dirs_set.count(base::FilePath(FPL("/g/d/e"))));
}

TEST(ChromeOSFileSystemBackendTest, AccessPermissions) {
  url::AddStandardScheme(extensions::kExtensionScheme, url::SCHEME_WITH_HOST);

  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<storage::ExternalMountPoints> system_mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  chromeos::FileSystemBackend backend(
      nullptr,  // file_system_provider_delegate
      nullptr,  // mtp_delegate
      nullptr,  // arc_content_delegate
      nullptr,  // arc_documents_provider_delegate
      nullptr,  // drivefs_delegate
      mount_points.get(), system_mount_points.get());

  std::string extension("ddammdhioacbehjngdmkjcjbnfginlla");

  // Initialize mount points.
  ASSERT_TRUE(system_mount_points->RegisterFileSystem(
      "system",
      storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(FPL("/g/system"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "removable",
      storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(FPL("/media/removable"))));
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "oem",
      storage::kFileSystemTypeRestrictedNativeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(FPL("/usr/share/oem"))));

  // Backend specific mount point access.
  EXPECT_FALSE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));

  backend.GrantFileAccessToExtension(extension,
                                      base::FilePath(FPL("removable/foo")));
  EXPECT_TRUE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo1", mount_points.get())));

  // System mount point access.
  EXPECT_FALSE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "system/foo", system_mount_points.get())));

  backend.GrantFileAccessToExtension(extension,
                                      base::FilePath(FPL("system/foo")));
  EXPECT_TRUE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "system/foo", system_mount_points.get())));
  EXPECT_FALSE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "system/foo1",
                          system_mount_points.get())));

  // The extension cannot access new mount points.
  // TODO(tbarzic): This should probably be changed.
  ASSERT_TRUE(
      mount_points->RegisterFileSystem("test",
                                       storage::kFileSystemTypeNativeLocal,
                                       storage::FileSystemMountOption(),
                                       base::FilePath(FPL("/foo/test"))));
  EXPECT_FALSE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "test_/foo", mount_points.get())));

  backend.RevokeAccessForExtension(extension);
  EXPECT_FALSE(backend.IsAccessAllowed(
      CreateFileSystemURL(extension, "removable/foo", mount_points.get())));
}

TEST(ChromeOSFileSystemBackendTest, GetVirtualPathConflictWithSystemPoints) {
  scoped_refptr<storage::ExternalMountPoints> mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  scoped_refptr<storage::ExternalMountPoints> system_mount_points(
      storage::ExternalMountPoints::CreateRefCounted());
  chromeos::FileSystemBackend backend(
      nullptr,  // file_system_provider_delegate
      nullptr,  // mtp_delegate
      nullptr,  // arc_content_delegate
      nullptr,  // arc_documents_provider_delegate
      nullptr,  // drivefs_delegate
      mount_points.get(), system_mount_points.get());

  const storage::FileSystemType type = storage::kFileSystemTypeNativeLocal;
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

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    // Initialize virtual path with a value.
    base::FilePath virtual_path(FPL("/mount"));
    base::FilePath local_path(kTestCases[i].local_path);
    EXPECT_EQ(kTestCases[i].success,
              backend.GetVirtualPath(local_path, &virtual_path))
        << "Resolving " << kTestCases[i].local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!kTestCases[i].success)
      continue;

    base::FilePath expected_virtual_path(kTestCases[i].virtual_path);
    EXPECT_EQ(expected_virtual_path, virtual_path)
        << "Resolving " << kTestCases[i].local_path;
  }
}

}  // namespace
