// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/geolocation_navigation_throttle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/browser/geolocation_header_service_test_api.h"
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
    data.send_x_geo_header = true;
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

  // Synchronizes the mock handle's internal header map when SetRequestHeader is
  // called, ensuring that GetRequestHeaders() reflects those changes.
  void SetMockHandleHeaders(content::MockNavigationHandle& handle) {
    ON_CALL(handle, SetRequestHeader(testing::_, testing::_))
        .WillByDefault(
            [&handle](std::string_view name, std::string_view value) {
              net::HttpRequestHeaders headers = handle.GetRequestHeaders();
              headers.SetHeader(name, value);
              handle.set_request_headers(headers);
            });
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
  base::HistogramTester histogram_tester_;
};

// Tests that when the kPlatformAgnosticXGeo feature flag is disabled, the
// throttle is still created (to enable eligibility tracking) but does not
// attach any location header. Also test that the metric
// `Omnibox.Search.XGeoHeaderAttached` is recorded as `false`.
TEST_F(GeolocationNavigationThrottleTest, FeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(omnibox::kPlatformAgnosticXGeo);

  content::MockNavigationHandle handle(
      GURL("https://www.google.com/search?q=test"), main_rfh());
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);

#if BUILDFLAG(IS_ANDROID)
  ASSERT_TRUE(throttle);

  EXPECT_CALL(handle, SetRequestHeader(testing::_, testing::_)).Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());

  histogram_tester_.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                       false, 1);
#else
  ASSERT_FALSE(throttle);
#endif
}

// Tests that the GeolocationHeaderService (and therefore the navigation
// throttle) is never instantiated or created for incognito/off-the-record
// profiles.
TEST_F(GeolocationNavigationThrottleTest, FeatureDisabledForIncognito) {
  content::MockNavigationHandle handle;
  handle.set_url(GURL("https://www.google.com/"));

  // Use an off-the-record profile.
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // The MockNavigationHandle doesn't automatically wire to a web contents in
  // this barebones registry test. We'll verify that WillStartRequest returns
  // PROCEED but doesn't set the header because the service won't be available.
  // Instead, the test for Incognito behavior is simpler: The KeyedService
  // factory should return nullptr for incognito profiles. Because the throttle
  // creation helper requires the keyed service to exist to perform eligibility
  // checks, this null check guarantees the throttle will never be created in
  // OTR contexts.
  auto* service =
      GeolocationHeaderServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(service);
}

// Tests that the throttle is created even for non-DSE URLs (to handle
// redirects), but does not attach headers or record metrics until it
// redirects to a DSE URL.
TEST_F(GeolocationNavigationThrottleTest, RedirectFromNonDseToDse) {
  SetGeolocationPermission(CONTENT_SETTING_ALLOW);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  GeolocationHeaderServiceTestApi(service).SetLocationAge(base::Minutes(1));
  service->PrimeLocation();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  content::MockNavigationHandle handle(GURL("https://www.example.com/"),
                                       main_rfh());
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  SetMockHandleHeaders(handle);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  ASSERT_TRUE(throttle);

  // Initial navigation to non-DSE should proceed without header.
  EXPECT_CALL(handle, SetRequestHeader(testing::_, testing::_)).Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  histogram_tester_.ExpectTotalCount("Omnibox.Search.XGeoHeaderAttached", 0);

  // Redirect to DSE URL should attach the header.
  handle.set_url(GURL("https://www.google.com/search?q=test"));
  EXPECT_CALL(handle, WasServerRedirect())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(handle, SetRequestHeader(testing::Eq("X-Geo"),
                                       testing::StartsWith("w ")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  histogram_tester_.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                       true, 1);
}

// Tests that if we redirect from a DSE URL to a non-DSE URL, the X-Geo header
// is removed.
TEST_F(GeolocationNavigationThrottleTest, RedirectFromDseToNonDse) {
  SetGeolocationPermission(CONTENT_SETTING_ALLOW);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  GeolocationHeaderServiceTestApi(service).SetLocationAge(base::Minutes(1));
  service->PrimeLocation();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  content::MockNavigationHandle handle(
      GURL("https://www.google.com/search?q=test"), main_rfh());
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  SetMockHandleHeaders(handle);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  ASSERT_TRUE(throttle);

  // Initial navigation to DSE should attach header.
  EXPECT_CALL(handle, SetRequestHeader(testing::Eq("X-Geo"),
                                       testing::StartsWith("w ")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  histogram_tester_.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                       true, 1);

  // Redirect to non-DSE URL should remove the header.
  handle.set_url(GURL("https://www.example.com/"));
  EXPECT_CALL(handle, WasServerRedirect())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(handle, RemoveRequestHeader(testing::Eq("X-Geo")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());

  // We do not record UMA on non-eligible redirect destination.
  histogram_tester_.ExpectTotalCount("Omnibox.Search.XGeoHeaderAttached", 1);
}

// Tests that when the platform-agnostic feature flag is enabled, location
// permission is allowed, and the navigation is a DSE search from the address
// bar, the X-Geo header is successfully appended and the metric
// `Omnibox.Search.XGeoHeaderAttached` is recorded as `true`.
TEST_F(GeolocationNavigationThrottleTest, HeaderSentForAllowedDse) {
  SetGeolocationPermission(CONTENT_SETTING_ALLOW);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  GeolocationHeaderServiceTestApi(service).SetLocationAge(base::Minutes(1));
  service->PrimeLocation();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service->HasCachedLocation(); }));

  content::MockNavigationHandle handle(
      GURL("https://www.google.com/search?q=test"), main_rfh());
  handle.set_page_transition(static_cast<ui::PageTransition>(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  SetMockHandleHeaders(handle);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle =
      GeolocationNavigationThrottle::MaybeCreateThrottleFor(registry);
  ASSERT_TRUE(throttle);

  EXPECT_CALL(handle, SetRequestHeader(testing::Eq("X-Geo"),
                                       testing::StartsWith("w ")));
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());

  histogram_tester_.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                       true, 1);
}

// Tests that when geolocation permission is blocked, no location header is
// attached to DSE searches, and the metric `Omnibox.Search.XGeoHeaderAttached`
// is recorded as `false`.
TEST_F(GeolocationNavigationThrottleTest, HeaderNotSentForDeniedDse) {
  SetGeolocationPermission(CONTENT_SETTING_BLOCK);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  GeolocationHeaderServiceTestApi(service).SetLocationAge(base::Minutes(1));
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

  histogram_tester_.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                       false, 1);
}

// Tests that navigations that do not originate from the omnibox / address bar
// (e.g. regular link clicks) do not have location headers attached and do not
// emit telemetry metrics.
TEST_F(GeolocationNavigationThrottleTest,
       HeaderNotSentForNonAddressBarNavigations) {
  SetGeolocationPermission(CONTENT_SETTING_ALLOW);

  auto* service = GeolocationHeaderServiceFactory::GetForProfile(profile());
  GeolocationHeaderServiceTestApi(service).SetLocationAge(base::Minutes(1));
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

  // Page transition not from address bar, so ProcessNavigation returns early
  // before checking eligibility.
  histogram_tester_.ExpectTotalCount("Omnibox.Search.XGeoHeaderAttached", 0);
}
