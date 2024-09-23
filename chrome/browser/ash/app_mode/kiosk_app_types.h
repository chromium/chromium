// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "components/account_id/account_id.h"

namespace ash {

// Supported types of Kiosk apps.
enum class KioskAppType { kChromeApp, kWebApp, kIsolatedWebApp };

// Universal identifier for Kiosk apps.
class KioskAppId {
 public:
  static KioskAppId ForChromeApp(std::string_view chrome_app_id,
                                 const AccountId& account_id);
  static KioskAppId ForWebApp(const AccountId& account_id);
  static KioskAppId ForIsolatedWebApp(const AccountId& account_id);

  KioskAppId();
  KioskAppId(const KioskAppId&);
  KioskAppId(KioskAppId&&);
  KioskAppId& operator=(const KioskAppId&);
  KioskAppId& operator=(KioskAppId&&);
  ~KioskAppId();

  KioskAppType type;
  std::optional<std::string> app_id;
  AccountId account_id;

 private:
  KioskAppId(std::string_view chrome_app_id, const AccountId& account_id);
  KioskAppId(KioskAppType type, const AccountId& account_id);
};

std::ostream& operator<<(std::ostream& stream, const KioskAppId& app_id);
bool operator==(const KioskAppId& first, const KioskAppId& second);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_
