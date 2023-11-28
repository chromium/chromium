// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/rgb_keyboard/histogram_util.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager_observer.h"
#include "ash/rgb_keyboard/rgb_keyboard_util.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"

namespace ash {

namespace {

RgbKeyboardManager* g_instance = nullptr;

// The max number of zones possible across all RGB enabled devices.
const int kMaxNumberOfZones = 5;

}  // namespace

RgbKeyboardManager::RgbKeyboardManager(ImeControllerImpl* ime_controller)
    : ime_controller_ptr_(ime_controller) {
  DCHECK(ime_controller_ptr_);
  DCHECK(!g_instance);
  g_instance = this;

  RgbkbdClient::Get()->AddObserver(this);

  VLOG(1) << "Initializing RGB Keyboard support";
  FetchRgbKeyboardSupport();
}

RgbKeyboardManager::~RgbKeyboardManager() {
  RgbkbdClient::Get()->RemoveObserver(this);
  if (IsPerKeyKeyboard()) {
    ime_controller_ptr_->RemoveObserver(this);
  }

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void RgbKeyboardManager::FetchRgbKeyboardSupport() {
  DCHECK(RgbkbdClient::Get());
  RgbkbdClient::Get()->GetRgbKeyboardCapabilities(
      base::BindOnce(&RgbKeyboardManager::OnGetRgbKeyboardCapabilities,
                     weak_ptr_factory_.GetWeakPtr()));
}

rgbkbd::RgbKeyboardCapabilities RgbKeyboardManager::GetRgbKeyboardCapabilities()
    const {
  return capabilities_;
}

int RgbKeyboardManager::GetZoneCount() {
  switch (capabilities_) {
    case rgbkbd::RgbKeyboardCapabilities::kIndividualKey:
      return 5;
    case rgbkbd::RgbKeyboardCapabilities::kFourZoneFortyLed:
    case rgbkbd::RgbKeyboardCapabilities::kFourZoneTwelveLed:
    case rgbkbd::RgbKeyboardCapabilities::kFourZoneFourLed:
      return 4;
    case rgbkbd::RgbKeyboardCapabilities::kNone:
      LOG(ERROR) << "Attempted to get zone count for a non-RGB keyboard.";
      return 0;
  }
}

void RgbKeyboardManager::SetStaticBackgroundColor(uint8_t r,
                                                  uint8_t g,
                                                  uint8_t b) {
  DCHECK(RgbkbdClient::Get());
  background_type_ = BackgroundType::kStaticSingleColor;
  background_color_ = SkColorSetRGB(r, g, b);
  if (!IsRgbKeyboardSupported()) {
    LOG(ERROR) << "Attempted to set RGB keyboard color, but flag is disabled.";
    return;
  }

  VLOG(1) << "Setting RGB keyboard color to R:" << static_cast<int>(r)
          << " G:" << static_cast<int>(g) << " B:" << static_cast<int>(b);
  ash::rgb_keyboard::metrics::EmitRgbBacklightChangeType(
      ash::rgb_keyboard::metrics::RgbKeyboardBacklightChangeType::
          kStaticBackgroundColorChanged,
      capabilities_);
  RgbkbdClient::Get()->SetStaticBackgroundColor(r, g, b);
}

void RgbKeyboardManager::SetZoneColor(int zone,
                                      uint8_t r,
                                      uint8_t g,
                                      uint8_t b) {
  DCHECK(RgbkbdClient::Get());
  // Make sure the given zone is within the valid possible range
  // of values for zones. The zone colors are stored even if the actual zone
  // count is not known yet to solve a race condition where colors are set
  // before rgbkbd is initialized.
  if (zone < 0 || zone >= kMaxNumberOfZones) {
    LOG(ERROR) << "Zone #" << zone
               << " is outside the range for valid possible values [0,"
               << kMaxNumberOfZones << ").";
    return;
  }

  background_type_ = BackgroundType::kStaticZones;
  zone_colors_[zone] = SkColorSetRGB(r, g, b);

  if (zone < 0 || zone >= GetZoneCount()) {
    LOG(ERROR) << "Attempted to set an invalid zone: " << zone;
    return;
  }
  if (!IsRgbKeyboardSupported()) {
    LOG(ERROR)
        << "Attempted to set RGB keyboard zone color, but flag is disabled.";
    return;
  }

  VLOG(1) << "Setting RGB keyboard zone " << zone
          << " color to R:" << static_cast<int>(r)
          << " G:" << static_cast<int>(g) << " B:" << static_cast<int>(b);
  ash::rgb_keyboard::metrics::EmitRgbBacklightChangeType(
      ash::rgb_keyboard::metrics::RgbKeyboardBacklightChangeType::
          kStaticZoneColorChanged,
      capabilities_);
  RgbkbdClient::Get()->SetZoneColor(zone, r, g, b);
}

void RgbKeyboardManager::SetRainbowMode() {
  DCHECK(RgbkbdClient::Get());
  background_type_ = BackgroundType::kStaticRainbow;
  if (!IsRgbKeyboardSupported()) {
    LOG(ERROR) << "Attempted to set RGB rainbow mode, but flag is disabled.";
    return;
  }

  VLOG(1) << "Setting RGB keyboard to rainbow mode";
  ash::rgb_keyboard::metrics::EmitRgbBacklightChangeType(
      ash::rgb_keyboard::metrics::RgbKeyboardBacklightChangeType::
          kRainbowModeSelected,
      capabilities_);
  RgbkbdClient::Get()->SetRainbowMode();
}

void RgbKeyboardManager::SetAnimationMode(rgbkbd::RgbAnimationMode mode) {
  if (!features::IsExperimentalRgbKeyboardPatternsEnabled()) {
    LOG(ERROR) << "Attempted to set RGB animation mode, but flag is disabled.";
    return;
  }

  DCHECK(RgbkbdClient::Get());
  VLOG(1) << "Setting RGB keyboard animation mode to "
          << static_cast<uint32_t>(mode);
  RgbkbdClient::Get()->SetAnimationMode(mode);
}

void RgbKeyboardManager::OnCapsLockChanged(bool enabled) {
  VLOG(1) << "Setting RGB keyboard caps lock state to " << enabled;
  RgbkbdClient::Get()->SetCapsLockState(enabled);
}

// static
RgbKeyboardManager* RgbKeyboardManager::Get() {
  return g_instance;
}

void RgbKeyboardManager::AddObserver(RgbKeyboardManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void RgbKeyboardManager::RemoveObserver(RgbKeyboardManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void RgbKeyboardManager::OnCapabilityUpdatedForTesting(
    rgbkbd::RgbKeyboardCapabilities capability) {
  capabilities_ = capability;
}

void RgbKeyboardManager::OnGetRgbKeyboardCapabilities(
    std::optional<rgbkbd::RgbKeyboardCapabilities> reply) {
  if (!reply.has_value()) {
    if (base::SysInfo::IsRunningOnChromeOS()) {
      LOG(ERROR) << "No response received for GetRgbKeyboardCapabilities";
    }
    return;
  }

  capabilities_ = reply.value();
  ash::rgb_keyboard::metrics::EmitRgbKeyboardCapabilityType(capabilities_);
  VLOG(1) << "RGB Keyboard capabilities="
          << static_cast<uint32_t>(capabilities_);

  if (IsRgbKeyboardSupported()) {
    InitializeRgbKeyboard();
  }

  for (auto& observer : observers_) {
    observer.OnRgbKeyboardSupportedChanged(IsRgbKeyboardSupported());
  }
}

void RgbKeyboardManager::InitializeRgbKeyboard() {
  DCHECK(RgbkbdClient::Get());

  // Initialize the keyboard based on the correct current state.
  // `background_type_` will usually be set to kNone. In some cases, a color
  // may have been set by `KeyboardBacklightColorController` before we are fully
  // initialized, so here we will correctly set the color once ready.
  switch (background_type_) {
    case BackgroundType::kStaticSingleColor:
      SetStaticBackgroundColor(SkColorGetR(background_color_),
                               SkColorGetG(background_color_),
                               SkColorGetB(background_color_));
      break;
    case BackgroundType::kStaticZones:
      for (auto const& [zone, color] : zone_colors_) {
        SetZoneColor(zone, SkColorGetR(color), SkColorGetG(color),
                     SkColorGetB(color));
      }
      break;
    case BackgroundType::kStaticRainbow:
      SetRainbowMode();
      break;
    case BackgroundType::kNone:
      break;
  }

  // Initialize caps lock color changing if supported
  if (IsPerKeyKeyboard()) {
    VLOG(1) << "Setting initial RGB keyboard caps lock state to "
            << ime_controller_ptr_->IsCapsLockEnabled();
    RgbkbdClient::Get()->SetCapsLockState(
        ime_controller_ptr_->IsCapsLockEnabled());

    ime_controller_ptr_->AddObserver(this);
  }
}

bool RgbKeyboardManager::IsPerKeyKeyboard() const {
  return capabilities_ == rgbkbd::RgbKeyboardCapabilities::kIndividualKey;
}
}  // namespace ash
