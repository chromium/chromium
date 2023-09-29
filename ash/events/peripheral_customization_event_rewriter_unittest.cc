// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include <algorithm>
#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/test/mock_input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/test/test_event_rewriter_continuation.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ash {

namespace {

constexpr int kMouseDeviceId = 1;
constexpr int kGraphicsTabletDeviceId = 2;

class TestEventRewriterContinuation
    : public ui::test::TestEventRewriterContinuation {
 public:
  TestEventRewriterContinuation() = default;

  TestEventRewriterContinuation(const TestEventRewriterContinuation&) = delete;
  TestEventRewriterContinuation& operator=(
      const TestEventRewriterContinuation&) = delete;

  ~TestEventRewriterContinuation() override = default;

  ui::EventDispatchDetails SendEvent(const ui::Event* event) override {
    passthrough_event = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails SendEventFinally(const ui::Event* event) override {
    rewritten_event = event->Clone();
    return ui::EventDispatchDetails();
  }

  ui::EventDispatchDetails DiscardEvent() override {
    return ui::EventDispatchDetails();
  }

  void reset() {
    passthrough_event.reset();
    rewritten_event.reset();
  }

  bool discarded() { return !(passthrough_event || rewritten_event); }

  std::unique_ptr<ui::Event> passthrough_event;
  std::unique_ptr<ui::Event> rewritten_event;

  base::WeakPtrFactory<TestEventRewriterContinuation> weak_ptr_factory_{this};
};

class TestInputDeviceSettingsController
    : public MockInputDeviceSettingsController {
 public:
  void OnMouseButtonPressed(DeviceId device_id,
                            const mojom::Button& button) override {
    pressed_mouse_buttons_[device_id].push_back(button.Clone());
  }

  void OnGraphicsTabletButtonPressed(DeviceId device_id,
                                     const mojom::Button& button) override {
    pressed_graphics_tablet_buttons_[device_id].push_back(button.Clone());
  }

  const base::flat_map<int, std::vector<mojom::ButtonPtr>>&
  pressed_mouse_buttons() const {
    return pressed_mouse_buttons_;
  }

  const base::flat_map<int, std::vector<mojom::ButtonPtr>>&
  pressed_graphics_tablet_buttons() const {
    return pressed_graphics_tablet_buttons_;
  }

 private:
  base::flat_map<int, std::vector<mojom::ButtonPtr>> pressed_mouse_buttons_;
  base::flat_map<int, std::vector<mojom::ButtonPtr>>
      pressed_graphics_tablet_buttons_;
};

class TestAcceleratorObserver : public AcceleratorController::Observer {
 public:
  TestAcceleratorObserver() {
    Shell::Get()->accelerator_controller()->AddObserver(this);
  }

  ~TestAcceleratorObserver() override {
    Shell::Get()->accelerator_controller()->RemoveObserver(this);
  }

  void OnActionPerformed(AcceleratorAction action) override {
    action_performed_ = action;
  }

  bool has_action_performed() const { return action_performed_.has_value(); }

  AcceleratorAction action_performed() const { return *action_performed_; }

  void reset() { action_performed_.reset(); }

 private:
  absl::optional<AcceleratorAction> action_performed_;
};

using EventTypeVariant = absl::variant<ui::MouseEvent, ui::KeyEvent>;
struct EventRewriterTestData {
  EventTypeVariant incoming_event;
  absl::optional<EventTypeVariant> rewritten_event;
  absl::optional<mojom::Button> pressed_button;

  EventRewriterTestData(EventTypeVariant incoming_event,
                        absl::optional<EventTypeVariant> rewritten_event)
      : incoming_event(incoming_event),
        rewritten_event(rewritten_event),
        pressed_button(absl::nullopt) {}

  EventRewriterTestData(EventTypeVariant incoming_event,
                        absl::optional<EventTypeVariant> rewritten_event,
                        mojom::CustomizableButton button)
      : incoming_event(incoming_event), rewritten_event(rewritten_event) {
    pressed_button = mojom::Button();
    pressed_button->set_customizable_button(button);
  }

  EventRewriterTestData(EventTypeVariant incoming_event,
                        absl::optional<EventTypeVariant> rewritten_event,
                        ui::KeyboardCode key_code)
      : incoming_event(incoming_event), rewritten_event(rewritten_event) {
    pressed_button = mojom::Button();
    pressed_button->set_vkey(key_code);
  }

  EventRewriterTestData(const EventRewriterTestData& data) = default;
};

// Before test suites are initialized, paraterized data gets generated.
// `ui::KeyEvent` structs rely on the keyboard layout engine being setup.
// Therefore, before any suites are initialized, the keyboard layout engine
// must be configured before using/creating any `ui::KeyEvent` structs. Once a
// suite is setup, this function will be disabled which will stop any further
// layout engines from being created.
std::unique_ptr<ui::ScopedKeyboardLayoutEngine> CreateLayoutEngine(
    bool disable_permanently = false) {
  static bool disabled = false;
  if (disable_permanently || disabled) {
    disabled = true;
    return nullptr;
  }

  return std::make_unique<ui::ScopedKeyboardLayoutEngine>(
      std::make_unique<ui::StubKeyboardLayoutEngine>());
}

ui::KeyEvent CreateKeyButtonEvent(ui::EventType type,
                                  ui::KeyboardCode key_code,
                                  int flags = ui::EF_NONE,
                                  ui::DomCode code = ui::DomCode::NONE,
                                  ui::DomKey key = ui::DomKey::NONE,
                                  int device_id = kMouseDeviceId) {
  auto engine = CreateLayoutEngine();
  ui::KeyEvent key_event(type, key_code, code, flags, key, /*time_stamp=*/{});
  key_event.set_source_device_id(device_id);
  return key_event;
}

ui::MouseEvent CreateMouseButtonEvent(ui::EventType type,
                                      int flags,
                                      int changed_button_flags,
                                      int device_id = kMouseDeviceId) {
  ui::MouseEvent mouse_event(type, /*location=*/gfx::PointF{},
                             /*root_location=*/gfx::PointF{},
                             /*time_stamp=*/{}, flags, changed_button_flags);
  mouse_event.set_source_device_id(device_id);
  return mouse_event;
}

std::string ConvertToString(const ui::MouseEvent& mouse_event) {
  return base::StringPrintf(
      "MouseEvent type=%d flags=0x%X changed_button_flags=0x%X",
      mouse_event.type(), mouse_event.flags(),
      mouse_event.changed_button_flags());
}

std::string ConvertToString(const ui::KeyEvent& key_event) {
  auto engine = CreateLayoutEngine();
  return base::StringPrintf(
      "KeyboardEvent type=%d code=0x%06X flags=0x%X vk=0x%02X key=0x%08X "
      "scan=0x%08X",
      key_event.type(), static_cast<uint32_t>(key_event.code()),
      key_event.flags(), key_event.key_code(),
      static_cast<uint32_t>(key_event.GetDomKey()), key_event.scan_code());
}

std::string ConvertToString(const EventTypeVariant& event) {
  return absl::visit([](auto&& event) { return ConvertToString(event); },
                     event);
}

std::string ConvertToString(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    return ConvertToString(*event.AsMouseEvent());
  }
  if (event.IsKeyEvent()) {
    return ConvertToString(*event.AsKeyEvent());
  }
  NOTREACHED_NORETURN();
}

ui::Event& GetEventFromVariant(EventTypeVariant& event) {
  if (absl::holds_alternative<ui::MouseEvent>(event)) {
    return absl::get<ui::MouseEvent>(event);
  } else {
    return absl::get<ui::KeyEvent>(event);
  }
}

mojom::Button GetButton(ui::KeyboardCode key_code) {
  mojom::Button button;
  button.set_vkey(key_code);
  return button;
}

mojom::Button GetButton(mojom::CustomizableButton customizable_button) {
  mojom::Button button;
  button.set_customizable_button(customizable_button);
  return button;
}

}  // namespace

class PeripheralCustomizationEventRewriterTest : public AshTestBase {
 public:
  PeripheralCustomizationEventRewriterTest() {
    CreateLayoutEngine(/*disable_permanently=*/true);
  }
  PeripheralCustomizationEventRewriterTest(
      const PeripheralCustomizationEventRewriterTest&) = delete;
  PeripheralCustomizationEventRewriterTest& operator=(
      const PeripheralCustomizationEventRewriterTest&) = delete;
  ~PeripheralCustomizationEventRewriterTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kPeripheralCustomization,
                                           features::kInputDeviceSettingsSplit},
                                          {});
    AshTestBase::SetUp();
    controller_scoped_resetter_ = std::make_unique<
        InputDeviceSettingsController::ScopedResetterForTest>();
    controller_ = std::make_unique<TestInputDeviceSettingsController>();
    mouse_settings_ = mojom::MouseSettings::New();
    graphics_tablet_settings_ = mojom::GraphicsTabletSettings::New();
    ON_CALL(*controller_, GetMouseSettings(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetGraphicsTabletSettings(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetMouseSettings(kMouseDeviceId))
        .WillByDefault(testing::Return(mouse_settings_.get()));
    ON_CALL(*controller_, GetGraphicsTabletSettings(kGraphicsTabletDeviceId))
        .WillByDefault(testing::Return(graphics_tablet_settings_.get()));
    rewriter_ = std::make_unique<PeripheralCustomizationEventRewriter>(
        controller_.get());
  }

  void TearDown() override {
    rewriter_.reset();
    controller_.reset();
    controller_scoped_resetter_.reset();
    AshTestBase::TearDown();
    scoped_feature_list_.Reset();
  }

 protected:
  std::unique_ptr<PeripheralCustomizationEventRewriter> rewriter_;
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      controller_scoped_resetter_;
  std::unique_ptr<TestInputDeviceSettingsController> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  mojom::MouseSettingsPtr mouse_settings_;
  mojom::GraphicsTabletSettingsPtr graphics_tablet_settings_;
};

TEST_F(PeripheralCustomizationEventRewriterTest, MouseButtonWithoutObserving) {
  TestEventRewriterContinuation continuation;

  auto back_mouse_event = CreateMouseButtonEvent(
      ui::ET_MOUSE_PRESSED, ui::EF_BACK_MOUSE_BUTTON, ui::EF_BACK_MOUSE_BUTTON);

  rewriter_->RewriteEvent(back_mouse_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(back_mouse_event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       InvalidEventTypeMouseObserving) {
  TestEventRewriterContinuation continuation;

  rewriter_->StartObservingMouse(kMouseDeviceId);

  ui::MouseEvent event =
      CreateMouseButtonEvent(ui::ET_MOUSE_DRAGGED, ui::EF_NONE, ui::EF_NONE);

  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_F(PeripheralCustomizationEventRewriterTest, KeyEventActionRewriting) {
  TestAcceleratorObserver accelerator_observer;
  TestEventRewriterContinuation continuation;

  mouse_settings_->button_remappings.push_back(
      mojom::ButtonRemapping::New("", mojom::Button::NewVkey(ui::VKEY_A),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kBrightnessDown)));

  rewriter_->RewriteEvent(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A),
                          continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_TRUE(continuation.discarded());
  ASSERT_TRUE(accelerator_observer.has_action_performed());
  EXPECT_EQ(AcceleratorAction::kBrightnessDown,
            accelerator_observer.action_performed());

  continuation.reset();
  accelerator_observer.reset();
  rewriter_->RewriteEvent(CreateKeyButtonEvent(ui::ET_KEY_RELEASED, ui::VKEY_A),
                          continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_TRUE(continuation.discarded());
  ASSERT_FALSE(accelerator_observer.has_action_performed());
}

TEST_F(PeripheralCustomizationEventRewriterTest, MouseEventActionRewriting) {
  TestAcceleratorObserver accelerator_observer;
  TestEventRewriterContinuation continuation;

  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      mojom::RemappingAction::NewAcceleratorAction(
          AcceleratorAction::kLaunchApp0)));

  rewriter_->RewriteEvent(
      CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED, ui::EF_MIDDLE_MOUSE_BUTTON,
                             ui::EF_MIDDLE_MOUSE_BUTTON),
      continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_TRUE(continuation.discarded());
  ASSERT_TRUE(accelerator_observer.has_action_performed());
  EXPECT_EQ(AcceleratorAction::kLaunchApp0,
            accelerator_observer.action_performed());

  continuation.reset();
  accelerator_observer.reset();
  rewriter_->RewriteEvent(
      CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED, ui::EF_MIDDLE_MOUSE_BUTTON,
                             ui::EF_MIDDLE_MOUSE_BUTTON),
      continuation.weak_ptr_factory_.GetWeakPtr());
  EXPECT_TRUE(continuation.discarded());
  ASSERT_FALSE(accelerator_observer.has_action_performed());
}

class MouseButtonObserverTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<EventRewriterTestData> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    MouseButtonObserverTest,
    testing::ValuesIn(std::vector<EventRewriterTestData>{
        // MouseEvent tests:
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kBack,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_FORWARD_MOUSE_BUTTON,
                                   ui::EF_FORWARD_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kForward,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON |
                                       ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },

        // Observer notified only when mouse button pressed.
        {CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED,
                                ui::EF_BACK_MOUSE_BUTTON,
                                ui::EF_BACK_MOUSE_BUTTON),
         /*rewritten_event=*/absl::nullopt},

        // Left click ignored for buttons from a mouse.
        {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON),
         CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON)},

        // Right click ignored for buttons from a mouse.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON),
        },

        // Other flags are ignored when included in the event with other
        // buttons.
        {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                ui::EF_LEFT_MOUSE_BUTTON |
                                    ui::EF_BACK_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON),
         CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON)},
        {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                ui::EF_RIGHT_MOUSE_BUTTON |
                                    ui::EF_MIDDLE_MOUSE_BUTTON,
                                ui::EF_NONE),
         CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                ui::EF_RIGHT_MOUSE_BUTTON,
                                ui::EF_NONE)},

        // KeyEvent tests:
        {
            CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                 ui::VKEY_A,
                                 ui::EF_COMMAND_DOWN),
            absl::nullopt,
            ui::VKEY_A,
        },
        {
            CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::EF_NONE),
            absl::nullopt,
            ui::VKEY_B,
        },

        // Test that key releases are consumed, but not sent to observers.
        {
            CreateKeyButtonEvent(ui::ET_KEY_RELEASED, ui::VKEY_A),
            absl::nullopt,
        },
    }),
    [](const testing::TestParamInfo<EventRewriterTestData>& info) {
      std::string name = ConvertToString(info.param.incoming_event);
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '=', '_');
      return name;
    });

TEST_P(MouseButtonObserverTest, EventRewriting) {
  auto data = GetParam();

  rewriter_->StartObservingMouse(kMouseDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(GetEventFromVariant(data.incoming_event),
                          continuation.weak_ptr_factory_.GetWeakPtr());
  if (!data.rewritten_event) {
    ASSERT_TRUE(continuation.discarded());
    if (data.pressed_button) {
      const auto& actual_pressed_buttons =
          controller_->pressed_mouse_buttons().at(kMouseDeviceId);
      ASSERT_EQ(1u, actual_pressed_buttons.size());
      EXPECT_EQ(*data.pressed_button, *actual_pressed_buttons[0]);
    }
  } else {
    ASSERT_TRUE(continuation.passthrough_event);
    EXPECT_EQ(ConvertToString(*data.rewritten_event),
              ConvertToString(*continuation.passthrough_event));
  }

  rewriter_->StopObserving();
  continuation.reset();

  // After we stop observing, the passthrough event should be an identity of the
  // original.
  rewriter_->RewriteEvent(GetEventFromVariant(data.incoming_event),
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(data.incoming_event),
            ConvertToString(*continuation.passthrough_event));
}

class GraphicsTabletButtonObserverTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<EventRewriterTestData> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    GraphicsTabletButtonObserverTest,
    testing::ValuesIn(std::vector<EventRewriterTestData>{
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_RIGHT_MOUSE_BUTTON,
                                   ui::EF_RIGHT_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kRight,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kBack,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_FORWARD_MOUSE_BUTTON,
                                   ui::EF_FORWARD_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kForward,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_MIDDLE_MOUSE_BUTTON |
                                       ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_MIDDLE_MOUSE_BUTTON),
            absl::nullopt,
            mojom::CustomizableButton::kMiddle,
        },

        // Observer notified only when the button is pressed.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED,
                                   ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_BACK_MOUSE_BUTTON),
            absl::nullopt,
        },

        // Left click ignored for buttons from a graphics tablet.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
        },

        // Other flags are ignored when included in the event with other
        // buttons.
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_BACK_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON),
        },
        {
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON |
                                       ui::EF_MIDDLE_MOUSE_BUTTON,
                                   ui::EF_NONE),
            CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_NONE),
        },

        // KeyEvent tests:
        {
            CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                 ui::VKEY_A,
                                 ui::EF_COMMAND_DOWN),
            absl::nullopt,
            ui::VKEY_A,
        },
        {
            CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::EF_NONE),
            absl::nullopt,
            ui::VKEY_B,
        },

        // Test that key releases are consumed, but not sent to observers.
        {
            CreateKeyButtonEvent(ui::ET_KEY_RELEASED, ui::VKEY_A),
            absl::nullopt,
        },
    }),
    [](const testing::TestParamInfo<EventRewriterTestData>& info) {
      std::string name = ConvertToString(info.param.incoming_event);
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '=', '_');
      return name;
    });

TEST_P(GraphicsTabletButtonObserverTest, RewriteEvent) {
  auto data = GetParam();

  rewriter_->StartObservingGraphicsTablet(kGraphicsTabletDeviceId);

  auto& event = GetEventFromVariant(data.incoming_event);
  event.set_source_device_id(kGraphicsTabletDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());
  if (!data.rewritten_event) {
    ASSERT_TRUE(continuation.discarded());
    if (data.pressed_button) {
      const auto& actual_pressed_buttons =
          controller_->pressed_graphics_tablet_buttons().at(
              kGraphicsTabletDeviceId);
      ASSERT_EQ(1u, actual_pressed_buttons.size());
      EXPECT_EQ(*data.pressed_button, *actual_pressed_buttons[0]);
    }
  } else {
    ASSERT_TRUE(continuation.passthrough_event);
    EXPECT_EQ(ConvertToString(*data.rewritten_event),
              ConvertToString(*continuation.passthrough_event));
  }

  rewriter_->StopObserving();
  continuation.reset();

  // After we stop observing, the passthrough event should be an identity of the
  // original.
  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(data.incoming_event),
            ConvertToString(*continuation.passthrough_event));
}

class ButtonRewritingTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<std::pair<mojom::Button, mojom::KeyEvent>,
                     EventRewriterTestData>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ButtonRewritingTest,
    testing::ValuesIn(
        std::vector<std::tuple<std::pair<mojom::Button, mojom::KeyEvent>,
                               EventRewriterTestData>>{
            // KeyEvent rewriting test cases:
            // Remap A -> B.
            {{GetButton(ui::VKEY_A),
              mojom::KeyEvent(
                  ui::VKEY_B,
                  static_cast<int>(ui::DomCode::US_B),
                  static_cast<int>(ui::DomKey::Constant<'b'>::Character),
                  ui::EF_NONE)},
             {CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_B,
                                   ui::EF_NONE,
                                   ui::DomCode::US_B,
                                   ui::DomKey::Constant<'b'>::Character)}},
            // Remap A -> B, Pressing B is no-op.
            {{GetButton(ui::VKEY_A),
              mojom::KeyEvent(
                  ui::VKEY_B,
                  static_cast<int>(ui::DomCode::US_B),
                  static_cast<int>(ui::DomKey::Constant<'b'>::Character),
                  ui::EF_NONE)},
             {CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_B),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_B)}},
            // Remap CTRL -> ALT.
            {{GetButton(ui::VKEY_CONTROL),
              mojom::KeyEvent(ui::VKEY_MENU,
                              static_cast<int>(ui::DomCode::ALT_LEFT),
                              static_cast<int>(ui::DomKey::ALT),
                              ui::EF_ALT_DOWN)},
             {CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_CONTROL,
                                   ui::EF_CONTROL_DOWN),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_MENU,
                                   ui::EF_ALT_DOWN,
                                   ui::DomCode::ALT_LEFT,
                                   ui::DomKey::ALT)}},
            // Remap CTRL -> ALT and press with shift down.
            {{GetButton(ui::VKEY_CONTROL),
              mojom::KeyEvent(ui::VKEY_MENU,
                              static_cast<int>(ui::DomCode::ALT_LEFT),
                              static_cast<int>(ui::DomKey::ALT),
                              ui::EF_ALT_DOWN)},
             {CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_CONTROL,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_MENU,
                                   ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN,
                                   ui::DomCode::ALT_LEFT,
                                   ui::DomKey::ALT)}},
            // Remap A -> CTRL + SHIFT + B.
            {{GetButton(ui::VKEY_A),
              mojom::KeyEvent(
                  ui::VKEY_B,
                  static_cast<int>(ui::DomCode::US_B),
                  static_cast<int>(ui::DomKey::Constant<'b'>::Character),
                  ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)},
             {CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_B,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                                   ui::DomCode::US_B,
                                   ui::DomKey::Constant<'b'>::Character)}},

            // MouseEvent rewriting test cases:
            // Remap Middle -> CTRL + SHIFT + B.
            {{GetButton(mojom::CustomizableButton::kMiddle),
              mojom::KeyEvent(
                  ui::VKEY_B,
                  static_cast<int>(ui::DomCode::US_B),
                  static_cast<int>(ui::DomKey::Constant<'b'>::Character),
                  ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)},
             {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                     ui::EF_MIDDLE_MOUSE_BUTTON,
                                     ui::EF_MIDDLE_MOUSE_BUTTON),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_B,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                                   ui::DomCode::US_B,
                                   ui::DomKey::Constant<'b'>::Character)}},
            // Remap Middle -> CTRL + SHIFT + B with ALT down.
            {{GetButton(mojom::CustomizableButton::kMiddle),
              mojom::KeyEvent(
                  ui::VKEY_B,
                  static_cast<int>(ui::DomCode::US_B),
                  static_cast<int>(ui::DomKey::Constant<'b'>::Character),
                  ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)},
             {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                     ui::EF_MIDDLE_MOUSE_BUTTON |
                                         ui::EF_ALT_DOWN,
                                     ui::EF_MIDDLE_MOUSE_BUTTON),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_B,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN |
                                       ui::EF_ALT_DOWN,
                                   ui::DomCode::US_B,
                                   ui::DomKey::Constant<'b'>::Character)}},
            // Remap Back -> Meta.
            {{GetButton(mojom::CustomizableButton::kBack),
              mojom::KeyEvent(ui::VKEY_LWIN,
                              static_cast<int>(ui::DomCode::META_LEFT),
                              static_cast<int>(ui::DomKey::META),
                              ui::EF_COMMAND_DOWN)},
             {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                     ui::EF_BACK_MOUSE_BUTTON,
                                     ui::EF_BACK_MOUSE_BUTTON),
              CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                   ui::VKEY_LWIN,
                                   ui::EF_COMMAND_DOWN,
                                   ui::DomCode::META_LEFT,
                                   ui::DomKey::META)}},
            // Remap Middle -> B and check left mouse button is a no-op.
            {{GetButton(mojom::CustomizableButton::kMiddle),
              mojom::KeyEvent(
                  ui::VKEY_B,
                  static_cast<int>(ui::DomCode::US_B),
                  static_cast<int>(ui::DomKey::Constant<'b'>::Character),
                  ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN)},
             {CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                     ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN,
                                     ui::EF_LEFT_MOUSE_BUTTON),
              CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                                     ui::EF_LEFT_MOUSE_BUTTON | ui::EF_ALT_DOWN,
                                     ui::EF_LEFT_MOUSE_BUTTON)}},

        }));

TEST_P(ButtonRewritingTest, GraphicsPenRewriteEvent) {
  auto [tuple, data] = GetParam();
  auto& [button, key_event] = tuple;

  graphics_tablet_settings_->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "", button.Clone(),
          mojom::RemappingAction::NewKeyEvent(key_event.Clone())));

  auto& event = GetEventFromVariant(data.incoming_event);
  event.set_source_device_id(kGraphicsTabletDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(*data.rewritten_event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_P(ButtonRewritingTest, GraphicsTabletRewriteEvent) {
  auto [tuple, data] = GetParam();
  auto& [button, key_event] = tuple;

  graphics_tablet_settings_->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "", button.Clone(),
          mojom::RemappingAction::NewKeyEvent(key_event.Clone())));

  auto& event = GetEventFromVariant(data.incoming_event);
  event.set_source_device_id(kGraphicsTabletDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(*data.rewritten_event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_P(ButtonRewritingTest, MouseRewriteEvent) {
  auto [tuple, data] = GetParam();
  auto& [button, key_event] = tuple;

  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", button.Clone(),
      mojom::RemappingAction::NewKeyEvent(key_event.Clone())));

  auto& event = GetEventFromVariant(data.incoming_event);
  event.set_source_device_id(kMouseDeviceId);

  TestEventRewriterContinuation continuation;
  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(*data.rewritten_event),
            ConvertToString(*continuation.passthrough_event));
}

class ModifierRewritingTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<ui::KeyboardCode, ui::EventFlags>> {
  void SetUp() override {
    PeripheralCustomizationEventRewriterTest::SetUp();
    std::tie(key_code, flag) = GetParam();
  }

  void TearDown() override {
    PeripheralCustomizationEventRewriterTest::TearDown();
  }

 protected:
  ui::KeyboardCode key_code;
  ui::EventFlags flag;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ModifierRewritingTest,
    testing::ValuesIn(
        std::vector<std::tuple<ui::KeyboardCode, ui::EventFlags>>({
            {ui::VKEY_LWIN, ui::EF_COMMAND_DOWN},
            {ui::VKEY_RWIN, ui::EF_COMMAND_DOWN},
            {ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN},
            {ui::VKEY_LSHIFT, ui::EF_SHIFT_DOWN},
            {ui::VKEY_RSHIFT, ui::EF_SHIFT_DOWN},
            {ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN},
            {ui::VKEY_MENU, ui::EF_ALT_DOWN},
            {ui::VKEY_RMENU, ui::EF_ALT_DOWN},
        })));

TEST_P(ModifierRewritingTest, ModifierKeyCombo) {
  TestEventRewriterContinuation continuation;

  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          key_code, (int)ui::DomCode::NONE, (int)ui::DomKey::NONE, flag))));

  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_0, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(
      ConvertToString(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, key_code, flag)),
      ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(
                CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, flag)),
            ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_RELEASED, ui::VKEY_0, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(
                CreateKeyButtonEvent(ui::ET_KEY_RELEASED, key_code, flag)),
            ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                                 ui::EF_NONE)),
            ConvertToString(*continuation.passthrough_event));
}

TEST_P(ModifierRewritingTest, MultiModifierKeyCombo) {
  TestEventRewriterContinuation continuation;

  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          key_code, (int)ui::DomCode::NONE, (int)ui::DomKey::NONE, flag))));

  const ui::EventFlags test_flag =
      flag == ui::EF_COMMAND_DOWN ? ui::EF_SHIFT_DOWN : ui::EF_COMMAND_DOWN;

  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_0, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(
      ConvertToString(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, key_code, flag)),
      ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, test_flag),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                                 test_flag | flag)),
            ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_RELEASED, ui::VKEY_0, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(
                CreateKeyButtonEvent(ui::ET_KEY_RELEASED, key_code, flag)),
            ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A, test_flag),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_A,
                                                 test_flag)),
            ConvertToString(*continuation.passthrough_event));
}

TEST_P(ModifierRewritingTest, MouseEvent) {
  TestEventRewriterContinuation continuation;
  const ui::EventFlags test_flag =
      flag == ui::EF_COMMAND_DOWN ? ui::EF_SHIFT_DOWN : ui::EF_COMMAND_DOWN;

  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          key_code, (int)ui::DomCode::NONE, (int)ui::DomKey::NONE, flag))));

  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_PRESSED, ui::VKEY_0, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(
      ConvertToString(CreateKeyButtonEvent(ui::ET_KEY_PRESSED, key_code, flag)),
      ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                             test_flag | ui::EF_FORWARD_MOUSE_BUTTON,
                             ui::EF_FORWARD_MOUSE_BUTTON),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(
      ConvertToString(CreateMouseButtonEvent(
          ui::ET_MOUSE_PRESSED, ui::EF_FORWARD_MOUSE_BUTTON | test_flag | flag,
          ui::EF_FORWARD_MOUSE_BUTTON)),
      ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateKeyButtonEvent(ui::ET_KEY_RELEASED, ui::VKEY_0, ui::EF_NONE),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(
                CreateKeyButtonEvent(ui::ET_KEY_RELEASED, key_code, flag)),
            ConvertToString(*continuation.passthrough_event));

  continuation.reset();
  rewriter_->RewriteEvent(
      CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED,
                             test_flag | ui::EF_FORWARD_MOUSE_BUTTON,
                             ui::EF_FORWARD_MOUSE_BUTTON),
      continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(CreateMouseButtonEvent(
                ui::ET_MOUSE_PRESSED, test_flag | ui::EF_FORWARD_MOUSE_BUTTON,
                ui::EF_FORWARD_MOUSE_BUTTON)),
            ConvertToString(*continuation.passthrough_event));
}

class StaticShortcutActionRewritingTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<mojom::StaticShortcutAction, ui::KeyEvent>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    StaticShortcutActionRewritingTest,
    testing::ValuesIn(
        std::vector<std::tuple<mojom::StaticShortcutAction, ui::KeyEvent>>({
            {mojom::StaticShortcutAction::kCopy,
             CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                  ui::VKEY_C,
                                  ui::EF_CONTROL_DOWN,
                                  ui::DomCode::US_C,
                                  ui::DomKey::Constant<'c'>::Character)},
            {mojom::StaticShortcutAction::kPaste,
             CreateKeyButtonEvent(ui::ET_KEY_PRESSED,
                                  ui::VKEY_V,
                                  ui::EF_CONTROL_DOWN,
                                  ui::DomCode::US_V,
                                  ui::DomKey::Constant<'v'>::Character)},
        })));

TEST_F(StaticShortcutActionRewritingTest, StaticShortcutDisableMouseRewriting) {
  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kDisable)));

  TestEventRewriterContinuation continuation;
  ui::MouseEvent mouse_pressed_event =
      CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED, ui::EF_FORWARD_MOUSE_BUTTON,
                             ui::EF_FORWARD_MOUSE_BUTTON, kMouseDeviceId);
  rewriter_->RewriteEvent(mouse_pressed_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.discarded());
  EXPECT_EQ(nullptr, continuation.passthrough_event);

  ui::MouseEvent mouse_release_event =
      CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED, ui::EF_FORWARD_MOUSE_BUTTON,
                             ui::EF_FORWARD_MOUSE_BUTTON, kMouseDeviceId);

  continuation.reset();
  rewriter_->RewriteEvent(mouse_release_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.discarded());
  EXPECT_EQ(nullptr, continuation.passthrough_event);
}

TEST_P(StaticShortcutActionRewritingTest, StaticShortcutMouseRewriting) {
  const auto& [static_shortcut_action, expected_key_event] = GetParam();

  mouse_settings_->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));

  TestEventRewriterContinuation continuation;
  ui::MouseEvent mouse_pressed_event =
      CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED, ui::EF_FORWARD_MOUSE_BUTTON,
                             ui::EF_FORWARD_MOUSE_BUTTON, kMouseDeviceId);
  ui::KeyEvent expected_mouse_pressed_event = expected_key_event;
  expected_mouse_pressed_event.set_source_device_id(kMouseDeviceId);

  rewriter_->RewriteEvent(mouse_pressed_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(expected_mouse_pressed_event),
            ConvertToString(*continuation.passthrough_event));
  ui::MouseEvent mouse_release_event =
      CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED, ui::EF_FORWARD_MOUSE_BUTTON,
                             ui::EF_FORWARD_MOUSE_BUTTON, kMouseDeviceId);

  continuation.reset();
  rewriter_->RewriteEvent(mouse_release_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  ui::KeyEvent expected_mouse_release_event = CreateKeyButtonEvent(
      ui::ET_KEY_RELEASED, expected_key_event.key_code(),
      expected_key_event.flags(), expected_key_event.code(),
      expected_key_event.GetDomKey());
  expected_mouse_release_event.set_source_device_id(kMouseDeviceId);
  EXPECT_EQ(ConvertToString(expected_mouse_release_event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_P(StaticShortcutActionRewritingTest,
       StaticShortcutGraphicsTabletRewriting) {
  const auto& [static_shortcut_action, expected_key_event] = GetParam();
  graphics_tablet_settings_->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kForward),
          mojom::RemappingAction::NewStaticShortcutAction(
              static_shortcut_action)));
  graphics_tablet_settings_->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kBack),
          mojom::RemappingAction::NewStaticShortcutAction(
              static_shortcut_action)));

  TestEventRewriterContinuation continuation;

  ui::MouseEvent pen_pressed_event = CreateMouseButtonEvent(
      ui::ET_MOUSE_PRESSED, ui::EF_FORWARD_MOUSE_BUTTON,
      ui::EF_FORWARD_MOUSE_BUTTON, kGraphicsTabletDeviceId);
  ui::KeyEvent expected_pen_pressed_event = expected_key_event;
  expected_pen_pressed_event.set_source_device_id(kGraphicsTabletDeviceId);

  rewriter_->RewriteEvent(pen_pressed_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(expected_pen_pressed_event),
            ConvertToString(*continuation.passthrough_event));

  ui::MouseEvent pen_release_event = CreateMouseButtonEvent(
      ui::ET_MOUSE_RELEASED, ui::EF_FORWARD_MOUSE_BUTTON,
      ui::EF_FORWARD_MOUSE_BUTTON, kGraphicsTabletDeviceId);
  ui::KeyEvent expected_pen_release_event = CreateKeyButtonEvent(
      ui::ET_KEY_RELEASED, expected_key_event.key_code(),
      expected_key_event.flags(), expected_key_event.code(),
      expected_key_event.GetDomKey());
  expected_pen_release_event.set_source_device_id(kGraphicsTabletDeviceId);

  continuation.reset();
  rewriter_->RewriteEvent(pen_release_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(expected_pen_release_event),
            ConvertToString(*continuation.passthrough_event));

  ui::MouseEvent tablet_pressed_event =
      CreateMouseButtonEvent(ui::ET_MOUSE_PRESSED, ui::EF_BACK_MOUSE_BUTTON,
                             ui::EF_BACK_MOUSE_BUTTON, kGraphicsTabletDeviceId);
  ui::KeyEvent expected_tablet_pressed_event = expected_key_event;
  expected_tablet_pressed_event.set_source_device_id(kGraphicsTabletDeviceId);

  continuation.reset();
  rewriter_->RewriteEvent(tablet_pressed_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(expected_tablet_pressed_event),
            ConvertToString(*continuation.passthrough_event));

  ui::MouseEvent tablet_release_event =
      CreateMouseButtonEvent(ui::ET_MOUSE_RELEASED, ui::EF_BACK_MOUSE_BUTTON,
                             ui::EF_BACK_MOUSE_BUTTON, kGraphicsTabletDeviceId);
  ui::KeyEvent expected_tablet_release_event = CreateKeyButtonEvent(
      ui::ET_KEY_RELEASED, expected_key_event.key_code(),
      expected_key_event.flags(), expected_key_event.code(),
      expected_key_event.GetDomKey());
  expected_tablet_release_event.set_source_device_id(kGraphicsTabletDeviceId);

  continuation.reset();
  rewriter_->RewriteEvent(tablet_release_event,
                          continuation.weak_ptr_factory_.GetWeakPtr());

  ASSERT_TRUE(continuation.passthrough_event);
  EXPECT_EQ(ConvertToString(expected_tablet_release_event),
            ConvertToString(*continuation.passthrough_event));
}

}  // namespace ash
