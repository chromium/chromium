// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/geolocation_navigation_throttle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

class GeolocationNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  GeolocationNavigationThrottleTest() {
    scoped_feature_list_.InitAndEnableFeature(omnibox::kPlatformAgnosticXGeo);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Setup DSE to point to Google
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetShortName(u"Test");
    data.SetKeyword(u"test");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void SetGeolocationPermission(ContentSetting setting) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    map->SetContentSettingDefaultScope(
        GURL("https://www.google.com/"), GURL("https://www.google.com/"),
        ContentSettingsType::GEOLOCATION, setting);
  }

 protected:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    TestingProfile::TestingFactories factories;
    factories.emplace_back(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    return factories;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  device::ScopedGeolocationOverrider geolocation_overrider_{20.3, 155.8};
};

TEST_F(GeolocationNavigationThrottleTest, FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(omnibox::kPlatformAgnosticXGeo);

  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://www.google.com/"));
  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  EXPECT_FALSE(throttle);
}

TEST_F(GeolocationNavigationThrottleTest, FeatureDisabledForIncognito) {
  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://www.google.com/"));

  // Use an off-the-record profile.
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // The MockNavigationHandle doesn't automatically wire to a web contents in
  // this barebones registry test. We'll verify that WillStartRequest returns
  // PROCEED but doesn't set the header because the service won't be available.
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));

  // We need the handle to return the incognito web contents if possible.
  // Instead, the test for Incognito behavior is simpler: The KeyedService
  // factory should return nullptr for incognito profiles.
  auto* service =
      GeolocationHeaderServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(service);
}

TEST_F(GeolocationNavigationThrottleTest, MainFrameCreated) {
  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://www.google.com/"));
  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  EXPECT_TRUE(throttle);
}

TEST_F(GeolocationNavigationThrottleTest, HeaderSentForAllowedDse) {
  SetGeolocationPermission(CONTENT_SETTING_ALLOW);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  service->SetLocationAgeForTesting(base::Minutes(1));
  service->PrimeLocation();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  content::MockNavigationHandle handle(
      GURL("https://www.google.com/search?q=test"), main_rfh());
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  ASSERT_TRUE(throttle);

  EXPECT_CALL(handle, SetRequestHeader(testing::Eq("X-Geo"),
                                       testing::StartsWith("w ")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
}

TEST_F(GeolocationNavigationThrottleTest, HeaderNotSentForDeniedDse) {
  SetGeolocationPermission(CONTENT_SETTING_BLOCK);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  service->SetLocationAgeForTesting(base::Minutes(1));
  service->PrimeLocation();
  EXPECT_FALSE(service->HasCachedLocation());

  content::MockNavigationHandle handle(
      GURL("https://www.google.com/search?q=test"), main_rfh());
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  ASSERT_TRUE(throttle);

  EXPECT_CALL(handle, SetRequestHeader(testing::_, testing::_)).Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
}

TEST_F(GeolocationNavigationThrottleTest,
       HeaderNotSentForNonAddressBarNavigations) {
  SetGeolocationPermission(CONTENT_SETTING_ALLOW);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  service->SetLocationAgeForTesting(base::Minutes(1));
  service->PrimeLocation();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  content::MockNavigationHandle handle(
      GURL("https://www.google.com/search?q=test"), main_rfh());
  handle.set_page_transition(ui::PAGE_TRANSITION_LINK);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  ASSERT_TRUE(throttle);

  EXPECT_CALL(handle, SetRequestHeader(testing::_, testing::_)).Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
}
