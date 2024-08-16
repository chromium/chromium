// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AFFILIATED_PLUS_PROFILES_CACHE_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AFFILIATED_PLUS_PROFILES_CACHE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_provider.h"
#include "components/plus_addresses/plus_address_types.h"

namespace autofill {
class AutofillClient;
}  // namespace autofill

namespace plus_addresses {
class PlusAddressService;
}  // namespace plus_addresses

// Helper class that queries `PlusAddressService` to obtain the list of
// affiliated plus profiles for the current domain. This class is not thread
// safe. External synchronization is required for it to be used from multiple
// threads.
class AffiliatedPlusProfilesCache : public AffiliatedPlusProfilesProvider {
 public:
  AffiliatedPlusProfilesCache(
      autofill::AutofillClient* client,
      plus_addresses::PlusAddressService* plus_address_service);
  AffiliatedPlusProfilesCache(AffiliatedPlusProfilesCache&) = delete;
  AffiliatedPlusProfilesCache& operator=(const AffiliatedPlusProfilesCache&) =
      delete;
  AffiliatedPlusProfilesCache(AffiliatedPlusProfilesCache&&) = delete;
  AffiliatedPlusProfilesCache& operator=(AffiliatedPlusProfilesCache&&) =
      delete;

  ~AffiliatedPlusProfilesCache() override;

  // Issues a query to the `PlusAddressService` for the affiliated plus profiles
  // list.
  void FetchAffiliatedPlusProfiles();
  // Clears any plus profiles stored in this class.
  void ClearCachedPlusProfiles();
  base::WeakPtr<AffiliatedPlusProfilesCache> GetWeakPtr();

  // AffiliatedPlusProfilesProvider:
  base::span<const plus_addresses::PlusProfile> GetAffiliatedPlusProfiles()
      const override;
  void AddObserver(AffiliatedPlusProfilesProvider::Observer* observer) override;
  void RemoveObserver(
      AffiliatedPlusProfilesProvider::Observer* observer) override;

 private:
  // Persists the fetched plus profiles and triggers all registered observers.
  void OnAffiliatedPlusProfilesFetched(
      std::vector<plus_addresses::PlusProfile> profiles);

  const raw_ref<const autofill::AutofillClient> client_;
  const raw_ref<plus_addresses::PlusAddressService> plus_address_service_;
  base::ObserverList<Observer> observers_;

  // Most recently cached list of affiliated plus profiles. This list is empty
  // after every call to `ClearCachedPlusProfiles()`.
  std::vector<plus_addresses::PlusProfile> cached_plus_profiles_;

  base::WeakPtrFactory<AffiliatedPlusProfilesCache> weak_factory_{this};
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AFFILIATED_PLUS_PROFILES_CACHE_H_
