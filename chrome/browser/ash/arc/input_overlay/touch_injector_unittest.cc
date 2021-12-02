// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/json/json_reader.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_tap_key.h"
#include "chrome/browser/ash/arc/input_overlay/input_overlay_resources_util.h"
#include "chrome/browser/ash/arc/input_overlay/test/event_capturer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc {
namespace {

constexpr float kScaleFactor = 1.5f;

constexpr const char kValidJsonActionTapKey[] =
    R"json({
      "tap": {
        "keyboard": [
          {
            "name": "Fight",
            "key": "KeyA",
            "location": [
              {
                "type": "position",
                "anchor": [
                  0,
                  0
                ],
                "anchor_to_target": [
                  0.5,
                  0.5
                ]
              },
              {
                "type": "position",
                "anchor": [
                  0,
                  0
                ],
                "anchor_to_target": [
                  0.3,
                  0.3
                ]
              }
            ]
          },
          {
            "name": "Run",
            "key": "KeyB",
            "location": [
              {
                "type": "position",
                "anchor_to_target": [
                  0.8,
                  0.8
                ]
              }
            ]
          }
        ]
      }
    })json";

constexpr const char kValidJsonActionMoveKey[] =
    R"json({
      "move": {
        "keyboard": [
          {
            "name": "Virtual Joystick",
            "keys": [
              "KeyW",
              "KeyA",
              "KeyS",
              "KeyD"
            ],
            "move_distance": 10,
            "location": [
              {
                "type": "position",
                "anchor": [
                  0,
                  0
                ],
                "anchor_to_target": [
                  0.5,
                  0.5
                ]
              }
            ]
          }
        ]
      }
    })json";
}  // namespace

class TouchInjectorTest : public views::ViewsTestBase {
 protected:
  TouchInjectorTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  std::unique_ptr<input_overlay::ActionTapKey> CreateActionTapKey(
      base::StringPiece json,
      aura::Window* window) {
    base::JSONReader::ValueWithError json_value =
        base::JSONReader::ReadAndReturnValueWithError(json);
    auto action = std::make_unique<input_overlay::ActionTapKey>(window);
    action->ParseFromJson(json_value.value.value());
    return action;
  }

  std::unique_ptr<views::Widget> CreateArcWindow() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(200, 100, 200, 400);
    params.context = root_window();
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    auto widget = std::make_unique<views::Widget>();
    widget->Init(std::move(params));
    widget->widget_delegate()->SetCanResize(true);
    widget->SetBounds(gfx::Rect(200, 100, 200, 400));
    auto app_id = absl::optional<std::string>("app_id");
    widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, *app_id);
    widget->GetNativeWindow()->SetProperty(
        aura::client::kAppType, static_cast<int>(ash::AppType::ARC_APP));
    widget->Show();
    widget->Activate();

    return widget;
  }

  bool IsPointsEqual(gfx::PointF& point_a, const gfx::PointF& point_b) {
    if (std::abs(point_a.x() - point_b.x()) < 0.0001 &&
        std::abs(point_a.y() - point_b.y()) < 0.0001) {
      return true;
    }
    return false;
  }

  aura::TestScreen* test_screen() {
    return aura::test::AuraTestHelper::GetInstance()->GetTestScreen();
  }

  aura::Window* root_window() { return GetContext(); }

  input_overlay::test::EventCapturer event_capturer_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<views::Widget> widget_;
  int caption_height_;
  std::unique_ptr<TouchInjector> injector_;

 private:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(root_window());

    test_screen()->SetDeviceScaleFactor(kScaleFactor);
    root_window()->SetBounds(gfx::Rect(800, 600));
    root_window()->AddPostTargetHandler(&event_capturer_);

    widget_ = CreateArcWindow();
    caption_height_ = -widget_->non_client_view()
                           ->frame_view()
                           ->GetWindowBoundsForClientBounds(gfx::Rect())
                           .y();
    injector_ = std::make_unique<TouchInjector>(widget_->GetNativeWindow());
  }

  void TearDown() override {
    injector_.reset();
    widget_->CloseNow();

    root_window()->RemovePreTargetHandler(&event_capturer_);

    event_generator_.reset();
    event_capturer_.Clear();

    views::ViewsTestBase::TearDown();
  }
};

TEST_F(TouchInjectorTest, TestEventRewriterActionTapKey) {
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value.value.value());
  auto* actionA = injector_->actions()[0].get();
  auto* actionB = injector_->actions()[1].get();
  injector_->RegisterEventRewriter();

  // Press and release key A, it should receive touch event, not key event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_TRUE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expectA1 =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_TRUE(IsPointsEqual(expectA1, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_FALSE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectA1, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);
  // Next touch position.
  EXPECT_EQ(1, actionA->current_position_index());
  // Unregister the event rewriter to see if extra events are sent.
  injector_->UnRegisterEventRewriter();

  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event_capturer_.Clear();
  injector_->RegisterEventRewriter();

  // Press and release key C, it should receive key event, not touch event.
  event_generator_->PressKey(ui::VKEY_C, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.key_events().size());
  event_generator_->ReleaseKey(ui::VKEY_C, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.key_events().size());
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release A -> Release B.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expectB =
      gfx::PointF(360, 100 + (400 - caption_height_) * 0.8 + caption_height_);
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expectA2 =
      gfx::PointF(260, 100 + (400 - caption_height_) * 0.3 + caption_height_);
  EXPECT_TRUE(IsPointsEqual(expectA2, event->root_location_f()));
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectA2, event->root_location_f()));
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release B -> Release A.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectA1, event->root_location_f()));
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectA1, event->root_location_f()));
  EXPECT_EQ(1, event->pointer_details().id);
  event_capturer_.Clear();

  // Test repeat key and it should receive only one touch event.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_generator_->PressKey(ui::VKEY_B, ui::EF_IS_REPEAT, 1);
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_capturer_.Clear();

  // Test cancel key.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(actionB->touch_id());
  injector_->UnRegisterEventRewriter();
  EXPECT_FALSE(actionB->touch_id());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events().back().get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_CANCELLED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();
  // Register the event-rewriter and press key again.
  injector_->RegisterEventRewriter();
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(actionB->touch_id());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectB, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_capturer_.Clear();
}

TEST_F(TouchInjectorTest, TestEventRewriterActionMoveKey) {
  base::JSONReader::ValueWithError json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionMoveKey);
  injector_->ParseActions(json_value.value.value());

  EXPECT_TRUE(injector_->actions().size() == 1);
  auto* action = injector_->actions()[0].get();
  injector_->RegisterEventRewriter();

  // Press key A and generate touch down and move left event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_TRUE(*(action->touch_id()) == 0);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  // Wait for touch move event.
  task_environment()->FastForwardBy(input_overlay::kSendTouchMoveDelay);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 2);
  // Generate touch down event.
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  gfx::PointF expect =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_TRUE(IsPointsEqual(expect, event->root_location_f()));
  // Generate touch move left event.
  gfx::PointF expectA = gfx::PointF(expect);
  expectA.Offset(-10, 0);
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectA, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);

  // Press key W (move left + up) and generate touch move (up and left) event.
  event_generator_->PressKey(ui::VKEY_W, ui::EF_NONE, 1);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 3);
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  auto expectW = gfx::PointF(expectA);
  expectW.Offset(0, -10);
  EXPECT_TRUE(IsPointsEqual(expectW, event->root_location_f()));

  // Release key A and generate touch move up event (Key W is still pressed).
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 4);
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  expectW = gfx::PointF(expect);
  expectW.Offset(0, -10);
  EXPECT_TRUE(IsPointsEqual(expectW, event->root_location_f()));

  // Press key D and generate touch move (up and right) event.
  event_generator_->PressKey(ui::VKEY_D, ui::EF_NONE, 1);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 5);
  event = event_capturer_.touch_events()[4].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  auto expectD = gfx::PointF(expectW);
  expectD.Offset(10, 0);
  EXPECT_TRUE(IsPointsEqual(expectD, event->root_location_f()));

  // Release key W and generate touch move (right) event (Key D is still
  // pressed).
  event_generator_->ReleaseKey(ui::VKEY_W, ui::EF_NONE, 1);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 6);
  event = event_capturer_.touch_events()[5].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  expectD = gfx::PointF(expect);
  expectD.Offset(10, 0);
  EXPECT_TRUE(IsPointsEqual(expectD, event->root_location_f()));

  // Release key D and generate touch release event.
  event_generator_->ReleaseKey(ui::VKEY_D, ui::EF_NONE, 1);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 7);
  event = event_capturer_.touch_events()[6].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectD, event->root_location_f()));
  event_capturer_.Clear();

  // Press A again, it should repeat the same as previous result.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_TRUE(*(action->touch_id()) == 0);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  task_environment()->FastForwardBy(input_overlay::kSendTouchMoveDelay);
  EXPECT_TRUE((int)event_capturer_.touch_events().size() == 2);
  // Generate touch down event.
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_TRUE(IsPointsEqual(expect, event->root_location_f()));
  // Generate touch move left event.
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  EXPECT_TRUE(IsPointsEqual(expectA, event->root_location_f()));
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  event_capturer_.Clear();
}
}  // namespace arc
