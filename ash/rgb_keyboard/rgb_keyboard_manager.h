// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_

#include <stdint.h>
#include <vector>

#include "ash/ash_export.h"
#include "ash/ime/ime_controller_impl.h"

namespace ash {

// TODO(jimmyxgong): Determine if this enum can be rolled from Dbus constants.
enum class RgbKeyboardCapabilities {
  kNone,
  kFiveZone,
  kIndividualKey,
};

// RgbKeyboardManager is singleton class that provides clients access to
// RGB keyboard-related API's. Clients should interact with this class instead
// of the rgbkbd DBus client.
// This class is owned by ash/shell and should NOT be created by any other
// means.
class ASH_EXPORT RgbKeyboardManager : public ImeControllerImpl::Observer {
 public:
  explicit RgbKeyboardManager(ImeControllerImpl* ime_controller);
  RgbKeyboardManager(const RgbKeyboardManager&) = delete;
  RgbKeyboardManager& operator=(const RgbKeyboardManager&) = delete;
  virtual ~RgbKeyboardManager();

  RgbKeyboardCapabilities GetRgbKeyboardCapabilities() const;
  void SetStaticBackgroundColor(uint8_t r, uint8_t g, uint8_t b);
  void SetRainbowMode();
  void SetCapsLockState(bool is_caps_lock_set);

  // Returns the global instance if initialized. May return null.
  static RgbKeyboardManager* Get();

  std::vector<uint8_t> recently_sent_rgb() const {
    return recently_sent_rgb_for_testing_;
  }

  bool is_rainbow_mode_set() const { return is_rainbow_mode_set_for_testing_; }

  bool is_caps_lock_set() const { return is_caps_lock_set_; }

 private:
  // ImeControllerImpl::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

  // TODO(jimmyxgong): Remove the following members after DBus client is
  // available.
  std::vector<uint8_t> recently_sent_rgb_for_testing_;
  bool is_rainbow_mode_set_for_testing_ = false;
  bool is_caps_lock_set_ = false;

  ImeControllerImpl* ime_controller_raw_ptr_;
};

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_