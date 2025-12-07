// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.PermissionInfo;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
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
@Batch(Batch.PER_CLASS)
public class PermissionInfoTest {
    private static final String DSE_ORIGIN = "https://www.google.com";

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Before
    public void setUp() throws TimeoutException {
        mActivityTestRule.startOnBlankPage();
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

    private static Profile getNonPrimaryOtrProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>)
                        () -> {
                            OtrProfileId otrProfileId = OtrProfileId.createUnique("CCT:Incognito");
                            return ProfileManager.getLastUsedRegularProfile()
                                    .getOffTheRecordProfile(
                                            otrProfileId, /* createIfNeeded= */ true);
                        });
    }

    private static Profile getPrimaryOtrProfile() {
        return ThreadUtils.runOnUiThreadBlocking(
                (Callable<Profile>)
                        () ->
                                ProfileManager.getLastUsedRegularProfile()
                                        .getPrimaryOtrProfile(/* createIfNeeded= */ true));
    }

    private void setSettingAndExpectValue(
            @ContentSettingsType.EnumType int type,
            String origin,
            String embedder,
            @ContentSetting int setting,
            Profile profile,
            @ContentSetting int expectedSetting) {
        PermissionInfo info =
                new PermissionInfo(
                        type, origin, embedder, /* isEmbargoed= */ false, SessionModel.DURABLE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                        info.setGeolocationSetting(
                                profile, new GeolocationSetting(setting, setting));
                    } else {
                        info.setContentSetting(profile, setting);
                    }
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    if (type == ContentSettingsType.GEOLOCATION_WITH_OPTIONS) {
                        return info.getGeolocationSetting(profile).mPrecise == expectedSetting;
                    } else {
                        return info.getContentSetting(profile) == expectedSetting;
                    }
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
    public void testResetDSEGeolocation_InPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile primaryOtrProfile = getPrimaryOtrProfile();
        setSettingAndExpectValue(
                getGeolocationType(),
                DSE_ORIGIN,
                null,
                ContentSetting.BLOCK,
                primaryOtrProfile,
                ContentSetting.BLOCK);
        setSettingAndExpectValue(
                getGeolocationType(),
                DSE_ORIGIN,
                null,
                ContentSetting.DEFAULT,
                primaryOtrProfile,
                ContentSetting.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation_InNonPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile nonPrimaryOtrProfile = getNonPrimaryOtrProfile();
        setSettingAndExpectValue(
                getGeolocationType(),
                DSE_ORIGIN,
                null,
                ContentSetting.BLOCK,
                nonPrimaryOtrProfile,
                ContentSetting.BLOCK);
        setSettingAndExpectValue(
                getGeolocationType(),
                DSE_ORIGIN,
                null,
                ContentSetting.DEFAULT,
                nonPrimaryOtrProfile,
                ContentSetting.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation_RegularProfile_DefaultsToAskFromBlock() throws Throwable {
        Profile regularProfile = getRegularProfile();
        setSettingAndExpectValue(
                getGeolocationType(),
                DSE_ORIGIN,
                null,
                ContentSetting.BLOCK,
                regularProfile,
                ContentSetting.BLOCK);
        setSettingAndExpectValue(
                getGeolocationType(),
                DSE_ORIGIN,
                null,
                ContentSetting.DEFAULT,
                regularProfile,
                ContentSetting.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSENotification_InPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile primaryOtrProfile = getPrimaryOtrProfile();

        // Resetting in incognito should not have the same behavior.
        resetNotificationsSettingsForTest();
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSetting.BLOCK,
                primaryOtrProfile,
                ContentSetting.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSetting.DEFAULT,
                primaryOtrProfile,
                ContentSetting.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSENotification_InNonPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile nonPrimaryOtrProfile = getNonPrimaryOtrProfile();

        // Resetting in incognito should not have the same behavior.
        resetNotificationsSettingsForTest();
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSetting.BLOCK,
                nonPrimaryOtrProfile,
                ContentSetting.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSetting.DEFAULT,
                nonPrimaryOtrProfile,
                ContentSetting.ASK);
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
                ContentSetting.BLOCK,
                regularProfile,
                ContentSetting.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSetting.DEFAULT,
                regularProfile,
                ContentSetting.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testGeolocationSetting() throws Throwable {
        Profile regularProfile = getRegularProfile();
        var info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                        "https://example.com",
                        "https://example.com",
                        false,
                        SessionModel.DURABLE);

        var defaultSetting = new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);
        var allowApproximate = new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(defaultSetting, info.getGeolocationSetting(regularProfile));
                    info.setGeolocationSetting(regularProfile, allowApproximate);
                    assertEquals(allowApproximate, info.getGeolocationSetting(regularProfile));
                });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testGeolocationPermissionDefault() throws Throwable {
        Profile regularProfile = getRegularProfile();
        var info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                        "https://permission.site",
                        "https://permission.site",
                        false,
                        SessionModel.DURABLE);

        var defaultSetting = new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK);

        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(defaultSetting, info.getGeolocationSetting(regularProfile)));
    }
}
