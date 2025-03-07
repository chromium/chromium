// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_item_metadata.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "content/public/browser/download_item_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace safe_browsing {

class DownloadItemMetadataTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Initialize file paths.
    tmp_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("download.exe"));
    target_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("target.exe"));

    SetupMockDownloadItem();

    metadata_ = std::make_unique<DownloadItemMetadata>(mock_item_.get());
  }

  void TearDown() override {
    metadata_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetupMockDownloadItem() {
    mock_item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();

    ON_CALL(*mock_item_, GetFullPath()).WillByDefault(ReturnRef(tmp_path_));
    ON_CALL(*mock_item_, GetTargetFilePath())
        .WillByDefault(ReturnRef(target_path_));
    ON_CALL(*mock_item_, GetHash()).WillByDefault(ReturnRef(hash_));
    ON_CALL(*mock_item_, GetTotalBytes()).WillByDefault(Return(total_bytes_));
    ON_CALL(*mock_item_, GetMimeType()).WillByDefault(Return(mime_type_));
    ON_CALL(*mock_item_, GetURL()).WillByDefault(ReturnRef(url_));
    ON_CALL(*mock_item_, GetTabUrl()).WillByDefault(ReturnRef(tab_url_));
    ON_CALL(*mock_item_, HasUserGesture())
        .WillByDefault(Return(has_user_gesture_));
    ON_CALL(*mock_item_, GetDangerType()).WillByDefault(Return(danger_type_));

    content::DownloadItemUtils::AttachInfoForTesting(mock_item_.get(),
                                                     profile(), web_contents());
  }

  base::ScopedTempDir temp_dir_;

  base::FilePath tmp_path_;
  base::FilePath target_path_;

  // Download metadata with sample values.
  const GURL url_{"https://example.com/url"};
  const GURL tab_url_{"https://example.com/tab-url"};
  const std::string hash_{"hash"};
  const int64_t total_bytes_{100};
  const std::string mime_type_{"application/octet-stream"};
  const bool has_user_gesture_{true};
  const download::DownloadDangerType danger_type_{
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS};

  // Test objects.
  std::unique_ptr<NiceMock<download::MockDownloadItem>> mock_item_;
  std::unique_ptr<DownloadItemMetadata> metadata_;
};

TEST_F(DownloadItemMetadataTest, BasicAccessors) {
  // Verify all simple delegating accessors.
  EXPECT_EQ(metadata_->GetBrowserContext(), profile());
  EXPECT_EQ(metadata_->GetFullPath(), tmp_path_);
  EXPECT_EQ(metadata_->GetTargetFilePath(), target_path_);
  EXPECT_EQ(metadata_->GetHash(), hash_);
  EXPECT_EQ(metadata_->GetTotalBytes(), total_bytes_);
  EXPECT_EQ(metadata_->GetMimeType(), mime_type_);
  EXPECT_EQ(metadata_->GetURL(), url_);
  EXPECT_EQ(metadata_->GetTabUrl(), tab_url_);
  EXPECT_TRUE(metadata_->HasUserGesture());
  EXPECT_EQ(metadata_->GetDangerType(), danger_type_);
}

TEST_F(DownloadItemMetadataTest, IsObfuscated) {
  // No obfuscation data.
  EXPECT_FALSE(metadata_->IsObfuscated());

  // File is not obfuscated.
  auto non_obfuscated_data =
      std::make_unique<enterprise_obfuscation::DownloadObfuscationData>(false);
  mock_item_->SetUserData(
      enterprise_obfuscation::DownloadObfuscationData::kUserDataKey,
      std::move(non_obfuscated_data));
  EXPECT_FALSE(metadata_->IsObfuscated());

  // File is obfuscated.
  auto obfuscated_data =
      std::make_unique<enterprise_obfuscation::DownloadObfuscationData>(true);
  mock_item_->SetUserData(
      enterprise_obfuscation::DownloadObfuscationData::kUserDataKey,
      std::move(obfuscated_data));
  EXPECT_TRUE(metadata_->IsObfuscated());
}

}  // namespace safe_browsing
