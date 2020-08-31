// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/dropped_frame_counter.h"

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "cc/animation/animation_host.h"
#include "cc/test/layer_tree_test.h"

namespace cc {
namespace {

class DroppedFrameCounterTestBase : public LayerTreeTest {
 public:
  DroppedFrameCounterTestBase() = default;
  ~DroppedFrameCounterTestBase() override = default;

  virtual void SetUpTestConfigAndExpectations() = 0;

  void InitializeSettings(LayerTreeSettings* settings) override {
    settings->commit_to_active_tree = false;
  }

  void RunTest(CompositorMode mode) override {
    SetUpTestConfigAndExpectations();
    LayerTreeTest::RunTest(mode);
  }

  void BeginTest() override {
    ASSERT_GT(config_.animation_frames, 0u);

    // Start with requesting main-frames.
    PostSetNeedsCommitToMainThread();
  }

  void AfterTest() override {
    EXPECT_GE(total_frames_, config_.animation_frames);
    // It is possible to drop even more frame than what the test expects (e.g.
    // in slower machines, slower builds such as asan/tsan builds, etc.), since
    // the test does not strictly control both threads and deadlines. Therefore,
    // it is not possible to check for strict equality here.
    EXPECT_LE(expect_.min_dropped_main, dropped_main_);
    EXPECT_LE(expect_.min_dropped_compositor, dropped_compositor_);
    EXPECT_LE(expect_.min_dropped_smoothness, dropped_smoothness_);
  }

  // Compositor thread function overrides:
  void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                  const viz::BeginFrameArgs& args) override {
    // Request a re-draw, and set a non-empty damage region (otherwise the
    // draw is aborted with 'no damage').
    host_impl->SetNeedsRedraw();
    host_impl->SetViewportDamage(gfx::Rect(0, 0, 10, 20));

    // Request update from the main-thread too.
    host_impl->SetNeedsCommit();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    // If the main-thread is blocked, then unblock it once the compositor thread
    // has already drawn a frame.
    base::WaitableEvent* wait = nullptr;
    {
      base::AutoLock lock(wait_lock_);
      wait = wait_;
    }

    if (wait) {
      wait->Signal();
    }
  }

  void DidReceivePresentationTimeOnThread(
      LayerTreeHostImpl* host_impl,
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override {
    ++presented_frames_;
    if (presented_frames_ < config_.animation_frames)
      return;

    auto* dropped_frame_counter = host_impl->dropped_frame_counter();
    DCHECK(dropped_frame_counter);

    total_frames_ = dropped_frame_counter->total_frames();
    dropped_main_ = dropped_frame_counter->total_main_dropped();
    dropped_compositor_ = dropped_frame_counter->total_compositor_dropped();
    dropped_smoothness_ = dropped_frame_counter->total_smoothness_dropped();
    EndTest();
  }

  // Main-thread function overrides:
  void BeginMainFrame(const viz::BeginFrameArgs& args) override {
    bool should_wait = false;
    if (config_.should_drop_main_every > 0) {
      should_wait =
          args.frame_id.sequence_number % config_.should_drop_main_every == 0;
    }

    if (should_wait) {
      base::WaitableEvent wait{base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED};
      {
        base::AutoLock lock(wait_lock_);
        wait_ = &wait;
      }
      wait.Wait();
      {
        base::AutoLock lock(wait_lock_);
        wait_ = nullptr;
      }
    } else if (!TestEnded()) {
      // Make sure BeginMainFrame keeps being issued.
      layer_tree_host()->SetNeedsAnimate();
      if (config_.should_register_main_thread_animation) {
        animation_host()->SetAnimationCounts(1, true, true);
      }
    }
  }

 protected:
  // The test configuration options. This is set before the test starts, and
  // remains unchanged after that. So it is safe to read these fields from
  // either threads.
  struct TestConfig {
    uint32_t should_drop_main_every = 0;
    uint32_t animation_frames = 0;
    bool should_register_main_thread_animation = false;
  } config_;

  // The test expectations. This is set before the test starts, and
  // remains unchanged after that. So it is safe to read these fields from
  // either threads.
  struct TestExpectation {
    uint32_t min_dropped_main = 0;
    uint32_t min_dropped_compositor = 0;
    uint32_t min_dropped_smoothness = 0;
  } expect_;

 private:
  // This field is used only on the compositor thread to track how many frames
  // have been processed.
  uint32_t presented_frames_ = 0;

  // The |wait_| event is used when the test wants to deliberately force the
  // main-thread to block while processing begin-main-frames.
  base::Lock wait_lock_;
  base::WaitableEvent* wait_ = nullptr;

  // These fields are populated in the compositor thread when the desired number
  // of frames have been processed. These fields are subsequently compared
  // against the expectation after the test ends.
  uint32_t total_frames_ = 0;
  uint32_t dropped_main_ = 0;
  uint32_t dropped_compositor_ = 0;
  uint32_t dropped_smoothness_ = 0;
};

class DroppedFrameCounterNoDropTest : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterNoDropTest() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_register_main_thread_animation = false;

    expect_.min_dropped_main = 0;
    expect_.min_dropped_compositor = 0;
    expect_.min_dropped_smoothness = 0;
  }
};

MULTI_THREAD_TEST_F(DroppedFrameCounterNoDropTest);

class DroppedFrameCounterMainDropsNoSmoothness
    : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterMainDropsNoSmoothness() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_drop_main_every = 5;
    config_.should_register_main_thread_animation = false;

    expect_.min_dropped_main = 5;
    expect_.min_dropped_smoothness = 0;
  }
};

MULTI_THREAD_TEST_F(DroppedFrameCounterMainDropsNoSmoothness);

class DroppedFrameCounterMainDropsSmoothnessTest
    : public DroppedFrameCounterTestBase {
 public:
  ~DroppedFrameCounterMainDropsSmoothnessTest() override = default;

  void SetUpTestConfigAndExpectations() override {
    config_.animation_frames = 28;
    config_.should_drop_main_every = 5;
    config_.should_register_main_thread_animation = true;

    expect_.min_dropped_main = 5;
    expect_.min_dropped_smoothness = 5;
  }
};

MULTI_THREAD_TEST_F(DroppedFrameCounterMainDropsSmoothnessTest);

}  // namespace
}  // namespace cc
