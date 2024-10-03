// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/content_uri_utils.h"

#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/android/content_uri_test_utils.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ContentUriUtilsTest, Test) {
  // Get the test image path.
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.AppendASCII("file_util");
  ASSERT_TRUE(PathExists(data_dir));
  FilePath image_file = data_dir.Append(FILE_PATH_LITERAL("red.png"));
  File::Info info;
  ASSERT_TRUE(GetFileInfo(image_file, &info));
  int image_size = info.size;

  // Insert the image into MediaStore. MediaStore will do some conversions, and
  // return the content URI.
  FilePath path = InsertImageIntoMediaStore(image_file);
  EXPECT_TRUE(path.IsContentUri());
  EXPECT_TRUE(PathExists(path));

  // Validate GetContentUriMimeType().
  std::string mime = GetContentUriMimeType(path);
  EXPECT_EQ(mime, std::string("image/png"));

  // Validate GetFileInfo() for content-URI.
  EXPECT_TRUE(GetFileInfo(path, &info));
  EXPECT_EQ(info.size, image_size);

  FilePath invalid_path("content://foo.bar");
  mime = GetContentUriMimeType(invalid_path);
  EXPECT_TRUE(mime.empty());
  EXPECT_FALSE(GetFileInfo(invalid_path, &info));
}

TEST(ContentUriUtilsTest, TranslateOpenFlagsToJavaMode) {
  constexpr auto kTranslations = MakeFixedFlatMap<uint32_t, std::string>({
      {File::FLAG_OPEN | File::FLAG_READ, "r"},
      {File::FLAG_OPEN_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rw"},
      {File::FLAG_OPEN_ALWAYS | File::FLAG_APPEND, "wa"},
      {File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rwt"},
      {File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE, "wt"},
  });

  for (const auto open_or_create : std::vector<uint32_t>(
           {0u, File::FLAG_OPEN, File::FLAG_CREATE, File::FLAG_OPEN_ALWAYS,
            File::FLAG_CREATE_ALWAYS, File::FLAG_OPEN_TRUNCATED})) {
    for (const auto read_write_append : std::vector<uint32_t>(
             {0u, File::FLAG_READ, File::FLAG_WRITE, File::FLAG_APPEND,
              File::FLAG_READ | File::FLAG_WRITE})) {
      for (const auto other : std::vector<uint32_t>(
               {0u, File::FLAG_DELETE_ON_CLOSE, File::FLAG_TERMINAL_DEVICE})) {
        uint32_t open_flags = open_or_create | read_write_append | other;
        auto mode = internal::TranslateOpenFlagsToJavaMode(open_flags);
        auto it = kTranslations.find(open_flags);
        if (it != kTranslations.end()) {
          EXPECT_TRUE(mode.has_value()) << "flag=0x" << std::hex << open_flags;
          EXPECT_EQ(mode.value(), it->second)
              << "flag=0x" << std::hex << open_flags;
        } else {
          EXPECT_FALSE(mode.has_value()) << "flag=0x" << std::hex << open_flags;
        }
      }
    }
  }
}

TEST(ContentUriUtilsTest, GetFileInfo) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath file = temp_dir.GetPath().Append("testfile");
  FilePath dir = temp_dir.GetPath().Append("testdir");
  FilePath not_exists = temp_dir.GetPath().Append("not-exists");
  ASSERT_TRUE(WriteFile(file, "123"));
  ASSERT_TRUE(CreateDirectory(dir));

  FilePath content_uri_file =
      *test::android::GetContentUriFromCacheDirFilePath(file);
  FilePath content_uri_dir =
      *test::android::GetContentUriFromCacheDirFilePath(dir);
  FilePath content_uri_not_exists =
      *test::android::GetContentUriFromCacheDirFilePath(not_exists);

  EXPECT_TRUE(PathExists(content_uri_file));
  EXPECT_TRUE(PathExists(content_uri_dir));
  EXPECT_FALSE(PathExists(content_uri_not_exists));

  File::Info info;
  EXPECT_TRUE(GetFileInfo(file, &info));
  File::Info content_uri_info;
  EXPECT_TRUE(GetFileInfo(content_uri_file, &content_uri_info));
  EXPECT_EQ(content_uri_info.size, 3);
  EXPECT_FALSE(content_uri_info.is_directory);
  EXPECT_EQ(content_uri_info.last_modified, info.last_modified);

  EXPECT_TRUE(GetFileInfo(dir, &info));
  EXPECT_TRUE(GetFileInfo(content_uri_dir, &content_uri_info));
  EXPECT_TRUE(content_uri_info.is_directory);
  EXPECT_EQ(content_uri_info.last_modified, info.last_modified);

  EXPECT_FALSE(GetFileInfo(not_exists, &info));
  EXPECT_FALSE(GetFileInfo(content_uri_not_exists, &info));
}

TEST(ContentUriUtilsTest, ContentUriBuildDocumentUriUsingTree) {
  base::FilePath tree_uri("content://authority/tree/foo");
  // The encoded_document_id will be encoded if it has any special chars.
  EXPECT_EQ(ContentUriBuildDocumentUriUsingTree(tree_uri, "doc:bar").value(),
            "content://authority/tree/foo/document/doc%3Abar");

  // `%` should not get encoded again to `%25` when it is a valid encoding, but
  // chars are upper-cased.
  EXPECT_EQ(ContentUriBuildDocumentUriUsingTree(tree_uri, "doc%3Abar").value(),
            "content://authority/tree/foo/document/doc%3Abar");
  EXPECT_EQ(ContentUriBuildDocumentUriUsingTree(tree_uri, "doc%3abar").value(),
            "content://authority/tree/foo/document/doc%3Abar");

  // Strange stuff happens if the encoding is invalid.
  EXPECT_EQ(ContentUriBuildDocumentUriUsingTree(tree_uri, "doc%").value(),
            "content://authority/tree/foo/document/doc%EF%BF%BD");
  EXPECT_EQ(ContentUriBuildDocumentUriUsingTree(tree_uri, "doc%3").value(),
            "content://authority/tree/foo/document/doc%EF%BF%BD");
  EXPECT_EQ(ContentUriBuildDocumentUriUsingTree(tree_uri, "doc%xy").value(),
            "content://authority/tree/foo/document/doc%EF%BF%BD%00y");
}

}  // namespace base
