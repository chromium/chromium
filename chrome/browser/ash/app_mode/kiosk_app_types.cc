// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"

namespace ash {

namespace {

std::string KioskAppTypeToString(KioskAppType type) {
  switch (type) {
    case KioskAppType::kArcApp:
      return "ArcKiosk";
    case KioskAppType::kChromeApp:
      return "ChromeAppKiosk";
    case KioskAppType::kWebApp:
      return "WebKiosk";
  }
}

}  // namespace

KioskAppId::KioskAppId() = default;
KioskAppId::~KioskAppId() = default;
KioskAppId::KioskAppId(const KioskAppId&) = default;

KioskAppId::KioskAppId(KioskAppType type, const std::string& app_id)
    : type(type), app_id(app_id) {}
KioskAppId::KioskAppId(KioskAppType type, const AccountId& account_id)
    : type(type), account_id(account_id) {}

// static
KioskAppId KioskAppId::ForChromeApp(const std::string& app_id) {
  return KioskAppId(KioskAppType::kChromeApp, app_id);
}

// static
KioskAppId KioskAppId::ForArcApp(const AccountId& account_id) {
  return KioskAppId(KioskAppType::kArcApp, account_id);
}

// static
KioskAppId KioskAppId::ForWebApp(const AccountId& account_id) {
  return KioskAppId(KioskAppType::kWebApp, account_id);
}

std::ostream& operator<<(std::ostream& stream, const KioskAppId& app_id) {
  stream << "{type: " << KioskAppTypeToString(app_id.type) << ", ";

  if (app_id.account_id) {
    stream << "account_id: " << app_id.account_id.value();
  } else {
    stream << "app_id: " << app_id.app_id.value();
  }

  stream << "}";
  return stream;
}

}  // namespace ash
