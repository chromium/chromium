// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.DeviceFeatureList;
import org.chromium.media.MediaFeatures;

import java.util.concurrent.TimeoutException;

/** Tests for Site Settings main menu and expected category preferences. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DisableFeatures({
    ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
    ChromeFeatureList.SETTINGS_MULTI_COLUMN,
    ChromeFeatureList.ANDROID_ANIMATED_PROGRESS_BAR_IN_BROWSER
})
public class SiteSettingsMenuTest {

    private static final String[] BINARY_TOGGLE_WITH_ANTI_ABUSE_PREF_KEYS = {
        "binary_toggle",
        "anti_abuse_when_on_header",
        "anti_abuse_when_on_section_one",
        "anti_abuse_when_on_section_two",
        "anti_abuse_when_on_section_three",
        "anti_abuse_things_to_consider_header",
        "anti_abuse_things_to_consider_section_one"
    };

    private static final String[] CLEAR_BROWSING_DATA_LINK_WITH_CONTAINMENT =
            new String[] {"clear_browsing_data_link"};

    private static final String[] NULL_ARRAY = new String[0];

    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    public PermissionTestRule mPermissionTestRule =
            new PermissionTestRule(mActivityTestRule.getActivityTestRule(), true);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    public AdvancedProtectionTestRule mAdvancedProtectionRule = new AdvancedProtectionTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mAdvancedProtectionRule)
                    .around(mActivityTestRule)
                    .around(mPermissionTestRule);

    @Before
    public void setUp() throws TimeoutException {
        try {
            SiteSettingsTestUtils.cleanUpCookiesAndPermissions();
        } catch (TimeoutException e) {
            // Sometimes there's a callback timeout here. Doesn't seem to impact test results.
        }
    }

    @After
    public void tearDown() throws Exception {
        SiteSettingsTestHelper.cleanUpContentSettingsAndExceptions(new CallbackHelper());
    }

    /** Test that showing the Site Settings menu doesn't crash (crbug.com/40468610). */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenu() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        settingsActivity.finish();
    }

    /**
     * Test that showing the Site Settings menu contains the "Third-party cookies" and "Site data"
     * rows.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenuWithPrivacySandboxSettings4Enabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNull(websitePreferences.findPreference("cookies"));
        assertNotNull(websitePreferences.findPreference("third_party_cookies"));
        assertNotNull(websitePreferences.findPreference("site_data"));
        settingsActivity.finish();
    }

    /** Test that showing the Site Settings menu contains the "Anti-abuse" row. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenuForAntiAbuse() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNotNull(websitePreferences.findPreference("anti_abuse"));
        settingsActivity.finish();
    }

    /**
     * Tests that only expected Preferences are shown for a category. This santiy checks the number
     * of categories only. Each category has its own individual test below.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesShown() {
        // If you add a category in the SiteSettings UI, please update this total AND add a test for
        // it below, named "testOnlyExpectedPreferences<Category>".
        Assert.assertEquals(39, SiteSettingsCategory.Type.NUM_ENTRIES);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesZoom() {
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.ZOOM, NULL_ARRAY);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAllSites() {
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.ALL_SITES, CLEAR_BROWSING_DATA_LINK_WITH_CONTAINMENT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAds() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.ADS,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAntiAbuse() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.ANTI_ABUSE,
                BINARY_TOGGLE_WITH_ANTI_ABUSE_PREF_KEYS,
                BINARY_TOGGLE_WITH_ANTI_ABUSE_PREF_KEYS);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAugmentedReality() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.AUGMENTED_REALITY,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAutoDarkWebContent() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testOnlyExpectedPreferencesAutoPictureInPicture() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.AUTO_PICTURE_IN_PICTURE,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesAutomaticDownloads() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesBackgroundSync() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.BACKGROUND_SYNC,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesBluetooth() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.BLUETOOTH,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesBluetoothScanning() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesCamera() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.CAMERA,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesClipboard() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.CLIPBOARD,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesFileEditing() {
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.FILE_EDITING,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesFederatedIdentityApi() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesJavascriptOptimizer() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesHandTracking() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.HAND_TRACKING,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesHidDevices() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.HID_DEVICES,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesIdleDetection() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.IDLE_DETECTION,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesLocalNetwork() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.LOCAL_NETWORK,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesLoopbackNetwork() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.LOOPBACK_NETWORK,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesWindowManagement() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.WINDOW_MANAGEMENT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesJavascript() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.JAVASCRIPT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesMicrophone() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.MICROPHONE,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);

        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.NFC,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);

        // Disable system nfc setting and check for the right preferences.
        NfcSystemLevelSetting.setNfcSettingForTesting(false);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("QuietNotificationPrompts")
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    public void testOnlyExpectedPreferencesNotifications() {
        String[] notificationsEnabled =
                new String[] {"info_text", "binary_radio_button", "notifications_quiet_ui"};
        String[] notificationsDisabled = SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT;

        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.NOTIFICATIONS,
                notificationsDisabled,
                notificationsEnabled);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesPopups() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.POPUPS,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesProtectedMedia() {
        String[] protectedMedia = new String[] {"info_text", "tri_state_toggle"};
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ALLOW);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, protectedMedia);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesRequestDesktopSite() {
        String[] rdsEnabled = {
            "info_text", "binary_radio_button", "desktop_site_window", "add_exception"
        };
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                rdsEnabled);
        Assert.assertTrue(
                "SharedPreference USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY should be"
                        + " updated.",
                ContextUtils.getAppSharedPreferences()
                        .contains(
                                SingleCategorySettingsConstants
                                        .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(DeviceFeatureList.SENSORS_ALLOW_ASK_BLOCK_PERMISSION_MODEL)
    public void testOnlyExpectedPreferencesSensors() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.SENSORS,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(DeviceFeatureList.SENSORS_ALLOW_ASK_BLOCK_PERMISSION_MODEL)
    public void testOnlyExpectedPreferencesSensorsAllowAskBlock() {
        String[] sensors = new String[] {"info_text", "tri_state_toggle"};
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.SENSORS, ContentSetting.ALLOW);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.SENSORS, sensors);
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.SENSORS, ContentSetting.ASK);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.SENSORS, sensors);
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.SENSORS, ContentSetting.BLOCK);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.SENSORS, sensors);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesSound() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.SOUND,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_EXCEPTION_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesUsb() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.USB,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesSerialPort() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.SERIAL_PORT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesUseStorage() {
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.USE_STORAGE, NULL_ARRAY);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesVirtualReality() {
        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.VIRTUAL_REALITY,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);
    }

    /** Tests system NFC support in Preferences. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSystemNfcSupport() {
        // Disable system nfc support and check for the right preferences.
        NfcSystemLevelSetting.setNfcSupportForTesting(false);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.NFC,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT);
    }

    /**
     * Tests that {@link SingleWebsiteSettings#SiteSettingsTestHelper.resetSite} doesn't crash (see
     * e.g. the crash on host names in issue 600232).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDoesntCrash() {
        WebsiteAddress address = WebsiteAddress.create("example.com");
        SiteSettingsTestHelper.resetSite(address);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testAutoPiPPermissionNotVisibleWhenDisabled() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        SiteSettings websitePreferences = (SiteSettings) settingsActivity.getMainFragment();
        assertNull(websitePreferences.findPreference("auto_picture_in_picture"));
        settingsActivity.finish();
    }
}
