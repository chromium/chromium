// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/kiosk_app_menu.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

KioskAppMenu* g_instance = nullptr;

}  // namespace

KioskAppMenuEntry::KioskAppMenuEntry(
    AppType type,
    const AccountId& account_id,
    const std::optional<std::string>& chrome_app_id,
    std::u16string name,
    gfx::ImageSkia icon)
    : type(type),
      account_id(account_id),
      chrome_app_id(chrome_app_id),
      name(std::move(name)),
      icon(icon) {
  bool should_have_chrome_app_id = type == AppType::kChromeApp;
  CHECK_EQ(should_have_chrome_app_id, chrome_app_id.has_value());
}
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
