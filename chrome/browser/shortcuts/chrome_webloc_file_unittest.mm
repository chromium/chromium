// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/chrome_webloc_file.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shortcuts {

class ChromeWeblocFileTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

 protected:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ChromeWeblocFileTest, LoadFromFile_ValidFile) {
  const base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("test.crwebloc");
  // This test intentionally hardcodes a valid file rather than using
  // MacShortcutFile or NSDictionary to write a file to disk. This way we make
  // sure that even if the file format changes (intentionally or not) we can
  // still read files that were written in this format.
  ASSERT_TRUE(
      base::WriteFile(file_path, R"xml(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>CrProfile</key>
        <string>DefaultName</string>
        <key>URL</key>
        <string>https://www.example.com:123/foo/bar#bla</string>
</dict>
</plist>
)xml"));

  std::optional<ChromeWeblocFile> shortcut =
      ChromeWeblocFile::LoadFromFile(file_path);
  ASSERT_TRUE(shortcut.has_value());

  EXPECT_EQ(GURL("https://www.example.com:123/foo/bar#bla"),
            shortcut->target_url());
  EXPECT_EQ(base::FilePath("DefaultName"),
            shortcut->profile_path_name().path());

  // Verify that the shortcut matchers work correctly as well.
  EXPECT_THAT(file_path, IsShortcutForUrl(
                             GURL("https://www.example.com:123/foo/bar#bla")));
  EXPECT_THAT(file_path,
              IsShortcutForProfile(FILE_PATH_LITERAL("DefaultName")));
  EXPECT_THAT(file_path, IsShortcutWithTitle(u"test"));
}

TEST_F(ChromeWeblocFileTest, LoadFromFile_FileDoesNotExist) {
  EXPECT_FALSE(
      ChromeWeblocFile::LoadFromFile(
          temp_dir_.GetPath().AppendASCII("file_does_not_exist.crwebloc"))
          .has_value());
}

TEST_F(ChromeWeblocFileTest, LoadFromFile_NotAPList) {
  const base::FilePath not_a_plist_path =
      temp_dir_.GetPath().AppendASCII("not_a_plist.crwebloc");
  ASSERT_TRUE(base::WriteFile(not_a_plist_path, "Hello world"));
  EXPECT_FALSE(ChromeWeblocFile::LoadFromFile(not_a_plist_path).has_value());
}

TEST_F(ChromeWeblocFileTest, LoadFromFile_MissingUrl) {
  const base::FilePath missing_url_path =
      temp_dir_.GetPath().AppendASCII("missing_url.crwebloc");
  ASSERT_TRUE([@{@"CrProfile" : @"DefaultName"}
      writeToURL:base::apple::FilePathToNSURL(missing_url_path)
           error:nil]);
  EXPECT_FALSE(ChromeWeblocFile::LoadFromFile(missing_url_path).has_value());
}

TEST_F(ChromeWeblocFileTest, LoadFromFile_MissingProfile) {
  const base::FilePath missing_profile_path =
      temp_dir_.GetPath().AppendASCII("missing_profile.crwebloc");
  ASSERT_TRUE([@{@"URL" : @"https://www.example.com/"}
      writeToURL:base::apple::FilePathToNSURL(missing_profile_path)
           error:nil]);
  EXPECT_FALSE(
      ChromeWeblocFile::LoadFromFile(missing_profile_path).has_value());
}

TEST_F(ChromeWeblocFileTest, LoadFromFile_InvalidUrl) {
  const base::FilePath invalid_url_path =
      temp_dir_.GetPath().AppendASCII("invalid_url.crwebloc");
  ASSERT_TRUE(([@{@"URL" : @"not-a-url", @"CrProfile" : @"DefaultName"}
      writeToURL:base::apple::FilePathToNSURL(invalid_url_path)
           error:nil]));
  EXPECT_FALSE(ChromeWeblocFile::LoadFromFile(invalid_url_path).has_value());
}

TEST_F(ChromeWeblocFileTest, SaveToFile) {
  const GURL url("https://www.example.com:123/foo/bar#bla");

  const base::FilePath file_path =
      temp_dir_.GetPath().AppendASCII("test.crwebloc");
  const base::SafeBaseName profile_path_name =
      *base::SafeBaseName::Create(FILE_PATH_LITERAL("Test Profile"));
  ChromeWeblocFile(url, profile_path_name).SaveToFile(file_path);

  std::optional<ChromeWeblocFile> shortcut =
      ChromeWeblocFile::LoadFromFile(file_path);
  ASSERT_TRUE(shortcut.has_value());

  EXPECT_EQ(url, shortcut->target_url());
  EXPECT_EQ(profile_path_name, shortcut->profile_path_name());
}

}  // namespace shortcuts
