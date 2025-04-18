// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/frame_sorter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "cc/metrics/frame_info.h"
#include "cc/test/fake_frame_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

// Test class for FrameSorter
class FrameSorterTest : public testing::Test {
 public:
  FrameSorterTest()
      : frame_sorter_(base::BindRepeating(&FrameSorterTest::FlushFrame,
                                          base::Unretained(this))) {
    IncreaseSourceId();
  }
  ~FrameSorterTest() override = default;

  viz::BeginFrameArgs GetNextFrameArgs() {
    uint64_t sequence_number = next_frame_sequence_number_++;
    return viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, next_frame_source_id_, sequence_number,
        last_begin_frame_time_,
        last_begin_frame_time_ + viz::BeginFrameArgs::DefaultInterval(),
        viz::BeginFrameArgs::DefaultInterval(), viz::BeginFrameArgs::NORMAL);
  }

  void IncreaseSourceId() {
    next_frame_source_id_++;
    current_frame_id_ = -1;
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
    // Keeps track of how many times a frame is terminated.
    std::map<int, int> end_counters;

    for (auto& query : queries) {
      int id;
      base::StringToInt(query.substr(1), &id);
      if (id > current_frame_id_) {
        current_frame_id_++;
        args_.push_back(GetNextFrameArgs());
      }
      switch (query[0]) {
        case 'S':
          frame_sorter_.AddNewFrame(args_[id]);
          break;
        case 'D': {
          ++end_counters[id];
          FrameInfo info =
              CreateFakeFrameInfo(FrameInfo::FrameFinalState::kDropped);
          if (end_counters[id] == 1) {
            // For the first response to the frame, mark it as not including
            // update from the main-thread.
            info.main_thread_response = FrameInfo::MainThreadResponse::kMissing;
          } else {
            DCHECK_EQ(2, end_counters[id]);
            info.main_thread_response =
                FrameInfo::MainThreadResponse::kIncluded;
          }
          frame_sorter_.AddFrameResult(args_[id], info);
          break;
        }
        case 'P': {
          ++end_counters[id];
          FrameInfo info =
              CreateFakeFrameInfo(FrameInfo::FrameFinalState::kPresentedAll);
          if (end_counters[id] == 1) {
            // For the first response to the frame, mark it as not including
            // update from the main-thread.
            info.main_thread_response = FrameInfo::MainThreadResponse::kMissing;
          } else {
            DCHECK_EQ(2, end_counters[id]);
            info.main_thread_response =
                FrameInfo::MainThreadResponse::kIncluded;
          }
          frame_sorter_.AddFrameResult(args_[id], info);
          break;
        }
        case 'I':
          IncreaseSourceId();
          break;
        case 'R':
          frame_sorter_.Reset();
          break;
      }
    }
  }

  void ValidateResults(
      std::vector<std::pair<uint64_t, bool>> expected_results) {
    EXPECT_EQ(sorted_frames_.size(), expected_results.size());
    int result_index = 0;
    for (auto result : expected_results) {
      auto& sorted_frame = sorted_frames_[result_index];
      EXPECT_EQ(sorted_frame.first.frame_id.sequence_number, result.first + 1);
      EXPECT_EQ(sorted_frame.second, result.second);
      result_index++;
    }
  }

 private:
  void FlushFrame(const viz::BeginFrameArgs& args, const FrameInfo& frame) {
    sorted_frames_.emplace_back(args, frame.IsDroppedAffectingSmoothness());
  }

  FrameSorter frame_sorter_;
  std::vector<std::pair<const viz::BeginFrameArgs, bool>> sorted_frames_;
  base::TimeTicks last_begin_frame_time_ = base::TimeTicks::Now();
  uint64_t next_frame_source_id_ = 0;
  uint64_t next_frame_sequence_number_ =
      viz::BeginFrameArgs::kStartingFrameNumber;
  std::vector<viz::BeginFrameArgs> args_ = {};
  int current_frame_id_ = -1;
};

TEST_F(FrameSorterTest, TestSortingFrames) {
  // Frame end in order of F0, F2, F1, F3, but they should be flushed in the
  // order they began (eg. F0, F1, F2, F3).
  std::vector<std::string> queries = {"S0", "S1", "P0", "S2",
                                      "P2", "S3", "P1", "D3"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {0, false}, {1, false}, {2, false}, {3, true}};

  SimulateQueries(queries);
  ValidateResults(expected_results);
}

TEST_F(FrameSorterTest, ResetInMiddleOfAcks) {
  // Reset occur in between the start and ack of F1 & F3, so except these two
  // all other frames should be flushed in order in the sorted_frames_.
  std::vector<std::string> queries = {"S0", "S1", "P0", "S2", "P2", "S3",
                                      "R",  "S4", "P1", "D4", "D3"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {0, false}, {2, false}, {4, true}};

  SimulateQueries(queries);
  ValidateResults(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcks) {
  // F1 has multiple acks and the final ack is received at the end. Which makes
  // other frames to wait in pending tree before being flushed, all in order.
  // Also in any of the duplicated acks report a dropped frame, the overall
  // status for that frame should be dropped.
  std::vector<std::string> queries = {"S0", "S1", "P0", "S1", "S2", "P2",
                                      "S3", "S4", "P1", "D3", "D4", "D1"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {0, false}, {1, true}, {2, false}, {3, true}, {4, true}};

  SimulateQueries(queries);
  ValidateResults(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcksWithReset) {
  // Combination of last two tests. Reset occurs in middle of start and ack of
  // F1 and F3. Also F1 expects two acks, which both are received after reset.
  std::vector<std::string> queries = {"S0", "S1", "P0", "S1", "S2", "P2", "S3",
                                      "R",  "S4", "P1", "D1", "D4", "D3"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {
      {0, false}, {2, false}, {4, true}};

  SimulateQueries(queries);
  ValidateResults(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcksWithReset2) {
  // Reset occurs in middle of the two starts of F0.
  // This is to test if F1 will be flushed properly while being in between the
  // two acks of F0, which should be ignored as a result of reset.
  std::vector<std::string> queries = {"S0", "R", "S0", "P0", "S1", "D0", "D1"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {{1, true}};

  SimulateQueries(queries);
  ValidateResults(expected_results);
}

TEST_F(FrameSorterTest, ExpectingMultipleAcksWithSourceIdIncrease) {
  // Reset and increase in source_id occurs in middle start and ack of F1.
  // This is to simulate a navigation. So the initial F0 has source_id of 0
  // while the F0 after reset has source_id of 1, and because of that the
  // start and ack for F0 after the reset is not ignored (The ignore is only
  // needed when frame has the same source_id and sequence_number).
  std::vector<std::string> queries = {"S0", "S0", "S1", "S1", "I",  "R",
                                      "S0", "S0", "P0", "S1", "P1", "D0"};
  std::vector<std::pair<uint64_t, bool>> expected_results = {{0, true},
                                                             {1, false}};

  SimulateQueries(queries);
  ValidateResults(expected_results);
}

}  // namespace cc
