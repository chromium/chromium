// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/kiosk_app_menu.h"

namespace ash {

namespace {
KioskAppMenu* g_instance = nullptr;
}

KioskAppMenuEntry::KioskAppMenuEntry() = default;
KioskAppMenuEntry::KioskAppMenuEntry(const KioskAppMenuEntry& other) = default;
KioskAppMenuEntry::KioskAppMenuEntry(KioskAppMenuEntry&& other) = default;
KioskAppMenuEntry::~KioskAppMenuEntry() = default;

KioskAppMenuEntry& KioskAppMenuEntry::operator=(KioskAppMenuEntry&& other) =
    default;
KioskAppMenuEntry& KioskAppMenuEntry::operator=(
    const KioskAppMenuEntry& other) = default;

// static
KioskAppMenu* KioskAppMenu::Get() {
  return g_instance;
}

KioskAppMenu::KioskAppMenu() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

KioskAppMenu::~KioskAppMenu() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
