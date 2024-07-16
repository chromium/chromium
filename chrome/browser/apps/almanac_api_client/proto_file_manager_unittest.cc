// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/proto_file_manager.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {
namespace {
class ProtoFileManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    temp_file_path_ = temp_dir_.GetPath().AppendASCII("user_context.pb");
    manager_ = std::make_unique<ProtoFileManager<proto::ClientUserContext>>(
        temp_file_path_);
  }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  std::unique_ptr<ProtoFileManager<proto::ClientUserContext>> manager_;
  base::FilePath temp_file_path_;

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ProtoFileManagerTest, WriteAndReadDataSuccess) {
  EXPECT_FALSE(base::PathExists(temp_file_path_));
  {
    proto::ClientUserContext data;
    data.set_language("en_US");
    data.set_user_type(proto::ClientUserContext::USERTYPE_MANAGED);

    base::test::TestFuture<bool> write_result;
    manager_->WriteProtoToFile(data, write_result.GetCallback());
    EXPECT_TRUE(write_result.Get());
  }

  ASSERT_TRUE(base::PathExists(temp_file_path_));
  {
    base::test::TestFuture<std::optional<proto::ClientUserContext>> read_data;
    manager_->ReadProtoFromFile(read_data.GetCallback());
    ASSERT_TRUE(read_data.Get().has_value());
    ASSERT_EQ(read_data.Get()->language(), "en_US");
    ASSERT_EQ(read_data.Get()->user_type(),
              proto::ClientUserContext::USERTYPE_MANAGED);
  }

  // Check we can overwrite the data.
  EXPECT_TRUE(base::PathExists(temp_file_path_));
  {
    proto::ClientUserContext data;
    data.set_language("ja_JP");
    data.set_user_type(proto::ClientUserContext::USERTYPE_UNMANAGED);

    base::test::TestFuture<bool> write_result;
    manager_->WriteProtoToFile(data, write_result.GetCallback());
    EXPECT_TRUE(write_result.Get());
  }

  ASSERT_TRUE(base::PathExists(temp_file_path_));
  {
    base::test::TestFuture<std::optional<proto::ClientUserContext>> read_data;
    manager_->ReadProtoFromFile(read_data.GetCallback());
    ASSERT_TRUE(read_data.Get().has_value());

    ASSERT_EQ(read_data.Get()->language(), "ja_JP");
    ASSERT_EQ(read_data.Get()->user_type(),
              proto::ClientUserContext::USERTYPE_UNMANAGED);
  }
}
}  // namespace
}  // namespace apps
