// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/shortcut_input_handler.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/ash/right_alt_event_property.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

class TestObserver : public ShortcutInputHandler::Observer {
 public:
  void OnShortcutInputEventPressed(const mojom::KeyEvent& key_event) override {
    ++num_input_events_pressed_;
  }
  void OnShortcutInputEventReleased(const mojom::KeyEvent& key_event) override {
    ++num_input_events_released_;
  }
  void OnPrerewrittenShortcutInputEventPressed(
      const mojom::KeyEvent& key_event) override {
    ++num_prerewritten_input_events_pressed_;
    key_code_ = key_event.vkey;
  }
  void OnPrerewrittenShortcutInputEventReleased(
      const mojom::KeyEvent& key_event) override {
    ++num_prerewritten_input_events_released_;
  }

  int num_input_events_pressed() { return num_input_events_pressed_; }
  int num_input_events_released() { return num_input_events_released_; }
  int num_prerewritten_input_events_pressed() {
    return num_prerewritten_input_events_pressed_;
  }
  int num_prerewritten_input_events_released() {
    return num_prerewritten_input_events_released_;
  }
  ui::KeyboardCode key_code() { return key_code_; }

 private:
  int num_input_events_pressed_ = 0;
  int num_input_events_released_ = 0;
  int num_prerewritten_input_events_pressed_ = 0;
  int num_prerewritten_input_events_released_ = 0;
  ui::KeyboardCode key_code_ = ui::KeyboardCode::VKEY_UNKNOWN;
};

}  // namespace

class ShortcutInputHandlerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = std::make_unique<TestObserver>();
    shortcut_input_handler_ = std::make_unique<ShortcutInputHandler>();
    shortcut_input_handler_->AddObserver(observer_.get());
  }

  void TearDown() override {
    shortcut_input_handler_->RemoveObserver(observer_.get());
    observer_.reset();
    shortcut_input_handler_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestObserver> observer_;
  std::unique_ptr<ShortcutInputHandler> shortcut_input_handler_;
};

TEST_F(ShortcutInputHandlerTest, ObserverTest) {
  ui::KeyEvent pressed_event(ui::EventType::kKeyPressed, ui::VKEY_0,
                             ui::EF_NONE);
  shortcut_input_handler_->OnEvent(&pressed_event);
  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(0, observer_->num_input_events_released());

  ui::KeyEvent released_event(ui::EventType::kKeyReleased, ui::VKEY_0,
                              ui::EF_NONE);
  shortcut_input_handler_->OnEvent(&released_event);
  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());

  ui::KeyEvent prewritten_pressed_event(ui::EventType::kKeyPressed, ui::VKEY_1,
                                        ui::EF_NONE);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(prewritten_pressed_event);
  EXPECT_EQ(1, observer_->num_prerewritten_input_events_pressed());
  EXPECT_EQ(0, observer_->num_prerewritten_input_events_released());

  ui::KeyEvent prewritten_released_event(ui::EventType::kKeyReleased,
                                         ui::VKEY_1, ui::EF_NONE);
  shortcut_input_handler_->OnPrerewriteKeyInputEvent(prewritten_released_event);
  EXPECT_EQ(1, observer_->num_prerewritten_input_events_pressed());
  EXPECT_EQ(1, observer_->num_prerewritten_input_events_released());
}

TEST_F(ShortcutInputHandlerTest, ConsumeTest) {
  ui::KeyEvent pressed_event(ui::EventType::kKeyPressed, ui::VKEY_0,
                             ui::EF_NONE);
  shortcut_input_handler_->OnEvent(&pressed_event);
  EXPECT_FALSE(pressed_event.stopped_propagation());

  shortcut_input_handler_->SetShouldConsumeKeyEvents(
      /*should_consume_key_events=*/true);
  ui::KeyEvent released_event(ui::EventType::kKeyReleased, ui::VKEY_0,
                              ui::EF_NONE);
  shortcut_input_handler_->OnEvent(&released_event);
  EXPECT_TRUE(released_event.stopped_propagation());
}

TEST_F(ShortcutInputHandlerTest, ShowAllWindows) {
  ui::KeyEvent prerewritten_pressed_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN,
                   ui::DomCode::SHOW_ALL_WINDOWS, ui::EF_NONE);

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  EXPECT_EQ(observer_->key_code(), ui::VKEY_MEDIA_LAUNCH_APP1);
}

TEST_F(ShortcutInputHandlerTest, RightAlt) {
  ui::KeyEvent prerewritten_pressed_event =
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_ASSISTANT,
                   ui::DomCode::LAUNCH_ASSISTANT, ui::EF_NONE);
  ui::SetRightAltProperty(&prerewritten_pressed_event);

  shortcut_input_handler_->OnPrerewriteKeyInputEvent(
      prerewritten_pressed_event);
  EXPECT_EQ(observer_->key_code(), ui::VKEY_RIGHT_ALT);
}

}  // namespace ash
