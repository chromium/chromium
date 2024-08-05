// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AFFILIATED_PLUS_PROFILES_PROVIDER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AFFILIATED_PLUS_PROFILES_PROVIDER_H_

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"

namespace plus_addresses {
struct PlusProfile;
}  // namespace plus_addresses

// Helper interface used by `AccessoryController` implementations to get access
// to the list of affiliated plus profiles. Fetching the list of affiliated plus
// profiles is an asynchronous operation. This helper class provides synchronous
// access to plus profiles and their affiliation data to consumers.
class AffiliatedPlusProfilesProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAffiliatedPlusProfilesFetched() = 0;
  };
  AffiliatedPlusProfilesProvider() = default;
  AffiliatedPlusProfilesProvider(const AffiliatedPlusProfilesProvider&) =
      delete;
  AffiliatedPlusProfilesProvider& operator=(
      const AffiliatedPlusProfilesProvider&) = delete;
  AffiliatedPlusProfilesProvider(AffiliatedPlusProfilesProvider&&) = delete;
  AffiliatedPlusProfilesProvider& operator=(AffiliatedPlusProfilesProvider&&) =
      delete;

  virtual ~AffiliatedPlusProfilesProvider() = default;

  virtual base::span<const plus_addresses::PlusProfile>
  GetAffiliatedPlusProfiles() const = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AFFILIATED_PLUS_PROFILES_PROVIDER_H_
