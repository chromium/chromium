// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

class PlusAddressServiceBrowserTest : public PlatformBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(plus_addresses::kFeature);
    PlatformBrowserTest::SetUp();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A very basic test for now, to ensure that the service plumbing is set up
// correctly.
IN_PROC_BROWSER_TEST_F(PlusAddressServiceBrowserTest, VerifyNonNullService) {
  plus_addresses::PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetActiveWebContents()->GetBrowserContext());
  EXPECT_NE(plus_address_service, nullptr);
}

// With the primary account available, with an email address, and the feature
// enabled, `SupportsPlusAddresses` should return true. In contrast with the
// unit tests, this ensures the various `KeyedService` factories are wired
// correctly.
IN_PROC_BROWSER_TEST_F(PlusAddressServiceBrowserTest,
                       VerifySupportsPlusAddresses) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "plus@plus.plus",
                                      signin::ConsentLevel::kSignin);
  plus_addresses::PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetActiveWebContents()->GetBrowserContext());
  EXPECT_NE(plus_address_service, nullptr);
  EXPECT_TRUE(plus_address_service->SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example"))));
}
