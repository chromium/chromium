// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_POLICY_UTIL_H_
#define CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_POLICY_UTIL_H_

#include <optional>
#include <string>
#include <variant>

#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"

namespace ash {

using IwaPinnedVersion = std::optional<web_app::IwaVersion>;

// Returns an `UpdateChannel` object for a valid `raw_policy_value` input.
// Otherwise returns unexpected.
// Note: Returns `default` channel for the empty (unset) policy value.
base::expected<web_app::UpdateChannel, std::monostate> GetUpdateChannel(
    const std::string& raw_policy_value);

// Returns an optional version object for a valid `raw_policy_value` input.
// Otherwise returns unexpected.
// Note: Returns `nullopt` for the empty (unset) policy value meaning no pinned
// version was set and latest available should be used.
base::expected<IwaPinnedVersion, std::monostate> GetPinnedVersion(
    const std::string& raw_policy_value);

// Returns a web bundle id of an IWA that is currently running in kiosk mode.
// Otherwise returns nullopt.
std::optional<web_package::SignedWebBundleId> GetCurrentKioskIwaBundleId();

struct KioskIwaUpdateData {
  KioskIwaUpdateData(const web_package::SignedWebBundleId& web_bundle_id,
                     GURL update_manifest_url,
                     web_app::UpdateChannel update_channel,
                     IwaPinnedVersion pinned_version,
                     bool allow_downgrades);
  ~KioskIwaUpdateData();
  KioskIwaUpdateData(const KioskIwaUpdateData& other) = delete;
  KioskIwaUpdateData& operator=(const KioskIwaUpdateData&) = delete;

  const web_package::SignedWebBundleId web_bundle_id;
  const GURL update_manifest_url;
  const web_app::UpdateChannel update_channel;
  const IwaPinnedVersion pinned_version;
  const bool allow_downgrades;
};

// If an Isolated Web App is running in kiosk mode, returns app update data from
// DeviceLocalAccounts policy. Returns nullopt if update data is invalid or not
// in IWA kiosk mode.
std::optional<KioskIwaUpdateData> GetCurrentKioskIwaUpdateData();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ISOLATED_WEB_APP_KIOSK_IWA_POLICY_UTIL_H_
