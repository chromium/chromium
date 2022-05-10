// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/ime/ime_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace ash {

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

  rgbkbd::RgbKeyboardCapabilities GetRgbKeyboardCapabilities() const;
  void SetStaticBackgroundColor(uint8_t r, uint8_t g, uint8_t b);
  void SetRainbowMode();
  void SetAnimationMode(rgbkbd::RgbAnimationMode mode);

  // Returns the global instance if initialized. May return null.
  static RgbKeyboardManager* Get();

  bool IsRgbKeyboardSupported() const {
    return capabilities_ != rgbkbd::RgbKeyboardCapabilities::kNone;
  }

 private:
  // ImeControllerImpl::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

  void FetchRgbKeyboardSupport();

  void OnGetRgbKeyboardCapabilities(
      absl::optional<rgbkbd::RgbKeyboardCapabilities> reply);

  rgbkbd::RgbKeyboardCapabilities capabilities_ =
      rgbkbd::RgbKeyboardCapabilities::kNone;

  raw_ptr<ImeControllerImpl> ime_controller_ptr_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RgbKeyboardManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_