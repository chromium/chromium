// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"

#include <optional>

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

void VerifyEventsSize(test::EventCapturer& event_capturer,
                      size_t expected_key_event_size,
                      size_t expected_mouse_event_size,
                      size_t expected_touch_event_size) {
  EXPECT_EQ(expected_key_event_size, event_capturer.key_events().size());
  EXPECT_EQ(expected_mouse_event_size, event_capturer.mouse_events().size());
  EXPECT_EQ(expected_touch_event_size, event_capturer.touch_events().size());
}

}  // namespace

class TouchInjectorTest : public views::ViewsTestBase {
 protected:
  TouchInjectorTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  int GetRewrittenTouchIdForTesting(ui::PointerId original_id) {
    return injector_->GetRewrittenTouchIdForTesting(original_id);
  }

  gfx::PointF GetRewrittenRootLocationForTesting(ui::PointerId original_id) {
    return injector_->GetRewrittenRootLocationForTesting(original_id);
  }

  std::unique_ptr<AppDataProto> ConvertToProto() {
    return injector_->ConvertToProto();
  }

  void AddMenuEntryToProtoIfCustomized(AppDataProto& temp_proto) {
    injector_->AddMenuEntryToProtoIfCustomized(temp_proto);
  }

  void LoadMenuEntryFromProto(AppDataProto& temp_proto) {
    injector_->LoadMenuEntryFromProto(temp_proto);
  }

  void PrepareToBindPosition(Action* action,
                             std::unique_ptr<Position> position) {
    action->PrepareToBindPositionForTesting(std::move(position));
  }

  bool GetHasPendingTouchEvents() {
    return injector_->has_pending_touch_events_;
  }

  aura::TestScreen* test_screen() {
    return aura::test::AuraTestHelper::GetInstance()->GetTestScreen();
  }

  aura::Window* root_window() { return GetContext(); }

  test::EventCapturer event_capturer_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  std::unique_ptr<views::Widget> widget_;
  int caption_height_;
  std::unique_ptr<TouchInjector> injector_;

 protected:
  void InitWithFeature(const base::test::FeatureRef& feature, bool enable) {
    if (enable) {
      scoped_feature_list_.InitAndEnableFeature(*feature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(*feature);
    }
    Init();
  }

  // views::ViewsTestBase:
  void TearDown() override {
    injector_.reset();
    widget_->CloseNow();

    root_window()->RemovePreTargetHandler(&event_capturer_);

    event_generator_.reset();
    event_capturer_.Clear();

    views::ViewsTestBase::TearDown();
  }

 private:
  void Init() {
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

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TouchInjectorTest, TestAddRemoveActionWithProtoConversion) {
  InitWithFeature(ash::features::kGameDashboard, /*enable=*/true);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  CheckActions(injector_.get(), /*expect_size=*/2u,
               /*expect_types=*/{ActionType::TAP, ActionType::TAP},
               /*expect_ids=*/{0, 1});
  EXPECT_EQ(2u, injector_->GetActiveActionsSize());

  auto bounds = injector_->content_bounds();
  auto center = gfx::Point(bounds.width() / 2, bounds.height() / 2);
  // Step 1: Add a new action move.
  injector_->AddNewAction(ActionType::MOVE, center);
  CheckActions(
      injector_.get(), /*expect_size=*/3u,
      /*expect_types=*/{ActionType::TAP, ActionType::TAP, ActionType::MOVE},
      /*expect_ids=*/{0, 1, kMaxDefaultActionID + 1});
  EXPECT_EQ(3u, injector_->GetActiveActionsSize());

  // Step 2: Add a new action tap.
  injector_->AddNewAction(ActionType::TAP, center);
  CheckActions(
      injector_.get(), /*expect_size=*/4u,
      /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::MOVE, ActionType::TAP},
      /*expect_ids=*/{0, 1, kMaxDefaultActionID + 1, kMaxDefaultActionID + 2});
  EXPECT_EQ(4u, injector_->GetActiveActionsSize());

  // Step 3: Remove the action added at step 1.
  injector_->RemoveAction(injector_->actions()[2].get());
  CheckActions(
      injector_.get(), /*expect_size=*/3u,
      /*expect_types=*/{ActionType::TAP, ActionType::TAP, ActionType::TAP},
      /*expect_ids=*/{0, 1, kMaxDefaultActionID + 2});
  EXPECT_EQ(3u, injector_->GetActiveActionsSize());

  // Step 4: Add a new action tap.
  injector_->AddNewAction(ActionType::TAP, center);
  // Re-use the minimum new user-added ID which is removed at step 3.
  CheckActions(
      injector_.get(), /*expect_size=*/4u,
      /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::TAP, ActionType::TAP},
      /*expect_ids=*/{0, 1, kMaxDefaultActionID + 2, kMaxDefaultActionID + 1});
  EXPECT_EQ(4u, injector_->GetActiveActionsSize());

  // Step 5: Remove a default action.
  EXPECT_FALSE(injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(injector_->actions()[1]->IsDeleted());
  injector_->RemoveAction(injector_->actions()[0].get());
  // The action size doesn't change when removing a default action.
  CheckActions(
      injector_.get(), /*expect_size=*/4u,
      /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::TAP, ActionType::TAP},
      /*expect_ids=*/{0, 1, kMaxDefaultActionID + 2, kMaxDefaultActionID + 1});
  // The deleted default action is still in the list.
  EXPECT_EQ(3u, injector_->GetActiveActionsSize());
  EXPECT_TRUE(injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(injector_->actions()[1]->IsDeleted());

  // Step 6: Add two more actions, remove the first added action in this
  // step and then add a new action again. This is to test it gets the right
  // action ID in the middle. Add two new actions.
  injector_->AddNewAction(ActionType::TAP, center);
  injector_->AddNewAction(ActionType::MOVE, center);
  CheckActions(injector_.get(), /*expect_size=*/6u,
               /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::TAP,
                ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/
               {0, 1, kMaxDefaultActionID + 2, kMaxDefaultActionID + 1,
                kMaxDefaultActionID + 3, kMaxDefaultActionID + 4});
  EXPECT_EQ(5u, injector_->GetActiveActionsSize());
  EXPECT_TRUE(injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(injector_->actions()[1]->IsDeleted());
  // Remove the first action.
  injector_->RemoveAction(injector_->actions()[4].get());
  CheckActions(injector_.get(), /*expect_size=*/5u,
               /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::TAP,
                ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/
               {0, 1, kMaxDefaultActionID + 2, kMaxDefaultActionID + 1,
                kMaxDefaultActionID + 4});
  EXPECT_EQ(4u, injector_->GetActiveActionsSize());
  EXPECT_TRUE(injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(injector_->actions()[1]->IsDeleted());
  // Add a new action.
  injector_->AddNewAction(ActionType::TAP, center);
  CheckActions(injector_.get(), /*expect_size=*/6u,
               /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::TAP,
                ActionType::TAP, ActionType::MOVE, ActionType::TAP},
               /*expect_ids=*/
               {0, 1, kMaxDefaultActionID + 2, kMaxDefaultActionID + 1,
                kMaxDefaultActionID + 4, kMaxDefaultActionID + 3});
  EXPECT_EQ(5u, injector_->GetActiveActionsSize());
  EXPECT_TRUE(injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(injector_->actions()[1]->IsDeleted());

  // Convert it to proto and parse the proto and check if the proto conversion
  // is correct.
  auto proto = ConvertToProto();
  auto injector = std::make_unique<TouchInjector>(
      widget_->GetNativeWindow(),
      *widget_->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<AppDataProto>, std::string) {}));
  injector->ParseActions(json_value->GetDict());
  injector->OnProtoDataAvailable(*proto);
  CheckActions(injector.get(), /*expect_size=*/6u,
               /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::TAP,
                ActionType::TAP, ActionType::MOVE, ActionType::TAP},
               /*expect_ids=*/
               {0, 1, kMaxDefaultActionID + 2, kMaxDefaultActionID + 1,
                kMaxDefaultActionID + 4, kMaxDefaultActionID + 3});
  EXPECT_EQ(5u, injector_->GetActiveActionsSize());
  EXPECT_TRUE(injector->actions()[0]->IsDeleted());
  EXPECT_FALSE(injector->actions()[1]->IsDeleted());
}

TEST_F(TouchInjectorTest, TestActionTypeChangeWithProtoConversion) {
  InitWithFeature(ash::features::kGameDashboard, /*enable=*/true);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(2u, injector_->actions().size());
  EXPECT_EQ(0, injector_->actions()[0]->id());
  EXPECT_EQ(1, injector_->actions()[1]->id());

  // Step 1: Add a new action.
  auto bounds = injector_->content_bounds();
  injector_->AddNewAction(ActionType::TAP,
                          gfx::Point(bounds.width() / 2, bounds.height() / 2));
  EXPECT_EQ(3u, injector_->actions().size());
  EXPECT_EQ(kMaxDefaultActionID + 1, injector_->actions()[2]->id());

  // Step 2: Change the type of the action with ID 0 which is at index 0.
  auto* action = injector_->actions()[0].get();
  EXPECT_EQ(ActionType::TAP, action->GetType());
  injector_->ChangeActionType(action, ActionType::MOVE);
  EXPECT_EQ(3u, injector_->actions().size());
  // Check the new action type at index 0.
  action = injector_->actions()[0].get();
  EXPECT_EQ(ActionType::MOVE, action->GetType());
  EXPECT_EQ(0, action->id());
  // Action with ID 1 on index 1 is not changed.
  action = injector_->actions()[1].get();
  EXPECT_EQ(ActionType::TAP, action->GetType());
  EXPECT_EQ(1, action->id());

  // Step 3: Change the action type which is added in step 1.
  injector_->ChangeActionType(injector_->actions()[2].get(), ActionType::MOVE);
  // Check the new action type at index 2.
  action = injector_->actions()[2].get();
  EXPECT_EQ(ActionType::MOVE, action->GetType());
  EXPECT_EQ(kMaxDefaultActionID + 1, action->id());

  // Convert it to proto and parse the proto and check if the proto conversion
  // is correct.
  auto proto = ConvertToProto();
  auto injector = std::make_unique<TouchInjector>(
      widget_->GetNativeWindow(),
      *widget_->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<AppDataProto>, std::string) {}));
  injector->ParseActions(json_value->GetDict());
  injector->OnProtoDataAvailable(*proto);
  EXPECT_EQ(3u, injector->actions().size());
  // Action with ID 0 at index 0 has type changed.
  action = injector->actions()[0].get();
  EXPECT_EQ(ActionType::MOVE, action->GetType());
  EXPECT_EQ(0, action->id());
  // Action with ID 1 at index 1 is not changed.
  action = injector->actions()[1].get();
  EXPECT_EQ(ActionType::TAP, action->GetType());
  EXPECT_EQ(1, action->id());
  // Check action with ID kMaxDefaultActionID+1 at index 2.
  action = injector->actions()[2].get();
  EXPECT_EQ(ActionType::MOVE, action->GetType());
  EXPECT_EQ(kMaxDefaultActionID + 1, action->id());
}

// -----------------------------------------------------------------------------
// VersionTouchInjectorTest:
// Test fixture to test both pre-beta and beta version depending on the test
// param (true for beta version, false for pre-beta version).
class VersionTouchInjectorTest : public TouchInjectorTest,
                                 public testing::WithParamInterface<bool> {
 public:
  VersionTouchInjectorTest() = default;
  ~VersionTouchInjectorTest() override = default;

  // TouchInjectorTest:
  void SetUp() override {
    TouchInjectorTest::SetUp();
    InitWithFeature(ash::features::kGameDashboard, IsBetaVersion());
  }

 private:
  bool IsBetaVersion() const { return GetParam(); }
};

TEST_P(VersionTouchInjectorTest, TestEventRewriterActionTapKey) {
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  // Extra Action with the same ID is removed.
  EXPECT_EQ(2, (int)injector_->actions().size());
  auto* actionA = injector_->actions()[0].get();
  auto* actionB = injector_->actions()[1].get();
  injector_->RegisterEventRewriter();

  // Press and release key A, it should receive touch event, not key event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  auto expectA1 =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, kTolerance);
  EXPECT_FALSE(actionA->touch_id());
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
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
  event_generator_->PressKey(ui::VKEY_C, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.key_events().size());
  event_generator_->ReleaseKey(ui::VKEY_C, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.key_events().size());
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release A -> Release B.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  auto expectB =
      gfx::PointF(360, 100 + (400 - caption_height_) * 0.8 + caption_height_);
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  auto expectA2 =
      gfx::PointF(260, 100 + (400 - caption_height_) * 0.3 + caption_height_);
  EXPECT_POINTF_NEAR(expectA2, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_POINTF_NEAR(expectA2, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, kTolerance);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();

  // Test multi-key tap: Press B -> Press A -> Release B -> Release A.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(4, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_POINTF_NEAR(expectA1, event->root_location_f(), kTolerance);
  EXPECT_EQ(1, event->pointer_details().id);
  event_capturer_.Clear();

  // Test repeat key and it should receive only one touch event.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  event_generator_->PressKey(ui::VKEY_B, ui::EF_IS_REPEAT,
                             /*source_device_id=*/1);
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  event_capturer_.Clear();

  // Test release touch event when unregistering the window.
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(actionB->touch_id());
  injector_->UnRegisterEventRewriter();
  EXPECT_FALSE(actionB->touch_id());
  EXPECT_EQ(2, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events().back().get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_capturer_.Clear();
  // Register the event-rewriter and press key again.
  injector_->RegisterEventRewriter();
  event_generator_->PressKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(actionB->touch_id());
  EXPECT_EQ(1, (int)event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  EXPECT_POINTF_NEAR(expectB, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_B, ui::EF_NONE, /*source_device_id=*/1);
  event_capturer_.Clear();
}

TEST_P(VersionTouchInjectorTest, TestEventRewriterActionTapMouse) {
  injector_->set_enable_mouse_lock(true);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapMouse);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(2u, injector_->actions().size());
  injector_->RegisterEventRewriter();
  EXPECT_FALSE(GetHasPendingTouchEvents());

  auto* primary_action = injector_->actions()[0].get();
  auto* primary_binding = primary_action->current_input();
  EXPECT_EQ(primary_binding->mouse_action(), MouseAction::PRIMARY_CLICK);
  EXPECT_TRUE(
      primary_binding->mouse_types().contains(ui::EventType::kMousePressed));
  EXPECT_TRUE(
      primary_binding->mouse_types().contains(ui::EventType::kMouseReleased));
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON, primary_binding->mouse_flags());
  auto* secondary_action = injector_->actions()[1].get();
  auto* secondary_binding = secondary_action->current_input();
  EXPECT_EQ(secondary_binding->mouse_action(), MouseAction::SECONDARY_CLICK);
  EXPECT_TRUE(
      secondary_binding->mouse_types().contains(ui::EventType::kMousePressed));
  EXPECT_TRUE(
      secondary_binding->mouse_types().contains(ui::EventType::kMouseReleased));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, secondary_binding->mouse_flags());

  event_generator_->MoveMouseTo(gfx::Point(300, 200));
  EXPECT_EQ(2u, event_capturer_.mouse_events().size());
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  EXPECT_FALSE(GetHasPendingTouchEvents());
  event_capturer_.Clear();

  // Lock mouse and check primary and secondary click.
  event_generator_->PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE,
                                       /*source_device_id=*/1);
  event_generator_->PressLeftButton();
  EXPECT_TRUE(GetHasPendingTouchEvents());
  EXPECT_TRUE(event_capturer_.mouse_events().empty());
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  EXPECT_EQ(0, event->pointer_details().id);
  auto expect_primary =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_POINTF_NEAR(expect_primary, event->root_location_f(), kTolerance);
  // Mouse secondary button click.
  event_generator_->PressRightButton();
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  EXPECT_EQ(1, event->pointer_details().id);
  auto expect_secondary = gfx::PointF(
      200 + 200 * 0.8, 100 + (400 - caption_height_) * 0.8 + caption_height_);
  EXPECT_POINTF_NEAR(expect_secondary, event->root_location_f(), kTolerance);

  event_generator_->ReleaseRightButton();
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_EQ(1, event->pointer_details().id);
  EXPECT_POINTF_NEAR(expect_secondary, event->root_location_f(), kTolerance);

  event_generator_->ReleaseLeftButton();
  EXPECT_EQ(4u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_EQ(0, event->pointer_details().id);
  EXPECT_POINTF_NEAR(expect_primary, event->root_location_f(), kTolerance);
}

TEST_P(VersionTouchInjectorTest, TestEventRewriterActionMoveKey) {
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionMoveKey);
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(1u, injector_->actions().size());
  auto* action = injector_->actions()[0].get();
  injector_->RegisterEventRewriter();
  EXPECT_FALSE(GetHasPendingTouchEvents());

  // Press key A and generate touch down and move left event.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(0, *(action->touch_id()));
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_TRUE(GetHasPendingTouchEvents());
  // Wait for touch move event.
  task_environment()->FastForwardBy(kSendTouchMoveDelay);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  // Generate touch down event.
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  auto expect =
      gfx::PointF(300, 100 + (400 - caption_height_) * 0.5 + caption_height_);
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  // Generate touch move left event.
  auto expectA = gfx::PointF(expect);
  auto* action_move = static_cast<ActionMove*>(action);
  int move_distance = action_move->move_distance();
  expectA.Offset(-move_distance, 0);
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
  EXPECT_POINTF_NEAR(expectA, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);

  // Press key W (move left + up) and generate touch move (up and left) event.
  event_generator_->PressKey(ui::VKEY_W, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
  auto expectW = gfx::PointF(expectA);
  expectW.Offset(0, -move_distance);
  EXPECT_POINTF_NEAR(expectW, event->root_location_f(), kTolerance);

  // Release key A and generate touch move up event (Key W is still pressed).
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(4u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[3].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
  expectW = gfx::PointF(expect);
  expectW.Offset(0, -move_distance);
  EXPECT_POINTF_NEAR(expectW, event->root_location_f(), kTolerance);

  // Press key D and generate touch move (up and right) event.
  event_generator_->PressKey(ui::VKEY_D, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(5u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[4].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
  auto expectD = gfx::PointF(expectW);
  expectD.Offset(move_distance, 0);
  EXPECT_POINTF_NEAR(expectD, event->root_location_f(), kTolerance);

  // Release key W and generate touch move (right) event (Key D is still
  // pressed).
  event_generator_->ReleaseKey(ui::VKEY_W, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(6u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[5].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
  expectD = gfx::PointF(expect);
  expectD.Offset(move_distance, 0);
  EXPECT_POINTF_NEAR(expectD, event->root_location_f(), kTolerance);

  // Release key D and generate touch release event.
  event_generator_->ReleaseKey(ui::VKEY_D, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(7u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[6].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_POINTF_NEAR(expectD, event->root_location_f(), kTolerance);
  event_capturer_.Clear();

  // Press A again, it should repeat the same as previous result.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_EQ(0, *(action->touch_id()));
  EXPECT_TRUE(event_capturer_.key_events().empty());
  task_environment()->FastForwardBy(kSendTouchMoveDelay);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  // Generate touch down event.
  event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  // Generate touch move left event.
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
  EXPECT_POINTF_NEAR(expectA, event->root_location_f(), kTolerance);
  EXPECT_EQ(0, event->pointer_details().id);
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  event_capturer_.Clear();
}

TEST_P(VersionTouchInjectorTest, TestEventRewriterActionMoveMouse) {
  injector_->set_enable_mouse_lock(true);
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionMoveMouse);
  EXPECT_TRUE(json_value.has_value() && json_value->is_dict());
  injector_->ParseActions(json_value->GetDict());
  EXPECT_EQ(2u, injector_->actions().size());
  injector_->RegisterEventRewriter();
  EXPECT_FALSE(GetHasPendingTouchEvents());

  auto* hover_action = static_cast<ActionMove*>(injector_->actions()[0].get());
  auto* hover_binding = hover_action->current_input();
  EXPECT_EQ(hover_binding->mouse_action(), MouseAction::HOVER_MOVE);
  EXPECT_TRUE(
      hover_binding->mouse_types().contains(ui::EventType::kMouseEntered));
  EXPECT_TRUE(
      hover_binding->mouse_types().contains(ui::EventType::kMouseMoved));
  EXPECT_TRUE(
      hover_binding->mouse_types().contains(ui::EventType::kMouseExited));
  EXPECT_EQ(0, hover_binding->mouse_flags());

  auto* right_action = static_cast<ActionMove*>(injector_->actions()[1].get());
  auto* right_binding = right_action->current_input();
  EXPECT_EQ(right_binding->mouse_action(), MouseAction::SECONDARY_DRAG_MOVE);
  EXPECT_TRUE(
      right_binding->mouse_types().contains(ui::EventType::kMousePressed));
  EXPECT_TRUE(
      right_binding->mouse_types().contains(ui::EventType::kMouseDragged));
  EXPECT_TRUE(
      right_binding->mouse_types().contains(ui::EventType::kMouseReleased));
  EXPECT_EQ(ui::EF_RIGHT_MOUSE_BUTTON, right_binding->mouse_flags());

  // When the mouse is unlocked and test target action mouse hover move. Mouse
  // events are received as mouse events.
  event_generator_->SendMouseEnter();
  EXPECT_FALSE(GetHasPendingTouchEvents());
  EXPECT_FALSE(hover_action->touch_id());
  EXPECT_EQ(1u, event_capturer_.mouse_events().size());
  event_generator_->MoveMouseTo(gfx::Point(250, 150));
  EXPECT_EQ(3u, event_capturer_.mouse_events().size());
  event_capturer_.Clear();

  // Lock mouse.
  EXPECT_FALSE(injector_->is_mouse_locked());
  event_generator_->PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE,
                                       /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_TRUE(injector_->is_mouse_locked());
  // Mouse hover events are transformed to touch events.
  event_generator_->MoveMouseTo(gfx::Point(300, 200), 1);
  EXPECT_TRUE(GetHasPendingTouchEvents());
  EXPECT_TRUE(hover_action->touch_id() && *(hover_action->touch_id()) == 0);
  EXPECT_TRUE(event_capturer_.mouse_events().empty());
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  auto* event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, event->type());
  auto expect = gfx::PointF(350, 200);
  EXPECT_POINTF_NEAR(expect, event->root_location_f(), kTolerance);
  event_generator_->MoveMouseTo(gfx::Point(350, 250), 1);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchMoved, event->type());
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
                                       /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event = event_capturer_.touch_events()[2].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, event->type());
  EXPECT_FALSE(hover_action->touch_id());
  event_generator_->MoveMouseTo(gfx::Point(330, 220), 1);
  EXPECT_FALSE(event_capturer_.mouse_events().empty());
  event_capturer_.Clear();
}

TEST_P(VersionTouchInjectorTest, TestEventRewriterWithModifierKeyOnActionTap) {
  const auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  const auto* first_action_ptr = injector_->actions()[0].get();
  injector_->RegisterEventRewriter();

  // Press key `A`, then key `Ctrl` and then release key `A` and then key
  // `Ctrl`. The keyboard-binded `Action` injects simulated touch events.
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  EXPECT_TRUE(first_action_ptr->touch_id());
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN, kTolerance);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  EXPECT_FALSE(first_action_ptr->touch_id());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event_capturer_.Clear();

  // Press key `A`, then key `Ctrl` and then release key `Ctrl` and then key
  // `A`. The keyboard-binded `Action` injects simulated touch events.
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  EXPECT_TRUE(first_action_ptr->touch_id());
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(1u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, kTolerance);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  EXPECT_FALSE(first_action_ptr->touch_id());
  event_capturer_.Clear();

  // Press key `Ctrl`, then key `A` and then release key `A` and then key
  // `Ctrl`. The keyboard-binded `Action` doesn't inject simulated touch events.
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN, kTolerance);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());

  // Press key `Ctrl`, then key `A` and then release key `Ctrl` and then key
  // `A`. The keyboard-binded `Action` doesn't inject simulated touch events.
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, kTolerance);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());

  // Press `Ctrl + W` to close the window and doesn't receive any simulated
  // touch events.
  event_generator_->PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
}

TEST_P(VersionTouchInjectorTest, TestEventRewriterWithModifierKeyOnActionMove) {
  const auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionMoveKey);
  injector_->ParseActions(json_value->GetDict());
  const auto* first_action_ptr = injector_->actions()[0].get();
  injector_->RegisterEventRewriter();

  // Press key `A`, then key `Ctrl` and then release key `A` and then key
  // `Ctrl`. The keyboard-binded `Action` injects simulated touch events.
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  task_environment()->FastForwardBy(kSendTouchMoveDelay);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  EXPECT_TRUE(first_action_ptr->touch_id());
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN, kTolerance);
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  EXPECT_FALSE(first_action_ptr->touch_id());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  event_capturer_.Clear();

  // Press key `A`, then key `Ctrl` and then release key `Ctrl` and then key
  // `A`. The keyboard-binded `Action` injects simulated touch events.
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE);
  task_environment()->FastForwardBy(kSendTouchMoveDelay);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  EXPECT_TRUE(first_action_ptr->touch_id());
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, kTolerance);
  EXPECT_EQ(3u, event_capturer_.touch_events().size());
  EXPECT_FALSE(first_action_ptr->touch_id());
  event_capturer_.Clear();

  // Press key `Ctrl`, then key `A` and then release key `A` and then key
  // `Ctrl`. The keyboard-binded `Action` doesn't inject simulated touch events.
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_CONTROL_DOWN, kTolerance);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());

  // Press key `Ctrl`, then key `A` and then release key `Ctrl` and then key
  // `A`. The keyboard-binded `Action` doesn't inject simulated touch events.
  event_generator_->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, kTolerance);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());

  // Press `Ctrl + W` to close the window and doesn't receive any simulated
  // touch events.
  event_generator_->PressAndReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0u, event_capturer_.touch_events().size());
}

TEST_P(VersionTouchInjectorTest, TestCleanupTouchEvents) {
  // Setup.
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  injector_->RegisterEventRewriter();
  EXPECT_FALSE(GetHasPendingTouchEvents());

  // Verify initial states.
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_TRUE(event_capturer_.touch_events().empty());
  EXPECT_EQ(0u, event_capturer_.touch_events().size());

  // Verify the logic by creating a touch by a key press, then unregistering the
  // event rewriter.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  EXPECT_TRUE(event_capturer_.key_events().empty());
  EXPECT_EQ(1u, event_capturer_.touch_events().size());

  // Verify DispatchTouchReleaseEvent logic through UnRegisterEventRewriter.
  EXPECT_TRUE(GetHasPendingTouchEvents());
  injector_->UnRegisterEventRewriter();
  EXPECT_FALSE(GetHasPendingTouchEvents());

  // Verify the existing event have generated a release event.
  EXPECT_EQ(2u, event_capturer_.touch_events().size());
  auto* touch_event = event_capturer_.touch_events()[0].get();
  EXPECT_EQ(ui::EventType::kTouchPressed, touch_event->type());
  touch_event = event_capturer_.touch_events()[1].get();
  EXPECT_EQ(ui::EventType::kTouchReleased, touch_event->type());

  event_capturer_.Clear();
}

TEST_P(VersionTouchInjectorTest, TestActivePlayMode) {
  // Setup.
  auto json_value =
      base::JSONReader::ReadAndReturnValueWithError(kValidJsonActionTapKey);
  injector_->ParseActions(json_value->GetDict());
  injector_->RegisterEventRewriter();
  EXPECT_FALSE(GetHasPendingTouchEvents());

  // Verify initial states.
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/0u);

  // 1. Verify mouse event is discarded in the active play mode.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/1u);
  EXPECT_TRUE(GetHasPendingTouchEvents());

  // Mouse starts to interrupt and verify the mouse event is discarded and other
  // touch events stay the same.
  event_generator_->SendMouseEnter();
  event_generator_->ClickLeftButton();
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/1u);
  EXPECT_TRUE(GetHasPendingTouchEvents());

  // Release the key A.
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/2u);

  // Mouse event is resumed if it is not in the active play mode anymore.
  event_generator_->PressLeftButton();
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/1u,
                   /*expected_touch_event_size=*/2u);
  event_capturer_.Clear();

  // 2. Verify trackpad scroll event doesn't stop the active play mode.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/1u);
  EXPECT_TRUE(GetHasPendingTouchEvents());
  // Generate trackpad scroll event and it is still in the active play mode.
  event_generator_->GenerateTrackpadRest();
  event_generator_->CancelTrackpadRest();
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/1u);
  EXPECT_TRUE(GetHasPendingTouchEvents());
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/2u);
  event_capturer_.Clear();

  // 3. Verify original touch event cancels the simulated touch events.
  event_generator_->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/1u);

  event_generator_->PressTouchId(/*touch_id=*/0, gfx::Point(360, 420));
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/3u);
  event_generator_->ReleaseKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/3u);
  event_generator_->ReleaseTouchId(/*touch_id=*/0);
  VerifyEventsSize(event_capturer_, /*expected_key_event_size=*/0u,
                   /*expected_mouse_event_size=*/0u,
                   /*expected_touch_event_size=*/4u);
}

TEST_P(VersionTouchInjectorTest, TestProtoConversion) {
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
  PrepareToBindPosition(injector_->actions()[0].get(), std::move(new_pos));
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

INSTANTIATE_TEST_SUITE_P(All, VersionTouchInjectorTest, ::testing::Bool());

}  // namespace arc::input_overlay
