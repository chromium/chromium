// Copyright 2015 The Chromium Authors. All rights reserved.
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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for GeolocationHeader and GeolocationTracker.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class GeolocationHeaderTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String SEARCH_URL_1 = "https://www.google.com/search?q=potatoes";
    private static final String SEARCH_URL_2 = "https://www.google.co.jp/webhp?#q=dinosaurs";
    private static final String DISABLE_FEATURES = "disable-features=";
    private static final String ENABLE_FEATURES = "enable-features=";
    private static final String GOOGLE_BASE_URL_SWITCH = "google-base-url=https://www.google.com";
    private static final double LOCATION_LAT = 20.3;
    private static final double LOCATION_LONG = 155.8;
    private static final float LOCATION_ACCURACY = 20f;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    @CommandLineFlags.Add({GOOGLE_BASE_URL_SWITCH})
    public void testConsistentHeader() {
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
    public void testPermission() {
        long now = setMockLocationNow();

        // X-Geo shouldn't be sent when location is disallowed for the origin.
        checkHeaderWithPermission(ContentSettingValues.ALLOW, now, false);
        checkHeaderWithPermission(ContentSettingValues.BLOCK, now, true);

        // The default permission for the DSE is to allow access, so the header
        // should be sent in this case.
        checkHeaderWithPermission(ContentSettingValues.DEFAULT, now, false);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    public void testProtoEncoding() {
        long now = setMockLocationNow();

        // X-Geo should be sent for Google search results page URLs using proto encoding.
        assertNonNullHeader(SEARCH_URL_1, false, now);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    public void testGpsFallback() {
        // Only GPS location, should be sent when flag is on.
        long now = System.currentTimeMillis();
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(null, gpsLocation);

        assertNonNullHeader(SEARCH_URL_1, false, now);
    }

    @Test
    @SmallTest
    @Feature({"Location"})
    public void testGpsFallbackYounger() {
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
        long now = System.currentTimeMillis();
        // GPS location is older.
        Location gpsLocation = generateMockLocation(LocationManager.GPS_PROVIDER, now - 100);
        // Network location is younger.
        Location netLocation = generateMockLocation(LocationManager.NETWORK_PROVIDER, now);
        GeolocationTracker.setLocationForTesting(netLocation, gpsLocation);

        // The younger (Network) should be used.
        assertNonNullHeader(SEARCH_URL_1, false, now);
    }

    private void checkHeaderWithPermission(final @ContentSettingValues int httpsPermission,
            final long locationTime, final boolean shouldBeNull) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionInfo infoHttps =
                    new PermissionInfo(ContentSettingsType.GEOLOCATION, SEARCH_URL_1, null, false);
            infoHttps.setContentSetting(Profile.getLastUsedRegularProfile(), httpsPermission);
            String header = GeolocationHeader.getGeoHeader(
                    SEARCH_URL_1, mActivityTestRule.getActivity().getActivityTab());
            assertHeaderState(header, locationTime, shouldBeNull);
        });
    }

    private void checkHeaderWithLocation(final long locationTime, final boolean shouldBeNull) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            setMockLocation(locationTime);
            String header = GeolocationHeader.getGeoHeader(
                    SEARCH_URL_1, mActivityTestRule.getActivity().getActivityTab());
            assertHeaderState(header, locationTime, shouldBeNull);
        });
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
        final Tab tab = mActivityTestRule.loadUrlInNewTab("about:blank", isIncognito);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNull(GeolocationHeader.getGeoHeader(url, tab)); });
    }

    private void assertNonNullHeader(
            final String url, final boolean isIncognito, final long locationTime) {
        final Tab tab = mActivityTestRule.loadUrlInNewTab("about:blank", isIncognito);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertHeaderEquals(locationTime, GeolocationHeader.getGeoHeader(url, tab));
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
        PartnerLocationDescriptor.LatLng latlng = PartnerLocationDescriptor.LatLng.newBuilder()
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

        String locationProto = Base64.encodeToString(
                locationDescriptor.toByteArray(), Base64.NO_WRAP | Base64.URL_SAFE);
        String expectedHeader = "X-Geo: w " + locationProto;
        Assert.assertEquals(expectedHeader, header);
    }
}
