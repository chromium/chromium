// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include <stdint.h>
#include <vector>

#include "ash/ime/ime_controller_impl.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"

namespace ash {

namespace {

RgbKeyboardManager* g_instance = nullptr;

}  // namespace

RgbKeyboardManager::RgbKeyboardManager(ImeControllerImpl* ime_controller)
    : ime_controller_ptr_(ime_controller) {
  DCHECK(ime_controller_ptr_);
  DCHECK(!g_instance);
  g_instance = this;

  ime_controller_ptr_->AddObserver(this);

  FetchRgbKeyboardSupport();
}

RgbKeyboardManager::~RgbKeyboardManager() {
  ime_controller_ptr_->RemoveObserver(this);

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

void RgbKeyboardManager::SetStaticBackgroundColor(uint8_t r,
                                                  uint8_t g,
                                                  uint8_t b) {
  DCHECK(RgbkbdClient::Get());
  // TODO(michaelcheco): Check RGB capabilities before proceeding.
  RgbkbdClient::Get()->SetStaticBackgroundColor(r, g, b);
}

void RgbKeyboardManager::SetRainbowMode() {
  DCHECK(RgbkbdClient::Get());
  // TODO(michaelcheco): Check RGB capabilities before proceeding.
  RgbkbdClient::Get()->SetRainbowMode();
}

void RgbKeyboardManager::OnCapsLockChanged(bool enabled) {
  if (IsRgbKeyboardSupported()) {
    RgbkbdClient::Get()->SetCapsLockState(enabled);
  }
}

// static
RgbKeyboardManager* RgbKeyboardManager::Get() {
  return g_instance;
}

void RgbKeyboardManager::OnGetRgbKeyboardCapabilities(
    absl::optional<rgbkbd::RgbKeyboardCapabilities> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "rgbkbd: No response received for GetRgbKeyboardCapabilities";
    return;
  }
  capabilities_ = reply.value();

  // Upon login, CapsLock may already be enabled.
  if (IsRgbKeyboardSupported()) {
    RgbkbdClient::Get()->SetCapsLockState(
        ime_controller_ptr_->IsCapsLockEnabled());
  }
}

}  // namespace ash
