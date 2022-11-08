// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "content/public/common/url_constants.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace arc {

TEST(ArcContentFileSystemUrlUtilTest, EncodeAndDecodeExternalFileUrl) {
  {
    GURL src("file://foo/bar/baz");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    GURL src("content://org.chromium.foo/bar/baz");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    GURL src("content://org.chromium.foo/bar/%19%20%21");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    GURL src("content://org.chromium.foo/!@#$%^&*()_+|~-=\\`[]{};':\"<>?,./");
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
  {
    std::u16string utf16_string = {
        0x307b,  // HIRAGANA_LETTER_HO
        0x3052,  // HIRAGANA_LETTER_GE
    };
    GURL src("content://org.chromium.foo/" + base::UTF16ToUTF8(utf16_string));
    GURL dest = ArcUrlToExternalFileUrl(src);
    EXPECT_TRUE(dest.is_valid());
    EXPECT_EQ(content::kExternalFileScheme, dest.scheme());
    GURL result = ExternalFileUrlToArcUrl(dest);
    EXPECT_TRUE(result.is_valid());
    EXPECT_EQ(src, result);
  }
}

TEST(ArcContentFileSystemUrlUtilTest, FileSystemUrlToArcUrl) {
  GURL arc_url("content://org.chromium.foo/bar/baz");

  base::FilePath path =
      base::FilePath(kContentFileSystemMountPointPath)
          .Append(base::FilePath::FromUTF8Unsafe(EscapeArcUrl(arc_url)));
  storage::FileSystemURL file_system_url =
      storage::FileSystemURL::CreateForTest(
          blink::StorageKey(), storage::kFileSystemTypeArcContent, path);

  EXPECT_EQ(arc_url, FileSystemUrlToArcUrl(file_system_url));
}

TEST(ArcContentFileSystemUrlUtilTest, PathToArcUrl) {
  GURL arc_url("content://org.chromium.foo/bar/baz");

  base::FilePath path =
      base::FilePath(kContentFileSystemMountPointPath)
          .Append(base::FilePath::FromUTF8Unsafe(EscapeArcUrl(arc_url)));
  EXPECT_EQ(arc_url, PathToArcUrl(path));
}

}  // namespace arc
