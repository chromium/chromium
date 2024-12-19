// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_TEST_FRAME_SINK_HOST_TEST_BASE_H_
#define ASH_FRAME_SINK_TEST_FRAME_SINK_HOST_TEST_BASE_H_

#include <optional>

#include "ash/frame_sink/frame_sink_host.h"
#include "ash/frame_sink/test/test_begin_frame_source.h"
#include "ash/frame_sink/test/test_layer_tree_frame_sink.h"
#include "ash/test/ash_test_base.h"
#include "cc/trees/layer_tree_frame_sink.h"

namespace ash {

template <typename FrameSinkImpl>
class FrameSinkHostTestBase : public AshTestBase {
  static_assert(std::is_base_of_v<FrameSinkHost, FrameSinkImpl>,
                "FrameSinkImpl must be a derived class of FrameSinkHost");

 public:
  FrameSinkHostTestBase() = default;

  FrameSinkHostTestBase(const FrameSinkHostTestBase&) = delete;
  FrameSinkHostTestBase& operator=(const FrameSinkHostTestBase&) = delete;

  ~FrameSinkHostTestBase() = default;

  TestLayerTreeFrameSink* layer_tree_frame_sink() {
    CHECK(frame_sink_host_);
    return layer_tree_frame_sink_;
  }

  FrameSinkImpl* frame_sink_host() { return frame_sink_host_.get(); }
  aura::Window* host_window() { return host_window_; }

  int frame_sinks_created_count() const { return frame_sinks_created_counter_; }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    if (display_specs_) {
      UpdateDisplay(display_specs_.value());
    }

    auto* root_window = ash_test_helper()->GetHost()->window();
    gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                               nullptr, kShellWindowId_OverlayContainer);
    widget_->SetBounds(screen_bounds);
    host_window_ = widget_->GetNativeWindow();

    frame_sink_host_ = std::make_unique<FrameSinkImpl>();
    frame_sink_host_->InitForTesting(
        host_window_,
        base::BindRepeating(&FrameSinkHostTestBase::TestFrameSinkFactory,
                            base::Unretained(this)));

    begin_frame_source_ = std::make_unique<TestBeginFrameSource>();
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  void OnBeginFrame() {
    layer_tree_frame_sink()->client()->SetBeginFrameSource(
        begin_frame_source_.get());

    // Request a frame from FrameSinkHost.
    begin_frame_source_->GetBeginFrameObserver()->OnBeginFrame(
        CreateValidBeginFrameArgsForTesting());
  }

  void SetDisplaySpecs(const std::string& specs) { display_specs_ = specs; }

 private:
  std::unique_ptr<cc::LayerTreeFrameSink> TestFrameSinkFactory() {
    auto frame_sink = std::make_unique<TestLayerTreeFrameSink>();
    layer_tree_frame_sink_ = frame_sink.get();
    ++frame_sinks_created_counter_;
    return std::move(frame_sink);
  }

  std::optional<std::string> display_specs_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<aura::Window, DanglingUntriaged> host_window_;

  std::unique_ptr<FrameSinkImpl> frame_sink_host_;
  raw_ptr<TestLayerTreeFrameSink, DanglingUntriaged> layer_tree_frame_sink_;
  std::unique_ptr<TestBeginFrameSource> begin_frame_source_;

  int frame_sinks_created_counter_ = 0;
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_TEST_FRAME_SINK_HOST_TEST_BASE_H_
