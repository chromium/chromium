// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"

#include <string_view>

#include "ash/constants/chrome_webui_url_constants.h"
#include "base/containers/fixed_flat_set.h"

namespace ash {

bool IsSystemWebUIHost(std::string_view host) {
  // Compares host instead of full URL for performance (the strings are
  // shorter).
  constexpr auto kHosts = base::MakeFixedFlatSet<std::string_view>({
      kChromeUIAccountManagerErrorHost,
      kChromeUIAccountMigrationWelcomeHost,
      kChromeUIAddSupervisionHost,
      kChromeUIAppInstallDialogHost,
      kChromeUIBluetoothPairingHost,
      kChromeUIBorealisCreditsHost,
      chrome_urls::kChromeUICertificateManagerHost,
      kChromeUICloudUploadHost,
      kChromeUICrostiniCreditsHost,
      kChromeUICrostiniInstallerHost,
      kChromeUICryptohomeHost,
      kChromeUIEmojiPickerHost,
      kChromeUIExtendedUpdatesDialogHost,
      kChromeUIInternetConfigDialogHost,
      kChromeUIInternetDetailDialogHost,
      kChromeUILockScreenNetworkHost,
      kChromeUILockScreenStartReauthHost,
      kChromeUIMobileSetupHost,
      kChromeUIMultiDeviceSetupHost,
      kChromeUINetworkHost,
      kChromeUINotificationTesterHost,
      kChromeUIOobeHost,
      kChromeUIOSCreditsHost,
      kChromeUIOSSettingsHost,
      kChromeUIPasswordChangeHost,
      kChromeUIPowerHost,
      kChromeUISetTimeHost,
      kChromeUISmbCredentialsHost,
      kChromeUISmbShareHost,
  });

  return kHosts.contains(host);
}

}  // namespace ash
