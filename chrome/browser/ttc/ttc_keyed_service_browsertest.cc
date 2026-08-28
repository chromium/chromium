// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_keyed_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/features.h"
#include "chrome/browser/ttc/ttc_keyed_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"

namespace ttc {

class TtcKeyedServiceBrowserTest : public PlatformBrowserTest {
 public:
  TtcKeyedServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtc);
  }
  ~TtcKeyedServiceBrowserTest() override = default;

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TtcKeyedServiceBrowserTest, CreatedForRegularProfile) {
  EXPECT_NE(TtcKeyedService::Get(profile()), nullptr);
}

IN_PROC_BROWSER_TEST_F(TtcKeyedServiceBrowserTest, NotCreatedForOTRProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(TtcKeyedService::Get(otr_profile), nullptr);
}

class TtcKeyedServiceDisabledBrowserTest : public TtcKeyedServiceBrowserTest {
 public:
  TtcKeyedServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(kTtc);
  }
  ~TtcKeyedServiceDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TtcKeyedServiceDisabledBrowserTest,
                       NotCreatedWhenDisabled) {
  EXPECT_EQ(TtcKeyedService::Get(profile()), nullptr);
}

}  // namespace ttc
