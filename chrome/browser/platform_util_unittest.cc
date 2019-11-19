// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/platform_util_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/common/file_system/file_system_types.h"
#else
#include "content/public/test/browser_task_environment.h"
#endif

namespace platform_util {

namespace {

#if defined(OS_CHROMEOS)

// ChromeContentBrowserClient subclass that sets up a custom file system backend
// that allows the test to grant file access to the file manager extension ID
// without having to install the extension.
class PlatformUtilTestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  void GetAdditionalFileSystemBackends(
      content::BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) override {
    storage::ExternalMountPoints* external_mount_points =
        content::BrowserContext::GetMountPoints(browser_context);

    // New FileSystemBackend that uses our MockSpecialStoragePolicy.
    additional_backends->push_back(
        std::make_unique<chromeos::FileSystemBackend>(
            nullptr, nullptr, nullptr, nullptr, nullptr, external_mount_points,
            storage::ExternalMountPoints::GetSystemInstance()));
  }
};

// Base test fixture class to be used on Chrome OS.
class PlatformUtilTestBase : public BrowserWithTestWindowTest {
 protected:
  void SetUpPlatformFixture(const base::FilePath& test_directory) {
    content_browser_client_.reset(new PlatformUtilTestContentBrowserClient());
    old_content_browser_client_ =
        content::SetBrowserClientForTesting(content_browser_client_.get());

    // The test_directory needs to be mounted for it to be accessible.
    content::BrowserContext::GetMountPoints(GetProfile())
        ->RegisterFileSystem("test", storage::kFileSystemTypeNativeLocal,
                             storage::FileSystemMountOption(), test_directory);

    // To test opening a file, we are going to register a mock extension that
    // handles .txt files. The extension doesn't actually need to exist due to
    // the DisableShellOperationsForTesting() call which prevents the extension
    // from being invoked.
    std::string error;
    int error_code = 0;

    std::string json_manifest =
        "{"
        "  \"manifest_version\": 2,"
        "  \"name\": \"Test extension\","
        "  \"version\": \"0\","
        "  \"app\": { \"background\": { \"scripts\": [\"main.js\"] }},"
        "  \"file_handlers\": {"
        "    \"text\": {"
        "      \"extensions\": [ \"txt\" ],"
        "      \"title\": \"Text\""
        "      }"
        "    }"
        "}";
    JSONStringValueDeserializer json_string_deserializer(json_manifest);
    std::unique_ptr<base::Value> manifest =
        json_string_deserializer.Deserialize(&error_code, &error);
    base::DictionaryValue* manifest_dictionary;

    manifest->GetAsDictionary(&manifest_dictionary);
    ASSERT_TRUE(manifest_dictionary);

    scoped_refptr<extensions::Extension> extension =
        extensions::Extension::Create(
            test_directory.AppendASCII("invalid-extension"),
            extensions::Manifest::INVALID_LOCATION, *manifest_dictionary,
            extensions::Extension::NO_FLAGS, &error);
    ASSERT_TRUE(error.empty()) << error;
    extensions::ExtensionRegistry::Get(GetProfile())->AddEnabled(extension);
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    content::ContentBrowserClient* content_browser_client =
        content::SetBrowserClientForTesting(old_content_browser_client_);
    old_content_browser_client_ = nullptr;
    DCHECK_EQ(static_cast<content::ContentBrowserClient*>(
                  content_browser_client_.get()),
              content_browser_client)
        << "ContentBrowserClient changed during test.";
    BrowserWithTestWindowTest::TearDown();
  }

 private:
  std::unique_ptr<content::ContentBrowserClient> content_browser_client_;
  content::ContentBrowserClient* old_content_browser_client_ = nullptr;
};

#else

// Test fixture used by all desktop platforms other than Chrome OS.
class PlatformUtilTestBase : public testing::Test {
 protected:
  Profile* GetProfile() { return nullptr; }
  void SetUpPlatformFixture(const base::FilePath&) {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

#endif

class PlatformUtilTest : public PlatformUtilTestBase {
 public:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(PlatformUtilTestBase::SetUp());

    static const char kTestFileData[] = "Cow says moo!";
    const int kTestFileDataLength = base::size(kTestFileData) - 1;

    // This prevents platform_util from invoking any shell or external APIs
    // during tests. Doing so may result in external applications being launched
    // and intefering with tests.
    internal::DisableShellOperationsForTesting();

    ASSERT_TRUE(directory_.CreateUniqueTempDir());

    // A valid file.
    existing_file_ = directory_.GetPath().AppendASCII("test_file.txt");
    ASSERT_EQ(
        kTestFileDataLength,
        base::WriteFile(existing_file_, kTestFileData, kTestFileDataLength));

    // A valid folder.
    existing_folder_ = directory_.GetPath().AppendASCII("test_folder");
    ASSERT_TRUE(base::CreateDirectory(existing_folder_));

    // A non-existent path.
    nowhere_ = directory_.GetPath().AppendASCII("nowhere");

    SetUpPlatformFixture(directory_.GetPath());
  }

  OpenOperationResult CallOpenItem(const base::FilePath& path,
                                   OpenItemType item_type) {
    base::RunLoop run_loop;
    OpenOperationResult result = OPEN_SUCCEEDED;
    OpenOperationCallback callback =
        base::Bind(&OnOpenOperationDone, run_loop.QuitClosure(), &result);
    OpenItem(GetProfile(), path, item_type, callback);
    run_loop.Run();
    return result;
  }

  base::FilePath existing_file_;
  base::FilePath existing_folder_;
  base::FilePath nowhere_;

 protected:
  base::ScopedTempDir directory_;

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  static void OnOpenOperationDone(const base::Closure& closure,
                                  OpenOperationResult* store_result,
                                  OpenOperationResult result) {
    *store_result = result;
    closure.Run();
  }
};

}  // namespace

TEST_F(PlatformUtilTest, OpenFile) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(existing_file_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(existing_folder_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND, CallOpenItem(nowhere_, OPEN_FILE));
}

TEST_F(PlatformUtilTest, OpenFolder) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(existing_folder_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(existing_file_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND, CallOpenItem(nowhere_, OPEN_FOLDER));
}

#if defined(OS_POSIX)
// Symbolic links are currently only supported on Posix. Windows technically
// supports it as well, but not on Windows XP.
class PlatformUtilPosixTest : public PlatformUtilTest {
 public:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(PlatformUtilTest::SetUp());

    symlink_to_file_ = directory_.GetPath().AppendASCII("l_file.txt");
    ASSERT_TRUE(base::CreateSymbolicLink(existing_file_, symlink_to_file_));
    symlink_to_folder_ = directory_.GetPath().AppendASCII("l_folder");
    ASSERT_TRUE(base::CreateSymbolicLink(existing_folder_, symlink_to_folder_));
    symlink_to_nowhere_ = directory_.GetPath().AppendASCII("l_nowhere");
    ASSERT_TRUE(base::CreateSymbolicLink(nowhere_, symlink_to_nowhere_));
  }

 protected:
  base::FilePath symlink_to_file_;
  base::FilePath symlink_to_folder_;
  base::FilePath symlink_to_nowhere_;
};
#endif  // OS_POSIX

#if defined(OS_CHROMEOS)
// ChromeOS doesn't follow symbolic links in sandboxed filesystems. So all the
// symbolic link tests should return PATH_NOT_FOUND.

TEST_F(PlatformUtilPosixTest, OpenFileWithPosixSymlinksChromeOS) {
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_file_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_folder_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_nowhere_, OPEN_FILE));
}

TEST_F(PlatformUtilPosixTest, OpenFolderWithPosixSymlinksChromeOS) {
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_folder_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_file_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_nowhere_, OPEN_FOLDER));
}

TEST_F(PlatformUtilTest, OpenFileWithUnhandledFileType) {
  base::FilePath unhandled_file =
      directory_.GetPath().AppendASCII("myfile.filetype");
  ASSERT_EQ(3, base::WriteFile(unhandled_file, "cat", 3));
  EXPECT_EQ(OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE,
            CallOpenItem(unhandled_file, OPEN_FILE));
}
#endif  // OS_CHROMEOS

#if defined(OS_POSIX) && !defined(OS_CHROMEOS)
// On all other Posix platforms, the symbolic link tests should work as
// expected.

TEST_F(PlatformUtilPosixTest, OpenFileWithPosixSymlinks) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(symlink_to_file_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(symlink_to_folder_, OPEN_FILE));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_nowhere_, OPEN_FILE));
}

TEST_F(PlatformUtilPosixTest, OpenFolderWithPosixSymlinks) {
  EXPECT_EQ(OPEN_SUCCEEDED, CallOpenItem(symlink_to_folder_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_INVALID_TYPE,
            CallOpenItem(symlink_to_file_, OPEN_FOLDER));
  EXPECT_EQ(OPEN_FAILED_PATH_NOT_FOUND,
            CallOpenItem(symlink_to_nowhere_, OPEN_FOLDER));
}
#endif  // OS_POSIX && !OS_CHROMEOS

}  // namespace platform_util
