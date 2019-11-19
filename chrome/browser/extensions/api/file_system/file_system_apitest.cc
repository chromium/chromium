// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

// TODO(michaelpg): Port these tests to app_shell: crbug.com/505926.

namespace content {
class BrowserContext;
}

namespace extensions {

namespace {

class AppLoadObserver : public ExtensionRegistryObserver {
 public:
  AppLoadObserver(content::BrowserContext* browser_context,
                  base::Callback<void(const Extension*)> callback)
      : callback_(callback) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context));
  }

  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    callback_.Run(extension);
  }

 private:
  base::Callback<void(const Extension*)> callback_;
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};
  DISALLOW_COPY_AND_ASSIGN(AppLoadObserver);
};

void SetLastChooseEntryDirectory(const base::FilePath& choose_entry_directory,
                                 ExtensionPrefs* prefs,
                                 const Extension* extension) {
  file_system_api::SetLastChooseEntryDirectory(
      prefs, extension->id(), choose_entry_directory);
}

void AddSavedEntry(const base::FilePath& path_to_save,
                   bool is_directory,
                   apps::SavedFilesService* service,
                   const Extension* extension) {
  service->RegisterFileEntry(
      extension->id(), "magic id", path_to_save, is_directory);
}

const int kGraylistedPath = base::DIR_HOME;

}  // namespace

class FileSystemApiTest : public PlatformAppBrowserTest {
 public:
  FileSystemApiTest() {
    set_open_about_blank_on_browser_launch(false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    test_root_folder_ = test_data_dir_.AppendASCII("api_test")
        .AppendASCII("file_system");
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_root", test_root_folder_);
  }

  void TearDown() override {
    FileSystemChooseEntryFunction::StopSkippingPickerForTest();
    PlatformAppBrowserTest::TearDown();
  }

 protected:
  base::FilePath TempFilePath(const std::string& destination_name,
                              bool copy_gold) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "CreateUniqueTempDir failed";
      return base::FilePath();
    }
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_temp", temp_dir_.GetPath());

    base::FilePath destination =
        temp_dir_.GetPath().AppendASCII(destination_name);
    if (copy_gold) {
      base::FilePath source = test_root_folder_.AppendASCII("gold.txt");
      EXPECT_TRUE(base::CopyFile(source, destination));
    }
    return destination;
  }

  std::vector<base::FilePath> TempFilePaths(
      const std::vector<std::string>& destination_names,
      bool copy_gold) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "CreateUniqueTempDir failed";
      return std::vector<base::FilePath>();
    }
    FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
        "test_temp", temp_dir_.GetPath());

    std::vector<base::FilePath> result;
    for (auto it = destination_names.cbegin(); it != destination_names.cend();
         ++it) {
      base::FilePath destination = temp_dir_.GetPath().AppendASCII(*it);
      if (copy_gold) {
        base::FilePath source = test_root_folder_.AppendASCII("gold.txt");
        EXPECT_TRUE(base::CopyFile(source, destination));
      }
      result.push_back(destination);
    }
    return result;
  }

  void CheckStoredDirectoryMatches(const base::FilePath& filename) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const Extension* extension = GetSingleLoadedExtension();
    ASSERT_TRUE(extension);
    std::string extension_id = extension->id();
    ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
    base::FilePath stored_value =
        file_system_api::GetLastChooseEntryDirectory(prefs, extension_id);
    if (filename.empty()) {
      EXPECT_TRUE(stored_value.empty());
    } else {
      EXPECT_EQ(base::MakeAbsoluteFilePath(filename.DirName()),
                base::MakeAbsoluteFilePath(stored_value));
    }
  }

  base::FilePath test_root_folder_;
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetDisplayPath) {
  base::FilePath test_file = test_root_folder_.AppendASCII("gold.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/get_display_path"))
      << message_;
}

#if defined(OS_WIN) || defined(OS_POSIX)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetDisplayPathPrettify) {
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        base::DIR_HOME, test_root_folder_, false, false));
  }

  base::FilePath test_file = test_root_folder_.AppendASCII("gold.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_display_path_prettify")) << message_;
}
#endif

#if defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiGetDisplayPathPrettifyMac) {
  base::FilePath test_file;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // On Mac, "test.localized" will be localized into just "test".
    base::FilePath test_path = TempFilePath("test.localized", false);
    ASSERT_TRUE(base::CreateDirectory(test_path));

    test_file = test_path.AppendASCII("gold.txt");
    base::FilePath source = test_root_folder_.AppendASCII("gold.txt");
    EXPECT_TRUE(base::CopyFile(source, test_file));
  }

  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_display_path_prettify_mac")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenExistingFileTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenExistingFileUsingPreviousPathTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::
      SkipPickerAndSelectSuggestedPathForTest();
  {
    AppLoadObserver observer(profile(),
                             base::Bind(SetLastChooseEntryDirectory,
                                        test_file.DirName(),
                                        ExtensionPrefs::Get(profile())));
    ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
        << message_;
  }
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenExistingFilePreviousPathDoesNotExistTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_USER_DOCUMENTS, test_file.DirName(), false, false));
  }
  FileSystemChooseEntryFunction::
      SkipPickerAndSelectSuggestedPathForTest();
  {
    AppLoadObserver observer(
        profile(),
        base::Bind(SetLastChooseEntryDirectory,
                   test_file.DirName().Append(base::FilePath::FromUTF8Unsafe(
                       "fake_directory_does_not_exist")),
                   ExtensionPrefs::Get(profile())));
    ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
        << message_;
  }
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenExistingFileDefaultPathTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_USER_DOCUMENTS, test_file.DirName(), false, false));
  }
  FileSystemChooseEntryFunction::
      SkipPickerAndSelectSuggestedPathForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenMultipleSuggested) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        chrome::DIR_USER_DOCUMENTS, test_file.DirName(), false, false));
  }
  FileSystemChooseEntryFunction::SkipPickerAndSelectSuggestedPathForTest();
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_multiple_with_suggested_name"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenMultipleExistingFilesTest) {
  std::vector<std::string> names;
  names.push_back("open_existing1.txt");
  names.push_back("open_existing2.txt");
  std::vector<base::FilePath> test_files = TempFilePaths(names, true);
  ASSERT_EQ(2u, test_files.size());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_multiple_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenDirectoryTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryWithWriteTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(
      RunPlatformAppTest("api_test/file_system/open_directory_with_write"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryWithoutPermissionTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_without_permission"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryWithOnlyWritePermissionTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_directory_with_only_write"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

#if defined(OS_WIN) || defined(OS_POSIX)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryOnGraylistAndAllowTest) {
  FileSystemChooseEntryFunction::SkipDirectoryConfirmationForTest();
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        kGraylistedPath, test_directory, false, false));
  }
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryOnGraylistTest) {
  FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest();
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        kGraylistedPath, test_directory, false, false));
  }
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory_cancel"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectoryContainingGraylistTest) {
  FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest();
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  base::FilePath parent_directory = test_directory.DirName();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        kGraylistedPath, test_directory, false, false));
  }
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &parent_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory_cancel"))
      << message_;
  CheckStoredDirectoryMatches(test_directory);
}

// Test that choosing a subdirectory of a path does not require confirmation.
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenDirectorySubdirectoryOfGraylistTest) {
  // If a dialog is erroneously displayed, auto cancel it, so that the test
  // fails.
  FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest();
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  base::FilePath parent_directory = test_directory.DirName();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathService::OverrideAndCreateIfNeeded(
        kGraylistedPath, parent_directory, false, false));
  }
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_directory"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}
#endif  // defined(OS_WIN) || defined(OS_POSIX)

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiInvalidChooseEntryTypeTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/invalid_choose_file_type")) << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_FileSystemApiOpenExistingFileWithWriteTest DISABLED_FileSystemApiOpenExistingFileWithWriteTest
#else
#define MAYBE_FileSystemApiOpenExistingFileWithWriteTest FileSystemApiOpenExistingFileWithWriteTest
#endif
IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    MAYBE_FileSystemApiOpenExistingFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_existing_with_write")) << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenWritableExistingFileTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_writable_existing")) << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenWritableExistingFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_writable_existing_with_write")) << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiOpenMultipleWritableExistingFilesTest) {
  std::vector<std::string> names;
  names.push_back("open_existing1.txt");
  names.push_back("open_existing2.txt");
  std::vector<base::FilePath> test_files = TempFilePaths(names, true);
  ASSERT_EQ(2u, test_files.size());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_multiple_writable_existing_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenCancelTest) {
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_cancel"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenBackgroundTest) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveNewFileTest) {
  base::FilePath test_file = TempFilePath("save_new.txt", false);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveExistingFileTest) {
  base::FilePath test_file = TempFilePath("save_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_existing"))
      << message_;
  CheckStoredDirectoryMatches(base::FilePath());
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiSaveNewFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("save_new.txt", false);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new_with_write"))
      << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiSaveExistingFileWithWriteTest) {
  base::FilePath test_file = TempFilePath("save_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/save_existing_with_write")) << message_;
  CheckStoredDirectoryMatches(test_file);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveMultipleFilesTest) {
  std::vector<std::string> names;
  names.push_back("save1.txt");
  names.push_back("save2.txt");
  std::vector<base::FilePath> test_files = TempFilePaths(names, false);
  ASSERT_EQ(2u, test_files.size());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
      &test_files);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_multiple"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveCancelTest) {
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_cancel"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveBackgroundTest) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetWritableTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_file_entry")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiGetWritableWithWriteTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_file_entry_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiGetWritableRootEntryTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_root_entry")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiIsWritableTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/is_writable_file_entry"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
                       FileSystemApiIsWritableWithWritePermissionTest) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/is_writable_file_entry_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRetainEntry) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/retain_entry")) << message_;
  std::vector<SavedFileEntry> file_entries =
      apps::SavedFilesService::Get(profile())->GetAllFileEntries(
          GetSingleLoadedExtension()->id());
  ASSERT_EQ(1u, file_entries.size());
  EXPECT_EQ(test_file, file_entries[0].path);
  EXPECT_EQ(1, file_entries[0].sequence_number);
  EXPECT_FALSE(file_entries[0].is_directory);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRetainDirectoryEntry) {
  base::FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_directory);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/retain_directory"))
      << message_;
  std::vector<SavedFileEntry> file_entries =
      apps::SavedFilesService::Get(profile())->GetAllFileEntries(
          GetSingleLoadedExtension()->id());
  ASSERT_EQ(1u, file_entries.size());
  EXPECT_EQ(test_directory, file_entries[0].path);
  EXPECT_EQ(1, file_entries[0].sequence_number);
  EXPECT_TRUE(file_entries[0].is_directory);
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRestoreEntry) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  AppLoadObserver observer(profile(),
                           base::Bind(AddSavedEntry,
                                      test_file,
                                      false,
                                      apps::SavedFilesService::Get(profile())));
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/restore_entry"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiRestoreDirectoryEntry) {
  base::FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  base::FilePath test_directory = test_file.DirName();
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  AppLoadObserver observer(profile(),
                           base::Bind(AddSavedEntry,
                                      test_directory,
                                      true,
                                      apps::SavedFilesService::Get(profile())));
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/restore_directory"))
      << message_;
}

#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(FileSystemApiTest, RequestFileSystem_NotChromeOS) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "api_test/file_system/request_file_system_not_chromeos",
      kFlagIgnoreManifestWarnings))
      << message_;
}
#endif

}  // namespace extensions
