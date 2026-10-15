// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_client_data_storage.h"

#include <cstddef>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "components/prefs/pref_service.h"

namespace glic {
namespace {

constexpr base::TimeDelta kWriteToPrefsDelay = base::Seconds(1);

}  // namespace

GlicClientDataStorage::GlicClientDataStorage(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);
  for (const auto [key, val] :
       pref_service_->GetDict(prefs::kGlicClientDataStorage)) {
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

  std::string key_str(key);
  if (value.has_value()) {
    auto it = storage_.find(key);
    if (it != storage_.end() && it->second == *value) {
      return;
    }
    storage_.insert_or_assign(key_str, *value);
  } else {
    if (storage_.erase(key) == 0) {
      return;
    }
  }
  ScheduleWriteToPrefs();
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
  base::DictValue dict;
  size_t total_size = 0;
  for (const auto& [key, value] : storage_) {
    dict.Set(key, value);
    total_size += key.size();
    total_size += value.size();
  }
  if (total_size > kMaxTotalClientDataStorageBytes) {
    LOG(ERROR) << "Total GlicClientDataStorage exceeds max. Clearing.";
    dict.clear();
    storage_.clear();
    for (Observer& observer : observers_) {
      observer.OnClientStorageCleared();
    }
  }
  pref_service_->SetDict(prefs::kGlicClientDataStorage, std::move(dict));
}

}  // namespace glic
