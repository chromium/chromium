// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_

#include <set>

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"

namespace ash {

// KeyboardControllerObserver implementation for tests. This class
// implements a test client observer for tests running with the Window Service.

class TestKeyboardControllerObserver : public KeyboardControllerObserver {
 public:
  explicit TestKeyboardControllerObserver(KeyboardController* controller);
  ~TestKeyboardControllerObserver() override;

  // KeyboardControllerObserver:
  void OnKeyboardEnableFlagsChanged(
      const std::set<keyboard::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool enabled) override;
  void OnKeyboardConfigChanged(const keyboard::KeyboardConfig& config) override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardUIDestroyed() override;

  const keyboard::KeyboardConfig& config() const { return config_; }
  void set_config(const keyboard::KeyboardConfig& config) { config_ = config; }
  const std::set<keyboard::KeyboardEnableFlag>& enable_flags() const {
    return enable_flags_;
  }
  int destroyed_count() const { return destroyed_count_; }

 private:
  KeyboardController* controller_;
  std::set<keyboard::KeyboardEnableFlag> enable_flags_;
  keyboard::KeyboardConfig config_;
  int destroyed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardControllerObserver);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_
