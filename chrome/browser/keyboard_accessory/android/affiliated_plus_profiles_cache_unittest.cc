// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/affiliated_plus_profiles_cache.h"

#include "base/memory/weak_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/plus_addresses/fake_plus_address_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using autofill::AutofillClient;
using autofill::TestAutofillClientInjector;
using autofill::TestContentAutofillClient;
using plus_addresses::FakePlusAddressService;
using ::testing::Test;

class ObserverMock : public AffiliatedPlusProfilesProvider::Observer {
 public:
  MOCK_METHOD(void, OnAffiliatedPlusProfilesFetched, (), (override));
};
}  // namespace

class AffiliatedPlusProfilesCacheTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    plus_profiles_cache_ = std::make_unique<AffiliatedPlusProfilesCache>(
        autofill_client(), &plus_address_service_);
  }

  AutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  AffiliatedPlusProfilesCache& cache() { return *plus_profiles_cache_; }

 private:
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  FakePlusAddressService plus_address_service_;
  std::unique_ptr<AffiliatedPlusProfilesCache> plus_profiles_cache_;
};

TEST_F(AffiliatedPlusProfilesCacheTest, FetchesAffiliatedPlusProfiles) {
  EXPECT_TRUE(cache().GetAffiliatedPlusProfiles().empty());

  cache().FetchAffiliatedPlusProfiles();
  EXPECT_FALSE(cache().GetAffiliatedPlusProfiles().empty());

  cache().ClearCachedPlusProfiles();
  EXPECT_TRUE(cache().GetAffiliatedPlusProfiles().empty());
}

TEST_F(AffiliatedPlusProfilesCacheTest, NotifiesObservers) {
  ObserverMock observer;
  cache().AddObserver(&observer);

  EXPECT_CALL(observer, OnAffiliatedPlusProfilesFetched);
  cache().FetchAffiliatedPlusProfiles();
}

TEST_F(AffiliatedPlusProfilesCacheTest, AddAndRemoveObserver) {
  std::unique_ptr<ObserverMock> observer = std::make_unique<ObserverMock>();
  cache().AddObserver(observer.get());

  EXPECT_CALL(*observer, OnAffiliatedPlusProfilesFetched);
  cache().FetchAffiliatedPlusProfiles();

  cache().RemoveObserver(observer.get());
  observer.reset();
  cache().FetchAffiliatedPlusProfiles();
}
