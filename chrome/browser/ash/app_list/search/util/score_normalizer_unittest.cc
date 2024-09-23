// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/score_normalizer.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {
namespace {

constexpr double kDefaultScore = 0.5;

void ExpectBinsEqual(const ScoreNormalizerProto* proto,
                     const std::vector<double>& dividers,
                     const std::vector<double>& counts) {
  const auto& it = proto->normalizers().find("testing");
  if (it == proto->normalizers().end())
    FAIL();
  const auto& normalizer = it->second;

  ASSERT_EQ(dividers.size() + 1, counts.size());
  ASSERT_EQ(static_cast<size_t>(normalizer.bins_size()), counts.size());

  double total = 0.0;
  for (size_t i = 0; i < counts.size(); ++i) {
    const auto& bin = normalizer.bins().at(i);
    EXPECT_EQ(bin.count(), counts[i]);
    // The bottom bin has an automatically-created lower_divider of -infinity.
    EXPECT_EQ(bin.lower_divider(), i == 0 ? -INFINITY : dividers[i - 1]);
    total += counts[i];
  }
  EXPECT_EQ(normalizer.total(), total);
}

}  // namespace

class ScoreNormalizerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  ash::PersistentProto<ScoreNormalizerProto> GetProto() {
    return ash::PersistentProto<ScoreNormalizerProto>(GetPath(),
                                                      base::Seconds(0));
  }

  ScoreNormalizer::Params TestingParams(size_t bins = 4) {
    ScoreNormalizer::Params params;
    params.version = 3;
    params.max_bins = bins;
    return params;
  }

  ScoreNormalizerProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    ScoreNormalizerProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const std::vector<double>& dividers,
                   const std::vector<double>& counts) {
    ASSERT_EQ(dividers.size() + 1, counts.size());

    ScoreNormalizerProto proto;
    proto.set_model_version(1);
    proto.set_parameter_version(TestingParams().version);

    auto& normalizer = (*proto.mutable_normalizers())["testing"];
    double total = 0.0;
    for (size_t i = 0; i < counts.size(); ++i) {
      auto& bin = *normalizer.add_bins();
      // The bottom bin has an automatically-created lower_divider of -infinity.
      bin.set_lower_divider(i == 0 ? -INFINITY : dividers[i - 1]);
      bin.set_count(counts[i]);
      total += counts[i];
    }
    normalizer.set_total(total);

    WriteToDisk(proto);
  }

  void WriteToDisk(const ScoreNormalizerProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  ScoreNormalizerProto* get_proto(ScoreNormalizer& normalizer) {
    return normalizer.proto_.get();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

// A dummy test to ensure that the score normalizer doesn't crash on
// initialization.
TEST_F(ScoreNormalizerTest, Initialize) {
  ScoreNormalizer normalizer(GetProto(), TestingParams());
  Wait();
  SUCCEED();
}

TEST_F(ScoreNormalizerTest, StateClearedOnVersionChange) {
  {
    ScoreNormalizerProto proto;
    proto.set_parameter_version(1);
    WriteToDisk(proto);
  }

  {
    ScoreNormalizer normalizer(GetProto(), TestingParams());
    Wait();
    EXPECT_EQ(get_proto(normalizer)->parameter_version(), 3);
    // TODO(crbug.com/40177716): Check that state is reset once it's added to
    // the proto.
  }
}

// The first N scores don't do any bin merging/splitting logic, they just fill
// out the bins. Check we do this correctly.
TEST_F(ScoreNormalizerTest, PopulateBins) {
  ScoreNormalizer normalizer(GetProto(), TestingParams());
  Wait();
  normalizer.Update("testing", 1);
  normalizer.Update("testing", 2);
  normalizer.Update("testing", 3);

  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {0, 1, 1, 1});
}

// Updates to the bins should end up on disk.
TEST_F(ScoreNormalizerTest, BinUpdatesWrittenToDisk) {
  {
    ScoreNormalizer normalizer(GetProto(), TestingParams());
    Wait();
    normalizer.Update("testing", 1);
    normalizer.Update("testing", 2);
    normalizer.Update("testing", 3);
    Wait();
  }

  auto proto = ReadFromDisk();
  ExpectBinsEqual(&proto, {1, 2, 3}, {0, 1, 1, 1});
}

// Add scores in such a way that bins should never split.
TEST_F(ScoreNormalizerTest, AddScoresWithoutSplitting) {
  ScoreNormalizer normalizer(GetProto(), TestingParams());
  Wait();
  normalizer.Update("testing", 1);
  normalizer.Update("testing", 2);
  normalizer.Update("testing", 3);

  // Max entropy.
  normalizer.Update("testing", 0);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {1, 1, 1, 1});

  // These three are balanced, we can achieve equal entropy by doing a
  // split/merge, but not improve it. This shouldn't happen.
  normalizer.Update("testing", -100);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {2, 1, 1, 1});
  normalizer.Update("testing", 1.2);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {2, 2, 1, 1});
  normalizer.Update("testing", 2.3);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {2, 2, 2, 1});

  // Back to max entropy.
  normalizer.Update("testing", 400);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {2, 2, 2, 2});

  // Any split is lower entropy.
  normalizer.Update("testing", 2.5);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {2, 2, 3, 2});

  // Back to balance.
  normalizer.Update("testing", 2.1);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {2, 2, 4, 2});
}

// Add scores to cause one split/merge, where the split index is to the left of
// the merge index. In this case the split and merge and directly next to each
// other.
TEST_F(ScoreNormalizerTest, SmallLefthandedSplitMerge) {
  ScoreNormalizer normalizer(GetProto(), TestingParams());
  Wait();

  // Set up some bins.
  for (const auto& score : {1, 2, 3, 0, 1})
    normalizer.Update("testing", score);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {1, 2, 1, 1});

  // This should split/merge into:
  //     1     1     2
  // 1.0 | 1.5 | 1.5 | 2.0
  normalizer.Update("testing", 1);
  ExpectBinsEqual(get_proto(normalizer), {1, 1, 2}, {1.0, 1.5, 1.5, 2.0});
}

// Add scores to cause one split/merge, where the split index is to the left of
// the merge index. In this case there's a gap between the split and merge.
TEST_F(ScoreNormalizerTest, LargeLefthandedSplitMerge) {
  auto params = TestingParams();
  params.max_bins = 5;
  ScoreNormalizer normalizer(GetProto(), params);
  Wait();

  // Set up some bins.
  for (const auto& score : {1, 2, 3, 4, 0, 0, 1, 2, 3})
    normalizer.Update("testing", score);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3, 4}, {2, 2, 2, 2, 1});

  // This should split/merge into:
  //     0     1     2     3
  // 1.5 | 1.5 | 2.0 | 2.0 | 3.0
  normalizer.Update("testing", 0);
  ExpectBinsEqual(get_proto(normalizer), {0, 1, 2, 3},
                  {1.5, 1.5, 2.0, 2.0, 3.0});
}

// Add scores to cause one split/merge, where the split index is to the right of
// the merge index. In this case the split and merge and directly next to each
// other.
TEST_F(ScoreNormalizerTest, SmallRighthandedSplitMerge) {
  ScoreNormalizer normalizer(GetProto(), TestingParams());
  Wait();

  // Set up some bins.
  for (const auto& score : {1, 2, 3, 0, 2})
    normalizer.Update("testing", score);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3}, {1, 1, 2, 1});

  // This should split/merge into:
  //     2     3     3
  // 1.0 | 1.5 | 1.5 | 2.0
  normalizer.Update("testing", 2);
  ExpectBinsEqual(get_proto(normalizer), {2, 2, 3}, {2.0, 1.5, 1.5, 1.0});
}

// Add scores to cause one split/merge, where the split index is to the right of
// the merge index. In this case there's a gap between the split and merge.
TEST_F(ScoreNormalizerTest, LargeRighthandedSplitMerge) {
  ScoreNormalizer normalizer(GetProto(), TestingParams(5));
  Wait();

  // Set up some bins.
  for (const auto& score : {1, 2, 3, 4, 0, 1, 2, 3, 4})
    normalizer.Update("testing", score);
  ExpectBinsEqual(get_proto(normalizer), {1, 2, 3, 4}, {1, 2, 2, 2, 2});

  // This should split/merge into:
  //     2     3     4     5
  // 3.0 | 2.0 | 2.0 | 1.5 | 1.5
  normalizer.Update("testing", 5);
  ExpectBinsEqual(get_proto(normalizer), {2, 3, 4, 5},
                  {3.0, 2.0, 2.0, 1.5, 1.5});
}

// Tests early-exit cases when normalizing a score.
TEST_F(ScoreNormalizerTest, NormalizeSpecialCases) {
  {
    // A normalizer that hasn't finished initializing.
    ScoreNormalizer normalizer(GetProto(), TestingParams(5));
    EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 1.0), kDefaultScore);
  }

  {
    // An empty normalizer
    ScoreNormalizer normalizer(GetProto(), TestingParams(5));
    Wait();
    EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 1.0), kDefaultScore);
  }

  {
    // A normalizer with two bins
    ScoreNormalizer normalizer(GetProto(), TestingParams(5));
    Wait();
    normalizer.Update("testing", 1.0);
    EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 1.0), kDefaultScore);
  }
}

// Tests normalizing scores in the leftmost and rightmost bins.
TEST_F(ScoreNormalizerTest, NormalizeEdgeBins) {
  ScoreNormalizer normalizer(GetProto(), TestingParams(5));
  Wait();

  // Bin dividers are: -infinity, 2, 3, 4, 5
  for (const auto& score : {1, 2, 3, 4, 0, 1, 2, 3, 4, 5})
    normalizer.Update("testing", score);

  // Very close to the leftmost bin's right boundary.
  EXPECT_NEAR(normalizer.Normalize("testing", 2), 0.2 - 1.0e-5, 1.0e-3);
  // Partway into the leftmost bin, offset should be 2/3.
  EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 1.5), 2.0 / 15.0);
  // Far out into the leftmost bin, the score should tend to 0.
  EXPECT_NEAR(normalizer.Normalize("testing", -10000), 0.0, 1.0e-3);

  // Exactly on the rightmost bin's left boundary.
  EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 5), 0.8);
  // Partway into the rightmost bin, offset should be 2/3.
  EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 5.5), 13.0 / 15.0);
  // Far out into the rightmost bin, the score should tend to 1.
  EXPECT_NEAR(normalizer.Normalize("testing", 10000), 1.0, 1.0e-3);
}

// Tests normalizing scores in the non-leftmost and non-rightmost bins.
TEST_F(ScoreNormalizerTest, NormalizeMiddleBins) {
  ScoreNormalizer normalizer(GetProto(), TestingParams(5));
  Wait();

  // Bin dividers are: -infinity, 2, 3, 4, 5
  for (const auto& score : {1, 2, 3, 4, 0, 1, 2, 3, 4, 5})
    normalizer.Update("testing", score);

  // Halfway through the second bin:
  // - bin index is 1
  // - offset is 0.5
  // So score should be (1 + 0.5) / 5
  EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 2.5), 0.3);

  // Partway through the fourth bin:
  // - bin index is 3
  // - offset is 1/3
  // So score should be (3 + 1/3) / 5
  EXPECT_FLOAT_EQ(normalizer.Normalize("testing", 4.0 + 1.0 / 3.0), 2.0 / 3.0);
}

}  // namespace app_list::test
