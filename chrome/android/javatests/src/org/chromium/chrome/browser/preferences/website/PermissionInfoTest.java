// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/** Tests for the PermissionInfoTest. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PermissionInfoTest {
    private static final String DSE_ORIGIN = "https://www.google.com";
    private static final String OTHER_ORIGIN = "https://www.other.com";

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private void setGeolocation(
            String origin, String embedder, @ContentSettingValues int setting, boolean incognito) {
        PermissionInfo info =
                new PermissionInfo(PermissionInfo.Type.GEOLOCATION, origin, embedder, incognito);
        TestThreadUtils.runOnUiThreadBlocking(() -> info.setContentSetting(setting));
    }

    private @ContentSettingValues int getGeolocation(
            String origin, String embedder, boolean incognito) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionInfo info = new PermissionInfo(
                    PermissionInfo.Type.GEOLOCATION, origin, embedder, incognito);
            return info.getContentSetting();
        });
    }

    private void setNotifications(
            String origin, String embedder, @ContentSettingValues int setting, boolean incognito) {
        PermissionInfo info =
                new PermissionInfo(PermissionInfo.Type.NOTIFICATION, origin, embedder, incognito);
        TestThreadUtils.runOnUiThreadBlocking(() -> info.setContentSetting(setting));
    }

    private @ContentSettingValues int getNotifications(
            String origin, String embedder, boolean incognito) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionInfo info = new PermissionInfo(
                    PermissionInfo.Type.NOTIFICATION, origin, embedder, incognito);
            return info.getContentSetting();
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
    @DisableFeatures(ChromeFeatureList.PERMISSION_DELEGATION)
    public void testResetDSEGeolocationEmbeddedOrigin() throws Throwable {
        // It's not possible to set a permission for an embedded origin when permission delegation
        // is enabled. This code can be deleted when the feature is enabled by default.
        // Resetting an embedded DSE origin should not have the same behavior.
        boolean incognito = false;
        setGeolocation(DSE_ORIGIN, OTHER_ORIGIN, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getGeolocation(DSE_ORIGIN, OTHER_ORIGIN, incognito));
        setGeolocation(DSE_ORIGIN, OTHER_ORIGIN, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ASK, getGeolocation(DSE_ORIGIN, OTHER_ORIGIN, incognito));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(ChromeFeatureList.GRANT_NOTIFICATIONS_TO_DSE)
    public void testResetDSENotifications() throws Throwable {
        // On Android O+ we need to clear notification channels so they don't interfere with the
        // test.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> WebsitePreferenceBridgeJni.get().resetNotificationsSettingsForTest());

        // Resetting the DSE notifications permission should change it to ALLOW.
        boolean incognito = false;
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getNotifications(DSE_ORIGIN, null, incognito));
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ALLOW, getNotifications(DSE_ORIGIN, null, incognito));

        // Resetting in incognito should not have the same behavior.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> WebsitePreferenceBridgeJni.get().resetNotificationsSettingsForTest());
        incognito = true;
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getNotifications(DSE_ORIGIN, null, incognito));
        setNotifications(DSE_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ASK, getNotifications(DSE_ORIGIN, null, incognito));

        // // Resetting a different top level origin should not have the same behavior
        TestThreadUtils.runOnUiThreadBlocking(
                () -> WebsitePreferenceBridgeJni.get().resetNotificationsSettingsForTest());
        incognito = false;
        setNotifications(OTHER_ORIGIN, null, ContentSettingValues.BLOCK, incognito);
        Assert.assertEquals(
                ContentSettingValues.BLOCK, getNotifications(OTHER_ORIGIN, null, incognito));
        setNotifications(OTHER_ORIGIN, null, ContentSettingValues.DEFAULT, incognito);
        Assert.assertEquals(
                ContentSettingValues.ASK, getNotifications(OTHER_ORIGIN, null, incognito));
    }
}
