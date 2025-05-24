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
    mock_item_.reset();
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

TEST_F(DownloadItemMetadataTest, AddScanResultMetadata) {
  enterprise_connectors::FileMetadata file_metadata("test.txt", "hash",
                                                    "text/plain", 1000);
  metadata_->AddScanResultMetadata(file_metadata);

  // Verify file metadata was added to `DownloadItem`.
  auto* scan_result = static_cast<enterprise_connectors::ScanResult*>(
      mock_item_->GetUserData(enterprise_connectors::ScanResult::kKey));
  ASSERT_TRUE(scan_result);
  ASSERT_EQ(scan_result->file_metadata.size(), 1u);
  EXPECT_EQ(scan_result->file_metadata[0].filename, "test.txt");

  enterprise_connectors::FileMetadata second_metadata("test2.txt", "hash",
                                                      "text/plain", 1000);
  metadata_->AddScanResultMetadata(second_metadata);

  // Verify both metadata entries exist for `DownloadItem`.
  scan_result = static_cast<enterprise_connectors::ScanResult*>(
      mock_item_->GetUserData(enterprise_connectors::ScanResult::kKey));
  ASSERT_TRUE(scan_result);
  ASSERT_EQ(scan_result->file_metadata.size(), 2u);
  EXPECT_EQ(scan_result->file_metadata[1].filename, "test2.txt");
}

// Simple test observer that tracks the number of times `DownloadItem` is
// updated.
class TestCountingObserver : public download::DownloadItem::Observer {
 public:
  void OnDownloadUpdated(download::DownloadItem* item) override {
    update_count_++;
  }
  void OnDownloadOpened(download::DownloadItem* item) override {}
  void OnDownloadRemoved(download::DownloadItem* item) override {}
  void OnDownloadDestroyed(download::DownloadItem* item) override {}

  int update_count() const { return update_count_; }

 private:
  int update_count_ = 0;
};

TEST_F(DownloadItemMetadataTest, ObservationLifecycle) {
  // First test case: Observation is stopped explicitly.
  TestCountingObserver observer1;
  auto observation1 = metadata_->GetDownloadObservation(&observer1);
  mock_item_->NotifyObserversDownloadUpdated();

  // Observer is notified that update happened.
  EXPECT_EQ(1, observer1.update_count());

  // Observation stops and observer should not be notified.
  observation1->Stop();
  mock_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1, observer1.update_count());
  observation1.reset();

  // Second test case: Observation stops when out of scope.
  TestCountingObserver observer2;
  {
    auto observation2 = metadata_->GetDownloadObservation(&observer2);
    mock_item_->NotifyObserversDownloadUpdated();
    EXPECT_EQ(1, observer2.update_count());
  }

  // As `observation2` is now out of scope, update count should not increase.
  mock_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1, observer2.update_count());

  // Third test case: Observation stops when metadata object is destroyed.
  TestCountingObserver observer3;
  auto observation3 = metadata_->GetDownloadObservation(&observer3);

  mock_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1, observer3.update_count());

  // Observation should stop as metadata is destroyed.
  observation3.reset();
  metadata_.reset();
  mock_item_->NotifyObserversDownloadUpdated();
  EXPECT_EQ(1, observer3.update_count());
}

}  // namespace safe_browsing
