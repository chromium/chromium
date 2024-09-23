// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.content_public.common.ContentSwitches;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** Tests for the PermissionInfoTest. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@Batch(SiteSettingsTest.SITE_SETTINGS_BATCH_NAME)
public class PermissionInfoTest {
    private static final String DSE_ORIGIN = "https://www.google.com";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Before
    public void setUp() throws TimeoutException {
        clearPermissions();
    }

    @AfterClass
    public static void tearDown() throws TimeoutException {
        clearPermissions();
    }

    private static void clearPermissions() throws TimeoutException {
        // Clean up cookies and permissions.
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(getRegularProfile())
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }

    private static Profile getRegularProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>) () -> ProfileManager.getLastUsedRegularProfile());
    }

    private static Profile getNonPrimaryOTRProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>)
                        () -> {
                            OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
                            return ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileID, /* createIfNeeded= */ true);
                        });
    }

    private static Profile getPrimaryOTRProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>)
                        () ->
                                ProfileManager.getLastUsedRegularProfile()
                                        .getPrimaryOTRProfile(/* createIfNeeded= */ true));
    }

    private void setSettingAndExpectValue(
            @ContentSettingsType.EnumType int type,
            String origin,
            String embedder,
            @ContentSettingValues int setting,
            Profile profile,
            @ContentSettingValues int expectedSetting) {
        PermissionInfo info =
                new PermissionInfo(
                        type, origin, embedder, /* isEmbargoed= */ false, SessionModel.DURABLE);

        ThreadUtils.runOnUiThreadBlocking(() -> info.setContentSetting(profile, setting));

        CriteriaHelper.pollUiThread(
                () -> {
                    return info.getContentSetting(profile) == expectedSetting;
                });
    }

    private void resetNotificationsSettingsForTest() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridgeJni.get()
                            .resetNotificationsSettingsForTest(
                                    ProfileManager.getLastUsedRegularProfile());
                });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation_InPrimaryOTRProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile primaryOTRProfile = getPrimaryOTRProfile();
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                primaryOTRProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                primaryOTRProfile,
                ContentSettingValues.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation_InNonPrimaryOTRProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile nonPrimaryOTRProfile = getNonPrimaryOTRProfile();
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                nonPrimaryOTRProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                nonPrimaryOTRProfile,
                ContentSettingValues.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation_RegularProfile_DefaultsToAskFromBlock() throws Throwable {
        Profile regularProfile = getRegularProfile();
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                regularProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                regularProfile,
                ContentSettingValues.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSENotification_InPrimaryOTRProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile primaryOTRProfile = getPrimaryOTRProfile();

        // Resetting in incognito should not have the same behavior.
        resetNotificationsSettingsForTest();
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                primaryOTRProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                primaryOTRProfile,
                ContentSettingValues.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSENotification_InNonPrimaryOTRProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile nonPrimaryOTRProfile = getNonPrimaryOTRProfile();

        // Resetting in incognito should not have the same behavior.
        resetNotificationsSettingsForTest();
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                nonPrimaryOTRProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                nonPrimaryOTRProfile,
                ContentSettingValues.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSENotification_RegularProfile_DefaultsToAskFromBlock() throws Throwable {
        Profile regularProfile = getRegularProfile();
        resetNotificationsSettingsForTest();
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                regularProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                regularProfile,
                ContentSettingValues.ASK);
    }
}
