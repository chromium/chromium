// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_CLIENT_DATA_STORAGE_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_CLIENT_DATA_STORAGE_H_

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"

class PrefService;

namespace glic {

// Maximum amount of data that can be stored in GlicClientDataStorage.
// If exceeded, GlicClientDataStorage will clear itself.
inline constexpr size_t kMaxTotalClientDataStorageBytes = 5 * 1000 * 1000;

// Holds the authoritative in-memory copy of client data storage for the Glic
// web client and persists updates to `prefs::kGlicClientDataStorage`.
// Owned by `GlicKeyedService`.
class GlicClientDataStorage {
 public:
  using StorageMap = base::flat_map<std::string, std::string>;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnClientStorageItemChanged(
        const std::string& key,
        const std::optional<std::string>& value) = 0;
    virtual void OnClientStorageCleared() = 0;
  };

  explicit GlicClientDataStorage(PrefService* pref_service);
  ~GlicClientDataStorage();

  GlicClientDataStorage(const GlicClientDataStorage&) = delete;
  GlicClientDataStorage& operator=(const GlicClientDataStorage&) = delete;

  // Returns the authoritative in-memory key-value map.
  const StorageMap& GetAll() const { return storage_; }

  // Returns true if `key` is currently present in storage.
  bool Contains(std::string_view key) const;

  // Sets or removes `key` (when `value` is std::nullopt) in the in-memory
  // storage and persists the change to prefs.
  void SetItem(std::string_view key, const std::optional<std::string>& value);

  // Clears all stored items in memory and in prefs.
  void Clear();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Immediately writes any pending storage updates to prefs.
  void Flush();

  base::WeakPtr<GlicClientDataStorage> GetWeakPtr();

 private:
  void ScheduleWriteToPrefs();
  void WriteToPrefs();

  raw_ptr<PrefService> pref_service_;
  StorageMap storage_;
  base::OneShotTimer write_timer_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<GlicClientDataStorage> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_CLIENT_DATA_STORAGE_H_
