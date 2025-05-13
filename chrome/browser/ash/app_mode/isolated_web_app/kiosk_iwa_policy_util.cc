// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_policy_util.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/update_channel.h"
#include "url/gurl.h"

namespace {

bool IsIwaKioskSession() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsLoggedInAsKioskIWA();
}

policy::DeviceLocalAccount GetCurrentDeviceLocalAccount() {
  const user_manager::User& current_user =
      CHECK_DEREF(user_manager::UserManager::Get()->GetPrimaryUser());
  const AccountId& account_id = current_user.GetAccountId();

  const std::vector<policy::DeviceLocalAccount> device_local_accounts =
      policy::GetDeviceLocalAccounts(ash::CrosSettings::Get());

  // The current device local account should exist and match the type as already
  // checked via UserManager.
  for (const auto& account : device_local_accounts) {
    if (AccountId::FromUserEmail(account.user_id) == account_id) {
      CHECK_EQ(account.type,
               policy::DeviceLocalAccountType::kKioskIsolatedWebApp);
      return account;
    }
  }

  NOTREACHED();
}

}  // namespace

namespace ash {

// TODO(crbug.com/417411010): Unify with policy processing for managed user.
base::expected<web_app::UpdateChannel, std::monostate> GetUpdateChannel(
    const std::string& raw_policy_value) {
  // Empty policy value means using a default channel.
  if (raw_policy_value.empty()) {
    return web_app::UpdateChannel::default_channel();
  }

  auto update_channel_parsed = web_app::UpdateChannel::Create(raw_policy_value);
  // An invalid policy value results in an error. It doesn't fall back to the
  // default channel.
  if (!update_channel_parsed.has_value()) {
    return base::unexpected(std::monostate());
  }

  return update_channel_parsed.value();
}

base::expected<IwaPinnedVersion, std::monostate> GetPinnedVersion(
    const std::string& raw_policy_value) {
  // Empty policy value means no version pinning, not an error.
  if (raw_policy_value.empty()) {
    return std::nullopt;
  }

  base::Version parsed_pinned_version = base::Version(raw_policy_value);
  // An invalid policy value results in an error. It doesn't fall back to no
  // pinning.
  if (!parsed_pinned_version.IsValid()) {
    return base::unexpected(std::monostate());
  }

  return parsed_pinned_version;
}

KioskIwaUpdateData::KioskIwaUpdateData(
    const web_package::SignedWebBundleId& web_bundle_id,
    GURL update_manifest_url,
    web_app::UpdateChannel update_channel,
    IwaPinnedVersion pinned_version,
    bool allow_downgrades)
    : web_bundle_id(web_bundle_id),
      update_manifest_url(std::move(update_manifest_url)),
      update_channel(std::move(update_channel)),
      pinned_version(std::move(pinned_version)),
      allow_downgrades(allow_downgrades) {}

KioskIwaUpdateData::~KioskIwaUpdateData() = default;

std::optional<web_package::SignedWebBundleId> GetCurrentKioskIwaBundleId() {
  if (!ash::features::IsIsolatedWebAppKioskEnabled() || !IsIwaKioskSession()) {
    return std::nullopt;
  }

  auto current_kiosk_policy = GetCurrentDeviceLocalAccount();

  // Web bundle id in the current IWA kiosk account should be valid.
  auto current_web_bundle_id = web_package::SignedWebBundleId::Create(
      current_kiosk_policy.kiosk_iwa_info.web_bundle_id());
  CHECK(current_web_bundle_id.has_value());

  return current_web_bundle_id.value();
}

std::optional<KioskIwaUpdateData> GetCurrentKioskIwaUpdateData() {
  if (!ash::features::IsIsolatedWebAppKioskEnabled() || !IsIwaKioskSession()) {
    return std::nullopt;
  }

  auto current_kiosk_policy = GetCurrentDeviceLocalAccount();

  // Web bundle id and update manifest URL in the current IWA kiosk account
  // should be valid.
  auto maybe_web_bundle_id = web_package::SignedWebBundleId::Create(
      current_kiosk_policy.kiosk_iwa_info.web_bundle_id());
  CHECK(maybe_web_bundle_id.has_value());

  GURL current_update_manifest_url(
      current_kiosk_policy.kiosk_iwa_info.update_manifest_url());
  CHECK(current_update_manifest_url.is_valid());

  const std::string& update_channel_raw =
      current_kiosk_policy.kiosk_iwa_info.update_channel();
  ASSIGN_OR_RETURN(
      web_app::UpdateChannel update_channel,
      GetUpdateChannel(update_channel_raw),
      [update_channel_raw](auto) -> std::optional<KioskIwaUpdateData> {
        LOG(ERROR) << "Cannot update kiosk IWA. Invalid update channel: "
                   << update_channel_raw;
        return std::nullopt;
      });

  const std::string& pinned_version_raw =
      current_kiosk_policy.kiosk_iwa_info.pinned_version();
  ASSIGN_OR_RETURN(
      IwaPinnedVersion pinned_version, GetPinnedVersion(pinned_version_raw),
      [pinned_version_raw](auto) -> std::optional<KioskIwaUpdateData> {
        LOG(ERROR) << "Cannot update kiosk IWA. Invalid pinned version: "
                   << pinned_version_raw;
        return std::nullopt;
      });

  const bool allow_downgrades =
      current_kiosk_policy.kiosk_iwa_info.allow_downgrades();
  if (allow_downgrades && !pinned_version.has_value()) {
    LOG(ERROR) << "Cannot update kiosk IWA. Enabling downgrades requires "
                  "pinned version.";
    return std::nullopt;
  }

  return std::make_optional<KioskIwaUpdateData>(
      maybe_web_bundle_id.value(), current_update_manifest_url, update_channel,
      pinned_version, allow_downgrades);
}

}  // namespace ash
