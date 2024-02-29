// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "cc/slim/delayed_scheduler.h"
#include "cc/slim/scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc::slim {

namespace {

class TestSchedulerClient : public SchedulerClient {
 public:
  explicit TestSchedulerClient(Scheduler* scheduler) : scheduler_(scheduler) {}

  bool DoBeginFrame(const viz::BeginFrameArgs& begin_frame_args) override {
    last_do_begin_frame_args_ = begin_frame_args;
    if (do_begin_frame_result_) {
      scheduler_->SetIsSwapThrottled(true);
    }
    return do_begin_frame_result_;
  }
  void SendDidNotProduceFrame(
      const viz::BeginFrameArgs& begin_frame_args) override {
    last_did_not_produce_frame_args_ = begin_frame_args;
  }

  void SetDoBeginFrameResult(bool result) { do_begin_frame_result_ = result; }
  std::optional<viz::BeginFrameArgs> TakeLastDoBeginFrameArgs() {
    auto rv = last_do_begin_frame_args_;
    last_do_begin_frame_args_.reset();
    return rv;
  }
  std::optional<viz::BeginFrameArgs> TakeLastDidNotProduceFrameArgs() {
    auto rv = last_did_not_produce_frame_args_;
    last_did_not_produce_frame_args_.reset();
    return rv;
  }

 private:
  const raw_ptr<Scheduler> scheduler_;
  std::optional<viz::BeginFrameArgs> last_do_begin_frame_args_;
  std::optional<viz::BeginFrameArgs> last_did_not_produce_frame_args_;
  bool do_begin_frame_result_ = false;
};

class SlimDelayedSchedulerTest : public testing::Test {
 public:
  void SetUp() override { scheduler_.Initialize(&client_); }

  viz::BeginFrameArgs GenBeginFrameArgs() {
    base::TimeTicks frame_time = base::TimeTicks::Now();
    base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();
    return viz::BeginFrameArgs::Create(BEGINFRAME_FROM_HERE,
                                       /*source_id=*/1, ++sequence_id_,
                                       frame_time, frame_time + interval,
                                       interval, viz::BeginFrameArgs::NORMAL);
  }

  void CheckDoBeginFrameCalledWith(const viz::BeginFrameArgs& args) {
    auto args_opt = client_.TakeLastDoBeginFrameArgs();
    ASSERT_TRUE(args_opt);
    EXPECT_EQ(args_opt->frame_id, args.frame_id);
  }

  void CheckDidNotProcueFrameCalledWith(const viz::BeginFrameArgs& args) {
    auto args_opt = client_.TakeLastDidNotProduceFrameArgs();
    ASSERT_TRUE(args_opt);
    EXPECT_EQ(args_opt->frame_id, args.frame_id);
  }

  void CheckDoBeginFrameNotCalled() {
    EXPECT_FALSE(client_.TakeLastDoBeginFrameArgs());
  }

  void CheckDidNotProcueFrameNotCalled() {
    EXPECT_FALSE(client_.TakeLastDidNotProduceFrameArgs());
  }

 protected:
  uint64_t sequence_id_ = 0;
  DelayedScheduler scheduler_;
  TestSchedulerClient client_{&scheduler_};
};

TEST_F(SlimDelayedSchedulerTest, DelayedBeginFrame) {
  scheduler_.SetNeedsBeginFrame(true);

  client_.SetDoBeginFrameResult(false);
  viz::BeginFrameArgs args1 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args1);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  viz::BeginFrameArgs args2 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args2);
  CheckDoBeginFrameCalledWith(args1);
  CheckDidNotProcueFrameCalledWith(args1);

  viz::BeginFrameArgs args3 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args3);
  CheckDoBeginFrameCalledWith(args2);
  CheckDidNotProcueFrameCalledWith(args2);

  client_.SetDoBeginFrameResult(true);
  viz::BeginFrameArgs args4 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args4);
  CheckDoBeginFrameCalledWith(args3);
  CheckDidNotProcueFrameNotCalled();
  scheduler_.SetIsSwapThrottled(false);

  viz::BeginFrameArgs args5 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args5);
  CheckDoBeginFrameCalledWith(args4);
  CheckDidNotProcueFrameNotCalled();
  scheduler_.SetIsSwapThrottled(false);

  scheduler_.SetNeedsBeginFrame(false);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameCalledWith(args5);
}

TEST_F(SlimDelayedSchedulerTest, MaybeCompositeNow) {
  scheduler_.SetNeedsBeginFrame(true);
  client_.SetDoBeginFrameResult(false);

  viz::BeginFrameArgs args1 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args1);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  scheduler_.MaybeCompositeNow();
  CheckDoBeginFrameCalledWith(args1);
  CheckDidNotProcueFrameCalledWith(args1);

  viz::BeginFrameArgs args2 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args2);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  scheduler_.MaybeCompositeNow();
  CheckDoBeginFrameCalledWith(args2);
  CheckDidNotProcueFrameCalledWith(args2);

  scheduler_.MaybeCompositeNow();
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  viz::BeginFrameArgs args3 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args3);
  // Missed MaybeCompositeNow so begin frame immediately.
  CheckDoBeginFrameCalledWith(args3);
  CheckDidNotProcueFrameCalledWith(args3);

  scheduler_.SetNeedsBeginFrame(false);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();
}

TEST_F(SlimDelayedSchedulerTest, SwapThrottled) {
  scheduler_.SetNeedsBeginFrame(true);
  client_.SetDoBeginFrameResult(true);

  viz::BeginFrameArgs args1 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args1);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  scheduler_.MaybeCompositeNow();
  CheckDoBeginFrameCalledWith(args1);
  CheckDidNotProcueFrameNotCalled();
  // Swap throttled at this point.

  viz::BeginFrameArgs args2 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args1);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  scheduler_.MaybeCompositeNow();
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  scheduler_.SetIsSwapThrottled(false);
  CheckDoBeginFrameCalledWith(args1);
  CheckDidNotProcueFrameNotCalled();

  scheduler_.SetNeedsBeginFrame(false);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();
}

TEST_F(SlimDelayedSchedulerTest, BeginFramePaused) {
  scheduler_.SetNeedsBeginFrame(true);
  client_.SetDoBeginFrameResult(true);

  viz::BeginFrameArgs args1 = GenBeginFrameArgs();
  scheduler_.OnBeginFrameFromViz(args1);
  CheckDoBeginFrameNotCalled();
  CheckDidNotProcueFrameNotCalled();

  scheduler_.OnBeginFramePausedChanged(true);
  CheckDoBeginFrameCalledWith(args1);
  CheckDidNotProcueFrameNotCalled();
}

}  // namespace

}  // namespace cc::slim
