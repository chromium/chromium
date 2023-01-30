// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_cache.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps::deduplication {

class AppDeduplicationCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    temp_dir_path_ = temp_dir_.GetPath().AppendASCII(
        "app_deduplication_service/deduplication_data/");
    cache_ = std::make_unique<AppDeduplicationCache>(temp_dir_path_);
  }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  std::unique_ptr<AppDeduplicationCache> cache_;
  base::FilePath temp_dir_path_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(AppDeduplicationCacheTest, WriteAndReadDataSuccess) {
  proto::DeduplicateData data;
  auto* app = data.add_app_group()->add_app();
  app->set_app_id("com.skype.raidar");
  app->set_platform("phonehub");

  EXPECT_TRUE(cache_->WriteDeduplicateDataToDisk(data));

  EXPECT_TRUE(
      base::PathExists(temp_dir_path_.AppendASCII("deduplication_data.pb")));

  absl::optional<proto::DeduplicateData> data_read =
      cache_->ReadDeduplicateDataFromDisk();

  EXPECT_TRUE(data_read.has_value());
  EXPECT_EQ(data_read->app_group_size(), 1);

  auto observed_app = data_read->app_group(0).app(0);
  EXPECT_EQ(observed_app.app_id(), "com.skype.raidar");
  EXPECT_EQ(observed_app.platform(), "phonehub");
}

TEST_F(AppDeduplicationCacheTest, WriteAndReadDataMultipleVersions) {
  proto::DeduplicateData data_v1;
  auto* app_v1 = data_v1.add_app_group()->add_app();
  app_v1->set_app_id("com.skype.raidar");
  app_v1->set_platform("phonehub");

  EXPECT_TRUE(cache_->WriteDeduplicateDataToDisk(data_v1));
  EXPECT_TRUE(
      base::PathExists(temp_dir_path_.AppendASCII("deduplication_data.pb")));

  proto::DeduplicateData data_v2;
  auto* app_v2 = data_v2.add_app_group()->add_app();
  app_v2->set_app_id("https://web.skype.com/");
  app_v2->set_platform("website");

  EXPECT_TRUE(cache_->WriteDeduplicateDataToDisk(data_v2));
  EXPECT_TRUE(
      base::PathExists(temp_dir_path_.AppendASCII("deduplication_data.pb")));

  absl::optional<proto::DeduplicateData> data_read =
      cache_->ReadDeduplicateDataFromDisk();

  EXPECT_TRUE(data_read.has_value());
  EXPECT_EQ(data_read->app_group_size(), 1);

  auto observed_app = data_read->app_group(0).app(0);
  EXPECT_EQ(observed_app.app_id(), "https://web.skype.com/");
  EXPECT_EQ(observed_app.platform(), "website");
}

}  // namespace apps::deduplication
