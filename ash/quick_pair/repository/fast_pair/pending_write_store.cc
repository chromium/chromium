// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/base64.h"
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
  registry->RegisterListPref(kFastPairPendingDeletesPref);
}

PendingWriteStore::PendingWrite::PendingWrite(const std::string& mac_address,
                                              const std::string& hex_model_id)
    : mac_address(mac_address), hex_model_id(hex_model_id) {}

PendingWriteStore::PendingWrite::~PendingWrite() = default;

void PendingWriteStore::AddPairedDevice(const std::string& mac_address,
                                        const std::string& hex_model_id) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }
  DictionaryPrefUpdate update(pref_service, kFastPairPendingWritesPref);
  update->SetStringKey(mac_address, hex_model_id);
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

  DictionaryPrefUpdate update(pref_service, kFastPairPendingWritesPref);
  update->RemoveKey(mac_address);
}

void PendingWriteStore::DeletePairedDevice(const std::string& hex_account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }
  ListPrefUpdate update(pref_service, kFastPairPendingDeletesPref);
  update->Append(hex_account_key);
}

std::vector<const std::string> PendingWriteStore::GetPendingDeletes() {
  std::vector<const std::string> list;
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return list;
  }

  const base::Value::List& result =
      pref_service->GetValueList(kFastPairPendingDeletesPref);

  for (const auto& item : result) {
    list.emplace_back(item.GetString());
  }

  return list;
}

void PendingWriteStore::OnPairedDeviceDeleted(
    const std::string& hex_account_key) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    QP_LOG(WARNING) << __func__ << ": No user pref service available.";
    return;
  }

  ListPrefUpdate update(pref_service, kFastPairPendingDeletesPref);
  update->EraseListValue(base::Value(hex_account_key));
}

}  // namespace quick_pair
}  // namespace ash
