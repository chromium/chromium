// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_cache.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "components/autofill/content/browser/content_autofill_client.h"

AffiliatedPlusProfilesCache::AffiliatedPlusProfilesCache(
    autofill::AutofillClient* client,
    plus_addresses::PlusAddressService* plus_address_service)
    : client_(CHECK_DEREF(client)),
      plus_address_service_(CHECK_DEREF(plus_address_service)) {}

AffiliatedPlusProfilesCache::~AffiliatedPlusProfilesCache() = default;

void AffiliatedPlusProfilesCache::FetchAffiliatedPlusProfiles() {
  plus_address_service_->GetAffiliatedPlusProfiles(
      client_->GetLastCommittedPrimaryMainFrameOrigin(),
      base::BindOnce(
          &AffiliatedPlusProfilesCache::OnAffiliatedPlusProfilesFetched,
          GetWeakPtr()));
}

void AffiliatedPlusProfilesCache::ClearCachedPlusProfiles() {
  cached_plus_profiles_.clear();
}

base::WeakPtr<AffiliatedPlusProfilesCache>
AffiliatedPlusProfilesCache::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::span<const plus_addresses::PlusProfile>
AffiliatedPlusProfilesCache::GetAffiliatedPlusProfiles() const {
  return cached_plus_profiles_;
}

void AffiliatedPlusProfilesCache::AddObserver(
    AffiliatedPlusProfilesProvider::Observer* observer) {
  observers_.AddObserver(observer);
}

void AffiliatedPlusProfilesCache::RemoveObserver(
    AffiliatedPlusProfilesProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AffiliatedPlusProfilesCache::OnAffiliatedPlusProfilesFetched(
    std::vector<plus_addresses::PlusProfile> profiles) {
  cached_plus_profiles_ = std::move(profiles);
  for (AffiliatedPlusProfilesProvider::Observer& observer : observers_) {
    observer.OnAffiliatedPlusProfilesFetched();
  }
}
