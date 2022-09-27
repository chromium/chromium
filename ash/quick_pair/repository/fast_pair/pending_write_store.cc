// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

const char kFastPairPendingWritesPref[] = "fast_pair.pending_writes";
const char kFastPairPendingDeletesPref[] = "fast_pair.pending_deletes";

}  // namespace

namespace ash {
namespace quick_pair {

PendingWriteStore::PendingWriteStore() = default;
PendingWriteStore::~PendingWriteStore() = default;

void PendingWriteStore::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kFastPairPendingWritesPref);
  registry->RegisterDictionaryPref(kFastPairPendingDeletesPref);
}

PendingWriteStore::PendingWrite::PendingWrite(const std::string& mac_address,
                                              const std::string& hex_model_id)
    : mac_address(mac_address), hex_model_id(hex_model_id) {}

PendingWriteStore::PendingWrite::~PendingWrite() = default;

PendingWriteStore::PendingDelete::PendingDelete(
    const std::string& mac_address,
    const std::string& hex_account_key)
    : mac_address(mac_address), hex_account_key(hex_account_key) {}

PendingWriteStore::PendingDelete::~PendingDelete() = default;

void PendingWriteStore::AddPairedDevice(const std::string& mac_address,
                                        const std::string& hex_model_id) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }
  ScopedDictPrefUpdate update(pref_service, kFastPairPendingWritesPref);
  update->Set(mac_address, hex_model_id);
}

std::vector<PendingWriteStore::PendingWrite>
PendingWriteStore::GetPendingAdds() {
  std::vector<PendingWriteStore::PendingWrite> list;
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return list;
  }

  const base::Value& result =
      pref_service->GetValue(kFastPairPendingWritesPref);

  for (const auto item : result.DictItems()) {
    list.emplace_back(item.first, item.second.GetString());
  }

  return list;
}

void PendingWriteStore::OnPairedDeviceSaved(const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }

  ScopedDictPrefUpdate update(pref_service, kFastPairPendingWritesPref);
  update->Remove(mac_address);
}

void PendingWriteStore::DeletePairedDevice(const std::string& mac_address,
                                           const std::string& hex_account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }
  ScopedDictPrefUpdate update(pref_service, kFastPairPendingDeletesPref);
  update->Set(mac_address, hex_account_key);
}

std::vector<PendingWriteStore::PendingDelete>
PendingWriteStore::GetPendingDeletes() {
  std::vector<PendingWriteStore::PendingDelete> list;
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return list;
  }

  const base::Value& result =
      pref_service->GetValue(kFastPairPendingDeletesPref);

  for (const auto item : result.DictItems()) {
    list.emplace_back(item.first, item.second.GetString());
  }

  return list;
}

void PendingWriteStore::OnPairedDeviceDeleted(const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }

  ScopedDictPrefUpdate update(pref_service, kFastPairPendingDeletesPref);
  update->Remove(mac_address);
}

void PendingWriteStore::OnPairedDeviceDeleted(
    const std::vector<uint8_t>& account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }

  std::string hex_account_key = base::HexEncode(account_key);
  ScopedDictPrefUpdate update(pref_service, kFastPairPendingDeletesPref);
  const base::Value::Dict result =
      pref_service->GetDict(kFastPairPendingDeletesPref).Clone();
  for (const auto item : result) {
    if (item.second == hex_account_key) {
      QP_LOG(INFO) << __func__
                   << ": Successfully removed pending delete from prefs.";
      update->Remove(item.first);
    }
  }
}

}  // namespace quick_pair
}  // namespace ash
