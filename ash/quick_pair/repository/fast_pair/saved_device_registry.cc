// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/base64.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

const char kFastPairSavedDevicesPref[] = "fast_pair.saved_devices";

}  // namespace

namespace ash {
namespace quick_pair {

SavedDeviceRegistry::SavedDeviceRegistry() = default;
SavedDeviceRegistry::~SavedDeviceRegistry() = default;

void SavedDeviceRegistry::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kFastPairSavedDevicesPref);
}

void SavedDeviceRegistry::SaveAccountKey(
    const std::string& mac_address,
    const std::vector<uint8_t>& account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }
  std::string encoded = base::Base64Encode(account_key);
  DictionaryPrefUpdate update(pref_service, kFastPairSavedDevicesPref);
  update->SetStringKey(mac_address, encoded);
}

absl::optional<const std::vector<uint8_t>> SavedDeviceRegistry::GetAccountKey(
    const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return absl::nullopt;
  }

  const base::Value* result =
      pref_service->Get(kFastPairSavedDevicesPref)->FindKey(mac_address);
  if (!result || !result->is_string()) {
    return absl::nullopt;
  }

  std::string encoded;
  std::string decoded;
  result->GetAsString(&encoded);
  if (!base::Base64Decode(encoded, &decoded)) {
    QP_LOG(WARNING) << __func__
                    << ": Failed to decode the account key from Base64.";
    return absl::nullopt;
  }

  return std::vector<uint8_t>(decoded.begin(), decoded.end());
}

}  // namespace quick_pair
}  // namespace ash
