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
enum class KioskAppType { kChromeApp, kWebApp, kIsolatedWebApp, kArcvmApp };

// Universal identifier for Kiosk apps.
class KioskAppId {
 public:
  static KioskAppId ForChromeApp(std::string_view chrome_app_id,
                                 const AccountId& account_id);
  static KioskAppId ForWebApp(const AccountId& account_id);
  static KioskAppId ForIsolatedWebApp(const AccountId& account_id);
  static KioskAppId ForArcvmApp(const AccountId& account_id);

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

// Set of parameters required to install a Kiosk app from a cached location.
struct KioskAppInstallParams {
  KioskAppInstallParams(std::string id,
                        std::string crx_file_location,
                        std::string version,
                        bool is_store_app);
  KioskAppInstallParams(const KioskAppInstallParams& other);
  KioskAppInstallParams(KioskAppInstallParams&& other) noexcept;
  KioskAppInstallParams& operator=(const KioskAppInstallParams& other);
  KioskAppInstallParams& operator=(KioskAppInstallParams&& other) noexcept;
  ~KioskAppInstallParams();

  // Id of the primary Kiosk app.
  std::string id;
  // Location of the crx file in local cache.
  std::string crx_file_location;
  // Version of the app to be installed.
  std::string version;
  // Indicates whether the app should be downloaded from Chrome Web Store.
  bool is_store_app;
};

enum class KioskInstallResult {
  kUnknown = 0,
  // Installation completed successfully, kiosk is ready to launch.
  kSuccess = 1,
  // Primary app is not cached yet, network is required to rectify.
  kPrimaryAppNotCached = 2,
  // Install of primary app failed
  kPrimaryAppInstallFailed = 3,
  // Install of a secondary app failed
  kSecondaryAppInstallFailed = 4,
  // The primary app does not have kiosk support in the manifest
  kPrimaryAppNotKioskEnabled = 5,
  // Update of primary app failed, but an installed version already exists.
  kPrimaryAppUpdateFailed = 6,
  // Update of secondary app failed, but an installed version already exists.
  kSecondaryAppUpdateFailed = 7,
};

enum class KioskLaunchResult {
  kUnknown = 0,
  // Launch of kiosk app was successful.
  kSuccess = 1,
  // Primary or secondary apps are not ready for launch
  kUnableToLaunch = 2,
  // The primary app is not offline enabled, but network is not ready
  kNetworkMissing = 3,
  // The primary app is a deprecated Chrome App.
  kChromeAppDeprecated = 4,
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_TYPES_H_
