// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_POLICY_UTIL_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_POLICY_UTIL_H_

#include <optional>

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "url/gurl.h"

namespace ash {

struct KioskIwaPolicyData {
  web_package::SignedWebBundleId web_bundle_id;
  GURL update_manifest_url;
};

// If an Isolated Web App is currently running in kiosk mode, returns
// corresponding data from DeviceLocalAccounts policy. Otherwise returns
// nullopt.
std::optional<KioskIwaPolicyData> GetCurrentKioskIwaPolicyData();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_POLICY_UTIL_H_
