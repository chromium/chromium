// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_

#include <string>

#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Type of different kiosk apps to be launched.
enum class KioskAppType { kArcApp, kChromeApp, kWebApp };

// Universal identifier for all kiosk apps.
class KioskAppId {
 public:
  static KioskAppId ForChromeApp(
      const std::string& chrome_app_id,
      absl::optional<AccountId> account_id = absl::nullopt);
  static KioskAppId ForWebApp(const AccountId& account_id);
  static KioskAppId ForArcApp(const AccountId& account_id);

  KioskAppId();
  KioskAppId(const KioskAppId&);
  ~KioskAppId();

  KioskAppType type;
  absl::optional<std::string> app_id;
  absl::optional<AccountId> account_id;

 private:
  KioskAppId(const std::string& chrome_app_id,
             absl::optional<AccountId> account_id);
  KioskAppId(KioskAppType type, const AccountId& account_id);
};

// Overload << operator to allow logging of KioskAppId.
std::ostream& operator<<(std::ostream& stream, const KioskAppId& app_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_
