// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_move.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/test/event_capturer.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace arc::input_overlay {
namespace {

// TODO(cuicuiruan): Create test for other device scale.

// Consider two points are at the same position within kTolerance.
constexpr const float kTolerance = 0.999f;

constexpr const char kValidJsonActionTapKey[] =
    R"json({
      "tap": [
        {
          "id": 0,
          "input_sources": [
            "keyboard"
          ],
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
          "id": 0,
          "input_sources": [
            "keyboard"
          ],
          "name": "duplicate",
          "key": "KeyC",
          "location": [
            {
              "type": "position",
              "anchor_to_target": [
                0.5,
                0.5
              ]
            }
          ]
        },
        {
          "id": 1,
          "input_sources": [
            "keyboard"
          ],
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
    })json";

constexpr const char kValidJsonActionTapMouse[] =
    R"json({
      "mouse_lock": {
        "key": "KeyA"
      },
      "tap": [
        {
          "id": 0,
          "input_sources": [
            "mouse"
          ],
          "name": "any name",
          "mouse_action": "primary_click",
          "location": [
            {
              "type": "position",
              "anchor_to_target": [
                0.5,
                0.5
              ]
            }
          ]
        },
        {
          "id": 1,
          "input_sources": [
            "mouse"
          ],
          "name": "any name",
          "mouse_action": "secondary_click",
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
    })json";

constexpr const char kValidJsonActionMoveKey[] =
    R"json({
      "move": [
        {
          "id": 0,
          "input_sources": [
            "keyboard"
          ],
          "name": "Virtual Joystick",
          "keys": [
            "KeyW",
            "KeyA",
            "KeyS",
            "KeyD"
          ],
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
    })json";

constexpr const char kValidJsonActionMoveMouse[] =
    R"json({
      "mouse_lock": {
        "key": "KeyA"
      },
      "move": [
        {
          "id": 0,
          "input_sources": [
            "mouse"
          ],
          "name": "camera move",
          "mouse_action": "hover_move",
          "target_area": {
            "top_left": {
              "type": "position",
              "anchor_to_target": [
                0.5,
                0
              ]
            },
            "bottom_right": {
              "type": "position",
              "anchor_to_target": [
                0.9999,
                0.9999
              ]
            }
          }
        },
        {
          "id": 1,
          "name": "test name",
          "mouse_action": "secondary_drag_move"
        }
      ]
    })json";

void TouchInjectorResetAndAddTwoActions(TouchInjector* touch_injector) {
  DCHECK(touch_injector);
  touch_injector->OnBindingRestore();
  touch_injector->OnBindingSave();
  EXPECT_EQ(2u, touch_injector->actions().size());
  EXPECT_FALSE(touch_injector->actions()[0]->deleted());
  EXPECT_FALSE(touch_injector->actions()[1]->deleted());
  touch_injector->AddNewAction(ActionType::MOVE);
  touch_injector->AddNewAction(ActionType::TAP);
  touch_injector->OnBindingSave();
  EXPECT_EQ(4u, touch_injector->actions().size());
}

}  // namespace

class TouchInjectorTest : public views::ViewsTestBase {
 protected:
  TouchInjectorTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kArcInputOverlayAlphaV2}, {});
  }

  int GetRewrittenTouchIdForTesting(ui::PointerId original_id) {
    return injector_->GetRewrittenTouchIdForTesting(original_id);
  }

  gfx::PointF GetRewrittenRootLocationForTesting(ui::PointerId original_id) {
    return injector_->GetRewrittenRootLocationForTesting(original_id);
  }

  int GetRewrittenTouchInfoSizeForTesting() {
    return injector_->GetRewrittenTouchInfoSizeForTesting();
  }

  std::unique_ptr<AppDataProto> ConvertToProto() {
    return injector_->ConvertToProto();
  }

  const std::vector<std::unique_ptr<Action>>& GetPendingAddUserActions() const {
    return injector_->pending_add_user_actions_;
  }

  const std::vector<std::unique_ptr<Action>>& GetPendingDeleteUserActions()
      const {
    return injector_->pending_delete_user_actions_;
  }

  const std::vector<Action*> GetPendingAddDefaultActions() const {
    return injector_->pending_add_default_actions_;
  }

  const std::vector<Action*> GetPendingDeleteDefaultActions() const {
    return injector_->pending_delete_default_actions_;
  }

  void ExpectActionSizes(unsigned int size_pending_add_user_actions,
                         unsigned int size_pending_delete_user_actions,
                         unsigned int size_pending_add_default_actions,
                         unsigned int size_pending_delete_default_actions,
                         unsigned int size_actions) {
    EXPECT_EQ(size_pending_add_user_actions, GetPendingAddUserActions().size());
    EXPECT_EQ(size_pending_delete_user_actions,
              GetPendingDeleteUserActions().size());
    EXPECT_EQ(size_pending_add_default_actions,
              GetPendingAddDefaultActions().size());
    EXPECT_EQ(size_pending_delete_default_actions,
              GetPendingDeleteDefaultActions().size());
    EXPECT_EQ(size_actions, injector_->actions().size());
  }

  void AddMenuEntryToProtoIfCustomized(AppDataProto& temp_proto) {
    injector_->AddMenuEntryToProtoIfCustomized(temp_proto);
  }

  void LoadMenuEntryFromProto(AppDataProto& temp_proto) {
    injector_->LoadMenuEntryFromProto(temp_proto);
  }

  int GetNextActionID() { return injector_->next_action_id_; }

  aura::TestScreen* test_screen() {
    return aura::test::AuraTestHelper::GetInstance()->GetTestScreen();
  }

  aura::Window* root_window() { return GetContext(); }

  test::EventCapturer event_capturer_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<views::Widget> widget_;
  int caption_height_;
  std::unique_ptr<TouchInjector> injector_;

 private:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(root_window());

    root_window()->SetBounds(gfx::Rect(800, 600));
    root_window()->AddPostTargetHandler(&event_capturer_);

    widget_ = CreateArcWindow(root_window(), gfx::Rect(200, 100, 200, 400));
    caption_height_ = -widget_->non_client_view()
                           ->frame_view()
                           ->GetWindowBoundsForClientBounds(gfx::Rect())
                           .y();
    injector_ = std::make_unique<TouchInjector>(
        widget_->GetNativeWindow(),
        *widget_->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<AppDataProto>, std::string) {}));
  }

  void TearDown() override {
    injector_.reset();
    widget_->CloseNow();

    root_window()->RemovePreTargetHandler(&event_capturer_);

    event_generator_.reset();
    event_capturer_.Clear();

    views::ViewsTestBase::TearDown();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchInjectorTest, TestEventRewriterActionTapKey) {
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  // Extra Action with the same ID is removed.
  EXPECT_EQ(2, (int)injector_->actions().size());
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
  auto expectA1 =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, kTolerance);
  EXPECT_FALSE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  // Next touch position.
  EXPECT_EQ(1, actionA->current_position_idx());
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
  auto expectB =
      gfx::PointF(360, 100 + (400 - caption_height_) * 0.8 + caption_height_);
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  auto expectA2 =
      gfx::PointF(260, 100 + (400 - caption_height_) * 0.3 + caption_height_);
  EXPECT_POINTF_NEAR(expectA2, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_POINTF_NEAR(expectA2, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, kTolerance);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release B -> Release A.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);
  event_capturer_.Clear();

  // Test repeat key and it should receive only one touch event.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_generator_->PressKey(ui::VKEY_B, ui::EF_IS_REPEAT, 1);
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
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
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();
  // Register the event-rewriter and press key again.
  injector_->RegisterEventRewriter();
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, 1);
  EXPECT_TRUE(actionB->touch_id());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, 1);
  event_capturer_.Clear();
}

TEST_F(TouchInjectorTest, TestEventRewriterActionTapMouse) {
  injector_->set_enable_mouse_lock(true);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapMouse);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(2u, injector_->actions().size());
  injector_->RegisterEventRewriter();

  auto* primary_action = injector_->actions()[0].get();
  auto* primary_binding = primary_action->current_input();
  EXPECT_EQ(primary_binding->mouse_action(), MouseAction::PRIMARY_CLICK);
  EXPECT_TRUE(primary_binding->mouse_types().contains(ui::ET_MOUSE_PRESSED));
  EXPECT_TRUE(primary_binding->mouse_types().contains(ui::ET_MOUSE_RELEASED));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, primary_binding->mouse_flags());
  auto* secondary_action = injector_->actions()[1].get();
  auto* secondary_binding = secondary_action->current_input();
  EXPECT_EQ(secondary_binding->mouse_action(), MouseAction::SECONDARY_CLICK);
  EXPECT_TRUE(secondary_binding->mouse_types().contains(ui::ET_MOUSE_PRESSED));
  EXPECT_TRUE(secondary_binding->mouse_types().contains(ui::ET_MOUSE_RELEASED));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, secondary_binding->mouse_flags());

  event_generator_->MoveMouseTo(gfx::Point(300, 200));
  EXPECT_EQ(2u, event_capturer_.mouse_events().size());
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_capturer_.Clear();

  // Lock mouse and check primary and secondary click.
  event_generator_->PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE,
                                       1 /* keyboard id */);
  event_generator_->PressLeftButton();
  EXPECT_TRUE(event_capturer_.mouse_events().empty());
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_EQ(0, event->pointer_details().id);
  auto expect_primary =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_POINTF_NEAR(expect_primary, event->root_location_f(), kTolerance);
  // Mouse secondary button click.
  event_generator_->PressRightButton();
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_EQ(1, event->pointer_details().id);
  auto expect_secondary = gfx::PointF(
      200 + 200 * 0.8, 100 + (400 - caption_height_) * 0.8 + caption_height_);
  EXPECT_POINTF_NEAR(expect_secondary, event->root_location_f(), kTolerance);

  event_generator_->ReleaseRightButton();
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(1, event->pointer_details().id);
  EXPECT_POINTF_NEAR(expect_secondary, event->root_location_f(), kTolerance);

  event_generator_->ReleaseLeftButton();
  EXPECT_EQ(4u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_EQ(0, event->pointer_details().id);
  EXPECT_POINTF_NEAR(expect_primary, event->root_location_f(), kTolerance);
}

TEST_F(TouchInjectorTest, TestEventRewriterActionMoveKey) {
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionMoveKey);
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(1u, injector_->actions().size());
  auto* action = injector_->actions()[0].get();
  injector_->RegisterEventRewriter();

  // Press key A and generate touch down and move left event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_EQ(0, *(action->touch_id()));
  EXPECT_TRUE(event_capturer_.key_events().empty());
  // Wait for touch move event.
  task_environment()->FastForwardBy(kSendTouchMoveDelay);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  // Generate touch down event.
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  auto expect =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  // Generate touch move left event.
  auto expectA = gfx::PointF(expect);
  auto* action_move = static_cast<ActionMove*>(action);
  int move_distance = action_move->move_distance();
  expectA.Offset(-move_distance, 0);
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  EXPECT_POINTF_NEAR(expectA, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  // Press key W (move left + up) and generate touch move (up and left) event.
  event_generator_->PressKey(ui::VKEY_W, ui::EF_NONE, 1);
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  auto expectW = gfx::PointF(expectA);
  expectW.Offset(0, -move_distance);
  EXPECT_POINTF_NEAR(expectW, event->root_location_f(), kTolerance);

  // Release key A and generate touch move up event (Key W is still pressed).
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_EQ(4u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  expectW = gfx::PointF(expect);
  expectW.Offset(0, -move_distance);
  EXPECT_POINTF_NEAR(expectW, event->root_location_f(), kTolerance);

  // Press key D and generate touch move (up and right) event.
  event_generator_->PressKey(ui::VKEY_D, ui::EF_NONE, 1);
  EXPECT_EQ(5u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[4].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  auto expectD = gfx::PointF(expectW);
  expectD.Offset(move_distance, 0);
  EXPECT_POINTF_NEAR(expectD, event->root_location_f(), kTolerance);

  // Release key W and generate touch move (right) event (Key D is still
  // pressed).
  event_generator_->ReleaseKey(ui::VKEY_W, ui::EF_NONE, 1);
  EXPECT_EQ(6u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[5].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  expectD = gfx::PointF(expect);
  expectD.Offset(move_distance, 0);
  EXPECT_POINTF_NEAR(expectD, event->root_location_f(), kTolerance);

  // Release key D and generate touch release event.
  event_generator_->ReleaseKey(ui::VKEY_D, ui::EF_NONE, 1);
  EXPECT_EQ(7u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[6].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_POINTF_NEAR(expectD, event->root_location_f(), kTolerance);
  event_capturer_.Clear();

  // Press A again, it should repeat the same as previous result.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_EQ(0, *(action->touch_id()));
  EXPECT_TRUE(event_capturer_.key_events().empty());
  task_environment()->FastForwardBy(kSendTouchMoveDelay);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  // Generate touch down event.
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  // Generate touch move left event.
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  EXPECT_POINTF_NEAR(expectA, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  event_capturer_.Clear();
}

TEST_F(TouchInjectorTest, TestEventRewriterActionMoveMouse) {
  injector_->set_enable_mouse_lock(true);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionMoveMouse);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(2u, injector_->actions().size());
  injector_->RegisterEventRewriter();
  auto* hover_action = static_cast<ActionMove*>(injector_->actions()[0].get());
  auto* hover_binding = hover_action->current_input();
  EXPECT_EQ(hover_binding->mouse_action(), MouseAction::HOVER_MOVE);
  EXPECT_TRUE(hover_binding->mouse_types().contains(ui::ET_MOUSE_ENTERED));
  EXPECT_TRUE(hover_binding->mouse_types().contains(ui::ET_MOUSE_MOVED));
  EXPECT_TRUE(hover_binding->mouse_types().contains(ui::ET_MOUSE_EXITED));
  EXPECT_EQ(0, hover_binding->mouse_flags());

  auto* right_action = static_cast<ActionMove*>(injector_->actions()[1].get());
  auto* right_binding = right_action->current_input();
  EXPECT_EQ(right_binding->mouse_action(), MouseAction::SECONDARY_DRAG_MOVE);
  EXPECT_TRUE(right_binding->mouse_types().contains(ui::ET_MOUSE_PRESSED));
  EXPECT_TRUE(right_binding->mouse_types().contains(ui::ET_MOUSE_DRAGGED));
  EXPECT_TRUE(right_binding->mouse_types().contains(ui::ET_MOUSE_RELEASED));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, right_binding->mouse_flags());

  // When the mouse is unlocked and test target action mouse hover move. Mouse
  // events are received as mouse events.
  event_generator_->SendMouseEnter();
  EXPECT_FALSE(hover_action->touch_id());
  EXPECT_EQ(1u, event_capturer_.mouse_events().size());
  event_generator_->MoveMouseTo(gfx::Point(250, 150));
  EXPECT_EQ(3u, event_capturer_.mouse_events().size());
  event_capturer_.Clear();

  // Lock mouse.
  EXPECT_FALSE(injector_->is_mouse_locked());
  event_generator_->PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE,
                                       1 /* keyboard id */);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_TRUE(injector_->is_mouse_locked());
  // Mouse hover events are transformed to touch events.
  event_generator_->MoveMouseTo(gfx::Point(300, 200), 1);
  EXPECT_TRUE(hover_action->touch_id() && *(hover_action->touch_id()) == 0);
  EXPECT_TRUE(event_capturer_.mouse_events().empty());
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, event->type());
  auto expect = gfx::PointF(350, 200);
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  event_generator_->MoveMouseTo(gfx::Point(350, 250), 1);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_MOVED, event->type());
  expect = gfx::PointF(375, 250);
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  // Send mouse hover move outside of window content bounds when mouse is
  // locked. The mouse event will be discarded.
  event_generator_->MoveMouseTo(gfx::Point(500, 200), 1);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  EXPECT_TRUE(event_capturer_.mouse_events().empty());
  // Send other mouse events when the mouse is locked and events will be
  // discarded.
  event_generator_->PressLeftButton();
  event_generator_->ReleaseLeftButton();
  EXPECT_TRUE(event_capturer_.mouse_events().empty());
  EXPECT_EQ(2u, event_capturer_.touch_events().size());

  // Unlock the mouse and the mouse events received
  // as mouse events.
  event_generator_->PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE,
                                       1 /* keyboard id */);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_RELEASED, event->type());
  EXPECT_FALSE(hover_action->touch_id());
  event_generator_->MoveMouseTo(gfx::Point(330, 220), 1);
  EXPECT_FALSE(event_capturer_.mouse_events().empty());
  event_capturer_.Clear();
}

TEST_F(TouchInjectorTest, TestEventRewriterTouchToTouch) {
  // Setup.
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  injector_->RegisterEventRewriter();

  // Verify initial states.
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(0, (int)event_capturer_.touch_events().size());
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  // Case 1: Verify key event and touch event with matching ids will be handled
  // correctly. Press down a keyboard key (A key) that will be converted to a
  // touch event, then a physical touch on the screen. Both initial inputs will
  // have the same id of 0. Then release the ids.

  // First, press key A with id 0.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 0 /* keyboard id */);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  auto* keyEvent = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, keyEvent->type());
  EXPECT_EQ(0, keyEvent->pointer_details().id);
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  // Second, press touch event with same id as A key.
  event_generator_->PressTouchId(0, gfx::Point(360, 420));
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  EXPECT_EQ(1, GetRewrittenTouchInfoSizeForTesting());

  // Verify id has been updated to not match the A key.
  auto* touchEvent = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, touchEvent->type());
  EXPECT_NE(touchEvent->pointer_details().id, keyEvent->pointer_details().id);
  EXPECT_EQ(1, touchEvent->pointer_details().id);

  // Send release event for A key. Verify the touch id is still in our map.
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 0);
  EXPECT_EQ(1, GetRewrittenTouchInfoSizeForTesting());

  // Send release event for touch event. Verify touch id is removed from map.
  event_generator_->ReleaseTouchId(0);
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  event_capturer_.Clear();

  // Case 2: Similar to Test 2, but the order of the inputs pressed are
  // reversed. Press down a physical touch on the screen, then a a keyboard key
  // (A key) that will be converted to a touch event. Both initial inputs will
  // have the id of 0. Then release the ids.

  // First, press touch event with id 0.
  event_generator_->PressTouchId(0, gfx::Point(360, 420));
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  touchEvent = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, touchEvent->type());
  EXPECT_EQ(0, touchEvent->pointer_details().id);
  EXPECT_EQ(1, GetRewrittenTouchInfoSizeForTesting());

  // Second, press key A with same id as the physical touch event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 0 /* keyboard id */);
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  EXPECT_EQ(1, GetRewrittenTouchInfoSizeForTesting());

  // Verify ids do not match.
  keyEvent = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, keyEvent->type());
  EXPECT_EQ(1, keyEvent->pointer_details().id);
  EXPECT_NE(touchEvent->pointer_details().id, keyEvent->pointer_details().id);

  // Send release event for touch event. Verify touch id is removed from map.
  event_generator_->ReleaseTouchId(0);
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  // Send release event for A key. Verify our map is still empty.
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 0);
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  event_capturer_.Clear();

  // Case 3: Send a touch event in a location outside of the window content
  // bounds. This test will verify that we do not override the id that this
  // touch event was originally assigned, which is 10 for this test.
  event_generator_->PressTouchId(10, gfx::Point(5, 5));
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  touchEvent = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_PRESSED, touchEvent->type());

  // Verify the event still has id value 10 and wasn't added to our map.
  EXPECT_EQ(10, touchEvent->pointer_details().id);
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  // Send release event for touch event. Verify map still is empty.
  event_generator_->ReleaseTouchId(10);
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  event_capturer_.Clear();

  // Case 4: Verify the logic in DispatchTouchCancelEvent is correct by
  // creating a touch and key press, then unregistering the event rewriter.
  // This should result in touch cancel events occurring for the events.

  // First, press key A.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, 0 /* keyboard id */);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());

  // Second, press touch event.
  event_generator_->PressTouchId(0, gfx::Point(360, 420));
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  EXPECT_EQ(1, GetRewrittenTouchInfoSizeForTesting());

  // Verify DispatchTouchCancelEvent logic through UnRegisterEventRewriter.
  injector_->UnRegisterEventRewriter();

  // Verify the existing events have generated a canceled event.
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  touchEvent = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_CANCELLED, touchEvent->type());
  touchEvent = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::ET_TOUCH_CANCELLED, touchEvent->type());
  EXPECT_EQ(0, GetRewrittenTouchInfoSizeForTesting());
}

TEST_F(TouchInjectorTest, TestProtoConversion) {
  // Check whether AppDataProto is serialized correctly.
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  // Simulate a menu entry position change.
  auto menu_entry_location_point = gfx::Point(5, 5);
  injector_->SaveMenuEntryLocation(menu_entry_location_point);
  auto expected_menu_entry_location = injector_->menu_entry_location();
  // Change input binding on actions[1].
  auto new_input = InputElement::CreateActionTapKeyElement(ui::DomCode::US_C);
  auto* expected_input = new_input.get();
  injector_->OnInputBindingChange(&*injector_->actions()[1],
                                  std::move(new_input));
  injector_->OnApplyPendingBinding();
  // Change position binding on actions[0].
  auto new_pos = std::make_unique<Position>(PositionType::kDefault);
  new_pos->Normalize(gfx::Point(20, 20), gfx::RectF(100, 100));
  auto expected_pos = *new_pos;
  injector_->actions()[0]->PrepareToBindPosition(std::move(new_pos));
  injector_->OnApplyPendingBinding();
  auto proto = ConvertToProto();
  // Check if the system version is serialized correctly.
  EXPECT_TRUE(proto->has_system_version());
  EXPECT_EQ(kSystemVersionAlphaV2, proto->system_version());
  // Check that the menu entry position is serialized correctly.
  EXPECT_TRUE(proto->has_menu_entry_position());
  auto serialized_position = proto->menu_entry_position().anchor_to_target();
  EXPECT_EQ(expected_menu_entry_location->x(), serialized_position[0]);
  EXPECT_EQ(expected_menu_entry_location->y(), serialized_position[1]);
  // Check whether the actions[1] with new input binding is converted to proto
  // correctly.
  auto action_proto = proto->actions()[1];
  EXPECT_TRUE(action_proto.has_input_element());
  auto input_element =
      InputElement::ConvertFromProto(action_proto.input_element());
  EXPECT_EQ(*input_element, *expected_input);
  // Check whether the actions[0] with new position is converted to proto
  // correctly.
  action_proto = proto->actions()[0];
  EXPECT_FALSE(action_proto.positions().empty());
  auto position = Position::ConvertFromProto(action_proto.positions()[0]);
  EXPECT_EQ(*position, expected_pos);

  // Check whether AppDataProto is deserialized correctly.
  auto injector = std::make_unique<TouchInjector>(
      widget_->GetNativeWindow(),
      *widget_->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<AppDataProto>, std::string) {}));
  injector->ParseActions(json_value->GetDict());
  injector->OnProtoDataAvailable(*proto);
  EXPECT_EQ(injector_->actions().size(), injector->actions().size());
  for (size_t i = 0; i < injector_->actions().size(); i++) {
    const auto* action_a = injector_->actions()[i].get();
    const auto* action_b = injector->actions()[i].get();
    EXPECT_EQ(*action_a->current_input(), *action_b->current_input());
    EXPECT_EQ(action_a->current_positions(), action_b->current_positions());
  }
  auto deserialized_menu_entry_location = injector->menu_entry_location();
  EXPECT_TRUE(deserialized_menu_entry_location);
  EXPECT_EQ(*deserialized_menu_entry_location, *expected_menu_entry_location);
}

TEST_F(TouchInjectorTest, TestAddAction) {
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->set_beta(true);
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(2u, injector_->actions().size());

  // Add->Save.
  injector_->AddNewAction(ActionType::MOVE);
  EXPECT_EQ(1u, GetPendingAddUserActions().size());
  injector_->OnBindingSave();
  EXPECT_EQ(3u, injector_->actions().size());
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1, injector_->actions().back()->id());
  EXPECT_EQ(kMaxDefaultActionID + 2, GetNextActionID());

  // Add->Save->Restore->Save. Final result only has default actions.
  injector_->AddNewAction(ActionType::TAP);
  injector_->AddNewAction(ActionType::MOVE);
  EXPECT_EQ(2u, GetPendingAddUserActions().size());
  injector_->OnBindingSave();
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  EXPECT_EQ(5u, injector_->actions().size());
  EXPECT_EQ(kMaxDefaultActionID + 2,
            (injector_->actions().rbegin() + 1)->get()->id());
  EXPECT_EQ(kMaxDefaultActionID + 3, injector_->actions().back()->id());
  EXPECT_EQ(kMaxDefaultActionID + 4, GetNextActionID());
  injector_->OnBindingRestore();
  EXPECT_EQ(2u, injector_->actions().size());
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1, GetNextActionID());
  injector_->OnBindingSave();
  EXPECT_EQ(2u, injector_->actions().size());

  // Add->Cancel. Nothing is added.
  injector_->AddNewAction(ActionType::TAP);
  injector_->AddNewAction(ActionType::MOVE);
  EXPECT_EQ(2u, GetPendingAddUserActions().size());
  injector_->OnBindingCancel();
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  EXPECT_EQ(2u, injector_->actions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1, GetNextActionID());

  // Add->Cancel->Add->Save. Second "add" is saved.
  injector_->AddNewAction(ActionType::MOVE);
  injector_->AddNewAction(ActionType::TAP);
  EXPECT_EQ(kMaxDefaultActionID + 1,
            (GetPendingAddUserActions().rbegin() + 1)->get()->id());
  EXPECT_EQ(kMaxDefaultActionID + 2, GetPendingAddUserActions().back()->id());
  EXPECT_EQ(2u, GetPendingAddUserActions().size());
  injector_->OnBindingCancel();
  EXPECT_EQ(kMaxDefaultActionID + 1, GetNextActionID());
  EXPECT_EQ(2u, injector_->actions().size());
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  injector_->AddNewAction(ActionType::MOVE);
  EXPECT_EQ(1u, GetPendingAddUserActions().size());
  injector_->OnBindingSave();
  EXPECT_EQ(3u, injector_->actions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1, injector_->actions().back()->id());
  EXPECT_EQ(kMaxDefaultActionID + 2, GetNextActionID());
  // Reset.
  injector_->OnBindingRestore();
  injector_->OnBindingSave();

  // Add->Save->Restore->Cancel. "Restore" is not applied at the end.
  injector_->AddNewAction(ActionType::MOVE);
  injector_->AddNewAction(ActionType::TAP);
  injector_->OnBindingSave();
  EXPECT_EQ(4u, injector_->actions().size());
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1,
            (injector_->actions().rbegin() + 1)->get()->id());
  EXPECT_EQ(kMaxDefaultActionID + 2, injector_->actions().back()->id());
  EXPECT_EQ(kMaxDefaultActionID + 3, GetNextActionID());
  injector_->OnBindingRestore();
  EXPECT_EQ(2u, injector_->actions().size());
  EXPECT_EQ(2u, GetPendingDeleteUserActions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1, GetNextActionID());
  injector_->OnBindingCancel();
  EXPECT_EQ(4u, injector_->actions().size());
  EXPECT_EQ(0u, GetPendingAddUserActions().size());
  EXPECT_EQ(0u, GetPendingDeleteUserActions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1,
            (injector_->actions().rbegin() + 1)->get()->id());
  EXPECT_EQ(kMaxDefaultActionID + 2, injector_->actions().back()->id());
  EXPECT_EQ(kMaxDefaultActionID + 3, GetNextActionID());
}

TEST_F(TouchInjectorTest, TestDeleteAction) {
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->set_beta(true);
  injector_->ParseActions(json_value->GetDict());
  TouchInjectorResetAndAddTwoActions(injector_.get());
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/4u);

  // Delete->Save->Restore->Save.
  // Delete a default action.
  injector_->RemoveAction(injector_->actions()[1].get());
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/1u,
                    /*size_actions=*/4u);
  injector_->OnBindingSave();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/4u);
  EXPECT_TRUE(injector_->actions()[1]->deleted());
  // Delete a user-added action.
  injector_->RemoveAction(injector_->actions()[2].get());
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/1u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/3u);
  injector_->OnBindingSave();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/3u);

  // Delete->Cancel->Delete->Save.
  TouchInjectorResetAndAddTwoActions(injector_.get());
  injector_->RemoveAction(injector_->actions()[1].get());
  injector_->RemoveAction(injector_->actions()[2].get());
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/1u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/1u,
                    /*size_actions=*/3u);
  injector_->OnBindingCancel();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/4u);
  injector_->RemoveAction(injector_->actions()[1].get());
  injector_->RemoveAction(injector_->actions()[2].get());
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/1u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/1u,
                    /*size_actions=*/3u);
  injector_->OnBindingSave();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/3u);
  EXPECT_FALSE(injector_->actions()[0]->deleted());
  EXPECT_TRUE(injector_->actions()[1]->deleted());

  // Delete->Save->Restore->Cancel.
  TouchInjectorResetAndAddTwoActions(injector_.get());
  injector_->RemoveAction(injector_->actions()[1].get());
  injector_->RemoveAction(injector_->actions()[2].get());
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/1u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/1u,
                    /*size_actions=*/3u);
  injector_->OnBindingSave();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/3u);
  EXPECT_TRUE(injector_->actions()[1]->deleted());
  injector_->OnBindingRestore();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/1u,
                    /*size_pending_add_default_actions=*/1u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/2u);
  EXPECT_FALSE(injector_->actions()[1]->deleted());
  injector_->OnBindingCancel();
  ExpectActionSizes(/*size_pending_add_user_actions=*/0u,
                    /*size_pending_delete_user_actions=*/0u,
                    /*size_pending_add_default_actions=*/0u,
                    /*size_pending_delete_default_actions=*/0u,
                    /*size_actions=*/3u);
  EXPECT_TRUE(injector_->actions()[1]->deleted());
}

}  // namespace arc::input_overlay
