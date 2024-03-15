// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/persistent_proto.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ash/utility/persistent_proto_test.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Populate |proto| with some test data.
void PopulateTestProto(TestProto* proto) {
  proto->set_value(12345);
}

// Make a proto with test data.
TestProto MakeTestProto() {
  TestProto proto;
  PopulateTestProto(&proto);
  return proto;
}

// Returns whether |actual| and |expected| are equal.
bool ProtoEquals(const TestProto* actual, const TestProto* expected) {
  if (!actual->has_value())
    return !expected->has_value();
  return actual->value() == expected->value();
}

base::TimeDelta WriteDelay() {
  return base::Seconds(0);
}

}  // namespace

class PersistentProtoTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  void ClearDisk() {
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  // Read the file at GetPath and parse it as a TestProto.
  TestProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    TestProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const TestProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void OnInit() { ++read_count_; }

  base::OnceClosure GetInitCallback() {
    return base::BindOnce(&PersistentProtoTest::OnInit, base::Unretained(this));
  }

  void OnWrite(bool success) {
    ASSERT_TRUE(success);
    ++write_count_;
  }

  base::RepeatingCallback<void(bool)> GetWriteCallback() {
    return base::BindRepeating(&PersistentProtoTest::OnWrite,
                               base::Unretained(this));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // Records the information passed to the callbacks for later expectation.
  int read_count_ = 0;
  int write_count_ = 0;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
};

// Test that the underlying proto is nullptr until proto initialization
// completes, and isn't after that.
TEST_F(PersistentProtoTest, Initialization) {
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  EXPECT_EQ(pproto.get(), nullptr);
  Wait();
  EXPECT_NE(pproto.get(), nullptr);
}

// Test bool conversion and has_value.
TEST_F(PersistentProtoTest, BoolTests) {
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  EXPECT_EQ(pproto.get(), nullptr);
  EXPECT_FALSE(pproto);
  EXPECT_FALSE(pproto.has_value());
  Wait();
  EXPECT_NE(pproto.get(), nullptr);
  EXPECT_TRUE(pproto);
  EXPECT_TRUE(pproto.has_value());
}

// Test -> and *.
TEST_F(PersistentProtoTest, Getters) {
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  Wait();
  // We're really just checking these don't crash.
  EXPECT_EQ(pproto->value(), 0);
  pproto->set_value(1);
  EXPECT_EQ(pproto->value(), 1);
  const TestProto& val = *pproto;
  EXPECT_EQ(val.value(), 1);
}

// Test that the pproto correctly saves the in-memory proto to disk.
TEST_F(PersistentProtoTest, Read) {
  // Build a `PersistentProto` whose underlying file does not exist before init.
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  // Underlying proto should be nullptr until read is complete.
  EXPECT_EQ(pproto.get(), nullptr);

  base::HistogramTester histogram_tester;
  Wait();
  histogram_tester.ExpectBucketCount("Apps.AppList.PersistentProto.ReadStatus",
                                     internal::ReadStatus::kMissing,
                                     /*expected_count=*/1);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 1);

  PopulateTestProto(pproto.get());
  pproto.StartWrite();
  Wait();
  EXPECT_EQ(write_count_, 2);

  TestProto written = ReadFromDisk();
  EXPECT_TRUE(ProtoEquals(&written, pproto.get()));
}

// Test that invalid files on disk are handled correctly.
TEST_F(PersistentProtoTest, ReadInvalidProto) {
  ASSERT_TRUE(base::WriteFile(GetPath(), "this isn't a valid proto"));

  // Build a `PersistentProto` with an invalid proto file.
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());

  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  base::HistogramTester histogram_tester;
  Wait();
  histogram_tester.ExpectBucketCount("Apps.AppList.PersistentProto.ReadStatus",
                                     internal::ReadStatus::kParseError,
                                     /*expected_count=*/1);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 1);
}

// Test that the pproto correctly loads an on-disk proto into memory.
TEST_F(PersistentProtoTest, Write) {
  const auto test_proto = MakeTestProto();
  WriteToDisk(test_proto);

  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  EXPECT_EQ(pproto.get(), nullptr);

  base::HistogramTester histogram_tester;
  Wait();
  histogram_tester.ExpectBucketCount("Apps.AppList.PersistentProto.ReadStatus",
                                     internal::ReadStatus::kOk,
                                     /*expected_count=*/1);
  EXPECT_EQ(read_count_, 1);
  EXPECT_EQ(write_count_, 0);
  EXPECT_NE(pproto.get(), nullptr);
  EXPECT_TRUE(ProtoEquals(pproto.get(), &test_proto));
}

// Test that several saves all happen correctly.
TEST_F(PersistentProtoTest, MultipleWrites) {
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  EXPECT_EQ(pproto.get(), nullptr);

  Wait();
  EXPECT_EQ(write_count_, 1);

  for (int i = 1; i <= 10; ++i) {
    pproto->set_value(i * i);
    pproto.StartWrite();
    Wait();
    EXPECT_EQ(write_count_, i + 1);

    TestProto written = ReadFromDisk();
    ASSERT_EQ(written.value(), i * i);
  }
}

// Test that many calls to QueueWrite get batched, leading to only one real
// write.
TEST_F(PersistentProtoTest, QueueWrites) {
  PersistentProto<TestProto> pproto(GetPath(), WriteDelay());
  pproto.RegisterOnInitUnsafe(GetInitCallback());
  pproto.RegisterOnWriteUnsafe(GetWriteCallback());
  pproto.Init();
  Wait();
  EXPECT_EQ(write_count_, 1);

  // Three successive StartWrite calls result in three writes.
  write_count_ = 0;
  for (int i = 0; i < 3; ++i)
    pproto.StartWrite();
  Wait();
  EXPECT_EQ(write_count_, 3);

  // Three successive QueueWrite calls results in one write.
  write_count_ = 0;
  for (int i = 0; i < 3; ++i)
    pproto.QueueWrite();
  Wait();
  EXPECT_EQ(write_count_, 1);
}

}  // namespace ash
