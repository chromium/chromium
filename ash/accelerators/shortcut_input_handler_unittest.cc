// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/shortcut_input_handler.h"

#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
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

  int num_input_events_pressed() { return num_input_events_pressed_; }
  int num_input_events_released() { return num_input_events_released_; }

 private:
  int num_input_events_pressed_ = 0;
  int num_input_events_released_ = 0;
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
  ui::KeyEvent pressed_event(ui::ET_KEY_PRESSED, ui::VKEY_0, ui::EF_NONE);
  shortcut_input_handler_->OnEvent(&pressed_event);
  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(0, observer_->num_input_events_released());

  ui::KeyEvent released_event(ui::ET_KEY_RELEASED, ui::VKEY_0, ui::EF_NONE);
  shortcut_input_handler_->OnEvent(&released_event);
  EXPECT_EQ(1, observer_->num_input_events_pressed());
  EXPECT_EQ(1, observer_->num_input_events_released());
}

}  // namespace ash
