// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <vector>

#include "ash/ime/ime_controller_impl.h"
#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

RgbKeyboardManager* g_instance = nullptr;

const int kRGBLength = 3;

}  // namespace

RgbKeyboardManager::RgbKeyboardManager(ImeControllerImpl* ime_controller)
    : recently_sent_rgb_for_testing_(kRGBLength),
      ime_controller_raw_ptr_(ime_controller) {
  DCHECK(ime_controller_raw_ptr_);
  DCHECK(!g_instance);
  g_instance = this;

  ime_controller_raw_ptr_->AddObserver(this);
  // Upon login, CapsLock may already be enabled.
  SetCapsLockState(ime_controller_raw_ptr_->IsCapsLockEnabled());
}

RgbKeyboardManager::~RgbKeyboardManager() {
  ime_controller_raw_ptr_->RemoveObserver(this);

  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// TODO(jimmyxgong): This is a stub implementation, replace with real impl.
RgbKeyboardCapabilities RgbKeyboardManager::GetRgbKeyboardCapabilities() const {
  return RgbKeyboardCapabilities::kNone;
}

// TODO(jimmyxgong): This is a stub implementation, replace with real impl.
void RgbKeyboardManager::SetStaticBackgroundColor(uint8_t r,
                                                  uint8_t g,
                                                  uint8_t b) {
  // Reset the rainbow mode state.
  is_rainbow_mode_set_for_testing_ = false;

  recently_sent_rgb_for_testing_[0] = r;
  recently_sent_rgb_for_testing_[1] = g;
  recently_sent_rgb_for_testing_[2] = b;
}

// TODO(jimmyxgong): This is a stub implementation, replace with real impl.
void RgbKeyboardManager::SetRainbowMode() {
  is_rainbow_mode_set_for_testing_ = true;

  // Reset the stored static rgb values;
  recently_sent_rgb_for_testing_[0] = 0u;
  recently_sent_rgb_for_testing_[1] = 0u;
  recently_sent_rgb_for_testing_[2] = 0u;
}

void RgbKeyboardManager::SetCapsLockState(bool is_caps_lock_set) {
  is_caps_lock_set_ = is_caps_lock_set;
}

void RgbKeyboardManager::OnCapsLockChanged(bool enabled) {
  SetCapsLockState(enabled);
}

// static
RgbKeyboardManager* RgbKeyboardManager::Get() {
  return g_instance;
}

}  // namespace ash
