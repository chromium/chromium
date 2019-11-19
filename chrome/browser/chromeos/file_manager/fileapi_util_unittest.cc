
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/file_manager/mount_test_util.h"
#include "chrome/browser/chromeos/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager {
namespace util {
namespace {

// Passes the |result| to the |output| pointer.
void PassFileChooserFileInfoList(FileChooserFileInfoList* output,
                                 FileChooserFileInfoList result) {
  for (const auto& file : result)
    output->push_back(file->Clone());
}

constexpr char kExtensionId[] = "abc";
constexpr char kFileSystemId[] = "test-filesystem";

TEST(FileManagerFileAPIUtilTest,
     ConvertSelectedFileInfoListToFileChooserFileInfoList) {
  // Prepare the test environment.
  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext service_manager_context;
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  // Prepare the test profile.
  Profile* const profile = profile_manager.CreateTestingProfile("test-user");
  auto fake_provider =
      chromeos::file_system_provider::FakeExtensionProvider::Create(
          kExtensionId);
  const auto kProviderId = fake_provider->GetId();
  auto* service = chromeos::file_system_provider::Service::Get(profile);
  service->RegisterProvider(std::move(fake_provider));
  service->MountFileSystem(kProviderId,
                           chromeos::file_system_provider::MountOptions(
                               kFileSystemId, "Test FileSystem"));

  // Obtain the file system context.
  content::StoragePartition* const partition =
      content::BrowserContext::GetStoragePartitionForSite(
          profile, GURL("http://example.com"));
  ASSERT_TRUE(partition);
  storage::FileSystemContext* const context = partition->GetFileSystemContext();
  ASSERT_TRUE(context);

  // Prepare the test input.
  SelectedFileInfoList selected_info_list;

  // Native file.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(FILE_PATH_LITERAL("/native/File 1.txt"));
    info.local_path = base::FilePath(FILE_PATH_LITERAL("/native/File 1.txt"));
    info.display_name = "display_name";
    selected_info_list.push_back(info);
  }

  // Non-native file with cache.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(
        FILE_PATH_LITERAL("/provided/abc:test-filesystem:/hello.txt"));
    info.local_path = base::FilePath(FILE_PATH_LITERAL("/native/cache/xxx"));
    info.display_name = "display_name";
    selected_info_list.push_back(info);
  }

  // Non-native file without.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(
        FILE_PATH_LITERAL("/provided/abc:test-filesystem:/hello.txt"));
    selected_info_list.push_back(info);
  }

  // Run the test target.
  FileChooserFileInfoList result;
  ConvertSelectedFileInfoListToFileChooserFileInfoList(
      context, GURL("http://example.com"), selected_info_list,
      base::BindOnce(&PassFileChooserFileInfoList, &result));
  content::RunAllTasksUntilIdle();

  // Check the result.
  ASSERT_EQ(3u, result.size());

  EXPECT_TRUE(result[0]->is_native_file());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/File 1.txt"),
            result[0]->get_native_file()->file_path.value());
  EXPECT_EQ(base::ASCIIToUTF16("display_name"),
            result[0]->get_native_file()->display_name);

  EXPECT_TRUE(result[1]->is_native_file());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/cache/xxx"),
            result[1]->get_native_file()->file_path.value());
  EXPECT_EQ(base::ASCIIToUTF16("display_name"),
            result[1]->get_native_file()->display_name);

  EXPECT_TRUE(result[2]->is_file_system());
  EXPECT_TRUE(result[2]->get_file_system()->url.is_valid());
  const storage::FileSystemURL url =
      context->CrackURL(result[2]->get_file_system()->url);
  EXPECT_EQ(GURL("http://example.com"), url.origin().GetURL());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, url.mount_type());
  EXPECT_EQ(storage::kFileSystemTypeProvided, url.type());
  EXPECT_EQ(55u, result[2]->get_file_system()->length);
}

}  // namespace
}  // namespace util
}  // namespace file_manager
