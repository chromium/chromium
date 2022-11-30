// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/screen_backlight.h"

#include "base/check_op.h"

namespace ash {

namespace {
ScreenBacklight* g_instance = nullptr;
}

template <>
ScreenBacklight*&
ScreenBacklight::ScopedResetterForTest::GetGlobalInstanceHolder() {
  return g_instance;
}

// static
ScreenBacklight* ScreenBacklight::Get() {
  return g_instance;
}

ScreenBacklight::ScreenBacklight() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ScreenBacklight::~ScreenBacklight() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
