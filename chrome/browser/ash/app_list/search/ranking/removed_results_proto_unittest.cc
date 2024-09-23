// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/persistent_proto.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/ranking/removed_results.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
}  // namespace

class RemovedResultsProtoTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    proto_ = std::make_unique<ash::PersistentProto<RemovedResultsProto>>(
        GetPath(), /*write_delay=*/base::TimeDelta());
    proto_->Init();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  ash::PersistentProto<RemovedResultsProto>* proto() { return proto_.get(); }

  void PersistIds(const std::vector<std::string>& ids) {
    for (const auto& id : ids)
      (*proto_)->mutable_removed_ids()->insert({id, false});
    proto_->StartWrite();
    Wait();
  }

  RemovedResultsProto ReadFromDisk() {
    EXPECT_TRUE(base::PathExists(GetPath()));
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    RemovedResultsProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ash::PersistentProto<RemovedResultsProto>> proto_;
};

TEST_F(RemovedResultsProtoTest, CheckInitializeEmpty) {
  EXPECT_FALSE(proto_->initialized());

  Wait();
  EXPECT_TRUE(proto_->initialized());
  RemovedResultsProto proto_from_disk = ReadFromDisk();
  EXPECT_EQ(proto_from_disk.removed_ids_size(), 0);
}

TEST_F(RemovedResultsProtoTest, PersistIds) {
  Wait();
  std::vector<std::string> ids{"A", "B", "C"};
  PersistIds(ids);

  // Check proto for records of removed results.
  RemovedResultsProto proto_from_disk = ReadFromDisk();
  EXPECT_EQ(proto_from_disk.removed_ids_size(), 3);

  std::vector<std::string> recorded_ids;
  for (const auto& result : proto_from_disk.removed_ids())
    recorded_ids.push_back(result.first);
  EXPECT_THAT(ids, UnorderedElementsAreArray(recorded_ids));
}

TEST_F(RemovedResultsProtoTest, PersistDuplicateIds) {
  Wait();

  // Request to remove ids, with a duplicate.
  std::vector<std::string> ids{"A", "B", "B"};
  PersistIds(ids);

  // Check proto for records of removed results.
  RemovedResultsProto proto_from_disk = ReadFromDisk();
  EXPECT_EQ(proto_from_disk.removed_ids_size(), 2);

  std::vector<std::string> recorded_ids;
  for (const auto& result : proto_from_disk.removed_ids())
    recorded_ids.push_back(result.first);
  EXPECT_THAT(recorded_ids, UnorderedElementsAre("A", "B"));
}

}  // namespace app_list::test
