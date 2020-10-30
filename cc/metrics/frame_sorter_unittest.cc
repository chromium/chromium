// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sorter.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FrameSorterTest : public testing::Test {
 public:
  FrameSorterTest()
      : frame_sorter_(base::BindRepeating(&FrameSorterTest::flush_frame,
                                          base::Unretained(this))) {
    IncreaseSourceId();
  }
  ~FrameSorterTest() override = default;

  const viz::BeginFrameArgs GetNextFrameArgs() {
    uint64_t sequence_number = next_frame_sequence_number_++;
    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, next_frame_source_id_, sequence_number,
        last_begin_frame_time_,
        last_begin_frame_time_ + viz::BeginFrameArgs::DefaultInterval(),
        viz::BeginFrameArgs::DefaultInterval(), viz::BeginFrameArgs::NORMAL);
  }

  void IncreaseSourceId() {
    next_frame_source_id_++;
    current_frame_id = -1;
    args_.clear();
    next_frame_sequence_number_ = viz::BeginFrameArgs::kStartingFrameNumber;
  }

  // Simulates a sequence of queries
  // Possible queries:
  // "S{frame_number}": Start frame.
  // "D{frame_number}": End frame as dropped.
  // "P{frame_number}": End frame as none-dropped (presented).
  // "E{frame_number}": Frame expects twp acks.
  // "I": Increase source_id (simulating a crash).
  // "R": Reset the frame sorter.
  // Method expects the start of frames to be in order starting with 1.
  void SimulateQueries(std::vector<std::string> queries) {
    for (auto& query : queries) {
      int id;
      base::StringToInt(query.substr(1), &id);
      id--;  // Vector is 0 based, frames start from 1.
      if (id > current_frame_id) {
        current_frame_id++;
        args_.push_back(GetNextFrameArgs());
      }
      switch (query[0]) {
        case 'S':
          frame_sorter_.AddNewFrame(args_[id]);
          break;
        case 'D':
          frame_sorter_.AddFrameResult(args_[id], true);
          break;
        case 'P':
          frame_sorter_.AddFrameResult(args_[id], false);
          break;
        case 'I':
          IncreaseSourceId();
          break;
        case 'R':
          frame_sorter_.Reset();
          break;
      }
    }
  }

  void compare_results(
      std::vector<std::pair<uint64_t, bool>> expected_results) {
    int result_index = 0;
    for (auto result : expected_results) {
      EXPECT_EQ(sorted_frames_[result_index].first.frame_id.sequence_number,
                result.first);
      EXPECT_EQ(sorted_frames_[result_index].second, result.second);
      result_index++;
    }
  }

  FrameSorter frame_sorter_;
  std::vector<std::pair<const viz::BeginFrameArgs, bool>> sorted_frames_;

 private:
  void flush_frame(const viz::BeginFrameArgs& args, bool is_dropped) {
    sorted_frames_.emplace_back(std::make_pair(args, is_dropped));
  }

  base::TimeTicks last_begin_frame_time_ = base::TimeTicks::Now();
  uint64_t next_frame_source_id_ = 0;
  uint64_t next_frame_sequence_number_ =
      viz::BeginFrameArgs::kStartingFrameNumber;
  std::vector<const viz::BeginFrameArgs> args_ = {};
  int current_frame_id = -1;
};

TEST_F(FrameSorterTest, TestSortingFrames) {
  std::vector<std::string> queries = {"S1", "S2", "P1", "S3",
                                      "P3", "S4", "P2", "D4"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {1, false}, {2, false}, {3, false}, {4, true}};

  SimulateQueries(queries);

  EXPECT_EQ(sorted_frames_.size(), 4u);
  compare_results(expected_results);
}

TEST_F(FrameSorterTest, ResetInMiddleOfAcks) {
  std::vector<std::string> queries = {"S1", "S2", "P1", "S3", "P3", "S4",
                                      "R",  "S5", "P2", "D5", "D4"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {1, false}, {3, false}, {5, true}};

  SimulateQueries(queries);

  EXPECT_EQ(sorted_frames_.size(), 3u);
  compare_results(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcks) {
  std::vector<std::string> queries = {"S1", "S2", "P1", "S2", "S3", "P3",
                                      "S4", "S5", "P2", "D5", "D4", "D2"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {1, false}, {2, true}, {3, false}, {4, true}, {5, true}};

  SimulateQueries(queries);

  EXPECT_EQ(sorted_frames_.size(), 5u);
  compare_results(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcksWithReset) {
  std::vector<std::string> queries = {"S1", "S2", "P1", "S2", "S3", "P3", "S4",
                                      "R",  "S5", "P2", "D2", "D5", "D4"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {1, false}, {3, false}, {5, true}};

  SimulateQueries(queries);

  EXPECT_EQ(sorted_frames_.size(), 3u);
  compare_results(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcksWithSourceIdIncrease) {
  std::vector<std::string> queries = {"S1", "S1", "P1", "S2", "S2", "I", "R",
                                      "S1", "S1", "P1", "S2", "S2", "D1"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {{1, true}};

  SimulateQueries(queries);

  EXPECT_EQ(sorted_frames_.size(), 1u);
  compare_results(expected_results);
}

}  // namespace cc
