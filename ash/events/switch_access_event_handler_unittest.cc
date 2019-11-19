// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/switch_access_event_handler.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/switch_access_event_handler_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

// Records all key events for testing.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() = default;
  ~EventCapturer() override = default;

  void Reset() { last_key_event_.reset(); }

  ui::KeyEvent* last_key_event() { return last_key_event_.get(); }

 private:
  void OnKeyEvent(ui::KeyEvent* event) override {
    last_key_event_ = std::make_unique<ui::KeyEvent>(*event);
  }

  std::unique_ptr<ui::KeyEvent> last_key_event_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

class TestDelegate : public SwitchAccessEventHandlerDelegate {
 public:
  TestDelegate() = default;
  virtual ~TestDelegate() = default;

  SwitchAccessCommand last_command() { return commands_.back(); }
  int command_count() { return commands_.size(); }

 private:
  // SwitchAccessEventHandlerDelegate:
  void SendSwitchAccessCommand(SwitchAccessCommand command) override {
    commands_.push_back(command);
  }

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events_;
  std::vector<SwitchAccessCommand> commands_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class SwitchAccessEventHandlerTest : public AshTestBase {
 public:
  SwitchAccessEventHandlerTest() = default;
  ~SwitchAccessEventHandlerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // This test triggers a resize of WindowTreeHost, which will end up
    // throttling events. set_throttle_input_on_resize_for_testing() disables
    // this.
    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
    delegate_ = std::make_unique<TestDelegate>();

    generator_ = AshTestBase::GetEventGenerator();
    CurrentContext()->AddPreTargetHandler(&event_capturer_);

    controller_ = Shell::Get()->accessibility_controller();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalAccessibilitySwitchAccess);
    controller_->SetSwitchAccessEnabled(true);
    controller_->SetSwitchAccessEventHandlerDelegate(delegate_.get());
    controller_->SetSwitchAccessIgnoreVirtualKeyEventForTesting(false);
  }

  void TearDown() override {
    CurrentContext()->RemovePreTargetHandler(&event_capturer_);
    generator_ = nullptr;
    controller_ = nullptr;
    AshTestBase::TearDown();
  }

  void SetKeyCodesForCommand(std::set<int> key_codes,
                             SwitchAccessCommand command) {
    SwitchAccessEventHandler* handler =
        controller_->GetSwitchAccessEventHandlerForTest();
    handler->SetKeyCodesForCommand(key_codes, command);
  }

  const std::set<int> GetKeyCodesToCapture() {
    SwitchAccessEventHandler* handler =
        controller_->GetSwitchAccessEventHandlerForTest();
    if (handler)
      return handler->key_codes_to_capture_for_test();
    return std::set<int>();
  }

  const std::map<int, SwitchAccessCommand> GetCommandForKeyCodeMap() {
    SwitchAccessEventHandler* handler =
        controller_->GetSwitchAccessEventHandlerForTest();
    if (handler)
      return handler->command_for_key_code_map_for_test();
    return std::map<int, SwitchAccessCommand>();
  }

 protected:
  ui::test::EventGenerator* generator_ = nullptr;
  EventCapturer event_capturer_;
  AccessibilityControllerImpl* controller_ = nullptr;
  std::unique_ptr<TestDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SwitchAccessEventHandlerTest);
};

TEST_F(SwitchAccessEventHandlerTest, CaptureSpecifiedKeys) {
  // Set keys for Switch Access to capture.
  SetKeyCodesForCommand({ui::VKEY_1, ui::VKEY_2}, SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer_.last_key_event());

  generator_->PressKey(ui::VKEY_1, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());

  // Press the "2" key.
  generator_->PressKey(ui::VKEY_2, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_2, ui::EF_NONE);

  // We received a new event.

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press the "3" key.
  generator_->PressKey(ui::VKEY_3, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_3, ui::EF_NONE);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_TRUE(event_capturer_.last_key_event());
}

TEST_F(SwitchAccessEventHandlerTest, KeysNoLongerCaptureAfterUpdate) {
  // Set Switch Access to capture the keys {1, 2, 3}.
  SetKeyCodesForCommand({ui::VKEY_1, ui::VKEY_2, ui::VKEY_3},
                        SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE);

  // The event was handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());
  EXPECT_EQ(SwitchAccessCommand::kSelect, delegate_->last_command());

  // Update the Switch Access keys to capture {2, 3, 4}.
  SetKeyCodesForCommand({ui::VKEY_2, ui::VKEY_3, ui::VKEY_4},
                        SwitchAccessCommand::kSelect);

  // Press the "1" key.
  generator_->PressKey(ui::VKEY_1, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_1, ui::EF_NONE);

  // We received a new event.

  // The event was NOT handled by SwitchAccessEventHandler.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  // Press the "4" key.
  generator_->PressKey(ui::VKEY_4, ui::EF_NONE);
  generator_->ReleaseKey(ui::VKEY_4, ui::EF_NONE);

  // The event was handled by SwitchAccessEventHandler.
}

TEST_F(SwitchAccessEventHandlerTest, ForwardKeyEvents) {
  SetKeyCodesForCommand({ui::VKEY_1, ui::VKEY_2, ui::VKEY_3},
                        SwitchAccessCommand::kSelect);

  EXPECT_FALSE(event_capturer_.last_key_event());

  // Tell the Switch Access Event Handler to forward key events.
  Shell::Get()->accessibility_controller()->ForwardKeyEventsToSwitchAccess(
      true);

  // Press the "T" key.
  generator_->PressKey(ui::VKEY_T, ui::EF_NONE);

  // The event should be handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());

  // Release the "T" key.
  generator_->ReleaseKey(ui::VKEY_T, ui::EF_NONE);

  // The event should be handled by SwitchAccessEventHandler.
  EXPECT_FALSE(event_capturer_.last_key_event());

  // Tell the Switch Access Event Handler to stop forwarding key events.
  Shell::Get()->accessibility_controller()->ForwardKeyEventsToSwitchAccess(
      false);

  // Press the "T" key.
  generator_->PressKey(ui::VKEY_T, ui::EF_NONE);

  // The event is not handled by SwitchAccessEventHandler.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());

  // Release the "T" key.
  generator_->ReleaseKey(ui::VKEY_T, ui::EF_NONE);

  // The event is not handled by SwitchAccessEventHandler.
  EXPECT_TRUE(event_capturer_.last_key_event());
  EXPECT_FALSE(event_capturer_.last_key_event()->handled());
}

TEST_F(SwitchAccessEventHandlerTest, SetKeyCodesForCommand) {
  SwitchAccessEventHandler* handler =
      controller_->GetSwitchAccessEventHandlerForTest();
  EXPECT_NE(nullptr, handler);

  // Both the key codes to capture and the command map should be empty.
  EXPECT_EQ(0ul /* unsigned long */, GetKeyCodesToCapture().size());
  EXPECT_EQ(0ul /* unsigned long */, GetCommandForKeyCodeMap().size());

  // Set key codes for Select command.
  std::set<int> new_key_codes;
  new_key_codes.insert(48 /* '0' */);
  new_key_codes.insert(83 /* 's' */);
  handler->SetKeyCodesForCommand(new_key_codes, SwitchAccessCommand::kSelect);

  // Check that values are added to both data structures.
  std::set<int> kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(2ul /* unsigned long */, kc_to_capture.size());
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(48));
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(83));

  std::map<int, SwitchAccessCommand> command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(2ul /* unsigned long */, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(48));
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(83));

  // Set key codes for the Next command.
  new_key_codes.clear();
  new_key_codes.insert(49 /* '1' */);
  new_key_codes.insert(78 /* 'n' */);
  handler->SetKeyCodesForCommand(new_key_codes, SwitchAccessCommand::kNext);

  // Check that the new values are added and old values are not changed.
  kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(4ul /* unsigned long */, kc_to_capture.size());
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(49));
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(78));

  command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(4ul /* unsigned long */, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kNext, command_map.at(49));
  EXPECT_EQ(SwitchAccessCommand::kNext, command_map.at(78));

  // Set key codes for the Previous command. Re-use a key code from above.
  new_key_codes.clear();
  new_key_codes.insert(49 /* '1' */);
  new_key_codes.insert(80 /* 'p' */);
  handler->SetKeyCodesForCommand(new_key_codes, SwitchAccessCommand::kPrevious);

  // Check that '1' has been remapped to Previous.
  kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(5ul /* unsigned long */, kc_to_capture.size());
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(49));
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(80));

  command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(5ul /* unsigned long */, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kPrevious, command_map.at(49));
  EXPECT_EQ(SwitchAccessCommand::kPrevious, command_map.at(80));
  EXPECT_EQ(SwitchAccessCommand::kNext, command_map.at(78));

  // Set a new key code for the Select command.
  new_key_codes.clear();
  new_key_codes.insert(51 /* '3' */);
  new_key_codes.insert(83 /* 's' */);
  handler->SetKeyCodesForCommand(new_key_codes, SwitchAccessCommand::kSelect);

  // Check that the previously set values for Select have been cleared.
  kc_to_capture = GetKeyCodesToCapture();
  EXPECT_EQ(5ul /* unsigned long */, kc_to_capture.size());
  EXPECT_EQ(0ul /* unsigned long */, kc_to_capture.count(48));
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(51));
  EXPECT_EQ(1ul /* unsigned long */, kc_to_capture.count(83));

  command_map = GetCommandForKeyCodeMap();
  EXPECT_EQ(5ul /* unsigned long */, command_map.size());
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(51));
  EXPECT_EQ(SwitchAccessCommand::kSelect, command_map.at(83));
  EXPECT_EQ(command_map.end(), command_map.find(48));
}

}  // namespace
}  // namespace ash
