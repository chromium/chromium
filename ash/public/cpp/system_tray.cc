// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system_tray.h"

#include "base/check_op.h"

namespace ash {

namespace {

SystemTray* g_instance = nullptr;

}  // namespace

// static
SystemTray* SystemTray::Get() {
  return g_instance;
}

SystemTray::SystemTray() {
  DCHECK(!g_instance);
  g_instance = this;
}

SystemTray::~SystemTray() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
