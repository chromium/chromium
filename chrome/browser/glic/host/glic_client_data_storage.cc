// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_client_data_storage.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace glic {
namespace {

constexpr base::TimeDelta kWriteToPrefsDelay = base::Seconds(1);

// v1: consumer Gemini's items were top-level string values.
// v2: each provider's items are a dict at its guest URL key.
constexpr int kCurrentVersion = 2;

// Moves v1 items to `kGeminiGuestUrlKey`, unless the pref is already v2 or
// empty. Only consumer Gemini wrote v1 items. v1 data on profiles with a
// non-prod guest URL (e.g. via flags or presets) is left under this key, so
// those profiles won't read it.
// An empty pref is left untouched so that constructing the storage doesn't
// dirty prefs; `WriteToPrefs()` sets the version on the first write.
// TODO(crbug.com/581048961): Remove this migration after Nov 2027.
void MaybeMigrate(PrefService* pref_service) {
  const base::DictValue& dict =
      pref_service->GetDict(prefs::kGlicClientDataStorage);
  if (dict.empty() || dict.FindInt(kClientDataStorageVersionKey)) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service, prefs::kGlicClientDataStorage);
  std::vector<std::string> v1_keys;
  for (const auto [key, value] : *update) {
    if (!value.is_dict()) {
      v1_keys.push_back(key);
    }
  }
  base::DictValue v1_items;
  for (const std::string& key : v1_keys) {
    std::optional<base::Value> value = update->Extract(key);
    // v1 only wrote strings, and ignored anything else when reading.
    if (value && value->is_string()) {
      v1_items.Set(key, std::move(*value));
    }
  }
  if (!v1_items.empty()) {
    update->EnsureDict(kGeminiGuestUrlKey)->Merge(std::move(v1_items));
  }
  update->Set(kClientDataStorageVersionKey, kCurrentVersion);
}

// Returns the size of the keys and string values in `dict`, recursively.
size_t ComputeSize(const base::DictValue& dict) {
  size_t size = 0;
  for (const auto [key, value] : dict) {
    size += key.size();
    if (value.is_string()) {
      size += value.GetString().size();
    } else if (value.is_dict()) {
      size += ComputeSize(value.GetDict());
    }
  }
  return size;
}

}  // namespace

GlicClientDataStorage::GlicClientDataStorage(PrefService* pref_service,
                                             std::string guest_url_key)
    : pref_service_(pref_service), guest_url_key_(std::move(guest_url_key)) {
  CHECK(pref_service_);
  // Runs regardless of provider, so leftover consumer data is migrated even if
  // the profile now uses another provider.
  MaybeMigrate(pref_service_);
  const base::DictValue* persisted =
      pref_service_->GetDict(prefs::kGlicClientDataStorage)
          .FindDict(guest_url_key_);
  if (!persisted) {
    return;
  }
  for (const auto [key, val] : *persisted) {
    if (val.is_string()) {
      storage_.emplace(key, val.GetString());
    }
  }
}

GlicClientDataStorage::~GlicClientDataStorage() {
  if (write_timer_.IsRunning()) {
    write_timer_.FireNow();
  }
}

bool GlicClientDataStorage::Contains(std::string_view key) const {
  return storage_.contains(key);
}

void GlicClientDataStorage::SetItem(std::string_view key,
                                    const std::optional<std::string>& value) {
  if (key.size() + (value ? value->size() : 0) >
      kMaxTotalClientDataStorageBytes) {
    return;
  }

  if (value.has_value()) {
    auto it = storage_.find(key);
    if (it != storage_.end()) {
      if (it->second == *value) {
        return;
      }
      it->second = *value;
    } else {
      storage_.emplace(std::string(key), *value);
    }
  } else if (storage_.erase(key) == 0) {
    return;
  }
  ScheduleWriteToPrefs();
  const std::string key_str(key);
  for (Observer& observer : observers_) {
    observer.OnClientStorageItemChanged(key_str, value);
  }
}

void GlicClientDataStorage::Clear() {
  if (storage_.empty()) {
    return;
  }
  storage_.clear();
  ScheduleWriteToPrefs();
  for (Observer& observer : observers_) {
    observer.OnClientStorageCleared();
  }
}

void GlicClientDataStorage::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GlicClientDataStorage::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GlicClientDataStorage::Flush() {
  if (write_timer_.IsRunning()) {
    write_timer_.FireNow();
  }
}

base::WeakPtr<GlicClientDataStorage> GlicClientDataStorage::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GlicClientDataStorage::ScheduleWriteToPrefs() {
  if (!write_timer_.IsRunning()) {
    write_timer_.Start(FROM_HERE, kWriteToPrefsDelay, this,
                       &GlicClientDataStorage::WriteToPrefs);
  }
}

void GlicClientDataStorage::WriteToPrefs() {
  base::DictValue entry;
  for (const auto& [key, value] : storage_) {
    entry.Set(key, value);
  }

  // The limit applies to the whole pref, across all guest URL keys.
  size_t total_size = ComputeSize(entry) + guest_url_key_.size();
  for (const auto [key, value] :
       pref_service_->GetDict(prefs::kGlicClientDataStorage)) {
    if (key == guest_url_key_) {
      // Replaced by `entry`.
      continue;
    }
    total_size += key.size();
    if (value.is_dict()) {
      total_size += ComputeSize(value.GetDict());
    }
  }
  if (total_size > kMaxTotalClientDataStorageBytes) {
    LOG(ERROR) << "Total GlicClientDataStorage exceeds max. Clearing.";
    storage_.clear();
    for (Observer& observer : observers_) {
      observer.OnClientStorageCleared();
    }
    pref_service_->SetDict(
        prefs::kGlicClientDataStorage,
        base::DictValue().Set(kClientDataStorageVersionKey, kCurrentVersion));
    return;
  }

  if (entry.empty() && !pref_service_->GetDict(prefs::kGlicClientDataStorage)
                            .contains(guest_url_key_)) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service_, prefs::kGlicClientDataStorage);
  // `MaybeMigrate()` leaves an empty pref unversioned, so set it here.
  update->Set(kClientDataStorageVersionKey, kCurrentVersion);
  if (entry.empty()) {
    update->Remove(guest_url_key_);
  } else {
    update->Set(guest_url_key_, std::move(entry));
  }
}

}  // namespace glic
