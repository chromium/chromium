// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.LocationPermissionOptionsPreference;
import org.chromium.components.browser_ui.site_settings.LocationPermissionSubpageSettings;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;

/** Tests for {@link LocationPermissionSubpageSettings}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION})
public class LocationPermissionSubpageSettingsTest {
    private static final String EXAMPLE_ADDRESS = "https://example.com";

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule
    public SettingsActivityTestRule<LocationPermissionSubpageSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(LocationPermissionSubpageSettings.class);

    private SettingsActivity mSettingsActivity;
    private Website mWebsite;

    @Before
    public void setUp() {
        GeolocationSetting allowApproximateSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK);
        mWebsite = createWebsiteWithGeolocationPermission(allowApproximateSetting);
        assertEquals(allowApproximateSetting, getGeolocationSetting(mWebsite));

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsiteSettings.EXTRA_SITE, mWebsite);
        mSettingsActivity = mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);
    }

    @Test
    @SmallTest
    public void selectionChangesShouldUpdatePermission() {
        GeolocationSetting allowPreciseSetting =
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);

        // Initially the Approximate button should be selected.
        LocationPermissionOptionsPreference preference =
                mSettingsActivityTestRule
                        .getFragment()
                        .findPreference(LocationPermissionSubpageSettings.RADIO_BUTTON_GROUP_KEY);
        assertTrue(preference.getApproximateButtonForTesting().isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    preference.getPreciseButtonForTesting().performClick();
                });
        assertTrue(preference.getPreciseButtonForTesting().isChecked());
        assertEquals(allowPreciseSetting, getGeolocationSetting(mWebsite));
    }

    private static Website createWebsiteWithGeolocationPermission(GeolocationSetting setting) {
        WebsiteAddress address = WebsiteAddress.create(EXAMPLE_ADDRESS);
        Website website = new Website(address, address);
        PermissionInfo info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                        website.getAddress().getOrigin(),
                        website.getAddress().getOrigin(),
                        /* isEmbargoed= */ false,
                        SessionModel.DURABLE);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        info.setGeolocationSetting(
                                ProfileManager.getLastUsedRegularProfile(), setting));
        website.setPermissionInfo(info);
        return website;
    }

    private static GeolocationSetting getGeolocationSetting(Website website) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        website.getPermissionInfo(ContentSettingsType.GEOLOCATION_WITH_OPTIONS)
                                .getGeolocationSetting(ProfileManager.getLastUsedRegularProfile()));
    }
}
