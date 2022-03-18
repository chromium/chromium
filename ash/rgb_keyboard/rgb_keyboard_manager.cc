// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rgb_keyboard/rgb_keyboard_manager.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

RgbKeyboardManager* g_instance = nullptr;

}  // namespace

RgbKeyboardManager::RgbKeyboardManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

RgbKeyboardManager::~RgbKeyboardManager() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// TODO(jimmyxgong): This is a stub implementation, replace with real impl.
RgbKeyboardCapabilities RgbKeyboardManager::GetRgbKeyboardCapabilities() const {
  return RgbKeyboardCapabilities::kNone;
}

// static
RgbKeyboardManager* RgbKeyboardManager::Get() {
  return g_instance;
}

}  // namespace ash
