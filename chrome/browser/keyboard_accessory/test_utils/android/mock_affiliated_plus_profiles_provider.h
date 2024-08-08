// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_AFFILIATED_PLUS_PROFILES_PROVIDER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_AFFILIATED_PLUS_PROFILES_PROVIDER_H_

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_provider.h"
#include "components/plus_addresses/plus_address_types.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockAffiliatedPlusProfilesProvider
    : public AffiliatedPlusProfilesProvider {
 public:
  MockAffiliatedPlusProfilesProvider();
  ~MockAffiliatedPlusProfilesProvider() override;
  MOCK_METHOD(base::span<const plus_addresses::PlusProfile>,
              GetAffiliatedPlusProfiles,
              (),
              (const override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
  base::WeakPtr<MockAffiliatedPlusProfilesProvider> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAffiliatedPlusProfilesProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_TEST_UTILS_ANDROID_MOCK_AFFILIATED_PLUS_PROFILES_PROVIDER_H_
