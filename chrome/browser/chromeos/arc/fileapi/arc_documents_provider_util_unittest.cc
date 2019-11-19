// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"

#include "base/files/file_path.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

mojom::DocumentPtr MakeDocument(const std::string& display_name,
                                const std::string& mime_type) {
  mojom::DocumentPtr document = mojom::Document::New();
  document->display_name = display_name;
  document->mime_type = mime_type;
  return document;
}

TEST(ArcDocumentsProviderUtilTest, EscapePathComponent) {
  EXPECT_EQ("", EscapePathComponent(""));
  EXPECT_EQ("%2E", EscapePathComponent("."));
  EXPECT_EQ("%2E%2E", EscapePathComponent(".."));
  EXPECT_EQ("...", EscapePathComponent("..."));
  EXPECT_EQ("example.com", EscapePathComponent("example.com"));
  EXPECT_EQ("%2F%2F%2F", EscapePathComponent("///"));
  EXPECT_EQ("100%25", EscapePathComponent("100%"));
  EXPECT_EQ("a b", EscapePathComponent("a b"));
  EXPECT_EQ("ねこ", EscapePathComponent("ねこ"));
}

TEST(ArcDocumentsProviderUtilTest, UnescapePathComponent) {
  EXPECT_EQ("", UnescapePathComponent(""));
  EXPECT_EQ(".", UnescapePathComponent("%2E"));
  EXPECT_EQ("..", UnescapePathComponent("%2E%2E"));
  EXPECT_EQ("...", UnescapePathComponent("..."));
  EXPECT_EQ("example.com", UnescapePathComponent("example.com"));
  EXPECT_EQ("///", UnescapePathComponent("%2F%2F%2F"));
  EXPECT_EQ("100%", UnescapePathComponent("100%25"));
  EXPECT_EQ("a b", UnescapePathComponent("a b"));
  EXPECT_EQ("ねこ", UnescapePathComponent("ねこ"));
}

TEST(ArcDocumentsProviderUtilTest, GetDocumentsProviderMountPath) {
  EXPECT_EQ("/special/arc-documents-provider/authority/document_id",
            GetDocumentsProviderMountPath("authority", "document_id").value());
  EXPECT_EQ("/special/arc-documents-provider/a b/a b",
            GetDocumentsProviderMountPath("a b", "a b").value());
  EXPECT_EQ("/special/arc-documents-provider/a%2Fb/a%2Fb",
            GetDocumentsProviderMountPath("a/b", "a/b").value());
  EXPECT_EQ("/special/arc-documents-provider/%2E/%2E",
            GetDocumentsProviderMountPath(".", ".").value());
  EXPECT_EQ("/special/arc-documents-provider/%2E%2E/%2E%2E",
            GetDocumentsProviderMountPath("..", "..").value());
  EXPECT_EQ("/special/arc-documents-provider/.../...",
            GetDocumentsProviderMountPath("...", "...").value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrl) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL("home/calico.jpg"), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlEmptyPath) {
  std::string authority;
  std::string root_document_id;
  // Assign a non-empty arbitrary path to make sure an empty path is
  // set in ParseDocumentsProviderUrl().
  base::FilePath path(FILE_PATH_LITERAL("foobar"));

  // Should accept a path pointing to a root directory.
  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root"))),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL(""), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlEmptyPathSlash) {
  std::string authority;
  std::string root_document_id;
  // Assign a non-empty arbitrary path to make sure an empty path is
  // set in ParseDocumentsProviderUrl().
  base::FilePath path(FILE_PATH_LITERAL("foobar"));

  // Should accept a path pointing to a root directory.
  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root/"))),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL(""), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlInvalidType) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  // Not storage::kFileSystemTypeArcDocumentsProvider.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcContent,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlInvalidPath) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  // root_document_id part is missing.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(kDocumentsProviderMountPointPath)
              .Append(FILE_PATH_LITERAL("root-missing"))),
      &authority, &root_document_id, &path));

  // Leading / is missing.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(FILE_PATH_LITERAL(
              "special/arc-documents-provider/cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));

  // Not under /special.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(FILE_PATH_LITERAL(
              "/invalid/arc-documents-provider/cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));

  // Not under /special/arc-documents-provider.
  EXPECT_FALSE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(FILE_PATH_LITERAL(
              "/special/something-else/cats/root/home/calico.jpg"))),
      &authority, &root_document_id, &path));
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlUnescape) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(
              "/special/arc-documents-provider/cats/ro%2Fot/home/calico.jpg")),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("ro/ot", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL("home/calico.jpg"), path.value());
}

TEST(ArcDocumentsProviderUtilTest, ParseDocumentsProviderUrlUtf8) {
  std::string authority;
  std::string root_document_id;
  base::FilePath path;

  EXPECT_TRUE(ParseDocumentsProviderUrl(
      storage::FileSystemURL::CreateForTest(
          url::Origin(), storage::kFileSystemTypeArcDocumentsProvider,
          base::FilePath(
              "/special/arc-documents-provider/cats/root/home/みけねこ.jpg")),
      &authority, &root_document_id, &path));
  EXPECT_EQ("cats", authority);
  EXPECT_EQ("root", root_document_id);
  EXPECT_EQ(FILE_PATH_LITERAL("home/みけねこ.jpg"), path.value());
}

TEST(ArcDocumentsProviderUtilTest, BuildDocumentUrl) {
  EXPECT_EQ("content://authority/document/document_id",
            BuildDocumentUrl("authority", "document_id").spec());
  EXPECT_EQ("content://a%20b/document/a%20b",
            BuildDocumentUrl("a b", "a b").spec());
  EXPECT_EQ("content://a%2Fb/document/a%2Fb",
            BuildDocumentUrl("a/b", "a/b").spec());
  EXPECT_EQ("content://../document/..", BuildDocumentUrl("..", "..").spec());
}

TEST(ArcDocumentsProviderUtilTest, GetExtensionsForArcMimeType) {
  // MIME types already known to Chromium.
  EXPECT_NE(0u, GetExtensionsForArcMimeType("audio/mp3").size());
  EXPECT_NE(0u, GetExtensionsForArcMimeType("image/jpeg").size());
  EXPECT_NE(0u, GetExtensionsForArcMimeType("text/html").size());
  EXPECT_NE(
      0u, GetExtensionsForArcMimeType("application/x-chrome-extension").size());

  // MIME types known to Android only.
  EXPECT_NE(0u,
            GetExtensionsForArcMimeType("application/x-android-drm-fl").size());
  EXPECT_NE(0u, GetExtensionsForArcMimeType("audio/x-wav").size());

  // Unknown types.
  EXPECT_EQ(0u, GetExtensionsForArcMimeType("abc/xyz").size());
  EXPECT_EQ(
      0u, GetExtensionsForArcMimeType("vnd.android.document/directory").size());

  // Specially handled types.
  EXPECT_EQ(0u, GetExtensionsForArcMimeType("application/octet-stream").size());
}

TEST(ArcDocumentsProviderUtilTest, GetFileNameForDocument) {
  EXPECT_EQ("kitten.png",
            GetFileNameForDocument(MakeDocument("kitten.png", "image/png")));
  EXPECT_EQ("a__b.png",
            GetFileNameForDocument(MakeDocument("a//b.png", "image/png")));
  EXPECT_EQ("_.png", GetFileNameForDocument(MakeDocument("", "image/png")));
  EXPECT_EQ("_.png", GetFileNameForDocument(MakeDocument(".", "image/png")));
  EXPECT_EQ("_.png", GetFileNameForDocument(MakeDocument("..", "image/png")));
  EXPECT_EQ("_.png",
            GetFileNameForDocument(MakeDocument("......", "image/png")));
  EXPECT_EQ("kitten.png",
            GetFileNameForDocument(MakeDocument("kitten", "image/png")));
  EXPECT_EQ("kitten",
            GetFileNameForDocument(MakeDocument("kitten", "abc/xyz")));

  // Check that files with a mime type different than expected appends a
  // possible extension when a different mime type with a different category is
  // found in the Android mime types map.
  EXPECT_EQ("file.txt.3gp",
            GetFileNameForDocument(MakeDocument("file.txt", "video/3gpp")));

  // Check that files with a mime type different than expected don't append
  // an extension when a different mime type with the same category is
  // found in the Android mime types map.
  EXPECT_EQ("file.3gp",
            GetFileNameForDocument(MakeDocument("file.3gp", "video/mp4")));
}

TEST(ArcDocumentsProviderUtilTest, StripMimeSubType) {
  // Check that the category type is returned for a valid mime type.
  EXPECT_EQ("video", StripMimeSubType("video/mp4"));
  // Check that an empty string is returned for malformed mime types.
  EXPECT_EQ("", StripMimeSubType(""));
  EXPECT_EQ("", StripMimeSubType("video/"));
  EXPECT_EQ("", StripMimeSubType("/"));
  EXPECT_EQ("", StripMimeSubType("/abc"));
  EXPECT_EQ("", StripMimeSubType("/abc/xyz"));
}

TEST(ArcDocumentsProviderUtilTest, FindArcMimeTypeFromExtension) {
  // Test that a lone possible extension returns the correct type.
  EXPECT_EQ("application/msword", FindArcMimeTypeFromExtension("doc"));
  // Test that the first extension in the comma delimited list of extensions
  // returns the correct type.
  EXPECT_EQ("video/3gpp", FindArcMimeTypeFromExtension("3gp"));
  // Test that the second extension in the comma delimited list of extensions
  // returns the correct type.
  EXPECT_EQ("audio/mpeg", FindArcMimeTypeFromExtension("mpga"));
  // Test that matching suffixes (m4a) return null without a full match.
  EXPECT_EQ("", FindArcMimeTypeFromExtension("4a"));
  // Test that matching prefixes (imy) return null without a full match.
  EXPECT_EQ("", FindArcMimeTypeFromExtension("im"));
  // Test that ambiguous prefixes (mp4, mpg) return null.
  EXPECT_EQ("", FindArcMimeTypeFromExtension("mp"));
  // Test that invalid mime types return null.
  EXPECT_EQ("", FindArcMimeTypeFromExtension(""));
  EXPECT_EQ("", FindArcMimeTypeFromExtension("invalid"));
}

}  // namespace

}  // namespace arc
