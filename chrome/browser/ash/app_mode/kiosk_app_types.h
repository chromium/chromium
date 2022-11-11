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
  KioskAppType type;
  absl::optional<std::string> app_id;
  absl::optional<AccountId> account_id;

  KioskAppId();
  ~KioskAppId();
  KioskAppId(const KioskAppId&);

  static KioskAppId ForChromeApp(const std::string& app_id);
  static KioskAppId ForWebApp(const AccountId& account_id);
  static KioskAppId ForArcApp(const AccountId& account_id);

  // Use this method when we are unsure which type of kiosk app this AccountId
  // belongs to.
  static bool FromAccountId(const AccountId& account_id,
                            KioskAppId* kiosk_app_id);

 private:
  KioskAppId(KioskAppType type, const std::string& app_id);
  KioskAppId(KioskAppType type, const AccountId& account_id);
};

// Overload << operator to allow logging of KioskAppId.
std::ostream& operator<<(std::ostream& stream, const KioskAppId& app_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_
