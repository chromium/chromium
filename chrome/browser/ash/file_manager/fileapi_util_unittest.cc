// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/fileapi_util.h"

#include <memory>
#include <string>

#include "base/files/file_error_or.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace file_manager {
namespace util {
namespace {

// Helper class that sets up a temporary file system.
class TempFileSystem {
 public:
  TempFileSystem(Profile* profile, const GURL& appURL)
      : name_(base::UnguessableToken::Create().ToString()),
        appURL_(appURL),
        origin_(url::Origin::Create(appURL)),
        file_system_context_(
            GetFileSystemContextForSourceURL(profile, appURL)) {}

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
    ash::FileSystemBackend::Get(*file_system_context_)
        ->GrantFileAccessToOrigin(origin_, base::FilePath(name_));
    return true;
  }

  bool TearDown() {
    return storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
        name_);
  }

  // For the given FileSystemURL creates a file.
  base::File::Error CreateFile(const storage::FileSystemURL& url) {
    return storage::AsyncFileTestHelper::CreateFile(file_system_context_, url);
  }

  // For the given FileSystemURL creates a directory.
  base::File::Error CreateDirectory(const storage::FileSystemURL& url) {
    return storage::AsyncFileTestHelper::CreateDirectory(file_system_context_,
                                                         url);
  }

  // Creates an external file system URL for the given path.
  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFirstParty(origin_),
        storage::kFileSystemTypeExternal,
        base::FilePath().Append(name_).Append(
            base::FilePath::FromUTF8Unsafe(path)));
  }

 private:
  const std::string name_;
  const GURL appURL_;
  const url::Origin origin_;
  const raw_ptr<storage::FileSystemContext> file_system_context_;
  base::ScopedTempDir temp_dir_;
};

class FileManagerFileAPIUtilTest : public ::testing::Test {
 public:
  // Carries information on how to create a FileSystemURL for a given file name.
  // For !valid orders we create a test URL. Otherwise, we use temp file system.
  struct FileSystemURLOrder {
    std::string file_name;
    bool valid;
  };

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
  // Checks if the conversion of FileDefinition to EntryDefinition works
  // correctly for the given |appURLStr| and a set of |orders|. If the
  // order indicates that the file should not be created, we expect the
  // conversion to return base::File::FILE_ERROR_NOT_FOUND error. Otherwise,
  // we expect base::File::FILE_OK status.
  void CheckConvertFileDefinitionListToEntryDefinitionList(
      const std::string& appURLStr,
      const std::vector<FileSystemURLOrder>& orders) {
    GURL appURL(appURLStr);
    ASSERT_TRUE(appURL.is_valid());
    auto temp_file_system = std::make_unique<TempFileSystem>(profile_, appURL);
    ASSERT_TRUE(temp_file_system->SetUp());

    std::vector<base::File::Error> errors;
    std::vector<FileDefinition> file_definitions;
    for (const FileSystemURLOrder& order : orders) {
      storage::FileSystemURL fs_url;
      if (order.valid) {
        fs_url = temp_file_system->CreateFileSystemURL(order.file_name);
        errors.push_back(base::File::FILE_OK);
      } else {
        fs_url = storage::FileSystemURL::CreateForTest(
            blink::StorageKey::CreateFirstParty(url::Origin::Create(appURL)),
            storage::kFileSystemTypeExternal, base::FilePath(order.file_name));
        errors.push_back(base::File::FILE_ERROR_NOT_FOUND);
      }
      file_definitions.push_back({.virtual_path = fs_url.virtual_path()});
    }

    base::RunLoop run_loop;
    EntryDefinitionListCallback callback = base::BindOnce(
        [](std::unique_ptr<TempFileSystem> temp_file_system,
           std::vector<base::File::Error> errors,
           base::OnceClosure quit_closure,
           std::unique_ptr<EntryDefinitionList> entries) {
          ASSERT_EQ(errors.size(), entries->size());
          for (size_t i = 0; i < errors.size(); ++i) {
            const EntryDefinition& entry_def = (*entries)[i];
            EXPECT_EQ(errors[i], entry_def.error)
                << "for " << entry_def.full_path << " at " << i;
          }

          EXPECT_TRUE(temp_file_system->TearDown());
          std::move(quit_closure).Run();
        },
        std::move(temp_file_system), std::move(errors), run_loop.QuitClosure());
    ConvertFileDefinitionListToEntryDefinitionList(
        GetFileSystemContextForSourceURL(profile_, appURL),
        url::Origin::Create(appURL), file_definitions, std::move(callback));
    run_loop.Run();
  }

  void TestGenerateUnusedFilename(std::vector<std::string> existing_files,
                                  std::string target_filename,
                                  base::FileErrorOr<std::string> expected);

  const std::string file_system_id_ = "test-filesystem";

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
};

// Passes the |result| to the |output| pointer.
void PassFileChooserFileInfoList(FileChooserFileInfoList* output,
                                 FileChooserFileInfoList result) {
  for (const auto& file : result) {
    output->push_back(file->Clone());
  }
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertSelectedFileInfoListToFileChooserFileInfoList) {
  Profile* const profile = GetProfile();
  const std::string extension_id = "abc";
  auto fake_provider =
      ash::file_system_provider::FakeExtensionProvider::Create(extension_id);
  const auto kProviderId = fake_provider->GetId();
  auto* service = ash::file_system_provider::Service::Get(profile);
  service->RegisterProvider(std::move(fake_provider));
  service->MountFileSystem(
      kProviderId, ash::file_system_provider::MountOptions(file_system_id_,
                                                           "Test FileSystem"));

  // Obtain the file system context.
  content::StoragePartition* const partition =
      profile->GetStoragePartitionForUrl(GURL("http://example.com"));
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
      {"/provided/", extension_id, ":", file_system_id_, ":/hello.txt"}));
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
      context, url::Origin::Create(GURL("http://example.com")),
      selected_info_list,
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
      context->CrackURLInFirstPartyContext(result[2]->get_file_system()->url);
  EXPECT_EQ(GURL("http://example.com"), url.origin().GetURL());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, url.mount_type());
  EXPECT_EQ(storage::kFileSystemTypeProvided, url.type());
  EXPECT_EQ(55u, result[2]->get_file_system()->length);
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionListExtension) {
  std::vector<FileSystemURLOrder> orders = {
      {.file_name = "x.txt", .valid = true},
      {.file_name = "no-such-file.txt", .valid = false},
      {.file_name = "z.txt", .valid = true},
  };
  CheckConvertFileDefinitionListToEntryDefinitionList("chrome-extension://abc",
                                                      orders);
  CheckConvertFileDefinitionListToEntryDefinitionList("chrome-extension://abc/",
                                                      orders);
  CheckConvertFileDefinitionListToEntryDefinitionList(
      "chrome-extension://abc/efg", orders);
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionListApp) {
  std::vector<FileSystemURLOrder> orders = {
      {.file_name = "a.txt", .valid = false},
      {.file_name = "b.txt", .valid = false},
      {.file_name = "i-am-a-file.txt", .valid = true},
  };
  CheckConvertFileDefinitionListToEntryDefinitionList("chrome://file-manager",
                                                      orders);
  CheckConvertFileDefinitionListToEntryDefinitionList("chrome://file-manager/",
                                                      orders);
  CheckConvertFileDefinitionListToEntryDefinitionList(
      "chrome://file-manager/abc", orders);
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionNullContext) {
  Profile* const profile = GetProfile();
  const GURL appURL("chrome-extension://abc/");
  auto temp_file_system = std::make_unique<TempFileSystem>(profile, appURL);
  ASSERT_TRUE(temp_file_system->SetUp());
  storage::FileSystemURL x_file_url =
      temp_file_system->CreateFileSystemURL(".");
  FileDefinition x_fd = {.virtual_path = x_file_url.virtual_path()};

  // Check a simple case where the context is already null before we have
  // a chance to call the conversion function.
  base::RunLoop run_loop;
  EntryDefinitionListCallback callback = base::BindOnce(
      [](std::unique_ptr<TempFileSystem> temp_file_system,
         base::OnceClosure quit_closure,
         std::unique_ptr<EntryDefinitionList> entries) {
        ASSERT_EQ(1u, entries->size());
        EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION,
                  entries->at(0).error);
        EXPECT_TRUE(temp_file_system->TearDown());
        std::move(quit_closure).Run();
      },
      std::move(temp_file_system), run_loop.QuitClosure());

  ConvertFileDefinitionListToEntryDefinitionList(
      nullptr, url::Origin::Create(appURL), {x_fd}, std::move(callback));
  run_loop.Run();
}

TEST_F(FileManagerFileAPIUtilTest,
       ConvertFileDefinitionListToEntryDefinitionContextReset) {
  Profile* const profile = GetProfile();
  const GURL appURL("chrome-extension://abc/");
  auto temp_file_system = std::make_unique<TempFileSystem>(profile, appURL);
  ASSERT_TRUE(temp_file_system->SetUp());
  storage::FileSystemURL x_file_url =
      temp_file_system->CreateFileSystemURL(".");
  FileDefinition x_fd = {.virtual_path = x_file_url.virtual_path()};
  scoped_refptr<storage::FileSystemContext> file_system_context =
      GetFileSystemContextForSourceURL(profile, appURL);

  base::RunLoop run_loop;
  EntryDefinitionListCallback callback = base::BindOnce(
      [](std::unique_ptr<TempFileSystem> temp_file_system,
         base::OnceClosure quit_closure,
         std::unique_ptr<EntryDefinitionList> entries) {
        ASSERT_EQ(1u, entries->size());
        EXPECT_EQ(base::File::FILE_OK, entries->at(0).error);
        EXPECT_TRUE(temp_file_system->TearDown());
        std::move(quit_closure).Run();
      },
      std::move(temp_file_system), run_loop.QuitClosure());

  // Check the case where the context is not null, but is reset to null as
  // soon as function call is completed. Conversion takes place on a
  // different thread, after the function call returns. However, since
  // it holds to a copy of a scoped pointer we expect it to succeed.
  ConvertFileDefinitionListToEntryDefinitionList(file_system_context,
                                                 url::Origin::Create(appURL),
                                                 {x_fd}, std::move(callback));
  file_system_context.reset();

  run_loop.Run();
}

TEST_F(FileManagerFileAPIUtilTest, IsFileManagerURL) {
  EXPECT_TRUE(IsFileManagerURL(GetFileManagerURL()));
  EXPECT_TRUE(IsFileManagerURL(GetFileManagerURL().Resolve("/some/path")));
  EXPECT_TRUE(IsFileManagerURL(
      GetFileManagerURL().Resolve("/some/path").Resolve("#anchor")));
  EXPECT_TRUE(IsFileManagerURL(GetFileManagerURL()
                                   .Resolve("/some/path")
                                   .Resolve("#anchor")
                                   .Resolve("?a=b")));
  EXPECT_FALSE(IsFileManagerURL(GURL("chrome://not-file-manager")));
  EXPECT_FALSE(IsFileManagerURL(GURL("chrome://not-file-manager/")));
  EXPECT_FALSE(IsFileManagerURL(
      GURL("chrome-extension://iamnotafilemanagerextensionid")));
  EXPECT_FALSE(IsFileManagerURL(
      GURL("chrome-extension://iamnotafilemanagerextensionid/")));
}

void FileManagerFileAPIUtilTest::TestGenerateUnusedFilename(
    std::vector<std::string> existing_files,
    std::string target_filename,
    base::FileErrorOr<std::string> expected) {
  const GURL appURL("chrome-extension://abc/");
  auto temp_file_system =
      std::make_unique<TempFileSystem>(GetProfile(), appURL);
  ASSERT_TRUE(temp_file_system->SetUp());
  storage::FileSystemURL root_url = temp_file_system->CreateFileSystemURL("");
  scoped_refptr<storage::FileSystemContext> file_system_context =
      GetFileSystemContextForSourceURL(GetProfile(), appURL);

  for (const std::string& file : existing_files) {
    if (file.back() == '/') {
      temp_file_system->CreateDirectory(
          temp_file_system->CreateFileSystemURL(file));
    } else {
      temp_file_system->CreateFile(temp_file_system->CreateFileSystemURL(file));
    }
  }

  base::RunLoop run_loop;
  GenerateUnusedFilename(
      root_url, base::FilePath(target_filename), file_system_context,
      base::BindLambdaForTesting(
          [&](base::FileErrorOr<storage::FileSystemURL> result) {
            if (!expected.has_value()) {
              EXPECT_FALSE(result.has_value())
                  << "Unexpected result " << result->ToGURL();
              EXPECT_EQ(expected.error(), result.error());
            } else {
              EXPECT_TRUE(result.has_value())
                  << "Unexpected error " << result.error();
              EXPECT_EQ(temp_file_system->CreateFileSystemURL(expected.value())
                            .ToGURL(),
                        result->ToGURL());
            }
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameBasic) {
  TestGenerateUnusedFilename({}, "foo.bar", {"foo.bar"});
  TestGenerateUnusedFilename({"foo.bar"}, "foo.bar", {"foo (1).bar"});
  TestGenerateUnusedFilename({"foo.bar/"}, "foo.bar", {"foo (1).bar"});
  TestGenerateUnusedFilename({"foo (1).bar"}, "foo.bar", {"foo.bar"});
  TestGenerateUnusedFilename({"foo.bar", "foo (1).bar"}, "foo.bar",
                             {"foo (2).bar"});
  TestGenerateUnusedFilename({"foo.bar", "foo (1).bar/"}, "foo.bar",
                             {"foo (2).bar"});
  TestGenerateUnusedFilename({"foo.bar", "foo (2).bar"}, "foo.bar",
                             {"foo (1).bar"});
  TestGenerateUnusedFilename({"foo.bar/", "foo (1).bar"}, "foo (1).bar",
                             {"foo (2).bar"});
  TestGenerateUnusedFilename({"foo (3).bar"}, "foo (3).bar", {"foo (1).bar"});
  TestGenerateUnusedFilename({"foo (2).bar"}, "foo (1).bar", {"foo (1).bar"});
  TestGenerateUnusedFilename({"foo (2) (1).bar"}, "foo (2) (1).bar",
                             {"foo (2) (2).bar"});
  TestGenerateUnusedFilename({"foo (2) (2).bar"}, "foo (2) (2).bar",
                             {"foo (2) (1).bar"});
  TestGenerateUnusedFilename({}, " foo.bar", {" foo.bar"});
  TestGenerateUnusedFilename({" foo.bar"}, " foo.bar", {" foo (1).bar"});
  TestGenerateUnusedFilename({"foo.bar"}, " foo.bar", {" foo.bar"});
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameNewLine) {
  TestGenerateUnusedFilename({}, "new\nline.bar", {"new\nline.bar"});
  TestGenerateUnusedFilename({"new\nline.bar"}, "new\nline.bar",
                             {"new\nline (1).bar"});
  TestGenerateUnusedFilename({"new\nline.bar", "new\nline (1).bar"},
                             "new\nline.bar", {"new\nline (2).bar"});
  TestGenerateUnusedFilename({}, "new\nline (\n)", {"new\nline (\n)"});
  TestGenerateUnusedFilename({"new\nline (\n)"}, "new\nline (\n)",
                             {"new\nline (\n) (1)"});
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameUnicode) {
  TestGenerateUnusedFilename({}, "é è ê ô œ.txt€", {"é è ê ô œ.txt€"});
  TestGenerateUnusedFilename({"é è ê ô œ.txt€"}, "é è ê ô œ.txt€",
                             {"é è ê ô œ (1).txt€"});
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameNoExtension) {
  TestGenerateUnusedFilename({}, "no-ext", {"no-ext"});
  TestGenerateUnusedFilename({"no-ext"}, "no-ext", {"no-ext (1)"});
  TestGenerateUnusedFilename({"no-ext/"}, "no-ext", {"no-ext (1)"});
  TestGenerateUnusedFilename({"no-ext (1)"}, "no-ext (1)", {"no-ext (2)"});

  TestGenerateUnusedFilename({}, "a", {"a"});
  TestGenerateUnusedFilename({"a"}, "a", {"a (1)"});
  TestGenerateUnusedFilename({"a/"}, "a", {"a (1)"});
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameDoubleExtension) {
  TestGenerateUnusedFilename({}, "double.ext.10.13.txt",
                             {"double.ext.10.13.txt"});
  TestGenerateUnusedFilename({"double.ext.10.13.txt"}, "double.ext.10.13.txt",
                             {"double.ext.10.13 (1).txt"});
  TestGenerateUnusedFilename({"double.ext.10.13.txt/"}, "double.ext.10.13.txt",
                             {"double.ext.10.13 (1).txt"});

  TestGenerateUnusedFilename({}, "archive.tar.gz", {"archive.tar.gz"});
  TestGenerateUnusedFilename({"archive.tar.gz"}, "archive.tar.gz",
                             {"archive (1).tar.gz"});
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameInvalidFilename) {
  TestGenerateUnusedFilename(
      {}, "", base::unexpected(base::File::FILE_ERROR_INVALID_OPERATION));
  TestGenerateUnusedFilename(
      {}, "path/with/slashes",
      base::unexpected(base::File::FILE_ERROR_INVALID_OPERATION));
}

TEST_F(FileManagerFileAPIUtilTest, GenerateUnusedFilenameFileSystemProvider) {
  Profile* const profile = GetProfile();
  const std::string extension_id = "abc";

  // Create and mount the FileSystemProvider.
  auto fake_provider =
      ash::file_system_provider::FakeExtensionProvider::Create(extension_id);
  const auto kProviderId = fake_provider->GetId();
  auto* service = ash::file_system_provider::Service::Get(profile);
  service->RegisterProvider(std::move(fake_provider));
  const base::File::Error result = service->MountFileSystem(
      kProviderId, ash::file_system_provider::MountOptions(file_system_id_,
                                                           "Test FileSystem"));
  ASSERT_EQ(base::File::FILE_OK, result);

  auto* provided_file_system =
      static_cast<ash::file_system_provider::FakeProvidedFileSystem*>(
          service->GetProvidedFileSystem(kProviderId, file_system_id_));
  ASSERT_TRUE(provided_file_system);
  const base::FilePath mount_point_name =
      provided_file_system->GetFileSystemInfo().mount_path().BaseName();

  const std::string origin = "chrome-extension://abc/";
  storage::FileSystemContext* const context =
      GetFileSystemContextForSourceURL(profile, GURL(origin));
  ASSERT_TRUE(context);

  // Make sure we can access the filesystem from the above origin.
  ash::FileSystemBackend::Get(*context)->GrantFileAccessToOrigin(
      url::Origin::Create(GURL(origin)), base::FilePath(mount_point_name));

  const storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  auto destination_folder_url = mount_points->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFromStringForTesting(origin),
      storage::kFileSystemTypeExternal, mount_point_name);
  auto expected_url = mount_points->CreateCrackedFileSystemURL(
      blink::StorageKey::CreateFromStringForTesting(origin),
      storage::kFileSystemTypeExternal,
      mount_point_name.Append("hello (1).txt"));

  base::RunLoop run_loop;
  GenerateUnusedFilename(
      destination_folder_url,
      base::FilePath(ash::file_system_provider::kFakeFilePath).BaseName(),
      context,
      base::BindLambdaForTesting(
          [&](base::FileErrorOr<storage::FileSystemURL> result) {
            EXPECT_TRUE(result.has_value())
                << "Unexpected error " << result.error();
            EXPECT_EQ(expected_url.ToGURL(), result->ToGURL());
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace
}  // namespace util
}  // namespace file_manager
