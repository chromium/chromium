// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/fixed_array.h"
#include "components/cross_device/logging/logging.h"
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

PendingWriteStore::PendingWrite::PendingWrite(
    const std::string& mac_address,
    const nearby::fastpair::FastPairInfo fast_pair_info)
    : mac_address(mac_address), fast_pair_info(fast_pair_info) {}

PendingWriteStore::PendingWrite::PendingWrite(PendingWrite&& pending_write) =
    default;
PendingWriteStore::PendingWrite::~PendingWrite() = default;

PendingWriteStore::PendingDelete::PendingDelete(
    const std::string& mac_address,
    const std::string& hex_account_key)
    : mac_address(mac_address), hex_account_key(hex_account_key) {}

PendingWriteStore::PendingDelete::~PendingDelete() = default;

void PendingWriteStore::WritePairedDevice(
    const std::string& mac_address,
    const nearby::fastpair::FastPairInfo fast_pair_info) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return;
  }

  // |fast_pair_info| must be converted first to bytes and then to
  // hex-encoded string format so that no UTF-8 encoding errors are thrown.
  size_t fp_info_size = fast_pair_info.ByteSizeLong();
  base::FixedArray<uint8_t> fp_info_bytes(fp_info_size);
  if (!fast_pair_info.SerializeToArray(fp_info_bytes.data(), fp_info_size)) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": couldn't serialize fast pair info of device "
        << "with mac address " << mac_address
        << ". Not writing this device to PendingWrite list.";
    return;
  }

  ScopedDictPrefUpdate update(pref_service, kFastPairPendingWritesPref);
  update->Set(mac_address, base::HexEncode(fp_info_bytes));
}

std::vector<PendingWriteStore::PendingWrite>
PendingWriteStore::GetPendingWrites() {
  std::vector<PendingWriteStore::PendingWrite> list;
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return list;
  }

  const base::Value::Dict& result =
      pref_service->GetDict(kFastPairPendingWritesPref);

  for (const auto item : result) {
    // PendingWrite member variable |fast_pair_info| is
    // stored in |result| as a hex encoded string. This need to be parsed into
    // type nearby::fastpair::FastPairInfo before initializing a new
    // PendingWrite to store in |list|.

    // Parsing fast pair info stored as string.
    std::string fp_info_str = item.second.GetString();
    nearby::fastpair::FastPairInfo fast_pair_info;

    // To align with Android's Footprints use, removal of a device from
    // the list of devices saved to a user's account is performed by writing a
    // footprint to Footprints with empty fast pair info.
    //
    // Thus, an empty |fp_info_str| is a valid element that is
    // intended to be written to Footprints. We must explicitly catch
    // this edge-case here, because passing an empty |fp_info_str| to
    // HexStringToBytes() would result in an error.
    if (fp_info_str.empty()) {
      fast_pair_info.ParseFromString(std::string());
    } else {
      std::vector<uint8_t> fp_info_bytes;
      if (!base::HexStringToBytes(fp_info_str, &fp_info_bytes)) {
        CD_LOG(WARNING, Feature::FP)
            << __func__ << ": fast pair info of "
            << "PendingWrite with mac address " << item.first
            << " not perfectly parsed into bytes from a hex-encoded string. "
            << "PendingWrite not added to PendingWrite list.";
        continue;
      }
      // Create fast pair info from byte buffer.
      if (!fast_pair_info.ParseFromArray(fp_info_bytes.data(),
                                         fp_info_bytes.size())) {
        CD_LOG(WARNING, Feature::FP)
            << __func__ << ": failed to parse Fast Pair Info of "
            << " PendingWrite with mac address " << item.first
            << " from bytes to type nearby::fastpair::FastPairInfo. "
            << "PendingWrite not included in PendingWrite list.";
        continue;
      }
    }

    list.emplace_back(/*mac_address=*/item.first, fast_pair_info);
  }

  return list;
}

void PendingWriteStore::OnPairedDeviceSaved(const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
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
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
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
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return list;
  }

  const base::Value::Dict& result =
      pref_service->GetDict(kFastPairPendingDeletesPref);

  for (const auto item : result) {
    list.emplace_back(item.first, item.second.GetString());
  }

  return list;
}

void PendingWriteStore::OnPairedDeviceDeleted(const std::string& mac_address) {
  PrefService* pref_service =
      QuickPairBrowserDelegate::Get()->GetActivePrefService();
  if (!pref_service) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
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
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No user pref service available.";
    return;
  }

  std::string hex_account_key = base::HexEncode(account_key);
  ScopedDictPrefUpdate update(pref_service, kFastPairPendingDeletesPref);
  const base::Value::Dict result =
      pref_service->GetDict(kFastPairPendingDeletesPref).Clone();
  for (const auto item : result) {
    if (item.second == hex_account_key) {
      CD_LOG(INFO, Feature::FP)
          << __func__ << ": Successfully removed pending delete from prefs.";
      update->Remove(item.first);
    }
  }
}

}  // namespace quick_pair
}  // namespace ash
