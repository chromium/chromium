// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/crx_file/id_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

void CheckChromeAppIdIsValid(std::string_view id) {
  // TODO(b/304937903) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(crx_file::id_util::IdIsValid(id))
      << "Invalid Chrome App ID: " << id;
}

std::string ToString(KioskAppType type) {
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

// static
KioskAppId KioskAppId::ForChromeApp(std::string_view chrome_app_id,
                                    const AccountId& account_id) {
  // TODO(b/304937903) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(account_id.is_valid());
  return KioskAppId(chrome_app_id, account_id);
}

// static
KioskAppId KioskAppId::ForArcApp(const AccountId& account_id) {
  // TODO(b/304937903) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(account_id.is_valid());
  return KioskAppId(KioskAppType::kArcApp, account_id);
}

// static
KioskAppId KioskAppId::ForWebApp(const AccountId& account_id) {
  // TODO(b/304937903) upgrade to CHECK.
  DUMP_WILL_BE_CHECK(account_id.is_valid());
  return KioskAppId(KioskAppType::kWebApp, account_id);
}

KioskAppId::KioskAppId() = default;
KioskAppId::KioskAppId(std::string_view chrome_app_id,
                       const AccountId& account_id)
    : type(KioskAppType::kChromeApp),
      app_id(chrome_app_id),
      account_id(account_id) {
  CheckChromeAppIdIsValid(chrome_app_id);
}
KioskAppId::KioskAppId(KioskAppType type, const AccountId& account_id)
    : type(type), account_id(account_id) {}
KioskAppId::KioskAppId(const KioskAppId&) = default;
KioskAppId::~KioskAppId() = default;

std::ostream& operator<<(std::ostream& stream, const KioskAppId& id) {
  stream << "{type: " << ToString(id.type);
  stream << ", account_id: " << id.account_id;
  if (id.app_id.has_value()) {
    stream << ", app_id: " << id.app_id.value();
  }
  return stream << "}";
}

}  // namespace ash
