// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.location.Location;
import android.location.LocationManager;
import android.os.SystemClock;
import android.util.Base64;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;
import org.chromium.url.GURL;

/** Tests for GeolocationHeader and GeolocationTracker. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableFeatures({OmniboxFeatureList.PLATFORM_AGNOSTIC_X_GEO})
public class GeolocationHeaderTest {
    public @Rule AutoResetCtaTransitTestRule mAutoResetCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mCurrentWebPageStation;

    private static final String SEARCH_URL_1 = "https://www.google.com/search?q=potatoes";
    private static final String SEARCH_URL_2 = "https://www.google.co.jp/webhp?#q=dinosaurs";
    private static final String GOOGLE_BASE_URL_SWITCH = "google-base-url=https://www.google.com";
    private static final double LOCATION_LAT = 20.3;
    private static final double LOCATION_LONG = 155.8;
    private static final float LOCATION_ACCURACY = 20f;

    @Before
    public void setUp() {
        mCurrentWebPageStation = mAutoResetCtaTestRule.startOnBlankPage();
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        // With incognito windows, this test will create many windows so we need to increase the
        // ChromeTabbedActivity instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(1000);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @DisabledTest(message = "https://crbug.com/416787235")
    public void testProtoEncoding() {
        setPermission(ContentSetting.ALLOW);
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs using proto encoding.
        assertNonNullHeader(SEARCH_URL_1, false, now, /* isPrecise= */ true);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    public void testGeolocationHeaderPrimingEnabledPermissionAllow() {
        setPermission(ContentSetting.ALLOW);
        checkHeaderPriming(/* shouldPrimeHeader= */ true);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    public void testGeolocationHeaderPrimingDisabledPermissionBlock() {
        setPermission(ContentSetting.BLOCK);
        checkHeaderPriming(/* shouldPrimeHeader= */ false);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @DisabledTest(message = "Flaky. See crbug.com/392607758")
    public void testGeolocationHeaderPrimingDisabledPermissionAsk() {
        setPermission(ContentSetting.ASK);
        checkHeaderPriming(/* shouldPrimeHeader= */ false);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @RequiresRestart(value = "Needs to reset cached geolocation from previous tests")
    @DisabledTest(message = "Flaky. See crbug.com/392607758")
    public void testGeolocationHeaderPrimingDisabledOsPermissionBlocked() {
        setPermission(ContentSetting.ALLOW);
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        checkHeaderPriming(/* shouldPrimeHeader= */ false);
    }

    private void checkHeaderPriming(boolean shouldPrimeHeader) {
        openBlankPage(/* isIncognito= */ false);

        var omniboxTestUtils = new OmniboxTestUtils(mCurrentWebPageStation.getActivity());
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("aaaaaaaaaa", false);
        omniboxTestUtils.waitAnimationsComplete();
        Assert.assertEquals(shouldPrimeHeader, GeolocationHeader.isGeolocationPrimedForTesting());
    }

    private long setMockLocationNow() {
        long now = System.currentTimeMillis();
        setMockLocation(now);
        return now;
    }

    private Location generateMockLocation(String provider, long time) {
        Location location = new Location(provider);
        location.setLatitude(LOCATION_LAT);
        location.setLongitude(LOCATION_LONG);
        location.setAccuracy(LOCATION_ACCURACY);
        location.setTime(time);
        location.setElapsedRealtimeNanos(
                SystemClock.elapsedRealtimeNanos() + 1000000 * (time - System.currentTimeMillis()));
        return location;
    }

    private void setMockLocation(long time) {
        Location location = generateMockLocation(LocationManager.NETWORK_PROVIDER, time);
        GeolocationTracker.setLocationForTesting(location, null);
    }

    private void assertNonNullHeader(
            final String url,
            final boolean isIncognito,
            final long locationTime,
            boolean isPrecise) {
        openBlankPage(isIncognito);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var profile = mCurrentWebPageStation.getTab().getProfile();
                    var service = TemplateUrlServiceFactory.getForProfile(profile);
                    assertHeaderEquals(
                            locationTime,
                            GeolocationHeader.getGeoHeader(url, profile, service),
                            isPrecise);
                });
    }

    private void assertHeaderEquals(long locationTime, String header, boolean isPrecise) {
        long timestamp = locationTime * 1000;
        // Latitude times 1e7.
        int latitudeE7 = (int) (LOCATION_LAT * 10000000);
        // Longitude times 1e7.
        int longitudeE7 = (int) (LOCATION_LONG * 10000000);
        // Radius of 68% accuracy in mm.
        int radius = (int) (LOCATION_ACCURACY * 1000);

        // Create a LatLng for the coordinates.
        PartnerLocationDescriptor.LatLng latlng =
                PartnerLocationDescriptor.LatLng.newBuilder()
                        .setLatitudeE7(latitudeE7)
                        .setLongitudeE7(longitudeE7)
                        .build();

        // Populate a LocationDescriptor with the LatLng.
        PartnerLocationDescriptor.LocationDescriptor locationDescriptor =
                PartnerLocationDescriptor.LocationDescriptor.newBuilder()
                        .setLatlng(latlng)
                        // Include role, producer, timestamp and radius.
                        .setRole(PartnerLocationDescriptor.LocationRole.CURRENT_LOCATION)
                        .setProducer(PartnerLocationDescriptor.LocationProducer.DEVICE_LOCATION)
                        .setTimestamp(timestamp)
                        .setRadius((float) radius)
                        .setPermissionGranularity(
                                isPrecise
                                        ? PartnerLocationDescriptor.PermissionGranularity
                                                .PERMISSION_GRANULARITY_FINE
                                        : PartnerLocationDescriptor.PermissionGranularity
                                                .PERMISSION_GRANULARITY_COARSE)
                        .build();

        String locationProto =
                Base64.encodeToString(
                        locationDescriptor.toByteArray(), Base64.NO_WRAP | Base64.URL_SAFE);
        String expectedHeader = "X-Geo: w " + locationProto;
        Assert.assertEquals(expectedHeader, header);
    }

    private void setPermission(final @ContentSetting int setting) {
        setPermission(setting, setting, /* isOneTime= */ false);
    }

    private void setPermission(
            final @ContentSetting int approximate,
            final @ContentSetting int precise,
            boolean isOneTime) {
        final boolean approximateGelocationEnabled =
                PermissionsAndroidFeatureMap.isEnabled(
                        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION);
        PermissionInfo infoHttps =
                new PermissionInfo(
                        approximateGelocationEnabled
                                ? ContentSettingsType.GEOLOCATION_WITH_OPTIONS
                                : ContentSettingsType.GEOLOCATION,
                        SEARCH_URL_1,
                        /* embedder= */ null,
                        /* isEmbargoed= */ false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (isOneTime) {
                        if (approximateGelocationEnabled) {
                            WebsitePreferenceBridgeJni.get()
                                    .setGeolocationEphemeralGrantForTesting(
                                            ProfileManager.getLastUsedRegularProfile(),
                                            new GURL(SEARCH_URL_1),
                                            new GeolocationSetting(approximate, precise));
                        } else {
                            WebsitePreferenceBridgeJni.get()
                                    .setEphemeralGrantForTesting(
                                            ProfileManager.getLastUsedRegularProfile(),
                                            ContentSettingsType.GEOLOCATION,
                                            new GURL(SEARCH_URL_1),
                                            new GURL(SEARCH_URL_1));
                        }
                    } else {
                        if (approximateGelocationEnabled) {
                            infoHttps.setGeolocationSetting(
                                    ProfileManager.getLastUsedRegularProfile(),
                                    new GeolocationSetting(approximate, precise));
                        } else {
                            infoHttps.setContentSetting(
                                    ProfileManager.getLastUsedRegularProfile(), precise);
                        }
                    }
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    if (approximateGelocationEnabled) {
                        var expectedApproximate =
                                approximate == ContentSetting.DEFAULT
                                        ? ContentSetting.ASK
                                        : approximate;
                        var expectedPrecise =
                                precise == ContentSetting.DEFAULT ? ContentSetting.ASK : precise;
                        GeolocationSetting geolocationSetting =
                                infoHttps.getGeolocationSetting(
                                        ProfileManager.getLastUsedRegularProfile());
                        return geolocationSetting.mPrecise == expectedPrecise
                                && geolocationSetting.mApproximate == expectedApproximate;
                    } else {
                        var expectedSetting =
                                precise == ContentSetting.DEFAULT ? ContentSetting.ASK : precise;
                        Integer contentSetting =
                                infoHttps.getContentSetting(
                                        ProfileManager.getLastUsedRegularProfile());
                        return contentSetting == expectedSetting;
                    }
                });
    }

    private void openBlankPage(boolean isIncognito) {
        if (isIncognito) {
            mCurrentWebPageStation =
                    mCurrentWebPageStation.openNewIncognitoTabOrWindowFast().loadAboutBlank();
        } else {
            mCurrentWebPageStation =
                    mCurrentWebPageStation.openNewTabOrWindowFast().loadAboutBlank();
        }
    }
}
