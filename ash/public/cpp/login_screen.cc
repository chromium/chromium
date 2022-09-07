// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen.h"

#include "base/check_op.h"

namespace ash {

namespace {
LoginScreen* g_instance = nullptr;
}

// static
LoginScreen* LoginScreen::Get() {
  return g_instance;
}

LoginScreen::LoginScreen() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

LoginScreen::~LoginScreen() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
