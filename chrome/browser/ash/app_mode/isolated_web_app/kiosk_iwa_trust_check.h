// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_TRUST_CHECK_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_TRUST_CHECK_H_

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace ash {

// Returns true if `web_bundle_id` matches a kiosk session that is currently
// running.
bool IsTrustedAsKioskIwa(const web_package::SignedWebBundleId& web_bundle_id);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_TRUST_CHECK_H_
