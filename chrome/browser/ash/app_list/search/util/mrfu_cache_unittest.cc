// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/mrfu_cache.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/app_list/search/util/mrfu_cache.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

using testing::ElementsAre;
using testing::FloatNear;
using testing::Pair;
using testing::UnorderedElementsAre;

constexpr float kEps = 1e-3f;

}  // namespace

class MrfuCacheTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  ash::PersistentProto<MrfuCacheProto> GetProto() {
    return ash::PersistentProto<MrfuCacheProto>(GetPath(), base::Seconds(0));
  }

  MrfuCache::Params TestingParams(float half_life = 10.0f,
                                  float boost_factor = 5.0f) {
    MrfuCache::Params params;
    params.half_life = half_life;
    params.boost_factor = boost_factor;
    params.max_items = 3u;
    params.min_score = 0.01f;
    return params;
  }

  void ClearDisk() {
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  MrfuCacheProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    MrfuCacheProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const MrfuCacheProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  float boost_coeff(const MrfuCache& cache) { return cache.boost_coeff_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

TEST_F(MrfuCacheTest, UnusedItemHasScoreZero) {
  MrfuCache cache(GetProto(), TestingParams());
  Wait();

  EXPECT_FLOAT_EQ(cache.Get("A"), 0.0f);
}

TEST_F(MrfuCacheTest, CheckInitializeEmptyAndSize) {
  MrfuCache cache(GetProto(), TestingParams());
  EXPECT_FALSE(cache.initialized());
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());
  Wait();
  EXPECT_TRUE(cache.initialized());
  EXPECT_EQ(cache.size(), 0u);
  EXPECT_TRUE(cache.empty());

  cache.Use("A");
  EXPECT_EQ(cache.size(), 1u);
  EXPECT_FALSE(cache.empty());
}

TEST_F(MrfuCacheTest, UseAndGetOneItem) {
  MrfuCache cache(GetProto(), TestingParams());
  Wait();

  // Our boost_factor is set to 5, so it should take 5 consecutive uses for "A"
  // to get to score approximately 0.8.
  for (int i = 0; i < 5; ++i)
    cache.Use("A");
  EXPECT_NEAR(cache.Get("A"), 0.8f, kEps);
}

TEST_F(MrfuCacheTest, UseAndDecayItem) {
  MrfuCache cache(GetProto(), TestingParams());
  Wait();

  // Use "A" once and record that score.
  cache.Use("A");
  const float score = cache.Get("A");

  // Then use "B" another item 10 times. Because the half life is 10, the score
  // for "A" should have approximately halved.
  for (int i = 0; i < 10; ++i)
    cache.Use("B");
  EXPECT_NEAR(cache.Get("A"), score / 2.0f, kEps);
}

TEST_F(MrfuCacheTest, GetNormalized) {
  MrfuCache cache(GetProto(), TestingParams());
  Wait();

  EXPECT_FLOAT_EQ(cache.GetNormalized("A"), 0.0f);

  for (std::string item : {"A", "B", "A", "A", "B", "C"}) {
    cache.Use(item);
  }
  float a = cache.Get("A");
  float b = cache.Get("B");
  float c = cache.Get("C");
  float total = a + b + c;

  EXPECT_NEAR(cache.GetNormalized("A"), a / total, kEps);
  EXPECT_NEAR(cache.GetNormalized("B"), b / total, kEps);
  EXPECT_NEAR(cache.GetNormalized("C"), c / total, kEps);
}

TEST_F(MrfuCacheTest, GetAll) {
  MrfuCache cache(GetProto(),
                  TestingParams(/*half_life=*/1.0f, /*boost_factor=*/1.0f));
  EXPECT_TRUE(cache.GetAll().empty());
  Wait();
  EXPECT_TRUE(cache.GetAll().empty());

  for (std::string item : {"A", "B", "C"}) {
    cache.Use(item);
  }

  // These are hand-calculated scores.
  EXPECT_THAT(cache.GetAll(),
              UnorderedElementsAre(Pair("A", FloatNear(0.2f, kEps)),
                                   Pair("B", FloatNear(0.4f, kEps)),
                                   Pair("C", FloatNear(0.8f, kEps))));
}

TEST_F(MrfuCacheTest, GetAllNormalized) {
  MrfuCache cache(GetProto(),
                  TestingParams(/*half_life=*/1.0f, /*boost_factor=*/1.0f));
  EXPECT_TRUE(cache.GetAll().empty());
  Wait();
  EXPECT_TRUE(cache.GetAll().empty());

  for (std::string item : {"A", "B", "C"}) {
    cache.Use(item);
  }

  // These are hand-calculate scores.
  float total = 0.1666f + 0.333f + 0.666f;
  EXPECT_THAT(cache.GetAllNormalized(),
              UnorderedElementsAre(Pair("A", FloatNear(0.166f / total, kEps)),
                                   Pair("B", FloatNear(0.333f / total, kEps)),
                                   Pair("C", FloatNear(0.666f / total, kEps))));
}

TEST_F(MrfuCacheTest, CorrectBoostCoeffApproximation) {
  // This is a hand-optimized solution to the boost coefficient equation
  // accurate to 3 dp. It uses a half-life of 10 (so decay coefficient of about
  // 0.933033) and boost rate of 5.
  const float kExpected = 0.333f;

  MrfuCache cache(GetProto(), TestingParams());
  EXPECT_NEAR(boost_coeff(cache), kExpected, 0.001f);
}

TEST_F(MrfuCacheTest, GetAndUseBeforeInitComplete) {
  MrfuCache cache(GetProto(), TestingParams());

  // Get calls should return default values because init is incomplete.
  EXPECT_FLOAT_EQ(cache.Get("A"), 0.0f);
  EXPECT_FLOAT_EQ(cache.GetNormalized("A"), 0.0f);

  // The proto hasn't finished initializing from disk yet, so this use should be
  // ignored.
  cache.Use("A");

  // Now the class is finished initializing.
  Wait();

  // Get calls should return default values because the Use call was ignored.
  EXPECT_FLOAT_EQ(cache.Get("A"), 0.0f);
  EXPECT_FLOAT_EQ(cache.GetNormalized("A"), 0.0f);
}

TEST_F(MrfuCacheTest, CleanupOnTooManyItems) {
  MrfuCache cache(GetProto(), TestingParams());
  Wait();

  for (std::string item : {"A", "B", "C", "D", "E", "F"}) {
    cache.Use(item);
  }

  // We should have retained only D, E, F as the three highest-scoring items.
  float a = cache.Get("A");
  float b = cache.Get("B");
  float c = cache.Get("C");
  float d = cache.Get("D");
  float e = cache.Get("E");
  float f = cache.Get("F");
  EXPECT_GT(f, e);
  EXPECT_GT(e, d);
  EXPECT_GT(d, c);
  EXPECT_FLOAT_EQ(c, 0.0f);
  EXPECT_FLOAT_EQ(b, 0.0f);
  EXPECT_FLOAT_EQ(a, 0.0f);

  // There should only be three items on disk.
  Wait();
  MrfuCacheProto proto = ReadFromDisk();
  EXPECT_EQ(proto.items_size(), 3);

  // The total score should reflect the remaining three items.
  EXPECT_NEAR(proto.total_score(), d + e + f, kEps);
}

TEST_F(MrfuCacheTest, WriteToDisk) {
  // Create a cache and use some items, record the scores. Ensure it's written
  // to disk by calling Wait.
  float a_score;
  float b_score;
  {
    MrfuCache cache(GetProto(), TestingParams());
    Wait();
    cache.Use("A");
    cache.Use("B");
    a_score = cache.Get("A");
    b_score = cache.Get("B");
    Wait();
  }

  // Check the proto looks reasonable. We don't need to check all fields, as the
  // details of writing are tested by PersistentProto.
  MrfuCacheProto proto = ReadFromDisk();
  EXPECT_EQ(proto.items_size(), 2);
  EXPECT_FLOAT_EQ(proto.items().at("A").score(), a_score);
  EXPECT_FLOAT_EQ(proto.items().at("B").score(), b_score);
}

TEST_F(MrfuCacheTest, ReadFromDisk) {
  // Create a test proto and write it to disk.
  MrfuCacheProto proto;
  MrfuCacheProto::Score score;
  proto.set_version(2);
  proto.set_update_count(10);
  auto& items = *proto.mutable_items();
  score.set_score(0.5f);
  score.set_last_update_count(10);
  items["A"] = score;
  score.set_score(0.6f);
  score.set_last_update_count(10);
  items["B"] = score;

  WriteToDisk(proto);

  // Then create a cache and check the score values.
  MrfuCache cache(GetProto(), TestingParams());
  Wait();
  EXPECT_FLOAT_EQ(cache.Get("A"), 0.5f);
  EXPECT_FLOAT_EQ(cache.Get("B"), 0.6f);
}

TEST_F(MrfuCacheTest, Sort) {
  MrfuCache::Items items = {{"A", 0.5f}, {"B", 0.3f}, {"C", 0.4f}};
  MrfuCache::Sort(items);
  EXPECT_THAT(items,
              ElementsAre(Pair("A", 0.5f), Pair("C", 0.4f), Pair("B", 0.3f)));
}

TEST_F(MrfuCacheTest, ResetWithItems) {
  {
    MrfuCache cache(GetProto(), TestingParams());
    Wait();

    cache.Use("A");
    cache.ResetWithItems({{"B", 0.5f}, {"C", 0.6f}});
    EXPECT_THAT(cache.GetAll(),
                UnorderedElementsAre(Pair("B", 0.5f), Pair("C", 0.6f)));
    Wait();
  }

  // Check that the cache wrote to disk after the reset, and correctly set the
  // update count and total score.
  MrfuCacheProto proto = ReadFromDisk();
  EXPECT_EQ(proto.items_size(), 2);
  EXPECT_EQ(proto.update_count(), 0);
  EXPECT_FLOAT_EQ(proto.total_score(), 1.1f);
}

TEST_F(MrfuCacheTest, Delete) {
  {
    MrfuCache cache(GetProto(), TestingParams());
    Wait();
    cache.ResetWithItems({{"A", 0.1f}, {"B", 0.2f}});

    cache.Delete("A");
    EXPECT_THAT(cache.GetAll(), UnorderedElementsAre(Pair("B", 0.2f)));
    Wait();
  }

  // Check that the cache wrote to disk after the reset, and correctly set the
  // update count and total score.
  MrfuCacheProto proto = ReadFromDisk();
  EXPECT_EQ(proto.items_size(), 1);
  EXPECT_FLOAT_EQ(proto.total_score(), 0.2f);
}

}  // namespace app_list::test
