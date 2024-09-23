// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/rounded_window_targeter.h"

#include <memory>

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace {

constexpr int kRadius = 80;

}  // namespace

class RoundedWindowTargeterTest : public aura::test::AuraTestBase {
 public:
  RoundedWindowTargeterTest() = default;
  RoundedWindowTargeterTest(const RoundedWindowTargeterTest&) = delete;
  RoundedWindowTargeterTest& operator=(const RoundedWindowTargeterTest&) =
      delete;
  ~RoundedWindowTargeterTest() override = default;

 protected:
  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    window_.reset(CreateNormalWindow(1, root_window(), &delegate_));
  }

  void TearDown() override {
    window_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  std::unique_ptr<aura::Window> window_;

 private:
  aura::test::TestWindowDelegate delegate_;
};

/*
Window shape (global coordinates)
(0,0)_____________
    |.   * | *    | <- mouse move (1,1)
    | *    |    * |
    |*_____|     *|
    |*     (r,r) *|
    | *        *  |
    |____*___*____|
                  (2r, 2r)
This mouse event hits the square but not the circular window targeter.*/
TEST_F(RoundedWindowTargeterTest, HitTestTopLeftCorner) {
  constexpr gfx::Point kTopLeftCorner(1, 1);
  {
    // Without the RoundedWindowTargeter, the event in the top-left corner
    // should target the window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, kTopLeftCorner,
                        kTopLeftCorner, ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window_.get(), move.target());
  }

  window_->SetEventTargeter(
      std::make_unique<ash::RoundedWindowTargeter>(kRadius));

  {
    // With the RoundedWindowTargeter, the event in the top-left corner should
    // fall through to the root window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, kTopLeftCorner,
                        kTopLeftCorner, ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }
}

/*
Window shape (global coordinates)
(0,0)_____________
    |    * | *    |
    | *    |    * |
    |*_____|     *| <- mouse move (r,r)
    |*     (r,r) *|
    | *        *  |
    |____*___*____|
                  (2r, 2r)
This mouse event hits both the square and the circular window targeter.*/
TEST_F(RoundedWindowTargeterTest, HitTestCenter) {
  constexpr gfx::Point kCenter(kRadius, kRadius);
  {
    // Without the RoundedWindowTargeter, the event in the center should target
    // the window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, kCenter, kCenter,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window_.get(), move.target());
  }

  window_->SetEventTargeter(
      std::make_unique<ash::RoundedWindowTargeter>(kRadius));

  {
    // With the RoundedWindowTargeter, the event in the center should still
    // target the window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, kCenter, kCenter,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(window_.get(), move.target());
  }
}

/*
Window shape (global coordinates)
(0,0)_____________
    |    * | *    |
    | *    |    * |
    |*_____|     *|
    |*     (r,r) *|
    | *        *  |
    |____*___*____|
                  (2r, 2r) <- <- mouse move (2r, 2r)
This mouse event misses both the square and the circular window targeter.*/
TEST_F(RoundedWindowTargeterTest, HitTestBottomRightCorner) {
  constexpr gfx::Point kBottomRightCorner(2 * kRadius, 2 * kRadius);
  {
    // Without the RoundedWindowTargeter, the event in the bottom-right corner
    // should fall through to the root window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, kBottomRightCorner,
                        kBottomRightCorner, ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }

  window_->SetEventTargeter(
      std::make_unique<ash::RoundedWindowTargeter>(kRadius));

  {
    // With the RoundedWindowTargeter, the event in the bottom-right corner
    // should also fall through to the root window.
    ui::MouseEvent move(ui::EventType::kMouseMoved, kBottomRightCorner,
                        kBottomRightCorner, ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
    ui::EventDispatchDetails details = GetEventSink()->OnEventFromSource(&move);
    ASSERT_FALSE(details.dispatcher_destroyed);
    EXPECT_EQ(root_window(), move.target());
  }
}
