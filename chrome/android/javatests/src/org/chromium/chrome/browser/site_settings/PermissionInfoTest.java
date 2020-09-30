// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/** Tests for the PermissionInfoTest. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PermissionInfoTest {
    private static final String DSE_ORIGIN = "https://www.google.com";
    private static final String OTHER_ORIGIN = "https://www.other.com";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private Profile getProfile(boolean incognito) {
        return incognito ? Profile.getLastUsedRegularProfile().getOffTheRecordProfile()
                         : Profile.getLastUsedRegularProfile();
    }

    private void setGeolocation(
            String origin, String embedder, @ContentSettingValues int setting, boolean incognito) {
        PermissionInfo info =
                new PermissionInfo(ContentSettingsType.GEOLOCATION, origin, embedder, incognito);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> info.setContentSetting(getProfile(incognito), setting));
    }

    private @ContentSettingValues int getGeolocation(
            String origin, String embedder, boolean incognito) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionInfo info = new PermissionInfo(
                    ContentSettingsType.GEOLOCATION, origin, embedder, incognito);
            return info.getContentSetting(getProfile(incognito));
        });
    }

    private void setNotifications(
            String origin, String embedder, @ContentSettingValues int setting, boolean incognito) {
        PermissionInfo info =
                new PermissionInfo(ContentSettingsType.NOTIFICATIONS, origin, embedder, incognito);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> info.setContentSetting(getProfile(incognito), setting));
    }

    private @ContentSettingValues int getNotifications(
            String origin, String embedder, boolean incognito) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionInfo info = new PermissionInfo(
                    ContentSettingsType.NOTIFICATIONS, origin, embedder, incognito);
            return info.getContentSetting(getProfile(incognito));
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation() throws Throwable {
        // Resetting the DSE geolocation permission should change it to ALLOW.
        boolean incognito = false;
        setGeolocation(DSE_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getGeolocation(DSE_ORIGIN, null, incognito));
        setGeolocation(DSE_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ALLOW, getGeolocation(DSE_ORIGIN, null, incognito));

        // Resetting in incognito should not have the same behavior.
        incognito = true;
        setGeolocation(DSE_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getGeolocation(DSE_ORIGIN, null, incognito));
        setGeolocation(DSE_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(ContentSettingValues.ASK, getGeolocation(DSE_ORIGIN, null, incognito));

        // Resetting a different top level origin should not have the same behavior
        incognito = false;
        setGeolocation(OTHER_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getGeolocation(OTHER_ORIGIN, null, incognito));
        setGeolocation(OTHER_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ASK, getGeolocation(OTHER_ORIGIN, null, incognito));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.GRANT_NOTIFICATIONS_TO_DSE)
    public void testResetDSENotifications() throws Throwable {
        // On Android O+ we need to clear notification channels so they don't interfere with the
        // test.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridgeJni.get().resetNotificationsSettingsForTest(
                    Profile.getLastUsedRegularProfile());
        });

        // Resetting the DSE notifications permission should change it to ALLOW.
        boolean incognito = false;
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getNotifications(DSE_ORIGIN, null, incognito));
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ALLOW, getNotifications(DSE_ORIGIN, null, incognito));

        // Resetting in incognito should not have the same behavior.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridgeJni.get().resetNotificationsSettingsForTest(
                    Profile.getLastUsedRegularProfile());
        });
        incognito = true;
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getNotifications(DSE_ORIGIN, null, incognito));
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ASK, getNotifications(DSE_ORIGIN, null, incognito));

        // // Resetting a different top level origin should not have the same behavior
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridgeJni.get().resetNotificationsSettingsForTest(
                    Profile.getLastUsedRegularProfile());
        });
        incognito = false;
        setNotifications(OTHER_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getNotifications(OTHER_ORIGIN, null, incognito));
        setNotifications(OTHER_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ASK, getNotifications(OTHER_ORIGIN, null, incognito));
    }
}
