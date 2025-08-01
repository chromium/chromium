// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

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
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.SessionModel;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
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
    public void testResetDSEGeolocation_InPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile primaryOtrProfile = getPrimaryOtrProfile();
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                primaryOtrProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                primaryOtrProfile,
                ContentSettingValues.ASK);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDSEGeolocation_InNonPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile nonPrimaryOtrProfile = getNonPrimaryOtrProfile();
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                nonPrimaryOtrProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.GEOLOCATION,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                nonPrimaryOtrProfile,
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
    public void testResetDSENotification_InPrimaryOtrProfile_DefaultsToAskFromBlock()
            throws Throwable {
        Profile primaryOtrProfile = getPrimaryOtrProfile();

        // Resetting in incognito should not have the same behavior.
        resetNotificationsSettingsForTest();
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.BLOCK,
                primaryOtrProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                primaryOtrProfile,
                ContentSettingValues.ASK);
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
                ContentSettingValues.BLOCK,
                nonPrimaryOtrProfile,
                ContentSettingValues.BLOCK);
        setSettingAndExpectValue(
                ContentSettingsType.NOTIFICATIONS,
                DSE_ORIGIN,
                null,
                ContentSettingValues.DEFAULT,
                nonPrimaryOtrProfile,
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

        var defaultSetting =
                new GeolocationSetting(ContentSettingValues.ASK, ContentSettingValues.ASK);
        var allowApproximate =
                new GeolocationSetting(ContentSettingValues.ALLOW, ContentSettingValues.BLOCK);

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
    public void testGeolocationPermissionMockValues() throws Throwable {
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_SAMPLE_DATA.setForTesting(true);
        Profile regularProfile = getRegularProfile();
        var info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                        "https://permission.site",
                        "https://permission.site",
                        false,
                        SessionModel.DURABLE);

        var allowApproximate =
                new GeolocationSetting(ContentSettingValues.ALLOW, ContentSettingValues.BLOCK);
        var allowPrecise =
                new GeolocationSetting(ContentSettingValues.ALLOW, ContentSettingValues.ALLOW);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(allowApproximate, info.getGeolocationSetting(regularProfile));
                    info.setGeolocationSetting(regularProfile, allowPrecise);
                    assertEquals(allowPrecise, info.getGeolocationSetting(regularProfile));

                    var permissions =
                            new WebsitePreferenceBridge()
                                    .getPermissionInfo(
                                            regularProfile,
                                            ContentSettingsType.GEOLOCATION_WITH_OPTIONS);
                    assertEquals(1, permissions.size());
                    assertEquals(
                            allowPrecise, permissions.get(0).getGeolocationSetting(regularProfile));
                });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("ApproximateGeolocationPermission")
    public void testGeolocationPermissionDefault() throws Throwable {
        PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_SAMPLE_DATA.setForTesting(false);
        Profile regularProfile = getRegularProfile();
        var info =
                new PermissionInfo(
                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                        "https://permission.site",
                        "https://permission.site",
                        false,
                        SessionModel.DURABLE);

        var defaultSetting =
                new GeolocationSetting(ContentSettingValues.ASK, ContentSettingValues.ASK);

        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(defaultSetting, info.getGeolocationSetting(regularProfile)));
    }
}
