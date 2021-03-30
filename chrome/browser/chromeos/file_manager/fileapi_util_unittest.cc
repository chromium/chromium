// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/fileapi_util.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace file_manager {
namespace util {
namespace {

// Helper class that sets up a temporary file system.
class TempFileSystem {
 public:
  TempFileSystem(Profile* profile, const std::string& extension_id)
      : name_(base::UnguessableToken::Create().ToString()),
        extension_id_(extension_id),
        origin_(url::Origin::Create(
            extensions::Extension::GetBaseURLFromExtensionId(extension_id))),
        file_system_context_(
            GetFileSystemContextForExtensionId(profile, extension_id)) {}

  ~TempFileSystem() {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
  }

  // Finishes setting up the temporary file system. Must be called before use.
  bool SetUp() {
    if (!temp_dir_.CreateUniqueTempDir()) {
      return false;
    }
    if (!storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
            name_, storage::kFileSystemTypeLocal,
            storage::FileSystemMountOption(), temp_dir_.GetPath())) {
      return false;
    }

    // Grant the test extension the ability to access the just created
    // file system.
    file_system_context_->external_backend()->GrantFileAccessToExtension(
        extension_id_, base::FilePath(name_));
    return true;
  }

  // For the given FileSystemURL creates a file.
  base::File::Error CreateFile(const storage::FileSystemURL& url) {
    return storage::AsyncFileTestHelper::CreateFile(file_system_context_, url);
  }

  // Creates an external file system URL for the given path.
  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        origin_, storage::kFileSystemTypeExternal,
        base::FilePath().Append(name_).Append(
            base::FilePath::FromUTF8Unsafe(path)));
  }

  const url::Origin origin() const { return origin_; }

 private:
  const std::string name_;
  const std::string extension_id_;
  const url::Origin origin_;
  storage::FileSystemContext* const file_system_context_;
  base::ScopedTempDir temp_dir_;
};

class FileManagerFileAPIUtilTest : public ::testing::Test {
 public:
  FileManagerFileAPIUtilTest() {}

  void SetUp() override {
    testing::Test::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing_profile");
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    profile_ = nullptr;
    profile_manager_.reset();
  }

  TestingProfile* GetProfile() { return profile_; }

 protected:
  const std::string extension_id_ = "abc";
  const std::string file_system_id_ = "test-filesystem";

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
};

// Passes the |result| to the |output| pointer.
void PassFileChooserFileInfoList(FileChooserFileInfoList* output,
                                 FileChooserFileInfoList result) {
  for (const auto& file : result)
    output->push_back(file->Clone());
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertSelectedFileInfoListToFileChooserFileInfoList) {
  Profile* const profile = GetProfile();
  auto fake_provider =
      chromeos::file_system_provider::FakeExtensionProvider::Create(
          extension_id_);
  const auto kProviderId = fake_provider->GetId();
  auto* service = chromeos::file_system_provider::Service::Get(profile);
  service->RegisterProvider(std::move(fake_provider));
  service->MountFileSystem(kProviderId,
                           chromeos::file_system_provider::MountOptions(
                               file_system_id_, "Test FileSystem"));

  // Obtain the file system context.
  content::StoragePartition* const partition =
      content::BrowserContext::GetStoragePartitionForUrl(
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

  const std::string path = FILE_PATH_LITERAL(base::StrCat(
      {"/provided/", extension_id_, ":", file_system_id_, ":/hello.txt"}));
  // Non-native file with cache.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(path);
    info.local_path = base::FilePath(FILE_PATH_LITERAL("/native/cache/xxx"));
    info.display_name = "display_name";
    selected_info_list.push_back(info);
  }

  // Non-native file without.
  {
    ui::SelectedFileInfo info;
    info.file_path = base::FilePath(path);
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
  EXPECT_EQ(u"display_name", result[0]->get_native_file()->display_name);

  EXPECT_TRUE(result[1]->is_native_file());
  EXPECT_EQ(FILE_PATH_LITERAL("/native/cache/xxx"),
            result[1]->get_native_file()->file_path.value());
  EXPECT_EQ(u"display_name", result[1]->get_native_file()->display_name);

  EXPECT_TRUE(result[2]->is_file_system());
  EXPECT_TRUE(result[2]->get_file_system()->url.is_valid());
  const storage::FileSystemURL url =
      context->CrackURL(result[2]->get_file_system()->url);
  EXPECT_EQ(GURL("http://example.com"), url.origin().GetURL());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, url.mount_type());
  EXPECT_EQ(storage::kFileSystemTypeProvided, url.type());
  EXPECT_EQ(55u, result[2]->get_file_system()->length);
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionListSuccess) {
  base::RunLoop run_loop;
  EntryDefinitionListCallback callback = base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::unique_ptr<EntryDefinitionList> entries) {
        ASSERT_EQ(2, entries->size());
        EXPECT_EQ(base::File::FILE_OK, entries->at(0).error);
        EXPECT_EQ(base::File::FILE_OK, entries->at(1).error);

        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  Profile* const profile = GetProfile();
  TempFileSystem temp_file_system(profile, extension_id_);
  ASSERT_TRUE(temp_file_system.SetUp());

  // Create two external FileSystemURL objects for the test extension that
  // reference actual files created on the temporary file system.
  storage::FileSystemURL x_file_url =
      temp_file_system.CreateFileSystemURL("x.txt");
  storage::FileSystemURL y_file_url =
      temp_file_system.CreateFileSystemURL("y.txt");
  // Create the underlying files referenced by the above created FileSystemURLs.
  ASSERT_EQ(base::File::FILE_OK, temp_file_system.CreateFile(x_file_url));
  ASSERT_EQ(base::File::FILE_OK, temp_file_system.CreateFile(y_file_url));

  FileDefinition x_fd = {.virtual_path = x_file_url.virtual_path()},
                 y_fd = {.virtual_path = y_file_url.virtual_path()};
  ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForExtensionId(profile,
                                                             extension_id_),
      temp_file_system.origin(), {x_fd, y_fd}, std::move(callback));
  run_loop.Run();
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionListNotFound) {
  base::RunLoop run_loop;
  EntryDefinitionListCallback callback = base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::unique_ptr<EntryDefinitionList> entries) {
        ASSERT_EQ(1, entries->size());
        EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, entries->at(0).error);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  TempFileSystem temp_file_system(GetProfile(), extension_id_);
  storage::FileSystemURL x_file_url = storage::FileSystemURL::CreateForTest(
      temp_file_system.origin(), storage::kFileSystemTypeExternal,
      base::FilePath().Append("not-found"));
  FileDefinition x_fd = {.virtual_path = x_file_url.virtual_path()};

  ConvertFileDefinitionListToEntryDefinitionList(
      file_manager::util::GetFileSystemContextForExtensionId(GetProfile(),
                                                             extension_id_),
      temp_file_system.origin(), {x_fd}, std::move(callback));
  run_loop.Run();
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionNullContext) {
  base::RunLoop run_loop;
  EntryDefinitionListCallback callback = base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::unique_ptr<EntryDefinitionList> entries) {
        ASSERT_EQ(1, entries->size());
        EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
                  entries->at(0).error);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  TempFileSystem temp_file_system(GetProfile(), extension_id_);
  ASSERT_TRUE(temp_file_system.SetUp());
  storage::FileSystemURL x_file_url = temp_file_system.CreateFileSystemURL(".");
  FileDefinition x_fd = {.virtual_path = x_file_url.virtual_path()};

  // Check a simple case where the context is already null before we have
  // a chance to call the conversion function.
  ConvertFileDefinitionListToEntryDefinitionList(
      nullptr, temp_file_system.origin(), {x_fd}, std::move(callback));
  run_loop.Run();
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionContextReset) {
  base::RunLoop run_loop;
  EntryDefinitionListCallback callback = base::BindOnce(
      [](base::OnceClosure quit_closure,
         std::unique_ptr<EntryDefinitionList> entries) {
        ASSERT_EQ(1, entries->size());
        EXPECT_EQ(base::File::FILE_OK, entries->at(0).error);
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure());

  TempFileSystem temp_file_system(GetProfile(), extension_id_);
  ASSERT_TRUE(temp_file_system.SetUp());
  storage::FileSystemURL x_file_url = temp_file_system.CreateFileSystemURL(".");
  FileDefinition x_fd = {.virtual_path = x_file_url.virtual_path()};
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForExtensionId(GetProfile(),
                                                             extension_id_);

  // Check the case where the context is not null, but is reset to null as
  // soon as function call is completed. Conversion takes place on a
  // different thread, after the function call returns. However, since
  // it holds to a copy of a scoped pointer we expect it to succeed.
  ConvertFileDefinitionListToEntryDefinitionList(file_system_context,
                                                 temp_file_system.origin(),
                                                 {x_fd}, std::move(callback));
  file_system_context.reset();

  run_loop.Run();
}

}  // namespace
}  // namespace util
}  // namespace file_manager
