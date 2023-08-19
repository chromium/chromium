// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rounded_display/rounded_display_host.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

class TestRoundedDisplayHost : public RoundedDisplayHost {
 public:
  explicit TestRoundedDisplayHost(GetGuttersCallback callback)
      : RoundedDisplayHost(std::move(callback)) {}

  TestRoundedDisplayHost(const TestRoundedDisplayHost&) = delete;
  TestRoundedDisplayHost& operator=(const TestRoundedDisplayHost&) = delete;

  ~TestRoundedDisplayHost() override = default;

  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrameForTest(
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) {
    return RoundedDisplayHost::CreateCompositorFrame(
        viz::BeginFrameAck::CreateManualAckWithDamage(), resource_manager_,
        /*auto_update=*/false, last_submitted_frame_size,
        last_submitted_frame_dsf);
  }

 private:
  UiResourceManager resource_manager_;
};

class RoundedDisplayHostTest : public AshTestBase {
 public:
  RoundedDisplayHostTest() = default;

  RoundedDisplayHostTest(const RoundedDisplayHostTest&) = delete;
  RoundedDisplayHostTest& operator=(const RoundedDisplayHostTest&) = delete;

  ~RoundedDisplayHostTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    auto host_window = std::make_unique<aura::Window>(/*delegate=*/nullptr);
    host_window_ = host_window.release();
    host_window_->Init(ui::LayerType::LAYER_SOLID_COLOR);

    auto* root_window = ash_test_helper()->GetHost()->window();
    root_window->AddChild(host_window_);

    host_ = std::make_unique<TestRoundedDisplayHost>(base::DoNothing());
    host_->Init(host_window_);
  }

  // AshTestBase:
  void TearDown() override {
    // Host needs to outlive the `host_window_`.
    host_.reset();
    AshTestBase::TearDown();
  }

 protected:
  raw_ptr<aura::Window, DanglingUntriaged> host_window_;
  std::unique_ptr<TestRoundedDisplayHost> host_;
};

TEST_F(RoundedDisplayHostTest, NewSurfaceIdIsCreatedWhenNeeded) {
  // RoundedDisplayHost creates the frame of the same size as the display size.
  UpdateDisplay("1920x1080");
  host_window_->SetBounds(gfx::Rect(1920, 1080));

  auto primary_display = display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size initial_frame_size = primary_display.GetSizeInPixel();
  float initial_device_scale_factor = primary_display.device_scale_factor();

  // First frame is created.
  auto frame = host_->CreateCompositorFrameForTest(initial_frame_size,
                                                   initial_device_scale_factor);
  auto frame_sink_id = host_window_->GetSurfaceId();
  ASSERT_TRUE(frame_sink_id.is_valid());

  UpdateDisplay("1920x1080*2");

  // Second frame is created but the device scale has changed since the last
  // submitted frame.
  auto old_frame_sink_id = frame_sink_id;
  frame = host_->CreateCompositorFrameForTest(frame->size_in_pixels(),
                                              frame->device_scale_factor());
  auto new_frame_sink_id = host_window_->GetSurfaceId();
  ASSERT_TRUE(new_frame_sink_id.is_valid());
  EXPECT_TRUE(new_frame_sink_id.IsNewerThan(old_frame_sink_id));

  UpdateDisplay("1920x1080*2/r");

  // Third frame is created but frame size has changed since the last submitted
  // frame.
  old_frame_sink_id = new_frame_sink_id;
  frame = host_->CreateCompositorFrameForTest(frame->size_in_pixels(),
                                              frame->device_scale_factor());
  new_frame_sink_id = host_window_->GetSurfaceId();
  ASSERT_TRUE(new_frame_sink_id.is_valid());
  EXPECT_TRUE(new_frame_sink_id.IsNewerThan(old_frame_sink_id));

  old_frame_sink_id = new_frame_sink_id;

  // Fourth frame is created but neither frame_size nor device_scale_factor has
  // changed.
  frame = host_->CreateCompositorFrameForTest(frame->size_in_pixels(),
                                              frame->device_scale_factor());
  new_frame_sink_id = host_window_->GetSurfaceId();
  ASSERT_TRUE(new_frame_sink_id.is_valid());

  // We should have not identified a new surface.
  EXPECT_EQ(new_frame_sink_id, old_frame_sink_id);
}

}  // namespace
}  // namespace ash
