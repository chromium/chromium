// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_host.h"

#include <memory>

#include "ash/frame_sink/test/test_begin_frame_source.h"
#include "ash/frame_sink/test/test_layer_tree_frame_sink.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class TestFrameSinkHost : public FrameSinkHost {
 public:
  TestFrameSinkHost() = default;

  TestFrameSinkHost(const TestFrameSinkHost&) = delete;
  TestFrameSinkHost& operator=(const TestFrameSinkHost&) = delete;

  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_refresh,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) override {
    return std::make_unique<viz::CompositorFrame>();
  }

  void OnFirstFrameRequested() override { ++on_first_frame_requested_counter_; }

  int on_first_frame_requested_counter() const {
    return on_first_frame_requested_counter_;
  }

 private:
  int on_first_frame_requested_counter_ = 0;
};

class FrameSinkHostTest : public AshTestBase {
 public:
  FrameSinkHostTest() = default;
  FrameSinkHostTest(const FrameSinkHostTest&) = delete;
  FrameSinkHostTest& operator=(const FrameSinkHostTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto* root_window = ash_test_helper()->GetHost()->window();
    gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                         nullptr, kShellWindowId_OverlayContainer);
    widget_->SetBounds(screen_bounds);
    host_window_ = widget_->GetNativeWindow();

    auto layer_tree_frame_sink = std::make_unique<TestLayerTreeFrameSink>();
    layer_tree_frame_sink_ = layer_tree_frame_sink.get();

    frame_sink_host_ = std::make_unique<TestFrameSinkHost>();
    frame_sink_host_->InitForTesting(host_window_,
                                     std::move(layer_tree_frame_sink));

    begin_frame_source_ = std::make_unique<TestBeginFrameSource>();
    layer_tree_frame_sink_->client()->SetBeginFrameSource(
        begin_frame_source_.get());
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  void OnBeginFrame() {
    // Request a frame from FrameSinkHost.
    begin_frame_source_->GetBeginFrameObserver()->OnBeginFrame(
        CreateValidBeginFrameArgsForTesting());
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<aura::Window, DanglingUntriaged> host_window_;
  std::unique_ptr<TestFrameSinkHost> frame_sink_host_;
  raw_ptr<TestLayerTreeFrameSink, DanglingUntriaged> layer_tree_frame_sink_;
  std::unique_ptr<TestBeginFrameSource> begin_frame_source_;
};

TEST_F(FrameSinkHostTest, OnFirstFrameRequestedShouldOnlyBeCalledOnce) {
  EXPECT_EQ(frame_sink_host_->on_first_frame_requested_counter(), 0);

  // Request the first frame.
  OnBeginFrame();
  // `FrameSinkHost::OnFirstFrameRequested` should be called.
  EXPECT_EQ(frame_sink_host_->on_first_frame_requested_counter(), 1);

  // Request the second frame.
  OnBeginFrame();
  // `FrameSinkHost::OnFirstFrameRequested` should not be called again.
  EXPECT_EQ(frame_sink_host_->on_first_frame_requested_counter(), 1);
}

}  // namespace
}  // namespace ash
