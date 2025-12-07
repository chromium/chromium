// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_system_access_metadata.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class FileSystemAccessMetadataTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    auto fsa_item = CreateTestWriteItem(
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("temp_file.txt")),
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("target_file.txt")));

    NavigateAndCommit(tab_url_);
    metadata_ = std::make_unique<FileSystemAccessMetadata>(std::move(fsa_item));
  }

  void TearDown() override {
    metadata_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  // Create a test write item with basic test properties.
  std::unique_ptr<content::FileSystemAccessWriteItem> CreateTestWriteItem(
      const base::FilePath& full_path,
      const base::FilePath& target_path) {
    auto item = std::make_unique<content::FileSystemAccessWriteItem>();
    item->full_path = full_path;
    item->target_file_path = target_path;
    item->sha256_hash = hash_;
    item->size = 100;
    item->frame_url = frame_url_;
    item->has_user_gesture = true;

    item->web_contents = web_contents()->GetWeakPtr();
    item->browser_context = raw_ptr<content::BrowserContext>(profile());
    return item;
  }

  base::ScopedTempDir temp_dir_;

  // FSA metadata with sample values.
  std::string hash_ = "hash";
  GURL frame_url_{"https://example.com/frame_url"};
  GURL tab_url_{"https://example.com/tab_url"};

  std::unique_ptr<FileSystemAccessMetadata> metadata_;
};

TEST_F(FileSystemAccessMetadataTest, BasicAccessors) {
  // Test simple delegating accessors.
  EXPECT_EQ(metadata_->GetBrowserContext(), profile());
  EXPECT_EQ(metadata_->GetFullPath(),
            temp_dir_.GetPath().Append(FILE_PATH_LITERAL("temp_file.txt")));
  EXPECT_EQ(metadata_->GetTargetFilePath(),
            temp_dir_.GetPath().Append(FILE_PATH_LITERAL("target_file.txt")));
  EXPECT_EQ(metadata_->GetHash(), hash_);
  EXPECT_EQ(metadata_->GetTotalBytes(), 100);
  EXPECT_EQ(metadata_->GetURL(), frame_url_);
  EXPECT_EQ(metadata_->GetTabUrl(), tab_url_);
  EXPECT_TRUE(metadata_->HasUserGesture());

  // Methods with fixed return values.
  EXPECT_FALSE(metadata_->IsObfuscated());
  EXPECT_FALSE(metadata_->IsTopLevelEncryptedArchive());
  EXPECT_EQ(metadata_->GetDangerType(),
            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
}

TEST_F(FileSystemAccessMetadataTest, GetMimeType) {
  // Test with text file extension.
  EXPECT_EQ(metadata_->GetMimeType(), "text/plain");

  // Test with unknown extension.
  auto fsa_item = CreateTestWriteItem(
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.123xyz")),
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.123xyz")));
  FileSystemAccessMetadata unknown_ext_metadata(std::move(fsa_item));
  EXPECT_EQ(unknown_ext_metadata.GetMimeType(), "application/octet-stream");

  // Test with no extension.
  auto fsa_item_no_ext = CreateTestWriteItem(
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("noextension")),
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("noextension")));
  FileSystemAccessMetadata no_ext_metadata(std::move(fsa_item_no_ext));
  EXPECT_EQ(no_ext_metadata.GetMimeType(), "application/octet-stream");
}

}  // namespace safe_browsing
