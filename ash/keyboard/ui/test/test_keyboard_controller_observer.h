// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_
#define ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_

#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"

namespace keyboard {

// A KeyboardControllerObserver that counts occurrences of events for testing.
struct TestKeyboardControllerObserver : public ash::KeyboardControllerObserver {
  TestKeyboardControllerObserver();

  TestKeyboardControllerObserver(const TestKeyboardControllerObserver&) =
      delete;
  TestKeyboardControllerObserver& operator=(
      const TestKeyboardControllerObserver&) = delete;

  ~TestKeyboardControllerObserver() override;

  // KeyboardControllerObserver:
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  int enabled_count = 0;
  int disabled_count = 0;
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_
