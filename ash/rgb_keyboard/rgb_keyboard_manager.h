// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_
#define ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/ime/ime_controller_impl.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

class RgbKeyboardManagerObserver;

// RgbKeyboardManager is singleton class that provides clients access to
// RGB keyboard-related API's. Clients should interact with this class instead
// of the rgbkbd DBus client.
// This class is owned by ash/shell and should NOT be created by any other
// means.
class ASH_EXPORT RgbKeyboardManager : public ImeController::Observer,
                                      public RgbkbdClient::Observer {
 public:
  explicit RgbKeyboardManager(ImeControllerImpl* ime_controller);
  RgbKeyboardManager(const RgbKeyboardManager&) = delete;
  RgbKeyboardManager& operator=(const RgbKeyboardManager&) = delete;
  ~RgbKeyboardManager() override;

  rgbkbd::RgbKeyboardCapabilities GetRgbKeyboardCapabilities() const;
  int GetZoneCount();
  void SetStaticBackgroundColor(uint8_t r, uint8_t g, uint8_t b);
  void SetZoneColor(int zone, uint8_t r, uint8_t g, uint8_t b);
  void SetRainbowMode();
  void SetAnimationMode(rgbkbd::RgbAnimationMode mode);

  // RgbkbdClient::Observer:
  // Also used in tests to override the keyboard capability.
  void OnCapabilityUpdatedForTesting(
      rgbkbd::RgbKeyboardCapabilities capability) override;

  // Returns the global instance if initialized. May return null.
  static RgbKeyboardManager* Get();

  bool IsRgbKeyboardSupported() const {
    return capabilities_ != rgbkbd::RgbKeyboardCapabilities::kNone;
  }

  // Add and remove observers.
  void AddObserver(RgbKeyboardManagerObserver* observer);
  void RemoveObserver(RgbKeyboardManagerObserver* observer);

 private:
  friend class KeyboardBacklightColorControllerTest;

  // Enum to track the background mode sent to rgbkbd
  enum class BackgroundType {
    kNone,
    kStaticSingleColor,
    kStaticRainbow,
    kStaticZones,
  };

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

  void FetchRgbKeyboardSupport();

  void OnGetRgbKeyboardCapabilities(
      std::optional<rgbkbd::RgbKeyboardCapabilities> reply);

  void InitializeRgbKeyboard();

  bool IsPerKeyKeyboard() const;

  rgbkbd::RgbKeyboardCapabilities capabilities_ =
      rgbkbd::RgbKeyboardCapabilities::kNone;

  raw_ptr<ImeControllerImpl> ime_controller_ptr_;

  // Tracks the currently set background color when `background_type_` is set to
  // `BackgroundType::kStaticSingleColor`.
  SkColor background_color_;
  // Tracks the currently set zone colors when `background_type_` is set to
  // `BackgroundType::kStaticZones`.
  base::flat_map<int, SkColor> zone_colors_;
  BackgroundType background_type_ = BackgroundType::kNone;

  base::ObserverList<RgbKeyboardManagerObserver> observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RgbKeyboardManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_RGB_KEYBOARD_RGB_KEYBOARD_MANAGER_H_
