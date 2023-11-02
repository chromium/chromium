// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/begin_frame_source_webview.h"

#include "components/viz/test/begin_frame_source_test.h"

namespace android_webview {
namespace {

class TestBeginFrameSource : public viz::ExternalBeginFrameSource,
                             public viz::ExternalBeginFrameSourceClient {
 public:
  TestBeginFrameSource() : ExternalBeginFrameSource(this) {}
  ~TestBeginFrameSource() override = default;

  void OnNeedsBeginFrames(bool needs_begin_frames) override {
    needs_begin_frames_ = needs_begin_frames;
  }

  bool needs_begin_frames() const { return needs_begin_frames_; }

 private:
  bool needs_begin_frames_ = false;
};

}  // namespace

class BeginFrameSourceWebViewTest : public ::testing::Test {
 public:
  BeginFrameSourceWebViewTest() {
    root_begin_frame_source_.ObserveBeginFrameSource(&test_begin_frame_source_);
  }

  viz::BeginFrameArgs BeginFrameArgsForTesting(uint64_t sequence) {
    return viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 1,
                                               sequence);
  }

 protected:
  BeginFrameSourceWebView begin_frame_source_;
  RootBeginFrameSourceWebView root_begin_frame_source_;

  // Test classes used to issue and observe begin frames.
  testing::NiceMock<viz::MockBeginFrameObserver> observer_;
  TestBeginFrameSource test_begin_frame_source_;
};

TEST_F(BeginFrameSourceWebViewTest, RootBeginFrame) {
  EXPECT_FALSE(test_begin_frame_source_.needs_begin_frames());

  // Send BeginFrame without observer.
  auto args1 = BeginFrameArgsForTesting(1);
  test_begin_frame_source_.OnBeginFrame(args1);

  root_begin_frame_source_.AddObserver(&observer_);
  EXPECT_TRUE(test_begin_frame_source_.needs_begin_frames());

  // Send BeginFrame with observer, verify it gets it.
  auto args2 = BeginFrameArgsForTesting(2);
  EXPECT_BEGIN_FRAME_ARGS_USED(observer_, args2);
  test_begin_frame_source_.OnBeginFrame(args2);

  root_begin_frame_source_.RemoveObserver(&observer_);
  EXPECT_FALSE(test_begin_frame_source_.needs_begin_frames());
}

TEST_F(BeginFrameSourceWebViewTest, RootPausedWithObservers) {
  // External BFS is not paused => observer will be unpaused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  root_begin_frame_source_.AddObserver(&observer_);

  // External BFS is paused => observer will be paused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(true);

  // External BFS is unpaused => observer will be unpaused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(false);
  root_begin_frame_source_.RemoveObserver(&observer_);
}

TEST_F(BeginFrameSourceWebViewTest, RootPausedWithoutObservers) {
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(true);

  // External BFS is paused => observer will be paused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  root_begin_frame_source_.AddObserver(&observer_);

  // External BFS is unpaused => observer will be unpaused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(false);

  root_begin_frame_source_.RemoveObserver(&observer_);

  // External BFS is unpaused => observer will be unpaused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  root_begin_frame_source_.AddObserver(&observer_);

  root_begin_frame_source_.RemoveObserver(&observer_);
}

TEST_F(BeginFrameSourceWebViewTest, ChildBeginFrame) {
  begin_frame_source_.SetParentSource(&root_begin_frame_source_);

  EXPECT_FALSE(test_begin_frame_source_.needs_begin_frames());

  // Send BeginFrame without observer.
  auto args1 = BeginFrameArgsForTesting(1);
  test_begin_frame_source_.OnBeginFrame(args1);

  begin_frame_source_.AddObserver(&observer_);
  EXPECT_TRUE(test_begin_frame_source_.needs_begin_frames());

  // Send BeginFrame with observer, verify it gets it.
  auto args2 = BeginFrameArgsForTesting(2);
  EXPECT_BEGIN_FRAME_ARGS_USED(observer_, args2);
  test_begin_frame_source_.OnBeginFrame(args2);

  begin_frame_source_.RemoveObserver(&observer_);
  EXPECT_FALSE(test_begin_frame_source_.needs_begin_frames());
}

TEST_F(BeginFrameSourceWebViewTest, ChildPausedWithObservers) {
  begin_frame_source_.SetParentSource(&root_begin_frame_source_);

  // External BFS is not paused => observer will be not paused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  begin_frame_source_.AddObserver(&observer_);

  // External BFS paused => observer will be paused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(true);

  // External BFS unpaused => observer will be unpaused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(false);
  begin_frame_source_.RemoveObserver(&observer_);
}

TEST_F(BeginFrameSourceWebViewTest, ChildPausedWithoutObservers) {
  begin_frame_source_.SetParentSource(&root_begin_frame_source_);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(true);

  // External BFS is paused => observer will be paused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  begin_frame_source_.AddObserver(&observer_);

  // External BFS unpaused => observer will be unpaused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(false);

  begin_frame_source_.RemoveObserver(&observer_);

  // External BFS is unpaused => observer will be unpaused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  begin_frame_source_.AddObserver(&observer_);

  begin_frame_source_.RemoveObserver(&observer_);
}

TEST_F(BeginFrameSourceWebViewTest, ChildPausedNoParent) {
  // BFS doesn't have parent so it is paused => observer will be paused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  begin_frame_source_.AddObserver(&observer_);

  // As there is no parent we are always paused, so we don't expect the call.
  EXPECT_CALL(observer_, OnBeginFrameSourcePausedChanged(testing::_)).Times(0);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(true);

  // As there is no parent we are always paused, so we don't expect the call.
  EXPECT_CALL(observer_, OnBeginFrameSourcePausedChanged(testing::_)).Times(0);
  test_begin_frame_source_.OnSetBeginFrameSourcePaused(false);

  begin_frame_source_.RemoveObserver(&observer_);
}

TEST_F(BeginFrameSourceWebViewTest, ChildPauseChangeOnSetParent) {
  // BFS doesn't have parent so it is paused => observer will be paused on Add.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  begin_frame_source_.AddObserver(&observer_);

  // We set parent and it's unpaused => observer will be unpaused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, false);
  begin_frame_source_.SetParentSource(&root_begin_frame_source_);

  // We remove parent so BFS becomes paused => observer will be paused.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(observer_, true);
  begin_frame_source_.SetParentSource(nullptr);

  test_begin_frame_source_.OnSetBeginFrameSourcePaused(true);

  // We set parent and it's paused => observer will be paused.
  // As it's already paused we don't expect the call.
  EXPECT_CALL(observer_, OnBeginFrameSourcePausedChanged(testing::_)).Times(0);
  begin_frame_source_.SetParentSource(&root_begin_frame_source_);

  begin_frame_source_.RemoveObserver(&observer_);
}

TEST_F(BeginFrameSourceWebViewTest, Reentrancy) {
  begin_frame_source_.SetParentSource(&root_begin_frame_source_);
  begin_frame_source_.AddObserver(&observer_);

  // Re-Add observer inside OnBeginFrame so it will trigger missed BeginFrame
  EXPECT_CALL(observer_, OnBeginFrame(testing::_))
      .WillRepeatedly(testing::Invoke([&](const viz::BeginFrameArgs& args) {
        if (args.type == viz::BeginFrameArgs::MISSED)
          return;
        begin_frame_source_.RemoveObserver(&observer_);
        begin_frame_source_.AddObserver(&observer_);
      }));

  test_begin_frame_source_.OnBeginFrame(BeginFrameArgsForTesting(1));

  begin_frame_source_.RemoveObserver(&observer_);
}

}  // namespace android_webview
