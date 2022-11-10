// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/external_file_url_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/drive/file_system_core_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash {

namespace {

// Sets up ProfileManager for testing and marks the current thread as UI by
// BrowserTaskEnvironment. We need the thread since Profile objects must be
// touched from UI and hence has CHECK/DCHECKs for it.
class ExternalFileURLUtilTest : public testing::Test {
 protected:
  ExternalFileURLUtilTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(testing_profile_manager_.SetUp()); }

  TestingProfileManager& testing_profile_manager() {
    return testing_profile_manager_;
  }

  storage::FileSystemURL CreateExpectedURL(const base::FilePath& path) {
    return storage::FileSystemURL::CreateForTest(
        blink::StorageKey::CreateFromStringForTesting("chrome-extension://xxx"),
        storage::kFileSystemTypeExternal,
        base::FilePath("arc-documents-provider").Append(path), "",
        storage::kFileSystemTypeArcDocumentsProvider, base::FilePath(), "",
        storage::FileSystemMountOption());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

}  // namespace

TEST_F(ExternalFileURLUtilTest, FilePathToExternalFileURL) {
  storage::FileSystemURL url;

  // Path with alphabets and numbers.
  url = CreateExpectedURL(base::FilePath("foo/bar012.txt"));
  EXPECT_EQ(url.virtual_path(),
            ExternalFileURLToVirtualPath(FileSystemURLToExternalFileURL(url)));

  // Path with symbols.
  url = CreateExpectedURL(base::FilePath(" !\"#$%&'()*+,-.:;<=>?@[\\]^_`{|}~"));
  EXPECT_EQ(url.virtual_path(),
            ExternalFileURLToVirtualPath(FileSystemURLToExternalFileURL(url)));

  // Path with '%'.
  url = CreateExpectedURL(base::FilePath("%19%20%21.txt"));
  EXPECT_EQ(url.virtual_path(),
            ExternalFileURLToVirtualPath(FileSystemURLToExternalFileURL(url)));

  // Path with multi byte characters.
  std::u16string utf16_string;
  utf16_string.push_back(0x307b);  // HIRAGANA_LETTER_HO
  utf16_string.push_back(0x3052);  // HIRAGANA_LETTER_GE
  url = CreateExpectedURL(
      base::FilePath::FromUTF8Unsafe(base::UTF16ToUTF8(utf16_string) + ".txt"));
  EXPECT_EQ(url.virtual_path().AsUTF8Unsafe(),
            ExternalFileURLToVirtualPath(FileSystemURLToExternalFileURL(url))
                .AsUTF8Unsafe());
}

// Tests that given virtual path is encoded to an expected externalfile: URL
// and then the original path is reconstructed from it.
void ExpectVirtualPathRoundtrip(
    const base::FilePath::StringType& virtual_path_string,
    std::string expected_url) {
  base::FilePath virtual_path(virtual_path_string);
  GURL result = VirtualPathToExternalFileURL(virtual_path);
  EXPECT_TRUE(result.is_valid());
  EXPECT_EQ(content::kExternalFileScheme, result.scheme());
  EXPECT_EQ(expected_url, result.path());
  EXPECT_EQ(virtual_path.value(), ExternalFileURLToVirtualPath(result).value());
}

TEST_F(ExternalFileURLUtilTest, VirtualPathToExternalFileURL) {
  ExpectVirtualPathRoundtrip(FILE_PATH_LITERAL("foo/bar012.txt"),
                             "foo/bar012.txt");

  // Path containing precent character, which is also used for URL encoding.
  ExpectVirtualPathRoundtrip(FILE_PATH_LITERAL("foo/bar012%41%.txt"),
                             "foo/bar012%2541%25.txt");

  // Path containing some ASCII characters that are escaped by URL enconding.
  ExpectVirtualPathRoundtrip(FILE_PATH_LITERAL("foo/bar \"#<>?`{}.txt"),
                             "foo/bar%20%22%23%3C%3E%3F%60%7B%7D.txt");

  // (U+3000) IDEOGRAPHIC SPACE and (U+1F512) LOCK are examples of characters
  // potentially used for URL spoofing. Those are blocklisted from unescaping
  // when a URL is displayed, but this should not prevent it from being
  // unescaped when converting a URL to a virtual file path. See
  // crbug.com/585422 for detail.
  ExpectVirtualPathRoundtrip(FILE_PATH_LITERAL("foo/bar/space\u3000lock🔒.zip"),
                             "foo/bar/space%E3%80%80lock%F0%9F%94%92.zip");
}

}  // namespace ash
