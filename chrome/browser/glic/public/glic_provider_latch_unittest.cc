// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_provider_latch.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch_factory.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

constexpr char kGuestUrl[] = "https://business.gemini.google/side-panel";

// Creates every kind of profile with GEiC enabled and a guest URL configured
// before the profiles are created.
class GlicProviderLatchTest : public testing::Test {
 public:
  GlicProviderLatchTest() {
    feature_list_.InitAndEnableFeature(features::kGeic);
    // Spelled out rather than using `geic::kGeicGuestURLSwitch`: this target
    // can't include geic_enabling.h (see chrome/browser/glic/DEPS).
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        "geic-guest-url", kGuestUrl);
  }

  void SetUp() override { helper_.SetUp(); }

 protected:
  void ExpectLatchedAsGeminiEnterprise(Profile* profile) {
    ASSERT_TRUE(profile);
    EXPECT_EQ(GlicProviderLatch::GetProvider(*profile),
              GlicProvider::kGeminiEnterprise);
    EXPECT_EQ(GetGlicProviderSpec(profile).GetGuestUrl(profile),
              GetLocalizedGuestURL(GURL(kGuestUrl)));
  }

  void ExpectNotGeicLatched(Profile* profile) {
    ASSERT_TRUE(profile);
    KeyedServiceBaseFactory* factory = GlicProviderLatchFactory::GetInstance();
    factory->CreateServiceNowForTesting(profile);
    EXPECT_FALSE(factory->IsServiceCreated(profile));
    EXPECT_EQ(GlicProviderLatch::GetProvider(*profile), GlicProvider::kGemini);
    EXPECT_EQ(GetGlicProviderSpec(profile).GetSettingsSection(),
              GlicSettingsSection::kGemini);
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  ProfileTestingHelper helper_;
};

TEST_F(GlicProviderLatchTest, RegularProfileIsLatchedAsGeminiEnterprise) {
  ExpectLatchedAsGeminiEnterprise(helper_.regular_profile());
}

TEST_F(GlicProviderLatchTest, IncognitoProfileIsNotLatched) {
  ExpectNotGeicLatched(helper_.incognito_profile());
}

TEST_F(GlicProviderLatchTest, IsolatedModeProfileIsNotLatched) {
  ExpectNotGeicLatched(helper_.isolated_mode_profile());
}

TEST_F(GlicProviderLatchTest, GuestProfilesAreNotLatched) {
  ExpectNotGeicLatched(helper_.guest_profile());
  ExpectNotGeicLatched(helper_.guest_profile_otr());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(GlicProviderLatchTest, SystemProfilesAreNotLatched) {
  ExpectNotGeicLatched(helper_.system_profile());
  ExpectNotGeicLatched(helper_.system_profile_otr());
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(GlicProviderLatchTest, AshInternalProfilesAreNotLatched) {
  ExpectNotGeicLatched(helper_.signin_profile());
  ExpectNotGeicLatched(helper_.signin_profile_otr());
  ExpectNotGeicLatched(helper_.lockscreen_profile());
  ExpectNotGeicLatched(helper_.lockscreen_profile_otr());
  ExpectNotGeicLatched(helper_.shimless_rma_app_profile());
  ExpectNotGeicLatched(helper_.shimless_rma_app_profile_otr());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(GlicProviderLatchTest, LatchDoesNotChangeWithConfiguration) {
  Profile* profile = helper_.regular_profile();
  profile->GetPrefs()->SetString(
      prefs::kGlicGeminiEnterpriseUrl,
      "https://business.gemini.google/home/cid/updated");
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch("geic-guest-url");

  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile),
            GlicProvider::kGeminiEnterprise);
  EXPECT_EQ(GetGlicProviderSpec(profile).GetGuestUrl(profile),
            GetLocalizedGuestURL(GURL(kGuestUrl)));
}

// A test latch set before the profile's keyed services are created replaces
// the value the factory would resolve from the GEiC configuration.
class GlicProviderLatchSetForTestingTest : public GlicProviderLatchTest {
 public:
  void SetUp() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  GlicProviderLatchFactory::SetProviderForTesting(
                      context, GlicProvider::kGemini);
                }));
    GlicProviderLatchTest::SetUp();
  }

 private:
  base::CallbackListSubscription create_services_subscription_;
};

TEST_F(GlicProviderLatchSetForTestingTest, OverridesResolvedLatch) {
  // GEiC is configured, so the factory would latch Gemini Enterprise.
  Profile* profile = helper_.regular_profile();
  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile), GlicProvider::kGemini);
  EXPECT_NE(GetGlicProviderSpec(profile).GetGuestUrl(profile),
            GetLocalizedGuestURL(GURL(kGuestUrl)));
}

// Without GEiC configuration, profiles are latched as Gemini.
TEST(GlicProviderLatchDefaultTest, ProfileIsLatchedAsGemini) {
  ProfileTestingHelper helper;
  helper.SetUp();
  Profile* profile = helper.regular_profile();

  EXPECT_EQ(GlicProviderLatch::GetProvider(*profile), GlicProvider::kGemini);
  EXPECT_NE(GetGlicProviderSpec(profile).GetGuestUrl(profile),
            GetLocalizedGuestURL(GURL(kGuestUrl)));
}

}  // namespace
}  // namespace glic
