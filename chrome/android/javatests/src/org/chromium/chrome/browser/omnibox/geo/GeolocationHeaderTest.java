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
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
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
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.components.permissions.PermissionsAndroidFeatureMap;

/** Tests for GeolocationHeader and GeolocationTracker. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
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
    @CommandLineFlags.Add({GOOGLE_BASE_URL_SWITCH})
    public void testConsistentHeader() {
        setPermission(ContentSetting.ALLOW);
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs.
        assertNonNullHeader(SEARCH_URL_1, false, now);

        // But only the current CCTLD.
        assertNullHeader(SEARCH_URL_2, false);

        // X-Geo shouldn't be sent in incognito mode.
        assertNullHeader(SEARCH_URL_1, true);
        assertNullHeader(SEARCH_URL_2, true);

        // X-Geo shouldn't be sent with URLs that aren't the Google search results page.
        assertNullHeader("invalid$url", false);
        assertNullHeader("https://www.chrome.fr/", false);
        assertNullHeader("https://www.google.com/", false);

        // X-Geo shouldn't be sent over HTTP.
        assertNullHeader("http://www.google.com/search?q=potatoes", false);
        assertNullHeader("http://www.google.com/webhp?#q=dinosaurs", false);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({GOOGLE_BASE_URL_SWITCH})
    public void testConsistentHeaderForOneTimeGrant() {
        setOneTimeGrant();
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs.
        assertNonNullHeader(SEARCH_URL_1, false, now);

        // But only the current CCTLD.
        assertNullHeader(SEARCH_URL_2, false);

        // X-Geo shouldn't be sent in incognito mode.
        assertNullHeader(SEARCH_URL_1, true);
        assertNullHeader(SEARCH_URL_2, true);

        // X-Geo shouldn't be sent with URLs that aren't the Google search results page.
        assertNullHeader("invalid$url", false);
        assertNullHeader("https://www.chrome.fr/", false);
        assertNullHeader("https://www.google.com/", false);

        // X-Geo shouldn't be sent over HTTP.
        assertNullHeader("http://www.google.com/search?q=potatoes", false);
        assertNullHeader("http://www.google.com/webhp?#q=dinosaurs", false);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({GOOGLE_BASE_URL_SWITCH})
    public void testPermissionWithoutAutogrant() {
        long now = setMockLocationNow();

        // X-Geo should be sent if DSE autogrant is enabled only if the user has explicitly allowed
        // geolocation.
        checkHeaderWithPermission(ContentSetting.ALLOW, now, false);
        checkHeaderWithPermission(ContentSetting.BLOCK, now, true);
        checkHeaderWithPermission(ContentSetting.DEFAULT, now, true);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @DisabledTest(message = "https://crbug.com/416787235")
    public void testProtoEncoding() {
        setPermission(ContentSetting.ALLOW);
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs using proto encoding.
        assertNonNullHeader(SEARCH_URL_1, false, now);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/421965472")
    public void testGpsFallback() {
        setPermission(ContentSetting.ALLOW);
        // Only GPS location, should be sent when flag is on.
        long now = System.currentTimeMillis();
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(null, gpsLocation);

        assertNonNullHeader(SEARCH_URL_1, false, now);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @DisabledTest(message = "https://crbug.com/414769376")
    public void testGpsFallbackYounger() {
        setPermission(ContentSetting.ALLOW);
        long now = System.currentTimeMillis();
        // GPS location is younger.
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now + 100);
        // Network location is older
        Location netLocation = generateMockLocation(LocationManager.NETWORK_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(netLocation, gpsLocation);

        // The younger (GPS) should be used.
        assertNonNullHeader(SEARCH_URL_1, false, now + 100);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    public void testGpsFallbackOlder() {
        setPermission(ContentSetting.ALLOW);
        long now = System.currentTimeMillis();
        // GPS location is older.
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now - 100);
        // Network location is younger.
        Location netLocation = generateMockLocation(LocationManager.NETWORK_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(netLocation, gpsLocation);

        // The younger (Network) should be used.
        assertNonNullHeader(SEARCH_URL_1, false, now);
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

    private void checkHeaderWithPermission(
            final @ContentSetting int httpsPermission,
            final long locationTime,
            final boolean shouldBeNull) {
        setPermission(httpsPermission);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var profile = mCurrentWebPageStation.getTab().getProfile();
                    var service = TemplateUrlServiceFactory.getForProfile(profile);
                    String header = GeolocationHeader.getGeoHeader(SEARCH_URL_1, profile, service);
                    assertHeaderState(header, locationTime, shouldBeNull);
                });
    }

    private void checkHeaderPriming(boolean shouldPrimeHeader) {
        openBlankPage(/* isIncognito= */ false);

        var omniboxTestUtils = new OmniboxTestUtils(mCurrentWebPageStation.getActivity());
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("aaaaaaaaaa", false);
        omniboxTestUtils.waitAnimationsComplete();
        Assert.assertEquals(shouldPrimeHeader, GeolocationHeader.isGeolocationPrimedForTesting());
    }

    private void assertHeaderState(String header, long locationTime, boolean shouldBeNull) {
        if (shouldBeNull) {
            Assert.assertNull(header);
        } else {
            assertHeaderEquals(locationTime, header);
        }
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

    private void assertNullHeader(final String url, final boolean isIncognito) {
        openBlankPage(isIncognito);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var profile = mCurrentWebPageStation.getTab().getProfile();
                    var service = TemplateUrlServiceFactory.getForProfile(profile);
                    Assert.assertNull(GeolocationHeader.getGeoHeader(url, profile, service));
                });
    }

    private void assertNonNullHeader(
            final String url, final boolean isIncognito, final long locationTime) {
        openBlankPage(isIncognito);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var profile = mCurrentWebPageStation.getTab().getProfile();
                    var service = TemplateUrlServiceFactory.getForProfile(profile);
                    assertHeaderEquals(
                            locationTime, GeolocationHeader.getGeoHeader(url, profile, service));
                });
    }

    private void assertHeaderEquals(long locationTime, String header) {
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
                        .build();

        String locationProto =
                Base64.encodeToString(
                        locationDescriptor.toByteArray(), Base64.NO_WRAP | Base64.URL_SAFE);
        String expectedHeader = "X-Geo: w " + locationProto;
        Assert.assertEquals(expectedHeader, header);
    }

    private void setPermission(final @ContentSetting int setting) {
        setPermission(setting, SessionModel.DURABLE);
    }

    private void setOneTimeGrant() {
        setPermission(ContentSetting.ALLOW, SessionModel.ONE_TIME);
    }

    private void setPermission(
            final @ContentSetting int setting, @SessionModel.EnumType int sessionModel) {
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
                        /* isEmbargoed= */ false,
                        sessionModel);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (approximateGelocationEnabled) {
                        infoHttps.setGeolocationSetting(
                                ProfileManager.getLastUsedRegularProfile(),
                                new GeolocationSetting(setting, setting));
                    } else {
                        infoHttps.setContentSetting(
                                ProfileManager.getLastUsedRegularProfile(), setting);
                    }
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    var expectedSetting =
                            setting == ContentSetting.DEFAULT ? ContentSetting.ASK : setting;
                    if (approximateGelocationEnabled) {
                        GeolocationSetting geolocationSetting =
                                infoHttps.getGeolocationSetting(
                                        ProfileManager.getLastUsedRegularProfile());
                        return geolocationSetting.mPrecise == expectedSetting;
                    } else {
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
