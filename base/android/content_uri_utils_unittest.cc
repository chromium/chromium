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
#include "base/strings/stringprintf.h"
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

// A document that this app has not been granted access to has no MIME type.
// Regression test for crbug.com/588897368, where asking for it crashed.
TEST(ContentUriUtilsTest, GetContentUriMimeTypeOfUnreadableDocument) {
  FilePath path(
      "content://com.android.externalstorage.documents/document/"
      "primary%3ADownload%2Fno-such-file.png");
  EXPECT_TRUE(GetContentUriMimeType(path).empty());
}

TEST(ContentUriUtilsTest, TranslateOpenFlagsToJavaMode) {
  constexpr auto kTranslations = MakeFixedFlatMap<uint32_t, std::string>({
      {File::FLAG_OPEN | File::FLAG_READ, "r"},
      {File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE, "wt"},
      {File::FLAG_CREATE_ALWAYS | File::FLAG_READ | File::FLAG_WRITE, "rwt"},
  });

  for (const auto open_or_create : std::vector<uint32_t>(
           {0u, File::FLAG_OPEN, File::FLAG_CREATE, File::FLAG_OPEN_ALWAYS,
            File::FLAG_CREATE_ALWAYS, File::FLAG_OPEN_TRUNCATED,
            File::FLAG_OPEN | File::FLAG_CREATE_ALWAYS})) {
    for (const auto read_write_append : std::vector<uint32_t>(
             {0u, File::FLAG_READ, File::FLAG_WRITE, File::FLAG_APPEND,
              File::FLAG_READ | File::FLAG_WRITE,
              File::FLAG_READ | File::FLAG_APPEND,
              File::FLAG_APPEND | File::FLAG_WRITE,
              File::FLAG_READ | File::FLAG_WRITE | File::FLAG_APPEND})) {
      uint32_t core_flags = open_or_create | read_write_append;
      auto mode = internal::TranslateOpenFlagsToJavaMode(core_flags);
      auto it = kTranslations.find(core_flags);
      if (it != kTranslations.end()) {
        EXPECT_TRUE(mode.has_value()) << StringPrintf("flag=0x%x", core_flags);
        EXPECT_EQ(mode.value(), it->second)
            << StringPrintf("flag=0x%x", core_flags);
      } else {
        EXPECT_FALSE(mode.has_value()) << StringPrintf("flag=0x%x", core_flags);
      }

      static_assert(
          (File::FLAG_DELETE_ON_CLOSE | File::FLAG_TERMINAL_DEVICE |
           File::FLAG_NO_FOLLOW | File::FLAG_WRITE_ATTRIBUTES) ==
              internal::kUnhandledAndroidFileModifiers,
          "Keep this list in sync with kUnhandledAndroidFileModifiers.");
      for (const auto modifier : std::vector<uint32_t>(
               {File::FLAG_DELETE_ON_CLOSE, File::FLAG_TERMINAL_DEVICE,
                File::FLAG_NO_FOLLOW, File::FLAG_WRITE_ATTRIBUTES})) {
        EXPECT_FALSE(
            internal::TranslateOpenFlagsToJavaMode(core_flags | modifier))
            << StringPrintf("flag=0x%x", core_flags | modifier);
      }

      for (const auto unknown_bit :
           std::vector<uint32_t>({1u << 25, 1u << 30, 1u << 31})) {
        EXPECT_FALSE(
            internal::TranslateOpenFlagsToJavaMode(core_flags | unknown_bit))
            << StringPrintf("flag=0x%x", core_flags | unknown_bit);
      }

      if (it != kTranslations.end()) {
        for (const auto ignored_flag : std::vector<uint32_t>(
                 {File::FLAG_WIN_SHARE_DELETE, File::FLAG_WIN_EXCLUSIVE_WRITE,
                  File::FLAG_WIN_TEMPORARY, File::FLAG_WIN_HIDDEN,
                  File::FLAG_WIN_EXCLUSIVE_READ,
                  File::FLAG_WIN_BACKUP_SEMANTICS, File::FLAG_WIN_EXECUTE,
                  File::FLAG_WIN_SEQUENTIAL_SCAN, File::FLAG_WIN_NO_EXECUTE,
                  File::FLAG_CAN_DELETE_ON_CLOSE, File::FLAG_ASYNC})) {
          auto ignored_mode =
              internal::TranslateOpenFlagsToJavaMode(core_flags | ignored_flag);
          EXPECT_TRUE(ignored_mode.has_value())
              << StringPrintf("flag=0x%x", core_flags | ignored_flag);
          EXPECT_EQ(ignored_mode.value(), it->second)
              << StringPrintf("flag=0x%x", core_flags | ignored_flag);
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
  FilePath content_uri_document =
      *test::android::GetInMemoryContentDocumentUriFromCacheDirFilePath(file);
  FilePath content_uri_tree =
      *test::android::GetInMemoryContentTreeUriFromCacheDirDirectory(dir);
  FilePath content_uri_not_exists =
      *test::android::GetContentUriFromCacheDirFilePath(not_exists);

  EXPECT_TRUE(PathExists(content_uri_file));
  EXPECT_TRUE(PathExists(content_uri_dir));
  EXPECT_TRUE(PathExists(content_uri_document));
  EXPECT_TRUE(PathExists(content_uri_tree));
  EXPECT_FALSE(PathExists(content_uri_not_exists));

  File::Info info;
  EXPECT_TRUE(GetFileInfo(file, &info));
  File::Info content_uri_info;
  EXPECT_TRUE(GetFileInfo(content_uri_file, &content_uri_info));
  EXPECT_EQ(content_uri_info.size, 3);
  EXPECT_FALSE(content_uri_info.is_directory);
  EXPECT_EQ(content_uri_info.last_modified, info.last_modified);

  EXPECT_TRUE(GetFileInfo(content_uri_document, &content_uri_info));
  EXPECT_FALSE(content_uri_info.is_directory);
  // Java DocumentProvider only does resolution to seconds.
  EXPECT_EQ(content_uri_info.last_modified.ToTimeT(),
            info.last_modified.ToTimeT());

  EXPECT_TRUE(GetFileInfo(dir, &info));
  EXPECT_TRUE(GetFileInfo(content_uri_tree, &content_uri_info));
  EXPECT_TRUE(content_uri_info.is_directory);
  EXPECT_EQ(content_uri_info.last_modified.ToTimeT(),
            info.last_modified.ToTimeT());

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

TEST(ContentUriUtilsTest, GetOrCreateByDisplayName) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath dir = temp_dir.GetPath().Append("dir");
  ASSERT_TRUE(base::CreateDirectory(dir));
  FilePath child1 = dir.Append("child1.txt");
  FilePath child2 = dir.Append("child2.txt");
  ASSERT_TRUE(WriteFile(child1, "1"));
  FilePath parent =
      *test::android::GetInMemoryContentTreeUriFromCacheDirDirectory(dir);

  // If there is a match, the result should be the valid tree URI.
  bool is_directory = false;
  bool create = false;
  FilePath child = ContentUriGetChildDocumentOrQuery(
      parent, "child1.txt", "text/plain", is_directory, create);
  EXPECT_EQ(child.value(),
            "content://org.chromium.native_test.docprov/tree/" +
                temp_dir.GetPath().BaseName().value() + "%2Fdir/document/" +
                temp_dir.GetPath().BaseName().value() + "%2Fdir%2Fchild1.txt");
  EXPECT_FALSE(ContentUriIsCreateChildDocumentQuery(child));
  EXPECT_TRUE(internal::ContentUriExists(child));

  // If there is not a match, and create is not set, the result should be empty.
  child = ContentUriGetChildDocumentOrQuery(parent, "child2.txt", "text/plain",
                                            is_directory, create);
  EXPECT_TRUE(child.empty());
  EXPECT_FALSE(ContentUriIsCreateChildDocumentQuery(child));
  EXPECT_FALSE(internal::ContentUriExists(child));

  // If create is set the result should be a create-child-document query.
  create = true;
  FilePath query = ContentUriGetChildDocumentOrQuery(
      parent, "child2.txt", "text/plain", is_directory, create);
  EXPECT_EQ(query.value(),
            "content://org.chromium.native_test.docprov/create-child-document/"
            "tree/" +
                temp_dir.GetPath().BaseName().value() + "%2Fdir/document/" +
                temp_dir.GetPath().BaseName().value() +
                "%2Fdir/mime-type/text%2Fplain/display-name/child2.txt");
  EXPECT_TRUE(ContentUriIsCreateChildDocumentQuery(query));
  EXPECT_FALSE(internal::ContentUriExists(query));

  // Lookup should fail when create is false if doc does not exist.
  create = false;
  child = ContentUriGetDocumentFromQuery(query, create);
  EXPECT_TRUE(child.empty());
  EXPECT_FALSE(base::PathExists(child2));

  // Lookup should create the document, and return the valid URI when create is
  // set.
  create = true;
  child = ContentUriGetDocumentFromQuery(query, create);
  EXPECT_EQ(child.value(),
            "content://org.chromium.native_test.docprov/tree/" +
                temp_dir.GetPath().BaseName().value() + "%2Fdir/document/" +
                temp_dir.GetPath().BaseName().value() + "%2Fdir%2Fchild2.txt");
  EXPECT_FALSE(ContentUriIsCreateChildDocumentQuery(child));
  EXPECT_TRUE(internal::ContentUriExists(child));
  EXPECT_TRUE(base::PathExists(child2));
}

}  // namespace base
