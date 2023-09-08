// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/proto_file_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {
namespace {
class ProtoFileManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    temp_file_path_ = temp_dir_.GetPath().AppendASCII(
        "app_deduplication_service/deduplication_data.pb");
    manager_ = std::make_unique<ProtoFileManager<proto::DeduplicateData>>(
        temp_file_path_);
  }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  std::unique_ptr<ProtoFileManager<proto::DeduplicateData>> manager_;
  base::FilePath temp_file_path_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ProtoFileManagerTest, WriteAndReadDataSuccess) {
  EXPECT_FALSE(base::PathExists(temp_file_path_));
  {
    proto::DeduplicateData data;
    auto* group = data.add_app_group();
    group->set_app_group_uuid("15ca3ac3-c8cd-4a0c-a195-2ea210ea922c");
    *group->add_package_id() =
        "web:https://keep.google.com/?usp=installed_webapp";
    base::test::TestFuture<bool> write_result;
    manager_->WriteProtoToFile(data, write_result.GetCallback());
    EXPECT_TRUE(write_result.Get());
  }

  ASSERT_TRUE(base::PathExists(temp_file_path_));
  {
    base::test::TestFuture<absl::optional<proto::DeduplicateData>> read_data;
    manager_->ReadProtoFromFile(read_data.GetCallback());
    ASSERT_TRUE(read_data.Get().has_value());
    ASSERT_EQ(read_data.Get()->app_group_size(), 1);
    auto read_group = read_data.Get()->app_group(0);
    EXPECT_EQ(read_group.app_group_uuid(),
              "15ca3ac3-c8cd-4a0c-a195-2ea210ea922c");
    EXPECT_EQ(read_group.package_id(0),
              "web:https://keep.google.com/?usp=installed_webapp");
  }

  // Check we can overwrite the data.
  EXPECT_TRUE(base::PathExists(temp_file_path_));
  {
    proto::DeduplicateData data;
    auto* group = data.add_app_group();
    group->set_app_group_uuid("2ea210ea922c-15ca3ac3-c8cd-4a0c-a195");
    *group->add_package_id() =
        "web:https://keep.google.com/?usp=installed_webapp2";
    base::test::TestFuture<bool> write_result;
    manager_->WriteProtoToFile(data, write_result.GetCallback());
    EXPECT_TRUE(write_result.Get());
  }

  ASSERT_TRUE(base::PathExists(temp_file_path_));
  {
    base::test::TestFuture<absl::optional<proto::DeduplicateData>> read_data;
    manager_->ReadProtoFromFile(read_data.GetCallback());
    ASSERT_TRUE(read_data.Get().has_value());
    ASSERT_EQ(read_data.Get()->app_group_size(), 1);
    auto read_group = read_data.Get()->app_group(0);
    EXPECT_EQ(read_group.app_group_uuid(),
              "2ea210ea922c-15ca3ac3-c8cd-4a0c-a195");
    EXPECT_EQ(read_group.package_id(0),
              "web:https://keep.google.com/?usp=installed_webapp2");
  }
}
}  // namespace
}  // namespace apps
