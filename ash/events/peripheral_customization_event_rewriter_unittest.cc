// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/peripheral_customization_event_rewriter.h"

#include <linux/input.h>

#include <algorithm>
#include <memory>
#include <variant>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/accelerator_keycode_lookup_cache.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/test/mock_input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/mouse_button_property.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/test/test_event_rewriter_continuation.h"
#include "ui/events/test/test_event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ash {

namespace {

constexpr int kMouseDeviceId = 1;
constexpr int kGraphicsTabletDeviceId = 2;
constexpr int kRandomKeyboardDeviceId = 3;
constexpr int kComboDeviceId = 4;

class TestEventSink : public ui::EventSink {
 public:
  TestEventSink() = default;
  TestEventSink(const TestEventSink&) = delete;
  TestEventSink& operator=(const TestEventSink&) = delete;
  ~TestEventSink() override = default;

  // Returns the recorded events.
  std::vector<std::unique_ptr<ui::Event>> TakeEvents() {
    return std::move(events_);
  }

  // ui::EventSink:
  ui::EventDispatchDetails OnEventFromSource(ui::Event* event) override {
    events_.emplace_back(event->Clone());
    return ui::EventDispatchDetails();
  }

 private:
  std::vector<std::unique_ptr<ui::Event>> events_;
};

struct TestKeyEvent {
  ui::EventType type;
  ui::DomCode code;
  ui::DomKey key;
  ui::KeyboardCode keycode;
  ui::EventFlags flags = ui::EF_NONE;

  bool operator==(const TestKeyEvent&) const = default;
};

struct TestMouseEvent {
  ui::EventType type;
  ui::EventFlags flags;
  ui::EventFlags changed_button_flags;
  uint32_t linux_key_code;

  bool operator==(const TestMouseEvent&) const = default;
};

struct TestMouseScrollEvent {
  bool direction;
  ui::EventFlags flags;

  bool operator==(const TestMouseScrollEvent&) const = default;
};

using TestEventVariant =
    std::variant<TestKeyEvent, TestMouseEvent, TestMouseScrollEvent>;

std::string ConvertToString(const TestMouseScrollEvent& mouse_scroll_event) {
  std::string direction_name = mouse_scroll_event.direction ? "Left" : "Right";
  return base::StringPrintf("MouseScrollEvent direction=%s",
                            direction_name.c_str());
}

std::string ConvertToString(const TestMouseEvent& mouse_event) {
  std::string flags_name =
      base::JoinString(ui::EventFlagsNames(mouse_event.flags), "|");
  std::string changed_button_flags_name = base::JoinString(
      ui::EventFlagsNames(mouse_event.changed_button_flags), "|");
  return base::StringPrintf(
      "MouseEvent type=%d flags=%s(0x%X) changed_button_flags=%s(0x%X)",
      mouse_event.type, flags_name.c_str(), mouse_event.flags,
      changed_button_flags_name.c_str(), mouse_event.changed_button_flags);
}

std::string ConvertToString(const TestKeyEvent& key_event) {
  std::string flags_name =
      base::JoinString(ui::KeyEventFlagsNames(key_event.flags), "|");
  return base::StringPrintf(
      "KeyboardEvent type=%d code=%s(0x%06X) flags=%s(0x%X) vk=0x%02X "
      "key=%s(0x%08X)",
      key_event.type,
      ui::KeycodeConverter::DomCodeToCodeString(key_event.code).c_str(),
      static_cast<uint32_t>(key_event.code), flags_name.c_str(),
      key_event.flags, key_event.keycode,
      ui::KeycodeConverter::DomKeyToKeyString(key_event.key).c_str(),
      static_cast<uint32_t>(key_event.key));
}

std::string ConvertToString(const TestEventVariant& event) {
  return std::visit([](auto&& event) { return ConvertToString(event); }, event);
}

inline std::ostream& operator<<(std::ostream& os,
                                const TestEventVariant& event) {
  return os << ConvertToString(event);
}

// Factory template of TestKeyEvents just to reduce a lot of code/data
// duplication.
template <ui::DomCode code,
          ui::DomKey::Base key,
          ui::KeyboardCode keycode,
          ui::EventFlags modifier_flag = ui::EF_NONE,
          ui::DomKey::Base shifted_key = key>
struct TestKey {
  // Returns press key event.
  static constexpr TestKeyEvent Pressed(ui::EventFlags flags = ui::EF_NONE) {
    return {ui::EventType::kKeyPressed, code,
            (flags & ui::EF_SHIFT_DOWN) ? shifted_key : key, keycode,
            flags | modifier_flag};
  }

  // Returns release key event.
  static constexpr TestKeyEvent Released(ui::EventFlags flags = ui::EF_NONE) {
    // Note: modifier flag should not be present on release events.
    return {ui::EventType::kKeyReleased, code,
            (flags & ui::EF_SHIFT_DOWN) ? shifted_key : key, keycode, flags};
  }

  // Returns press then release key events.
  static std::vector<TestEventVariant> Typed(
      ui::EventFlags flags = ui::EF_NONE) {
    return {Pressed(flags), Released(flags)};
  }
};

// Short cut of TestKey construction for Character keys.
template <ui::DomCode code,
          char key,
          ui::KeyboardCode keycode,
          char shifted_key = key>
using TestCharKey = TestKey<code,
                            ui::DomKey::FromCharacter(key),
                            keycode,
                            ui::EF_NONE,
                            ui::DomKey::FromCharacter(shifted_key)>;

template <ui::EventFlags changed_button_flag, uint32_t linux_key_code = 0>
struct TestButton {
  // Returns press button event.
  static constexpr TestMouseEvent Pressed(ui::EventFlags flags = ui::EF_NONE) {
    return {ui::EventType::kMousePressed, flags | changed_button_flag,
            changed_button_flag, linux_key_code};
  }

  // Returns release button events.
  static constexpr TestMouseEvent Released(ui::EventFlags flags = ui::EF_NONE) {
    return {ui::EventType::kMouseReleased, flags | changed_button_flag,
            changed_button_flag, linux_key_code};
  }

  // Returns press then release button events.
  static std::vector<TestEventVariant> Typed(
      ui::EventFlags flags = ui::EF_NONE) {
    return {Pressed(flags), Released(flags)};
  }
};

template <bool direction>
struct TestScroll {
  // Returns scroll event.
  static constexpr std::vector<TestEventVariant> Typed(
      ui::EventFlags flags = ui::EF_NONE) {
    return std::vector<TestEventVariant>{
        TestMouseScrollEvent{direction, flags}};
  }
};

using ButtonLeft = TestButton<ui::EF_LEFT_MOUSE_BUTTON>;
using ButtonRight = TestButton<ui::EF_RIGHT_MOUSE_BUTTON>;
using ButtonMiddle = TestButton<ui::EF_MIDDLE_MOUSE_BUTTON>;
using ButtonForward = TestButton<ui::EF_FORWARD_MOUSE_BUTTON, BTN_FORWARD>;
using ButtonBack = TestButton<ui::EF_BACK_MOUSE_BUTTON, BTN_BACK>;
using ButtonExtra = TestButton<ui::EF_FORWARD_MOUSE_BUTTON, BTN_EXTRA>;
using ButtonSide = TestButton<ui::EF_BACK_MOUSE_BUTTON, BTN_SIDE>;

using ScrollLeft = TestScroll<true>;
using ScrollRight = TestScroll<false>;

using KeyA = TestCharKey<ui::DomCode::US_A, 'a', ui::VKEY_A, 'A'>;
using KeyB = TestCharKey<ui::DomCode::US_B, 'b', ui::VKEY_B, 'B'>;
using KeyC = TestCharKey<ui::DomCode::US_C, 'c', ui::VKEY_C, 'C'>;
using KeyD = TestCharKey<ui::DomCode::US_D, 'd', ui::VKEY_D, 'D'>;
using KeyM = TestCharKey<ui::DomCode::US_M, 'm', ui::VKEY_M, 'M'>;
using KeyN = TestCharKey<ui::DomCode::US_N, 'n', ui::VKEY_N, 'N'>;
using KeyV = TestCharKey<ui::DomCode::US_V, 'v', ui::VKEY_V, 'V'>;
using KeyZ = TestCharKey<ui::DomCode::US_Z, 'z', ui::VKEY_Z, 'Z'>;
using KeyComma = TestCharKey<ui::DomCode::COMMA, ',', ui::VKEY_OEM_COMMA, '<'>;
using KeyPeriod =
    TestCharKey<ui::DomCode::PERIOD, '.', ui::VKEY_OEM_PERIOD, '>'>;
using KeyDigit1 = TestCharKey<ui::DomCode::DIGIT1, '1', ui::VKEY_1, '!'>;
using KeyDigit2 = TestCharKey<ui::DomCode::DIGIT2, '2', ui::VKEY_2, '@'>;
using KeyDigit3 = TestCharKey<ui::DomCode::DIGIT3, '3', ui::VKEY_3, '#'>;
using KeyDigit4 = TestCharKey<ui::DomCode::DIGIT4, '4', ui::VKEY_4, '$'>;
using KeyDigit5 = TestCharKey<ui::DomCode::DIGIT5, '5', ui::VKEY_5, '%'>;
using KeyDigit6 = TestCharKey<ui::DomCode::DIGIT6, '6', ui::VKEY_6, '^'>;
using KeyDigit7 = TestCharKey<ui::DomCode::DIGIT7, '7', ui::VKEY_7, '&'>;
using KeyDigit8 = TestCharKey<ui::DomCode::DIGIT8, '8', ui::VKEY_8, '*'>;
using KeyDigit9 = TestCharKey<ui::DomCode::DIGIT9, '9', ui::VKEY_9, '('>;
using KeyDigit0 = TestCharKey<ui::DomCode::DIGIT0, '0', ui::VKEY_0, ')'>;
using KeyMinus = TestCharKey<ui::DomCode::MINUS, '-', ui::VKEY_OEM_MINUS, '_'>;
using KeyEqual = TestCharKey<ui::DomCode::EQUAL, '=', ui::VKEY_OEM_PLUS, '+'>;
using KeyArrowLeft =
    TestKey<ui::DomCode::ARROW_LEFT, ui::DomKey::ARROW_LEFT, ui::VKEY_LEFT>;
using KeyArrowRight =
    TestKey<ui::DomCode::ARROW_RIGHT, ui::DomKey::ARROW_RIGHT, ui::VKEY_RIGHT>;
using KeyArrowUp =
    TestKey<ui::DomCode::ARROW_UP, ui::DomKey::ARROW_UP, ui::VKEY_UP>;
using KeyArrowDown =
    TestKey<ui::DomCode::ARROW_DOWN, ui::DomKey::ARROW_DOWN, ui::VKEY_DOWN>;
using KeyBrowserBack = TestKey<ui::DomCode::BROWSER_BACK,
                               ui::DomKey::BROWSER_BACK,
                               ui::VKEY_BROWSER_BACK>;
using KeyBrowserForward = TestKey<ui::DomCode::BROWSER_FORWARD,
                                  ui::DomKey::BROWSER_FORWARD,
                                  ui::VKEY_BROWSER_FORWARD>;
using KeyRightAlt = TestKey<ui::DomCode::LAUNCH_ASSISTANT,
                            ui::DomKey::LAUNCH_ASSISTANT,
                            ui::VKEY_RIGHT_ALT>;

// Modifier keys.
using KeyLShift = TestKey<ui::DomCode::SHIFT_LEFT,
                          ui::DomKey::SHIFT,
                          ui::VKEY_SHIFT,
                          ui::EF_SHIFT_DOWN>;
using KeyRShift = TestKey<ui::DomCode::SHIFT_RIGHT,
                          ui::DomKey::SHIFT,
                          ui::VKEY_RSHIFT,
                          ui::EF_SHIFT_DOWN>;
using KeyLMeta = TestKey<ui::DomCode::META_LEFT,
                         ui::DomKey::META,
                         ui::VKEY_LWIN,
                         ui::EF_COMMAND_DOWN>;
using KeyRMeta = TestKey<ui::DomCode::META_RIGHT,
                         ui::DomKey::META,
                         ui::VKEY_RWIN,
                         ui::EF_COMMAND_DOWN>;
using KeyLControl = TestKey<ui::DomCode::CONTROL_LEFT,
                            ui::DomKey::CONTROL,
                            ui::VKEY_CONTROL,
                            ui::EF_CONTROL_DOWN>;
using KeyRControl = TestKey<ui::DomCode::CONTROL_RIGHT,
                            ui::DomKey::CONTROL,
                            ui::VKEY_RCONTROL,
                            ui::EF_CONTROL_DOWN>;
using KeyLAlt = TestKey<ui::DomCode::ALT_LEFT,
                        ui::DomKey::ALT,
                        ui::VKEY_MENU,
                        ui::EF_ALT_DOWN>;
using KeyRAlt = TestKey<ui::DomCode::ALT_RIGHT,
                        ui::DomKey::ALT,
                        ui::VKEY_RMENU,
                        ui::EF_ALT_DOWN>;

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
  std::optional<AcceleratorAction> action_performed_;
};

struct EventRewriterTestData {
  std::vector<TestEventVariant> incoming_events;
  std::vector<TestEventVariant> rewritten_events;
  std::optional<mojom::Button> pressed_button;

  EventRewriterTestData(std::vector<TestEventVariant> incoming_events,
                        std::vector<TestEventVariant> rewritten_events)
      : incoming_events(incoming_events),
        rewritten_events(rewritten_events),
        pressed_button(std::nullopt) {}

  EventRewriterTestData(std::vector<TestEventVariant> incoming_events,
                        std::vector<TestEventVariant> rewritten_events,
                        mojom::CustomizableButton button)
      : incoming_events(incoming_events), rewritten_events(rewritten_events) {
    pressed_button = mojom::Button();
    pressed_button->set_customizable_button(button);
  }

  EventRewriterTestData(std::vector<TestEventVariant> incoming_events,
                        std::vector<TestEventVariant> rewritten_events,
                        ui::KeyboardCode key_code)
      : incoming_events(incoming_events), rewritten_events(rewritten_events) {
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

std::string ConvertToString(const ui::Event& event) {
  if (event.IsMouseEvent()) {
    return ConvertToString(*event.AsMouseEvent());
  }
  if (event.IsKeyEvent()) {
    return ConvertToString(*event.AsKeyEvent());
  }
  NOTREACHED();
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
    controller_ = std::make_unique<
        testing::NiceMock<TestInputDeviceSettingsController>>();
    mouse_ = mojom::Mouse::New();
    mouse_->settings = mojom::MouseSettings::New();
    graphics_tablet_ = mojom::GraphicsTablet::New();
    graphics_tablet_->settings = mojom::GraphicsTabletSettings::New();
    keyboard_ = mojom::Keyboard::New();
    keyboard_->settings = mojom::KeyboardSettings::New();
    ON_CALL(*controller_, GetMouseSettings(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetGraphicsTabletSettings(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetKeyboardSettings(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetMouse(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetGraphicsTablet(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetKeyboard(testing::_))
        .WillByDefault(testing::Return(nullptr));
    ON_CALL(*controller_, GetMouseSettings(kMouseDeviceId))
        .WillByDefault(testing::Return(mouse_->settings.get()));
    ON_CALL(*controller_, GetGraphicsTabletSettings(kGraphicsTabletDeviceId))
        .WillByDefault(testing::Return(graphics_tablet_->settings.get()));
    ON_CALL(*controller_, GetKeyboardSettings(kRandomKeyboardDeviceId))
        .WillByDefault(testing::Return(keyboard_->settings.get()));
    ON_CALL(*controller_, GetKeyboard(kRandomKeyboardDeviceId))
        .WillByDefault(testing::Return(keyboard_.get()));
    ON_CALL(*controller_, GetKeyboard(kComboDeviceId))
        .WillByDefault(testing::Return(keyboard_.get()));
    ON_CALL(*controller_, GetGraphicsTablet(kGraphicsTabletDeviceId))
        .WillByDefault(testing::Return(graphics_tablet_.get()));
    ON_CALL(*controller_, GetMouse(kMouseDeviceId))
        .WillByDefault(testing::Return(mouse_.get()));
    ON_CALL(*controller_, GetMouse(kComboDeviceId))
        .WillByDefault(testing::Return(mouse_.get()));
    rewriter_ = std::make_unique<PeripheralCustomizationEventRewriter>(
        controller_.get());
    metrics_manager_ = std::make_unique<InputDeviceSettingsMetricsManager>();

    source_.AddEventRewriter(rewriter_.get());
  }

  void TearDown() override {
    source_.RemoveEventRewriter(rewriter_.get());

    rewriter_.reset();
    controller_.reset();
    controller_scoped_resetter_.reset();
    AshTestBase::TearDown();
    scoped_feature_list_.Reset();
    metrics_manager_.reset();
  }

  std::vector<TestEventVariant> RunRewriter(
      const std::vector<TestEventVariant>& events,
      ui::EventFlags extra_flags = ui::EF_NONE,
      int device_id = kMouseDeviceId) {
    struct ModifierInfo {
      ui::EventFlags flag;
      ui::DomCode code;
      ui::DomKey key;
      ui::KeyboardCode keycode;
    };
    // We'll use modifier keys at left side heuristically.
    static constexpr ModifierInfo kModifierList[] = {
        {ui::EF_SHIFT_DOWN, ui::DomCode::SHIFT_LEFT, ui::DomKey::SHIFT,
         ui::VKEY_SHIFT},
        {ui::EF_CONTROL_DOWN, ui::DomCode::CONTROL_LEFT, ui::DomKey::CONTROL,
         ui::VKEY_CONTROL},
        {ui::EF_ALT_DOWN, ui::DomCode::ALT_LEFT, ui::DomKey::ALT,
         ui::VKEY_MENU},
        {ui::EF_COMMAND_DOWN, ui::DomCode::META_LEFT, ui::DomKey::META,
         ui::VKEY_LWIN},
        {ui::EF_MOD3_DOWN, ui::DomCode::CAPS_LOCK, ui::DomKey::CAPS_LOCK,
         ui::VKEY_CAPITAL},
    };

    // Send modifier key press events to update rewriter's modifier flag state.
    ui::EventFlags current_flags = 0;
    for (const auto& modifier : kModifierList) {
      if (!(extra_flags & modifier.flag)) {
        continue;
      }
      current_flags |= modifier.flag;
      SendKeyEvent(TestKeyEvent{ui::EventType::kKeyPressed, modifier.code,
                                modifier.key, modifier.keycode, current_flags},
                   kRandomKeyboardDeviceId);
    }
    CHECK_EQ(current_flags, extra_flags);

    // Add extra_flags to each event.
    std::vector<TestEventVariant> events_with_added_flags;
    for (const auto& event : events) {
      if (const auto* test_key_event = std::get_if<TestKeyEvent>(&event)) {
        TestKeyEvent new_event = *test_key_event;
        new_event.flags = new_event.flags | current_flags;
        events_with_added_flags.push_back(new_event);
      } else if (const auto* test_mouse_event =
                     std::get_if<TestMouseEvent>(&event)) {
        TestMouseEvent new_event = *test_mouse_event;
        new_event.flags = new_event.flags | current_flags;
        events_with_added_flags.push_back(new_event);
      } else {
        const auto* test_scroll_event =
            std::get_if<TestMouseScrollEvent>(&event);
        TestMouseScrollEvent new_event = *test_scroll_event;
        new_event.flags = new_event.flags | current_flags;
        events_with_added_flags.push_back(new_event);
      }
    }
    auto result = SendKeyEvents(events_with_added_flags, device_id);

    // Send modifier key release events to unset rewriter'.s modifier flag
    // state.
    for (const auto& modifier : base::Reversed(kModifierList)) {
      if (!(extra_flags & modifier.flag)) {
        continue;
      }
      current_flags &= ~modifier.flag;
      SendKeyEvent(TestKeyEvent{ui::EventType::kKeyReleased, modifier.code,
                                modifier.key, modifier.keycode, current_flags},
                   kRandomKeyboardDeviceId);
    }
    CHECK_EQ(current_flags, 0);

    return result;
  }

  // Sends a KeyEvent to the rewriter, returns the rewritten events.
  // Note: one event may be rewritten into multiple events.
  std::vector<TestEventVariant> SendKeyEvent(const TestKeyEvent& event,
                                             int device_id) {
    return SendKeyEvents({event}, device_id);
  }

  std::vector<TestEventVariant> SendKeyEvents(
      const std::vector<TestEventVariant>& events,
      int device_id) {
    // Just in case some events may be there.
    if (!sink_.TakeEvents().empty()) {
      ADD_FAILURE() << "Rewritten events were left";
    }

    // Convert TestKeyEvent into ui::KeyEvent, then dispatch it to the
    // rewriter.
    for (const TestEventVariant& event : events) {
      if (const auto* test_key_event = std::get_if<TestKeyEvent>(&event)) {
        ui::KeyEvent key_event(test_key_event->type, test_key_event->keycode,
                               test_key_event->code, test_key_event->flags,
                               test_key_event->key, ui::EventTimeForNow());
        key_event.set_source_device_id(device_id);
        ui::EventDispatchDetails details = source_.Send(&key_event);
        CHECK(!details.dispatcher_destroyed);
      } else if (const auto* test_mouse_event =
                     std::get_if<TestMouseEvent>(&event)) {
        ui::MouseEvent mouse_event(test_mouse_event->type,
                                   /*location=*/gfx::PointF{},
                                   /*root_location=*/gfx::PointF{},
                                   /*time_stamp=*/ui::EventTimeForNow(),
                                   test_mouse_event->flags,
                                   test_mouse_event->changed_button_flags);
        mouse_event.set_source_device_id(device_id);
        if (test_mouse_event->linux_key_code) {
          ui::SetForwardBackMouseButtonProperty(
              mouse_event, test_mouse_event->linux_key_code);
        }
        ui::EventDispatchDetails details = source_.Send(&mouse_event);
        CHECK(!details.dispatcher_destroyed);
      } else {
        const auto* test_scroll_event =
            std::get_if<TestMouseScrollEvent>(&event);
        CHECK(test_scroll_event);
        // Left is negative, right is positive.
        const gfx::Vector2d offset(test_scroll_event->direction ? -1 : 1, 0);
        ui::MouseWheelEvent scroll_event(offset, /*location=*/gfx::PointF{},
                                         /*root_location=*/gfx::PointF{},
                                         /*time_stamp=*/ui::EventTimeForNow(),
                                         test_scroll_event->flags, ui::EF_NONE);
        scroll_event.set_source_device_id(device_id);
        ui::EventDispatchDetails details = source_.Send(&scroll_event);
        CHECK(!details.dispatcher_destroyed);
      }
    }

    // Convert the rewritten ui::Events back to TestKeyEvent.
    auto rewritten_events = sink_.TakeEvents();
    std::vector<TestEventVariant> result;
    for (const auto& rewritten_event : rewritten_events) {
      if (rewritten_event->IsKeyEvent()) {
        auto* rewritten_key_event = rewritten_event->AsKeyEvent();
        ui::KeyboardCode key_code =
            ui::HasRightAltProperty(*rewritten_key_event)
                ? ui::VKEY_RIGHT_ALT
                : rewritten_key_event->key_code();
        result.push_back(TestKeyEvent{rewritten_key_event->type(),
                                      rewritten_key_event->code(),
                                      rewritten_key_event->GetDomKey(),
                                      key_code, rewritten_key_event->flags()});

        // MouseWheelEvent must be checked before MouseEvent as its a subset of
        // mouse events.
      } else if (rewritten_event->IsMouseWheelEvent()) {
        auto* rewritten_scroll_event = rewritten_event->AsMouseWheelEvent();
        CHECK_EQ(0, rewritten_scroll_event->y_offset());
        result.push_back(TestMouseScrollEvent{
            // Left is negative, right is positive.
            (rewritten_scroll_event->x_offset() < 0 ? true : false),
            rewritten_scroll_event->flags()});
      } else if (rewritten_event->IsMouseEvent()) {
        auto* rewritten_mouse_event = rewritten_event->AsMouseEvent();
        auto property = ui::GetForwardBackMouseButtonProperty(*rewritten_event);
        result.push_back(TestMouseEvent{
            rewritten_mouse_event->type(), rewritten_mouse_event->flags(),
            rewritten_mouse_event->changed_button_flags(),
            property.value_or(0)});
      } else {
        ADD_FAILURE() << "Unexpected rewritten event: "
                      << rewritten_event->ToString();
        continue;
      }
    }
    return result;
  }

  void ApplyCustomizationFlag(std::vector<TestEventVariant>& test_events) {
    for (auto& test_event : test_events) {
      if (auto* test_key_event = std::get_if<TestKeyEvent>(&test_event)) {
        test_key_event->flags |= ui::EF_IS_CUSTOMIZED_FROM_BUTTON;
      }
    }
  }

 protected:
  std::unique_ptr<PeripheralCustomizationEventRewriter> rewriter_;
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      controller_scoped_resetter_;
  std::unique_ptr<testing::NiceMock<TestInputDeviceSettingsController>>
      controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  mojom::KeyboardPtr keyboard_;
  mojom::MousePtr mouse_;
  mojom::GraphicsTabletPtr graphics_tablet_;
  std::unique_ptr<InputDeviceSettingsMetricsManager> metrics_manager_;

  TestEventSink sink_;
  ui::test::TestEventSource source_{&sink_};
};

TEST_F(PeripheralCustomizationEventRewriterTest, MouseButtonWithoutObserving) {
  EXPECT_EQ(ButtonBack::Typed(), RunRewriter(ButtonBack::Typed()));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       InvalidEventTypeMouseObserving) {
  TestEventRewriterContinuation continuation;

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  ui::MouseEvent event = CreateMouseButtonEvent(ui::EventType::kMouseDragged,
                                                ui::EF_NONE, ui::EF_NONE);

  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseEvent());
  EXPECT_EQ(ConvertToString(event),
            ConvertToString(*continuation.passthrough_event));
}

TEST_F(PeripheralCustomizationEventRewriterTest, KeyEventActionRewriting) {
  TestAcceleratorObserver accelerator_observer;
  TestEventRewriterContinuation continuation;

  mouse_->settings->button_remappings.push_back(
      mojom::ButtonRemapping::New("", mojom::Button::NewVkey(ui::VKEY_A),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kBrightnessDown)));

  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter({KeyA::Pressed()}));
  ASSERT_TRUE(accelerator_observer.has_action_performed());

  accelerator_observer.reset();
  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter({KeyA::Released()}));
  ASSERT_FALSE(accelerator_observer.has_action_performed());
}

TEST_F(PeripheralCustomizationEventRewriterTest, MouseEventActionRewriting) {
  TestAcceleratorObserver accelerator_observer;

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      mojom::RemappingAction::NewAcceleratorAction(
          AcceleratorAction::kLaunchApp0)));

  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter({ButtonMiddle::Pressed()}));
  ASSERT_TRUE(accelerator_observer.has_action_performed());

  accelerator_observer.reset();
  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter({ButtonMiddle::Released()}));
  ASSERT_FALSE(accelerator_observer.has_action_performed());
}

TEST_F(PeripheralCustomizationEventRewriterTest, ScrollEventActionRewriting) {
  TestAcceleratorObserver accelerator_observer;

  mouse_->settings->button_remappings.push_back(
      mojom::ButtonRemapping::New("",
                                  mojom::Button::NewCustomizableButton(
                                      mojom::CustomizableButton::kScrollLeft),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kLaunchApp0)));

  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(ScrollLeft::Typed()));
  ASSERT_TRUE(accelerator_observer.has_action_performed());

  accelerator_observer.reset();
  EXPECT_EQ(ScrollRight::Typed(), RunRewriter(ScrollRight::Typed()));
  ASSERT_FALSE(accelerator_observer.has_action_performed());
}

TEST_F(PeripheralCustomizationEventRewriterTest, MouseWheelDuringObserving) {
  TestEventRewriterContinuation continuation;

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowAlphabetKeyEventRewrites);

  gfx::Vector2d expected_offset(/*x=*/100, /*y=*/50);
  ui::MouseWheelEvent event =
      ui::MouseWheelEvent(expected_offset, gfx::PointF{}, gfx::PointF{},
                          /*time_stamp=*/{}, ui::EF_NONE, ui::EF_NONE);
  event.set_source_device_id(kMouseDeviceId);

  rewriter_->RewriteEvent(event, continuation.weak_ptr_factory_.GetWeakPtr());
  ASSERT_TRUE(continuation.passthrough_event);
  ASSERT_TRUE(continuation.passthrough_event->IsMouseWheelEvent());
  EXPECT_EQ(expected_offset,
            continuation.passthrough_event->AsMouseWheelEvent()->offset());
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       MouseEventFlagAppliedOnRelease) {
  TestEventRewriterContinuation continuation;
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kDisable)));
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kMiddleClick)));

  EXPECT_EQ(ButtonMiddle::Typed(), RunRewriter(KeyDigit0::Typed()));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       KeyEventFlagNotAppliedOnRelease) {
  TestEventRewriterContinuation continuation;
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_CONTROL, static_cast<int>(ui::DomCode::CONTROL_LEFT),
          static_cast<int>(ui::DomKey::CONTROL), ui::EF_CONTROL_DOWN,
          /*key_display=*/""))));

  EXPECT_EQ(KeyLControl::Typed(ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
            RunRewriter(KeyDigit0::Typed()));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       ModifierReleasedDuringSequence) {
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_A, static_cast<int>(ui::DomCode::US_A),
          static_cast<int>(ui::DomKey::FromCharacter('a')), ui::EF_CONTROL_DOWN,
          /*key_display=*/""))));

  // Press digit 0 -> Ctrl + A.
  EXPECT_EQ((std::vector<TestEventVariant>{
                KeyLControl::Pressed(ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
                KeyA::Pressed(ui::EF_CONTROL_DOWN |
                              ui::EF_IS_CUSTOMIZED_FROM_BUTTON)}),
            (RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Pressed()})));

  // Press and release Ctrl on a different keyboard.
  EXPECT_EQ(KeyLControl::Typed(), RunRewriter(KeyLControl::Typed(), ui::EF_NONE,
                                              kRandomKeyboardDeviceId));

  // Expect that when digit 0 is released it emits A without the Ctrl modifier
  // flag.
  EXPECT_EQ(
      (std::vector<TestEventVariant>{
          KeyA::Released(ui::EF_IS_CUSTOMIZED_FROM_BUTTON)}),
      (RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Released()})));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       RemappedModifierReleasedDuringSequence) {
  // This test is only relevant when the keyboard rewriter fix is disabled.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kEnableKeyboardRewriterFix);

  keyboard_->settings->modifier_remappings[ui::mojom::ModifierKey::kAlt] =
      ui::mojom::ModifierKey::kControl;

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_A, static_cast<int>(ui::DomCode::US_A),
          static_cast<int>(ui::DomKey::FromCharacter('a')), ui::EF_CONTROL_DOWN,
          /*key_display=*/""))));

  // Press digit 0 -> Ctrl + A.
  EXPECT_EQ((std::vector<TestEventVariant>{
                KeyLControl::Pressed(ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
                KeyA::Pressed(ui::EF_CONTROL_DOWN |
                              ui::EF_IS_CUSTOMIZED_FROM_BUTTON)}),
            (RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Pressed()})));

  // Press and release Alt -> Ctrl on a different keyboard. Note that this still
  // appears as alt to this rewriter. This is a little weird, but nothing that
  // will break anything. We do not technically support modifier remappigns
  // across multiple keyboards (this case is already broken in
  // EventRewriterAsh). This change just makes it so we dont break anyone else.
  EXPECT_EQ(
      (std::vector<TestEventVariant>{KeyLAlt::Pressed(ui::EF_CONTROL_DOWN),
                                     KeyLAlt::Released()}),
      (RunRewriter(KeyLAlt::Typed(), ui::EF_NONE, kRandomKeyboardDeviceId)));

  // Expect that when digit 0 is released it emits A without the Ctrl modifier
  // flag.
  EXPECT_EQ(
      (std::vector<TestEventVariant>{
          KeyA::Released(ui::EF_IS_CUSTOMIZED_FROM_BUTTON)}),
      (RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Released()})));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       SwitchingLayoutsUpdatesDomKey) {
  std::unique_ptr<ui::StubKeyboardLayoutEngine> layout_engine =
      std::make_unique<ui::StubKeyboardLayoutEngine>();
  ui::KeyboardLayoutEngineManager::ResetKeyboardLayoutEngine();
  ui::KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(layout_engine.get());

  const std::vector<ui::StubKeyboardLayoutEngine::CustomLookupEntry> us_table =
      {{ui::DomCode::MINUS, ui::DomKey::FromCharacter(u'-'),
        ui::DomKey::FromCharacter(u'_'), ui::KeyboardCode::VKEY_OEM_MINUS},
       {ui::DomCode::BRACKET_LEFT, ui::DomKey::FromCharacter(u'['),
        ui::DomKey::FromCharacter(u'{'), ui::KeyboardCode::VKEY_OEM_4}};

  // Provide a custom layout that mimics behavior of a de-DE keyboard.
  // In the German keyboard, VKEY_OEM_4 is located at DomCode position MINUS
  // with DomKey `ß`. With positional remapping, VKEY_OEM_4 is remapped to
  // search for DomCode BRACKET_LEFT, resulting in DomKey `ü`.
  const std::vector<ui::StubKeyboardLayoutEngine::CustomLookupEntry> de_table =
      {{ui::DomCode::MINUS, ui::DomKey::FromCharacter(u'ß'),
        ui::DomKey::FromCharacter(u'?'), ui::KeyboardCode::VKEY_OEM_4},
       {ui::DomCode::BRACKET_LEFT, ui::DomKey::FromCharacter(u'ü'),
        ui::DomKey::FromCharacter(u'Ü'), ui::KeyboardCode::VKEY_OEM_1}};

  layout_engine->SetCustomLookupTableForTesting(us_table);

  TestEventRewriterContinuation continuation;
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_OEM_MINUS, static_cast<int>(ui::DomCode::MINUS),
          static_cast<int>(ui::DomKey::FromCharacter('-')), ui::EF_NONE,
          /*key_display=*/""))));
  EXPECT_EQ(KeyMinus::Typed(ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
            RunRewriter(KeyDigit0::Typed()));

  // Switch to German (DE) layout table and expect the remapped button to have a
  // different VKEY and DomKey.
  layout_engine->SetCustomLookupTableForTesting(de_table);
  ash::AcceleratorKeycodeLookupCache::Get()->Clear();

  EXPECT_EQ((TestKey<ui::DomCode::MINUS, ui::DomKey::FromCharacter(u'ß'),
                     ui::VKEY_OEM_4>::Typed(ui::EF_IS_CUSTOMIZED_FROM_BUTTON)),
            RunRewriter(KeyDigit0::Typed()));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       ModifiersAffectComputedDomKeyKeyEvent) {
  TestEventRewriterContinuation continuation;
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_A, static_cast<int>(ui::DomCode::US_A),
          static_cast<int>(ui::DomKey::FromCharacter('a')), ui::EF_NONE,
          /*key_display=*/""))));

  EXPECT_EQ(KeyA::Typed(ui::EF_SHIFT_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
            RunRewriter(KeyDigit0::Typed(ui::EF_SHIFT_DOWN)));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       ModifiersAffectComputedDomKeyMouseEvent) {
  TestEventRewriterContinuation continuation;
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_A, static_cast<int>(ui::DomCode::US_A),
          static_cast<int>(ui::DomKey::FromCharacter('a')), ui::EF_NONE,
          /*key_display=*/""))));

  EXPECT_EQ(KeyA::Typed(ui::EF_SHIFT_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
            RunRewriter(ButtonForward::Typed(ui::EF_SHIFT_DOWN)));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       ModifierPressedAffectsDomKeyOnOtherDevices) {
  TestEventRewriterContinuation continuation;
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_SHIFT, static_cast<int>(ui::DomCode::SHIFT_LEFT),
          static_cast<int>(ui::DomKey::SHIFT), ui::EF_SHIFT_DOWN,
          /*key_display=*/""))));

  EXPECT_EQ(std::vector<TestEventVariant>{KeyLShift::Pressed(
                ui::EF_IS_CUSTOMIZED_FROM_BUTTON)},
            RunRewriter({ButtonForward::Pressed()}));
  EXPECT_EQ(KeyA::Typed(ui::EF_SHIFT_DOWN),
            RunRewriter(KeyA::Typed(), ui::EF_NONE, kRandomKeyboardDeviceId));
}

TEST_F(PeripheralCustomizationEventRewriterTest, InvalidRegistrationMetric) {
  mojom::Mouse test_mouse;
  ON_CALL(*controller_, GetMouse(testing::_))
      .WillByDefault(testing::Return(&test_mouse));

  base::HistogramTester histogram_tester;

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      mojom::CustomizationRestriction::kDisableKeyEventRewrites);

  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed()));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Mouse.InvalidRegistration", KeyA::Pressed().keycode, 1);

  EXPECT_EQ(KeyB::Typed(), RunRewriter(KeyB::Typed()));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Mouse.InvalidRegistration", KeyB::Pressed().keycode, 1);
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       ComboMouseInvalidRegistrationMetric) {
  base::HistogramTester histogram_tester;

  rewriter_->StartObservingMouse(
      kComboDeviceId,
      mojom::CustomizationRestriction::kDisableKeyEventRewrites);

  EXPECT_EQ(KeyA::Typed(),
            RunRewriter(KeyA::Typed(), ui::EF_NONE, kComboDeviceId));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Mouse.InvalidRegistration.Combo",
      KeyA::Pressed().keycode, 1);

  EXPECT_EQ(KeyB::Typed(),
            RunRewriter(KeyB::Typed(), ui::EF_NONE, kComboDeviceId));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Mouse.InvalidRegistration.Combo",
      KeyB::Pressed().keycode, 1);

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      mojom::CustomizationRestriction::kDisableKeyEventRewrites);

  EXPECT_EQ(KeyA::Typed(), RunRewriter(KeyA::Typed()));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Mouse.InvalidRegistration.NonCombo",
      KeyA::Pressed().keycode, 1);

  EXPECT_EQ(KeyB::Typed(), RunRewriter(KeyB::Typed()));
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.Mouse.InvalidRegistration.NonCombo",
      KeyB::Pressed().keycode, 1);
}

TEST_F(PeripheralCustomizationEventRewriterTest, RightAltRewrite) {
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      /*name=*/"", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          ui::VKEY_RIGHT_ALT, static_cast<int>(ui::DomCode::LAUNCH_ASSISTANT),
          static_cast<int>(ui::DomKey::LAUNCH_ASSISTANT), ui::EF_NONE,
          /*key_display=*/""))));
  EXPECT_EQ(KeyRightAlt::Typed(ui::EF_IS_CUSTOMIZED_FROM_BUTTON),
            RunRewriter(KeyDigit0::Typed()));
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
            ButtonBack::Typed(),
            {},
            mojom::CustomizableButton::kBack,
        },
        {
            ButtonForward::Typed(),
            {},
            mojom::CustomizableButton::kForward,
        },
        {
            ButtonMiddle::Typed(),
            {},
            mojom::CustomizableButton::kMiddle,
        },
        {
            ButtonMiddle::Typed(ui::EF_LEFT_MOUSE_BUTTON),
            {},
            mojom::CustomizableButton::kMiddle,
        },

        // Observer notified only when mouse button pressed.
        {{ButtonBack::Released()},
         /*rewritten_events=*/std::vector<TestEventVariant>()},

        // Left click ignored for buttons from a mouse.
        {ButtonLeft::Typed(), ButtonLeft::Typed()},

        // Right click ignored for buttons from a mouse.
        {
            ButtonRight::Typed(),
            ButtonRight::Typed(),
        },

        // Remapped flags are ignored when included in the event with other
        // buttons.
        {ButtonLeft::Typed(ui::EF_BACK_MOUSE_BUTTON), ButtonLeft::Typed()},

        {
            ButtonRight::Typed(ui::EF_MIDDLE_MOUSE_BUTTON),
            ButtonRight::Typed(),
        },

        // KeyEvent tests:
        {
            KeyA::Typed(ui::EF_COMMAND_DOWN),
            std::vector<TestEventVariant>(),
            ui::VKEY_A,
        },
        {
            KeyB::Typed(),
            std::vector<TestEventVariant>(),
            ui::VKEY_B,
        },

        // Scroll tests:
        {
            ScrollLeft::Typed(),
            std::vector<TestEventVariant>(),
            mojom::CustomizableButton::kScrollLeft,
        },
        {
            ScrollRight::Typed(),
            std::vector<TestEventVariant>(),
            mojom::CustomizableButton::kScrollRight,
        },
    }),
    [](const testing::TestParamInfo<EventRewriterTestData>& info) {
      std::string name = ConvertToString(info.param.incoming_events.front());
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '=', '_');
      std::replace(name.begin(), name.end(), '_', '_');
      std::replace(name.begin(), name.end(), '(', '_');
      std::replace(name.begin(), name.end(), ')', '_');
      std::replace(name.begin(), name.end(), '|', '_');
      return name;
    });

TEST_P(MouseButtonObserverTest, EventRewriting) {
  auto data = GetParam();

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  auto rewritten_events = RunRewriter(data.incoming_events);
  if (data.rewritten_events.empty()) {
    ASSERT_TRUE(rewritten_events.empty());
    if (data.pressed_button) {
      const auto& actual_pressed_buttons =
          controller_->pressed_mouse_buttons().at(kMouseDeviceId);
      ASSERT_EQ(1u, actual_pressed_buttons.size());
      EXPECT_EQ(*data.pressed_button, *actual_pressed_buttons[0]);
    }
  } else {
    ASSERT_FALSE(rewritten_events.empty());
    EXPECT_EQ(data.rewritten_events, rewritten_events);
  }

  rewriter_->StopObserving();

  // After we stop observing, the passthrough event should be an identity of the
  // original.
  EXPECT_EQ(data.incoming_events, RunRewriter(data.incoming_events));
}

TEST_F(MouseButtonObserverTest, MouseBackButtonRecognition) {
  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(ButtonBack::Typed()));

  const auto& actual_pressed_buttons =
      controller_->pressed_mouse_buttons().at(kMouseDeviceId);
  ASSERT_EQ(1u, actual_pressed_buttons.size());
  EXPECT_EQ(
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack),
      *actual_pressed_buttons[0]);
}

TEST_F(MouseButtonObserverTest, MouseSideButtonRecognition) {
  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(ButtonSide::Typed()));

  const auto& actual_pressed_buttons =
      controller_->pressed_mouse_buttons().at(kMouseDeviceId);
  ASSERT_EQ(1u, actual_pressed_buttons.size());
  EXPECT_EQ(
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kSide),
      *actual_pressed_buttons[0]);
}

TEST_F(MouseButtonObserverTest, MouseForwardButtonRecognition) {
  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter(ButtonForward::Typed()));

  const auto& actual_pressed_buttons =
      controller_->pressed_mouse_buttons().at(kMouseDeviceId);
  ASSERT_EQ(1u, actual_pressed_buttons.size());
  EXPECT_EQ(*mojom::Button::NewCustomizableButton(
                mojom::CustomizableButton::kForward),
            *actual_pressed_buttons[0]);
}

TEST_F(MouseButtonObserverTest, MouseExtraButtonRecognition) {
  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(ButtonExtra::Typed()));

  const auto& actual_pressed_buttons =
      controller_->pressed_mouse_buttons().at(kMouseDeviceId);
  ASSERT_EQ(1u, actual_pressed_buttons.size());
  EXPECT_EQ(
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra),
      *actual_pressed_buttons[0]);
}

TEST_F(MouseButtonObserverTest, kDisableKeyEventRewritesRestriction) {
  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kDisableKeyEventRewrites);

  // Key events should not be modified if no key event customizations are
  // allowed.
  EXPECT_EQ(KeyA::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyA::Typed(ui::EF_COMMAND_DOWN)));

  // Mouse event should be discarded if only key event rewrites aren't allowed.
  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(ButtonSide::Typed()));
}

TEST_F(MouseButtonObserverTest, AllowCustomizationsRestriction) {
  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  // kAllowCustomizations should swallow both key events and mouse events.
  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter(KeyA::Typed(ui::EF_COMMAND_DOWN)));
  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(ButtonBack::Typed()));
}

TEST_F(PeripheralCustomizationEventRewriterTest,
       RewriteEventFromButtonEmitMetrics) {
  base::HistogramTester histogram_tester;
  mouse_->settings->button_remappings.push_back(
      mojom::ButtonRemapping::New("", mojom::Button::NewVkey(ui::VKEY_A),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kBrightnessDown)));

  graphics_tablet_->settings->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "", mojom::Button::NewVkey(ui::VKEY_Z),
          mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
              ui::KeyboardCode::VKEY_M, (int)ui::DomCode::US_M,
              (int)ui::DomKey::FromCharacter('M'),
              (int)ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
              /*key_display=*/""))));

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.AcceleratorAction."
      "Pressed",
      /*expected_count=*/0);

  RunRewriter(KeyA::Typed(), ui::EF_NONE, kMouseDeviceId);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.AcceleratorAction."
      "Pressed",
      /*expected_count=*/1u);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.GraphicsTabletPen.ButtonRemapping.KeyEvent."
      "Pressed",
      /*expected_count=*/0);

  RunRewriter(KeyZ::Typed(), ui::EF_NONE, kGraphicsTabletDeviceId);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.GraphicsTabletPen.ButtonRemapping.KeyEvent."
      "Pressed",
      /*expected_count=*/1u);
}

TEST_F(MouseButtonObserverTest, RewriteAlphabetKeyEvent) {
  TestEventRewriterContinuation continuation;

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowAlphabetKeyEventRewrites);

  // Key event shouldn't be discarded if the key code is not alphabet letter.
  EXPECT_EQ(KeyArrowLeft::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyArrowLeft::Typed(ui::EF_COMMAND_DOWN)));

  // New key event should be discarded if the key code is alphabet letter.
  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter(KeyA::Typed(ui::EF_COMMAND_DOWN)));
}

TEST_F(MouseButtonObserverTest, RewriteAlphabetOrNumberKeyEvent) {
  TestEventRewriterContinuation continuation;

  rewriter_->StartObservingMouse(
      kMouseDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowAlphabetOrNumberKeyEventRewrites);

  // Key event shouldn't be discarded if the key code is not alphabet letter or
  // number.
  EXPECT_EQ(KeyArrowLeft::Typed(ui::EF_COMMAND_DOWN),
            RunRewriter(KeyArrowLeft::Typed(ui::EF_COMMAND_DOWN)));

  // New key event should be discarded if the key code is alphabet letter.
  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter(KeyA::Typed()));

  // New key event should be discarded if the key code is a number.
  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter(KeyDigit0::Typed(ui::EF_COMMAND_DOWN)));
}

class GraphicsTabletButtonObserverTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<EventRewriterTestData> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    GraphicsTabletButtonObserverTest,
    testing::ValuesIn(std::vector<EventRewriterTestData>{
        {
            ButtonRight::Typed(),
            {},
            mojom::CustomizableButton::kRight,
        },
        {
            ButtonBack::Typed(),
            {},
            mojom::CustomizableButton::kBack,
        },
        {
            ButtonForward::Typed(),
            {},
            mojom::CustomizableButton::kForward,
        },
        {
            ButtonMiddle::Typed(),
            {},
            mojom::CustomizableButton::kMiddle,
        },
        {
            ButtonMiddle::Typed(ui::EF_LEFT_MOUSE_BUTTON),
            {},
            mojom::CustomizableButton::kMiddle,
        },

        // Left click ignored for buttons from a graphics tablet.
        {
            ButtonLeft::Typed(),
            ButtonLeft::Typed(),
        },

        // Other flags are ignored when included in the event with other
        // buttons.
        {
            ButtonLeft::Typed(ui::EF_BACK_MOUSE_BUTTON),
            ButtonLeft::Typed(),
        },
        {
            ButtonLeft::Typed(ui::EF_MIDDLE_MOUSE_BUTTON),
            ButtonLeft::Typed(),
        },

        // KeyEvent tests:
        {
            KeyA::Typed(ui::EF_COMMAND_DOWN),
            {},
            ui::VKEY_A,
        },
        {
            KeyB::Typed(),
            {},
            ui::VKEY_B,
        },
    }),
    [](const testing::TestParamInfo<EventRewriterTestData>& info) {
      std::string name = ConvertToString(info.param.incoming_events.front());
      std::replace(name.begin(), name.end(), ' ', '_');
      std::replace(name.begin(), name.end(), '=', '_');
      std::replace(name.begin(), name.end(), '_', '_');
      std::replace(name.begin(), name.end(), '(', '_');
      std::replace(name.begin(), name.end(), ')', '_');
      std::replace(name.begin(), name.end(), '|', '_');
      return name;
    });

TEST_P(GraphicsTabletButtonObserverTest, RewriteEvent) {
  auto data = GetParam();

  rewriter_->StartObservingGraphicsTablet(
      kGraphicsTabletDeviceId,
      /*customization_restriction=*/mojom::CustomizationRestriction::
          kAllowCustomizations);

  auto rewritten_events =
      RunRewriter(data.incoming_events, ui::EF_NONE, kGraphicsTabletDeviceId);
  if (data.rewritten_events.empty()) {
    ASSERT_TRUE(rewritten_events.empty());
    if (data.pressed_button) {
      const auto& actual_pressed_buttons =
          controller_->pressed_graphics_tablet_buttons().at(
              kGraphicsTabletDeviceId);
      ASSERT_EQ(1u, actual_pressed_buttons.size());
      EXPECT_EQ(*data.pressed_button, *actual_pressed_buttons[0]);
    }
  } else {
    ASSERT_FALSE(rewritten_events.empty());
    EXPECT_EQ(data.rewritten_events, rewritten_events);
  }

  rewriter_->StopObserving();

  // After we stop observing, the passthrough event should be an identity of the
  // original.
  EXPECT_EQ(data.incoming_events, RunRewriter(data.incoming_events, ui::EF_NONE,
                                              kGraphicsTabletDeviceId));
}

class ButtonRewritingTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<std::pair<mojom::Button, mojom::KeyEvent>,
                     EventRewriterTestData>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    ButtonRewritingTest,
    testing::ValuesIn(std::vector<
                      std::tuple<std::pair<mojom::Button, mojom::KeyEvent>,
                                 EventRewriterTestData>>{
        // KeyEvent rewriting test cases:
        // Remap A -> B.
        {{GetButton(ui::VKEY_A),
          mojom::KeyEvent(ui::VKEY_B,
                          static_cast<int>(ui::DomCode::US_B),
                          static_cast<int>(ui::DomKey::FromCharacter('b')),
                          ui::EF_NONE,
                          /*key_display=*/"")},
         {KeyA::Typed(), KeyB::Typed()}},

        // Remap A -> B, Pressing B is no-op.
        {{GetButton(ui::VKEY_A),
          mojom::KeyEvent(ui::VKEY_B,
                          static_cast<int>(ui::DomCode::US_B),
                          static_cast<int>(ui::DomKey::FromCharacter('b')),
                          ui::EF_NONE,
                          /*key_display=*/"")},
         {KeyB::Typed(), KeyB::Typed()}},

        // Remap CTRL -> ALT.
        {{GetButton(ui::VKEY_CONTROL),
          mojom::KeyEvent(ui::VKEY_MENU,
                          static_cast<int>(ui::DomCode::ALT_LEFT),
                          static_cast<int>(ui::DomKey::ALT),
                          ui::EF_ALT_DOWN,
                          /*key_display=*/"")},
         {KeyLControl::Typed(), KeyLAlt::Typed()}},

        // Remap CTRL -> ALT and press with shift down.
        {{GetButton(ui::VKEY_CONTROL),
          mojom::KeyEvent(ui::VKEY_MENU,
                          static_cast<int>(ui::DomCode::ALT_LEFT),
                          static_cast<int>(ui::DomKey::ALT),
                          ui::EF_ALT_DOWN,
                          /*key_display=*/"")},
         {KeyLControl::Typed(ui::EF_SHIFT_DOWN),
          KeyLAlt::Typed(ui::EF_SHIFT_DOWN)}},

        // Remap A -> CTRL + SHIFT + B.
        {{GetButton(ui::VKEY_A),
          mojom::KeyEvent(ui::VKEY_B,
                          static_cast<int>(ui::DomCode::US_B),
                          static_cast<int>(ui::DomKey::FromCharacter('b')),
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          /*key_display=*/"")},
         {KeyA::Typed(),
          std::vector<TestEventVariant>{
              KeyLControl::Pressed(), KeyLShift::Pressed(ui::EF_CONTROL_DOWN),
              KeyB::Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
              KeyB::Released(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
              KeyLShift::Released(ui::EF_CONTROL_DOWN),
              KeyLControl::Released()}}},

        // MouseEvent rewriting test cases:
        // Remap Middle -> CTRL + SHIFT + B.
        {{GetButton(mojom::CustomizableButton::kMiddle),
          mojom::KeyEvent(ui::VKEY_B,
                          static_cast<int>(ui::DomCode::US_B),
                          static_cast<int>(ui::DomKey::FromCharacter('b')),
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          /*key_display=*/"")},
         {ButtonMiddle::Typed(),
          std::vector<TestEventVariant>{
              KeyLControl::Pressed(), KeyLShift::Pressed(ui::EF_CONTROL_DOWN),
              KeyB::Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
              KeyB::Released(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
              KeyLShift::Released(ui::EF_CONTROL_DOWN),
              KeyLControl::Released()}}},

        // Remap Middle -> CTRL + SHIFT + B with ALT down.
        {{GetButton(mojom::CustomizableButton::kMiddle),
          mojom::KeyEvent(ui::VKEY_B,
                          static_cast<int>(ui::DomCode::US_B),
                          static_cast<int>(ui::DomKey::FromCharacter('b')),
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          /*key_display=*/"")},
         {ButtonMiddle::Typed(ui::EF_ALT_DOWN),
          std::vector<TestEventVariant>{
              KeyLControl::Pressed(ui::EF_ALT_DOWN),
              KeyLShift::Pressed(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
              KeyB::Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN |
                            ui::EF_ALT_DOWN),
              KeyB::Released(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN |
                             ui::EF_ALT_DOWN),
              KeyLShift::Released(ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
              KeyLControl::Released(ui::EF_ALT_DOWN)}}},

        // Remap Back -> Meta.
        {{GetButton(mojom::CustomizableButton::kBack),
          mojom::KeyEvent(ui::VKEY_LWIN,
                          static_cast<int>(ui::DomCode::META_LEFT),
                          static_cast<int>(ui::DomKey::META),
                          ui::EF_COMMAND_DOWN,
                          /*key_display=*/"")},
         {ButtonBack::Typed(), KeyLMeta::Typed()}},

        // Remap Middle -> B and check left mouse button is a no-op.
        {{GetButton(mojom::CustomizableButton::kMiddle),
          mojom::KeyEvent(ui::VKEY_B,
                          static_cast<int>(ui::DomCode::US_B),
                          static_cast<int>(ui::DomKey::FromCharacter('b')),
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          /*key_display=*/"")},
         {ButtonLeft::Typed(ui::EF_ALT_DOWN),
          ButtonLeft::Typed(ui::EF_ALT_DOWN)}},

        // Scroll Wheel tests:
        {{GetButton(mojom::CustomizableButton::kScrollLeft),
          mojom::KeyEvent(ui::VKEY_Z,
                          static_cast<int>(ui::DomCode::US_Z),
                          static_cast<int>(ui::DomKey::FromCharacter('z')),
                          ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                              ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN,
                          /*key_display=*/"")},
         {ScrollLeft::Typed(),
          std::vector<TestEventVariant>{
              KeyLMeta::Pressed(), KeyLControl::Pressed(ui::EF_COMMAND_DOWN),
              KeyLAlt::Pressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              KeyLShift::Pressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                                 ui::EF_ALT_DOWN),
              KeyZ::Pressed(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                            ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
              KeyZ::Released(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                             ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
              KeyLShift::Released(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN |
                                  ui::EF_ALT_DOWN),
              KeyLAlt::Released(ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN),
              KeyLControl::Released(ui::EF_COMMAND_DOWN),
              KeyLMeta::Released()}}},

        {{GetButton(mojom::CustomizableButton::kScrollLeft),
          mojom::KeyEvent(ui::VKEY_Z,
                          static_cast<int>(ui::DomCode::US_Z),
                          static_cast<int>(ui::DomKey::FromCharacter('z')),
                          ui::EF_COMMAND_DOWN,
                          /*key_display=*/"")},
         {ScrollLeft::Typed(),
          std::vector<TestEventVariant>{
              KeyLMeta::Pressed(), KeyZ::Pressed(ui::EF_COMMAND_DOWN),
              KeyZ::Released(ui::EF_COMMAND_DOWN), KeyLMeta::Released()}}},
    }));

TEST_P(ButtonRewritingTest, GraphicsPenRewriteEvent) {
  auto [tuple, data] = GetParam();
  auto& [button, key_event] = tuple;
  if (data.incoming_events != data.rewritten_events) {
    ApplyCustomizationFlag(data.rewritten_events);
  }

  graphics_tablet_->settings->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "", button.Clone(),
          mojom::RemappingAction::NewKeyEvent(key_event.Clone())));

  EXPECT_EQ(
      data.rewritten_events,
      RunRewriter(data.incoming_events, ui::EF_NONE, kGraphicsTabletDeviceId));
}

TEST_P(ButtonRewritingTest, GraphicsTabletRewriteEvent) {
  auto [tuple, data] = GetParam();
  auto& [button, key_event] = tuple;
  if (data.incoming_events != data.rewritten_events) {
    ApplyCustomizationFlag(data.rewritten_events);
  }

  graphics_tablet_->settings->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "", button.Clone(),
          mojom::RemappingAction::NewKeyEvent(key_event.Clone())));

  EXPECT_EQ(
      data.rewritten_events,
      RunRewriter(data.incoming_events, ui::EF_NONE, kGraphicsTabletDeviceId));
}

TEST_P(ButtonRewritingTest, MouseRewriteEvent) {
  auto [tuple, data] = GetParam();
  auto& [button, key_event] = tuple;
  if (data.incoming_events != data.rewritten_events) {
    ApplyCustomizationFlag(data.rewritten_events);
  }

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", button.Clone(),
      mojom::RemappingAction::NewKeyEvent(key_event.Clone())));

  EXPECT_EQ(data.rewritten_events,

            RunRewriter(data.incoming_events));
}

class ModifierRewritingTest : public PeripheralCustomizationEventRewriterTest,
                              public testing::WithParamInterface<TestKeyEvent> {
};

INSTANTIATE_TEST_SUITE_P(All,
                         ModifierRewritingTest,
                         testing::ValuesIn(std::vector<TestKeyEvent>({
                             KeyLMeta::Pressed(),
                             KeyRMeta::Pressed(),
                             KeyLShift::Pressed(),
                             KeyRShift::Pressed(),
                             KeyLControl::Pressed(),
                             KeyRControl::Pressed(),
                             KeyLAlt::Pressed(),
                             KeyRAlt::Pressed(),
                         })));

TEST_P(ModifierRewritingTest, ModifierKeyCombo) {
  const auto& data = GetParam();

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          data.keycode, (int)data.code, (int)data.key, data.flags,
          /*key_display=*/""))));

  auto modifier_pressed_event = data;
  modifier_pressed_event.flags |= ui::EF_IS_CUSTOMIZED_FROM_BUTTON;
  auto modifier_released_event = data;
  modifier_released_event.type = ui::EventType::kKeyReleased;
  modifier_released_event.flags = ui::EF_IS_CUSTOMIZED_FROM_BUTTON;

  // Press down remapped button that maps to a modifier.
  EXPECT_EQ((std::vector<TestEventVariant>{modifier_pressed_event}),
            RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Pressed()}));

  // When a key is pressed it should have the remapped modifier flag.
  EXPECT_EQ((KeyA::Typed(data.flags)), RunRewriter(KeyA::Typed()));

  // Release the remapped modifier button.
  EXPECT_EQ(std::vector<TestEventVariant>{modifier_released_event},
            RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Released()}));

  // Other pressed key should no longer have the remapped flag.
  EXPECT_EQ((KeyA::Typed()), RunRewriter(KeyA::Typed()));
}

TEST_P(ModifierRewritingTest, MultiModifierKeyCombo) {
  const auto& data = GetParam();

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          data.keycode, (int)data.code, (int)data.key, data.flags,
          /*key_display=*/""))));

  const ui::EventFlags test_flag = data.flags == ui::EF_COMMAND_DOWN
                                       ? ui::EF_SHIFT_DOWN
                                       : ui::EF_COMMAND_DOWN;

  auto modifier_pressed_event = data;
  modifier_pressed_event.flags |= ui::EF_IS_CUSTOMIZED_FROM_BUTTON;
  auto modifier_released_event = data;
  modifier_released_event.type = ui::EventType::kKeyReleased;
  modifier_released_event.flags = ui::EF_IS_CUSTOMIZED_FROM_BUTTON;

  // Press down remapped button that maps to a modifier.
  EXPECT_EQ((std::vector<TestEventVariant>{modifier_pressed_event}),
            RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Pressed()}));

  // When a key is pressed it should have the remapped modifier flag as well as
  // the additional test flag.
  EXPECT_EQ((KeyA::Typed(data.flags | test_flag)),
            RunRewriter(KeyA::Typed(test_flag)));

  // Release the remapped modifier button.
  EXPECT_EQ(std::vector<TestEventVariant>{modifier_released_event},
            RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Released()}));

  // Other pressed key should no longer have the remapped flag.
  EXPECT_EQ((KeyA::Typed(test_flag)), RunRewriter(KeyA::Typed(test_flag)));
}

TEST_P(ModifierRewritingTest, MouseEvent) {
  const auto& data = GetParam();

  const ui::EventFlags test_flag = data.flags == ui::EF_COMMAND_DOWN
                                       ? ui::EF_SHIFT_DOWN
                                       : ui::EF_COMMAND_DOWN;

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_0),
      mojom::RemappingAction::NewKeyEvent(mojom::KeyEvent::New(
          data.keycode, (int)data.code, (int)data.key, data.flags,
          /*key_display=*/""))));

  auto modifier_pressed_event = data;
  modifier_pressed_event.flags |= ui::EF_IS_CUSTOMIZED_FROM_BUTTON;
  auto modifier_released_event = data;
  modifier_released_event.type = ui::EventType::kKeyReleased;
  modifier_released_event.flags = ui::EF_IS_CUSTOMIZED_FROM_BUTTON;

  // Press down remapped button that maps to a modifier.
  EXPECT_EQ((std::vector<TestEventVariant>{modifier_pressed_event}),
            RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Pressed()}));

  // When a button is pressed it should have the remapped modifier flag as well
  // as the additional test flag.
  EXPECT_EQ((ButtonForward::Typed(data.flags | test_flag)),
            RunRewriter(ButtonForward::Typed(test_flag)));

  // Release the remapped modifier button.
  EXPECT_EQ(std::vector<TestEventVariant>{modifier_released_event},
            RunRewriter(std::vector<TestEventVariant>{KeyDigit0::Released()}));

  // Other pressed button should no longer have the remapped flag.
  EXPECT_EQ((KeyA::Typed(test_flag)), RunRewriter(KeyA::Typed(test_flag)));
}

class StaticShortcutActionRewritingTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<mojom::StaticShortcutAction,
                     std::vector<TestEventVariant>>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    StaticShortcutActionRewritingTest,
    testing::ValuesIn(std::vector<std::tuple<mojom::StaticShortcutAction,
                                             std::vector<TestEventVariant>>>({
        {mojom::StaticShortcutAction::kCopy,
         {KeyLControl::Pressed(), KeyC::Pressed(ui::EF_CONTROL_DOWN),
          KeyC::Released(ui::EF_CONTROL_DOWN), KeyLControl::Released()}},
        {mojom::StaticShortcutAction::kPaste,
         {KeyLControl::Pressed(), KeyV::Pressed(ui::EF_CONTROL_DOWN),
          KeyV::Released(ui::EF_CONTROL_DOWN), KeyLControl::Released()}},
        {mojom::StaticShortcutAction::kUndo,
         {KeyLControl::Pressed(), KeyZ::Pressed(ui::EF_CONTROL_DOWN),
          KeyZ::Released(ui::EF_CONTROL_DOWN), KeyLControl::Released()}},
        {mojom::StaticShortcutAction::kRedo,
         {KeyLControl::Pressed(), KeyLShift::Pressed(ui::EF_CONTROL_DOWN),
          KeyZ::Pressed(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
          KeyZ::Released(ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
          KeyLShift::Released(ui::EF_CONTROL_DOWN), KeyLControl::Released()}},
        {mojom::StaticShortcutAction::kZoomIn,
         {KeyLControl::Pressed(), KeyEqual::Pressed(ui::EF_CONTROL_DOWN),
          KeyEqual::Released(ui::EF_CONTROL_DOWN), KeyLControl::Released()}},
        {mojom::StaticShortcutAction::kZoomOut,
         {KeyLControl::Pressed(), KeyMinus::Pressed(ui::EF_CONTROL_DOWN),
          KeyMinus::Released(ui::EF_CONTROL_DOWN), KeyLControl::Released()}},
        {mojom::StaticShortcutAction::kPreviousPage, KeyBrowserBack::Typed()},
        {mojom::StaticShortcutAction::kNextPage, KeyBrowserForward::Typed()},
    })));

TEST_F(StaticShortcutActionRewritingTest, StaticShortcutDisableMouseRewriting) {
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kDisable)));
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_A),
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kDisable)));

  EXPECT_EQ(std::vector<TestEventVariant>{}, RunRewriter({KeyA::Typed()}));
  EXPECT_EQ(std::vector<TestEventVariant>{},
            RunRewriter({ButtonForward::Typed()}));
}

TEST_P(StaticShortcutActionRewritingTest, StaticShortcutMouseRewriting) {
  auto [static_shortcut_action, expected_key_events] = GetParam();
  ApplyCustomizationFlag(expected_key_events);

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));

  EXPECT_EQ(expected_key_events, (RunRewriter(ButtonForward::Typed())));
}

TEST_P(StaticShortcutActionRewritingTest, StaticShortcutMouseWheelRewriting) {
  auto [static_shortcut_action, expected_key_events] = GetParam();
  ApplyCustomizationFlag(expected_key_events);

  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kScrollLeft),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kScrollRight),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));

  EXPECT_EQ(expected_key_events, (RunRewriter(ScrollLeft::Typed())));
  EXPECT_EQ(expected_key_events, (RunRewriter(ScrollRight::Typed())));
}

TEST_P(StaticShortcutActionRewritingTest,
       StaticShortcutGraphicsTabletRewriting) {
  auto [static_shortcut_action, expected_key_events] = GetParam();
  ApplyCustomizationFlag(expected_key_events);

  graphics_tablet_->settings->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kForward),
          mojom::RemappingAction::NewStaticShortcutAction(
              static_shortcut_action)));
  graphics_tablet_->settings->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          "",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kBack),
          mojom::RemappingAction::NewStaticShortcutAction(
              static_shortcut_action)));

  EXPECT_EQ(expected_key_events,
            (RunRewriter(ButtonForward::Typed(), ui::EF_NONE,
                         kGraphicsTabletDeviceId)));
  EXPECT_EQ(expected_key_events, (RunRewriter(ButtonBack::Typed(), ui::EF_NONE,
                                              kGraphicsTabletDeviceId)));
}

class StaticShortcutActionMouseButtonRewritingTest
    : public PeripheralCustomizationEventRewriterTest,
      public testing::WithParamInterface<
          std::tuple<mojom::StaticShortcutAction,
                     std::vector<TestEventVariant>>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    StaticShortcutActionMouseButtonRewritingTest,
    testing::ValuesIn(std::vector<std::tuple<mojom::StaticShortcutAction,
                                             std::vector<TestEventVariant>>>{
        {mojom::StaticShortcutAction::kLeftClick, ButtonLeft::Typed()},
        {mojom::StaticShortcutAction::kRightClick, ButtonRight::Typed()},
        {mojom::StaticShortcutAction::kMiddleClick, ButtonMiddle::Typed()},
    }));

TEST_P(StaticShortcutActionMouseButtonRewritingTest, RewriteEvent) {
  const auto [static_shortcut_action, expected_events] = GetParam();
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));

  EXPECT_EQ(expected_events, RunRewriter({ButtonForward::Typed()}));
}

TEST_P(StaticShortcutActionMouseButtonRewritingTest, ScrollEventRewriteEvent) {
  const auto [static_shortcut_action, expected_events] = GetParam();
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kScrollLeft),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "",
      mojom::Button::NewCustomizableButton(
          mojom::CustomizableButton::kScrollRight),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));

  EXPECT_EQ(expected_events, RunRewriter({ScrollLeft::Typed()}));
  EXPECT_EQ(expected_events, RunRewriter({ScrollRight::Typed()}));
}

TEST_P(StaticShortcutActionMouseButtonRewritingTest, KeyEventRewrite) {
  auto [static_shortcut_action, expected_events] = GetParam();
  mouse_->settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "", mojom::Button::NewVkey(ui::VKEY_A),
      mojom::RemappingAction::NewStaticShortcutAction(static_shortcut_action)));

  for (auto& event : expected_events) {
    auto* test_mouse_event = std::get_if<TestMouseEvent>(&event);
    ASSERT_TRUE(test_mouse_event);
    test_mouse_event->flags = test_mouse_event->flags | ui::EF_COMMAND_DOWN;
  }
  EXPECT_EQ(expected_events, RunRewriter({KeyA::Typed()}, ui::EF_COMMAND_DOWN));
}

}  // namespace ash
