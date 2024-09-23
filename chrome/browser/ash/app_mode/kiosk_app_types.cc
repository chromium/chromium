// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "components/account_id/account_id.h"

namespace ash {

namespace {

std::string ToString(KioskAppType type) {
  switch (type) {
    case KioskAppType::kChromeApp:
      return "ChromeAppKiosk";
    case KioskAppType::kWebApp:
      return "WebKiosk";
    case KioskAppType::kIsolatedWebApp:
      return "IsolatedWebAppKiosk";
  }
}

}  // namespace

// static
KioskAppId KioskAppId::ForChromeApp(std::string_view chrome_app_id,
                                    const AccountId& account_id) {
  // TODO(b/304937903) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(account_id.is_valid());
  return KioskAppId(chrome_app_id, account_id);
}

// static
KioskAppId KioskAppId::ForWebApp(const AccountId& account_id) {
  // TODO(b/304937903) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(account_id.is_valid());
  return KioskAppId(KioskAppType::kWebApp, account_id);
}

// static
KioskAppId KioskAppId::ForIsolatedWebApp(const AccountId& account_id) {
  DUMP_WILL_BE_CHECK(account_id.is_valid());
  return {KioskAppType::kIsolatedWebApp, account_id};
}

KioskAppId::KioskAppId() = default;
KioskAppId::KioskAppId(std::string_view chrome_app_id,
                       const AccountId& account_id)
    : type(KioskAppType::kChromeApp),
      app_id(chrome_app_id),
      account_id(account_id) {
  // TODO(b/304937903): Update the caller code to never call us with invalid ChromeApp IDs.
  // See b/339172292 for a scenario when that currently happens.
}
KioskAppId::KioskAppId(KioskAppType type, const AccountId& account_id)
    : type(type), account_id(account_id) {}
KioskAppId::KioskAppId(const KioskAppId&) = default;
KioskAppId::KioskAppId(KioskAppId&&) = default;
KioskAppId& KioskAppId::operator=(const KioskAppId&) = default;
KioskAppId& KioskAppId::operator=(KioskAppId&&) = default;
KioskAppId::~KioskAppId() = default;

std::ostream& operator<<(std::ostream& stream, const KioskAppId& id) {
  stream << "{type: " << ToString(id.type);
  stream << ", account_id: " << id.account_id;
  if (id.app_id.has_value()) {
    stream << ", app_id: " << id.app_id.value();
  }
  return stream << "}";
}

bool operator==(const KioskAppId& first, const KioskAppId& second) {
  return (first.type == second.type) && (first.app_id == second.app_id) &&
         (first.account_id == second.account_id);
}

}  // namespace ash
