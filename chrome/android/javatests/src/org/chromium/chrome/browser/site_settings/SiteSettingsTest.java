// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.components.browser_ui.site_settings.AutoDarkMetrics.AutoDarkSettingsChangeSource.SITE_SETTINGS_GLOBAL;
import static org.chromium.components.content_settings.PrefNames.DESKTOP_SITE_WINDOW_SETTING_ENABLED;

import android.content.Context;
import android.content.Intent;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.FederatedIdentityTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.BinaryStatePermissionPreference;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettingsConstants;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.media.MediaFeatures;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Tests for global category switches, embargoed settings, and permissions in Site Settings. */
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
public class SiteSettingsTest {

    private static final String[] ANTI_ABUSE_PREF_KEYS = {
        "anti_abuse_when_on_header",
        "anti_abuse_when_on_section_one",
        "anti_abuse_when_on_section_two",
        "anti_abuse_when_on_section_three",
        "anti_abuse_things_to_consider_header",
        "anti_abuse_things_to_consider_section_one"
    };

    /** Test case for site settings with a global toggle. */
    private static class TwoStatePermissionTestCaseWithToggle
            extends SiteSettingsTestHelper.PermissionTestCase {
        public TwoStatePermissionTestCaseWithToggle(
                String testName, int siteSettingsType, int contentSettingsType, boolean enabled) {
            super(testName, siteSettingsType, contentSettingsType, enabled);
            mExpectedPreferenceKeys.add(SingleCategorySettings.BINARY_TOGGLE_KEY);
        }

        @Override
        public void doTest(SingleCategorySettings singleCategorySettings) {
            // Verify toggle related checks first as they may affect the preferences on the screen.
            assertToggleTitleAndSummary(singleCategorySettings);
            assertGlobalToggleForCategory(singleCategorySettings);

            super.doTest(singleCategorySettings);
        }

        /** Verify {@link SingleCategorySettings} is wired correctly. */
        private void assertGlobalToggleForCategory(SingleCategorySettings singleCategorySettings) {
            final String exceptionString =
                    "Test <"
                            + mTestName
                            + ">: Content setting category <"
                            + mContentSettingsType
                            + "> should be "
                            + (mIsCategoryEnabled ? "enabled" : "disabled")
                            + " with Site Settings <"
                            + mSiteSettingsType
                            + ">.";

            ChromeSwitchPreference toggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
            assertNotNull("Toggle should not be null.", toggle);

            singleCategorySettings.onPreferenceChange(toggle, mIsCategoryEnabled);
            Assert.assertEquals(
                    exceptionString,
                    mIsCategoryEnabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            SiteSettingsTestHelper.getBrowserContextHandle(),
                            mContentSettingsType));
        }

        /** Verify {@link ContentSettingsResources} is set correctly. */
        private void assertToggleTitleAndSummary(SingleCategorySettings singleCategorySettings) {
            ChromeSwitchPreference toggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);
            Assert.assertNotNull(toggle);

            Assert.assertEquals(
                    "Preference title is not set correctly.",
                    singleCategorySettings
                            .getResources()
                            .getString(ContentSettingsResources.getTitle(mContentSettingsType)),
                    toggle.getTitle());
            assertNotNull("Enabled summary text should not be null.", toggle.getSummaryOn());
            assertNotNull("Disabled summary text should not be null.", toggle.getSummaryOff());

            String summary =
                    mIsCategoryEnabled
                            ? toggle.getSummaryOn().toString()
                            : toggle.getSummaryOff().toString();
            String expected =
                    singleCategorySettings
                            .getResources()
                            .getString(
                                    mIsCategoryEnabled
                                            ? ContentSettingsResources.getEnabledSummary(
                                                    mContentSettingsType)
                                            : ContentSettingsResources.getDisabledSummary(
                                                    mContentSettingsType));
            Assert.assertEquals(
                    "Summary text in state <" + mIsCategoryEnabled + "> does not match.",
                    expected,
                    summary);
        }
    }

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

    private PermissionUpdateWaiter mPermissionUpdateWaiter;

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
        if (mPermissionUpdateWaiter != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mPermissionTestRule
                                .getActivityTab()
                                .removeObserver(mPermissionUpdateWaiter);
                    });
        }
        SiteSettingsTestHelper.cleanUpContentSettingsAndExceptions(new CallbackHelper());
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(
                        SingleCategorySettingsConstants
                                .USER_ENABLED_DESKTOP_SITE_GLOBAL_SETTING_PREFERENCE_KEY)
                .apply();
    }

    private void initializeUpdateWaiter(final boolean expectGranted) {
        if (mPermissionUpdateWaiter != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mPermissionTestRule
                                .getActivityTab()
                                .removeObserver(mPermissionUpdateWaiter);
                    });
        }
        Tab tab = mPermissionTestRule.getActivityTab();

        mPermissionUpdateWaiter =
                new PermissionUpdateWaiter(
                        expectGranted ? "Granted" : "Denied", mPermissionTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(mPermissionUpdateWaiter));
    }

    private void triggerEmbargoForOrigin(String url) throws TimeoutException {
        // Ignore notification request 4 times to enter embargo. 5th one ensures that notifications
        // are blocked by actually causing a deny-by-embargo.
        for (int i = 0; i < 5; i++) {
            mPermissionTestRule.loadUrl(url);
            mPermissionTestRule.runJavaScriptCodeInCurrentTab("requestPermissionAndRespond()");
        }
    }

    private int getTabCount() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mPermissionTestRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    private void testTwoStateToggleDisabledByPolicy(@SiteSettingsCategory.Type int type) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);
        SingleCategorySettings singleCategorySettings =
                (SingleCategorySettings) settingsActivity.getMainFragment();

        if (type != SiteSettingsCategory.Type.ANTI_ABUSE) {
            BinaryStatePermissionPreference binaryRadioButton =
                    singleCategorySettings.findPreference(
                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);

            Assert.assertFalse(binaryRadioButton.isEnabled());
        } else {
            ChromeSwitchPreference binaryToggle =
                    singleCategorySettings.findPreference(SingleCategorySettings.BINARY_TOGGLE_KEY);

            Assert.assertFalse(binaryToggle.isEnabled());
        }

        ApplicationTestUtils.finishActivity(settingsActivity);
    }

    /** Sets Allow Popups Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsBlocked() throws TimeoutException {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Popups",
                        SiteSettingsCategory.Type.POPUPS,
                        ContentSettingsType.POPUPS,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that the popup doesn't open.
        mPermissionTestRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(1, getTabCount());
    }

    /** Sets Allow Popups Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsNotBlocked() throws TimeoutException {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Popups",
                        SiteSettingsCategory.Type.POPUPS,
                        ContentSettingsType.POPUPS,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that a popup opens.
        mPermissionTestRule.setUpUrl("/chrome/test/data/android/popup.html");
        mPermissionTestRule.runJavaScriptCodeInCurrentTab("openPopup();");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(2, getTabCount());
    }

    /** Sets Allow Camera Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    @DisableIf.Device(DeviceFormFactor.ONLY_TABLET) // crbug.com/41490094
    public void testCameraBlocked() throws Exception {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Camera",
                        SiteSettingsCategory.Type.CAMERA,
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that the camera permission doesn't get requested.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                0,
                /* withGesture= */ true);
    }

    /** Sets Allow Camera Enabled to be true and make sure it is set correctly. */
    @DisabledTest(message = "https://crbug.com/429083114")
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisableIf.Device(DeviceFormFactor.ONLY_TABLET) // crbug.com/41490094
    public void testCameraNotBlocked() throws Exception {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Camera",
                        SiteSettingsCategory.Type.CAMERA,
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionTestRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: true, audio: false});",
                0,
                /* withGesture= */ true);
    }

    /** Sets Allow Mic Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testMicBlocked() throws Exception {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Mic",
                        SiteSettingsCategory.Type.MICROPHONE,
                        ContentSettingsType.MEDIASTREAM_MIC,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Test that the microphone permission doesn't get requested.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                0,
                true);
    }

    /** Sets Allow Mic Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisabledTest(message = "crbug.com/41490094 && crbug.com/425926397")
    public void testMicNotBlocked() throws Exception {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Mic",
                        SiteSettingsCategory.Type.MICROPHONE,
                        ContentSettingsType.MEDIASTREAM_MIC,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();

        // Launch a page that uses the microphone and make sure a permission prompt shows up.
        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionTestRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStopLegacy({video: false, audio: true});",
                0,
                true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBackgroundSync() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "BackgroundSync",
                        SiteSettingsCategory.Type.BACKGROUND_SYNC,
                        ContentSettingsType.BACKGROUND_SYNC,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBackgroundSync() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "BackgroundSync",
                        SiteSettingsCategory.Type.BACKGROUND_SYNC,
                        ContentSettingsType.BACKGROUND_SYNC,
                        false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowNotifications() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Notifications",
                        SiteSettingsCategory.Type.NOTIFICATIONS,
                        ContentSettingsType.NOTIFICATIONS,
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.NOTIFICATIONS_TRI_STATE_PREF_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockNotifications() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Notifications",
                        SiteSettingsCategory.Type.NOTIFICATIONS,
                        ContentSettingsType.NOTIFICATIONS,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowUsb() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "USB", SiteSettingsCategory.Type.USB, ContentSettingsType.USB_GUARD, true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockUsb() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "USB", SiteSettingsCategory.Type.USB, ContentSettingsType.USB_GUARD, false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowSerialPort() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "SerialPort",
                        SiteSettingsCategory.Type.SERIAL_PORT,
                        ContentSettingsType.SERIAL_GUARD,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockSerialPort() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "SerialPort",
                        SiteSettingsCategory.Type.SERIAL_PORT,
                        ContentSettingsType.SERIAL_GUARD,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAutomaticDownloads() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AutomaticDownloads",
                        SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                        ContentSettingsType.AUTOMATIC_DOWNLOADS,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAutomaticDownloads() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AutomaticDownloads",
                        SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                        ContentSettingsType.AUTOMATIC_DOWNLOADS,
                        false)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBluetoothScanning() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothScanning",
                        SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                        ContentSettingsType.BLUETOOTH_SCANNING,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBluetoothScanning() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothScanning",
                        SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                        ContentSettingsType.BLUETOOTH_SCANNING,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBluetoothGuard() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothGuard",
                        SiteSettingsCategory.Type.BLUETOOTH,
                        ContentSettingsType.BLUETOOTH_GUARD,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBluetoothGuard() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "BluetoothGuard",
                        SiteSettingsCategory.Type.BLUETOOTH,
                        ContentSettingsType.BLUETOOTH_GUARD,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "NFC", SiteSettingsCategory.Type.NFC, ContentSettingsType.NFC, true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockNfc() {
        NfcSystemLevelSetting.setNfcSettingForTesting(true);
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "NFC", SiteSettingsCategory.Type.NFC, ContentSettingsType.NFC, false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAntiAbuse() {
        new TwoStatePermissionTestCaseWithToggle(
                        "AntiAbuse",
                        SiteSettingsCategory.Type.ANTI_ABUSE,
                        ContentSettingsType.ANTI_ABUSE,
                        true)
                .withExpectedPrefKeys(ANTI_ABUSE_PREF_KEYS)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAntiAbuse() {
        new TwoStatePermissionTestCaseWithToggle(
                        "AntiAbuse",
                        SiteSettingsCategory.Type.ANTI_ABUSE,
                        ContentSettingsType.ANTI_ABUSE,
                        false)
                .withExpectedPrefKeys(ANTI_ABUSE_PREF_KEYS)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAr() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AR",
                        SiteSettingsCategory.Type.AUGMENTED_REALITY,
                        ContentSettingsType.AR,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAr() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AR",
                        SiteSettingsCategory.Type.AUGMENTED_REALITY,
                        ContentSettingsType.AR,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowVr() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "VR",
                        SiteSettingsCategory.Type.VIRTUAL_REALITY,
                        ContentSettingsType.VR,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockVr() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "VR",
                        SiteSettingsCategory.Type.VIRTUAL_REALITY,
                        ContentSettingsType.VR,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowHandTracking() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "HandTracking",
                        SiteSettingsCategory.Type.HAND_TRACKING,
                        ContentSettingsType.HAND_TRACKING,
                        true)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockHandTracking() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "HandTracking",
                        SiteSettingsCategory.Type.HAND_TRACKING,
                        ContentSettingsType.HAND_TRACKING,
                        false)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowIdleDetection() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "IdleDetection",
                        SiteSettingsCategory.Type.IDLE_DETECTION,
                        ContentSettingsType.IDLE_DETECTION,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockIdleDetection() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "IdleDetection",
                        SiteSettingsCategory.Type.IDLE_DETECTION,
                        ContentSettingsType.IDLE_DETECTION,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowLocalNetwork() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "LocalNetwork",
                        SiteSettingsCategory.Type.LOCAL_NETWORK,
                        ContentSettingsType.LOCAL_NETWORK,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockLocalNetwork() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "LocalNetwork",
                        SiteSettingsCategory.Type.LOCAL_NETWORK,
                        ContentSettingsType.LOCAL_NETWORK,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowLoopbackNetwork() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "LoopbackNetwork",
                        SiteSettingsCategory.Type.LOOPBACK_NETWORK,
                        ContentSettingsType.LOOPBACK_NETWORK,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockLoopbackNetwork() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "LoopbackNetwork",
                        SiteSettingsCategory.Type.LOOPBACK_NETWORK,
                        ContentSettingsType.LOOPBACK_NETWORK,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowWindowManager() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "WindowManagement",
                        SiteSettingsCategory.Type.WINDOW_MANAGEMENT,
                        ContentSettingsType.WINDOW_MANAGEMENT,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockWindowManager() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "WindowManagement",
                        SiteSettingsCategory.Type.WINDOW_MANAGEMENT,
                        ContentSettingsType.WINDOW_MANAGEMENT,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testAllowAutoPictureInPicture() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AutoPictureInPicture",
                        SiteSettingsCategory.Type.AUTO_PICTURE_IN_PICTURE,
                        ContentSettingsType.AUTO_PICTURE_IN_PICTURE,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testBlockAutoPictureInPicture() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AutoPictureInPicture",
                        SiteSettingsCategory.Type.AUTO_PICTURE_IN_PICTURE,
                        ContentSettingsType.AUTO_PICTURE_IN_PICTURE,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAutoDark() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Enabled";
        final int preTestCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL);
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AutoDarkWebContent",
                        SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
        Assert.assertEquals(
                "<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAutoDark() {
        final String histogramName = "Android.DarkTheme.AutoDarkMode.SettingsChangeSource.Disabled";
        final int preTestCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL);
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "AutoDarkWebContent",
                        SiteSettingsCategory.Type.AUTO_DARK_WEB_CONTENT,
                        ContentSettingsType.AUTO_DARK_WEB_CONTENT,
                        false)
                .run();
        Assert.assertEquals(
                "<" + histogramName + "> should be recorded for SITE_SETTINGS_GLOBAL.",
                preTestCount + 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, SITE_SETTINGS_GLOBAL));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowRequestDesktopSite() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "RequestDesktopSite",
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                        ContentSettingsType.REQUEST_DESKTOP_SITE,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.DESKTOP_SITE_WINDOW_TOGGLE_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockRequestDesktopSite() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "RequestDesktopSite",
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE,
                        ContentSettingsType.REQUEST_DESKTOP_SITE,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowFederatedIdentityApi() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "FederatedIdentityApi",
                        SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                        ContentSettingsType.FEDERATED_IDENTITY_API,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockFederatedIdentityApi() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "FederatedIdentityApi",
                        SiteSettingsCategory.Type.FEDERATED_IDENTITY_API,
                        ContentSettingsType.FEDERATED_IDENTITY_API,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowJavascriptOptimizer() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "JavascriptOptimizer",
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                        ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockJavascriptOptimizer() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "JavascriptOptimizer",
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER,
                        ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .withExpectedPrefKeys(SingleCategorySettings.ADD_EXCEPTION_KEY)
                .run();
    }

    @Test
    @SmallTest
    @RequiresRestart
    @Feature({"Preferences"})
    public void testOsBlocksJavascriptOptimizer() {
        String pageOrigin = mPermissionTestRule.getOrigin();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            SiteSettingsTestHelper.getBrowserContextHandle(),
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            new GURL(pageOrigin),
                            new GURL(pageOrigin),
                            ContentSetting.ALLOW);
                });

        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    SiteSettingsTestHelper.checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_RADIO_BUTTON_KEY,
                                SingleCategorySettings.TOGGLE_DISABLE_REASON_KEY,
                                SingleCategorySettings.ALLOWED_GROUP,
                                SingleCategorySettings.ADD_EXCEPTION_KEY,
                            });

                    BinaryStatePermissionPreference binaryRadioButton =
                            (BinaryStatePermissionPreference)
                                    singleCategorySettings.findPreference(
                                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
                    Assert.assertFalse(binaryRadioButton.isChecked());
                    Assert.assertFalse(binaryRadioButton.isEnabled());

                    Preference radioButtonDisableReason =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.TOGGLE_DISABLE_REASON_KEY);
                    Context context = ApplicationProvider.getApplicationContext();
                    Assert.assertEquals(
                            context.getString(
                                    R.string
                                            .javascript_optimizer_disabled_due_to_advanced_protection_settings_message),
                            radioButtonDisableReason.getTitle());

                    settingsActivity.finish();
                });
    }

    // Due to bug DefaultPassthroughCommandDecoder feature needs to be on whenever
    // BaseSwitches.ENABLE_LOW_END_DEVICE_MODE feature is on to avoid crash.
    // See https://issues.chromium.org/448715624
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @EnableFeatures("DefaultPassthroughCommandDecoder")
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/527592668
    public void testAddingJavascriptOptimizerExceptionsBlockedIfNotEnoughRam() {
        /* This test relies on site isolation memory thresholds being enabled. Skip if that
         * feature is disable.
         */
        if (!ChromeFeatureList.isEnabled("SiteIsolationEnableMemoryThresholdAndroid")) {
            Assume.assumeTrue(
                    "Skipping test because SiteIsolationEnableMemoryThresholdAndroid is disabled.",
                    false);
        }

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    SiteSettingsTestHelper.checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_RADIO_BUTTON_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_DISABLED_REASON_KEY,
                            });

                    Preference addExceptionButton =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_KEY);
                    Assert.assertFalse(addExceptionButton.isEnabled());

                    Preference addExceptionButtonDisabledReason =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_DISABLED_REASON_KEY);
                    Context context = ApplicationProvider.getApplicationContext();
                    int expectedReasonId =
                            R.string.website_settings_js_opt_add_exceptions_disabled_reason;
                    Assert.assertEquals(
                            context.getString(expectedReasonId),
                            addExceptionButtonDisabledReason.getTitle());

                    settingsActivity.finish();
                });
    }

    /**
     * Test that if the Javascript-optimizer is enabled by enterprise policy but disabled by the OS
     * advanced-portection-mode setting that the enterprise policy is given precedence.
     */
    @Test
    @SmallTest
    @RequiresRestart
    @Feature({"Preferences"})
    @Policies.Add({@Policies.Item(key = "DefaultJavaScriptOptimizerSetting", string = "1")})
    public void testPolicyHigherPriorityThanOsBlockingJavascriptOptimizer() {
        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SingleCategorySettings singleCategorySettings =
                            (SingleCategorySettings) settingsActivity.getMainFragment();

                    SiteSettingsTestHelper.checkPreferencesForSettingsActivity(
                            settingsActivity,
                            new String[] {
                                SingleCategorySettings.INFO_TEXT_KEY,
                                SingleCategorySettings.BINARY_RADIO_BUTTON_KEY,
                                SingleCategorySettings.ADD_EXCEPTION_KEY
                            });

                    BinaryStatePermissionPreference binaryRadioButton =
                            (BinaryStatePermissionPreference)
                                    singleCategorySettings.findPreference(
                                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
                    Assert.assertTrue(binaryRadioButton.isChecked());
                    Assert.assertFalse(binaryRadioButton.isEnabled());

                    Preference addExceptionPreference =
                            singleCategorySettings.findPreference(
                                    SingleCategorySettings.ADD_EXCEPTION_KEY);
                    Assert.assertFalse(addExceptionPreference.isEnabled());

                    // Proabably never worked. crbug.com/446200399
                    // onData(withKey(SingleCategorySettings.ALLOWED_GROUP))
                    //         .inAdapterView(
                    //                 allOf(
                    //                         withContentDescription(
                    //                                 R.string.managed_by_your_organization),
                    //                         withText(R.string.managed_by_your_organization)))
                    //         .check(matches(isDisplayed()));

                    settingsActivity.finish();
                });
    }

    /**
     * Tests that when (1) a single website has a Javascript-optimizer exception AND the
     * Javascript-optimizer permission toggle is present on the {@link SingleWebsiteSettings} screen
     * AND (2) Advanced protection is requested by the operating system THAT the toggle is still
     * enabled because explicit Javascript-optimizer content settings have priority over
     * advanced-protection-mode.
     */
    @Test
    @SmallTest
    @RequiresRestart
    @Feature({"Preferences"})
    public void testOsBlocksJavascriptOptimizerSingleWebsite() throws Exception {
        final String pageUrl = mPermissionTestRule.getURL("/chrome/test/data/android/simple.html");
        String pageOrigin = mPermissionTestRule.getOrigin();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            SiteSettingsTestHelper.getBrowserContextHandle(),
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            new GURL(pageOrigin),
                            new GURL(pageOrigin),
                            ContentSetting.ALLOW);
                });

        mAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(pageUrl));
        final SettingsActivity settingsActivity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    Preference javascriptOptimizerPreference =
                            websitePreferences.findPreference("javascript_optimizer");
                    Assert.assertTrue(javascriptOptimizerPreference.isEnabled());
                });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    // Auto does not have actions to handle ACTION_CHANNEL_NOTIFICATION_SETTINGS
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testEmbargoedNotificationSiteSettings() throws Exception {
        final String url =
                mPermissionTestRule.getURLWithHostName(
                        "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(url);

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(url));
        final SettingsActivity settingsActivity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();

                    final Preference notificationPreference =
                            websitePreferences.findPreference("push_notifications_list");

                    Assert.assertEquals(
                            context.getString(R.string.automatically_blocked),
                            notificationPreference.getSummary());
                    websitePreferences.launchOsChannelSettingsFromPreference(
                            notificationPreference);
                });
        // There are lots of native posted tasks since start up. So we need to wait for
        // all tasks to settle.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            "Channel was not found",
                            SiteSettingsTestHelper.getChannelId(url),
                            not(ChromeChannelDefinitions.ChannelId.SITES));
                });
        // Close the OS notification settings UI.
        UiAutomatorUtils.getInstance().pressBack();
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/40699792")
    public void testEmbargoedNotificationCategorySiteSettings() throws Exception {
        final String urlToEmbargo =
                mPermissionTestRule.getURLWithHostName(
                        "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(urlToEmbargo);

        final String urlToBlock =
                mPermissionTestRule.getURLWithHostName(
                        "exampleToBlock.com",
                        "/chrome/test/data/notifications/notification_tester.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebsitePreferenceBridgeJni.get()
                            .setPermissionSettingForOrigin(
                                    SiteSettingsTestHelper.getBrowserContextHandle(),
                                    ContentSettingsType.NOTIFICATIONS,
                                    urlToBlock,
                                    urlToBlock,
                                    ContentSetting.BLOCK);
                });

        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.NOTIFICATIONS);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean blockedByEmbargo =
                            WebsitePreferenceBridgeJni.get()
                                    .isNotificationEmbargoedForOrigin(
                                            SiteSettingsTestHelper.getBrowserContextHandle(),
                                            urlToEmbargo);
                    Assert.assertTrue(blockedByEmbargo);

                    final String blockedGroupKey = "blocked_group";
                    // Click on Blocked group in Category Settings. By default Blocked is closed, to
                    // be able to find any origins inside, Blocked should be opened.
                    SingleCategorySettings websitePreferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    websitePreferences.findPreference(blockedGroupKey).performClick();

                    // After triggering onClick on Blocked group, all UI will be discarded and
                    // reinitialized from scratch. Init all variables again, otherwise it will use
                    // stale information.
                    websitePreferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    ExpandablePreferenceGroup blockedGroup =
                            (ExpandablePreferenceGroup)
                                    websitePreferences.findPreference(blockedGroupKey);

                    Assert.assertTrue(blockedGroup.isExpanded());
                    // Only |url| has been added under embargo.
                    Assert.assertEquals(2, blockedGroup.getPreferenceCount());

                    Assert.assertEquals(
                            ApplicationProvider.getApplicationContext()
                                    .getString(R.string.automatically_blocked),
                            blockedGroup.getPreference(0).getSummary());

                    // Blocked origin should has no summary.
                    assertNull(blockedGroup.getPreference(1).getSummary());
                });
        settingsActivity.finish();
    }

    /**
     * Test that embargoing federated identity permission displays "Automatically Blocked" message
     * in page info UI. Federated identity is a content setting. Content settings use a different
     * code path than permissions (like notifications).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testEmbargoedFederatedIdentity() throws Exception {
        final String rpUrl =
                mPermissionTestRule.getURLWithHostName(
                        "example.com", "/chrome/test/data/android/simple.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FederatedIdentityTestUtils.embargoFedCmForRelyingParty(new GURL(rpUrl));
                });

        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                settingsNavigation.createSettingsIntent(
                        context,
                        SingleWebsiteSettings.class,
                        SingleWebsiteSettings.createFragmentArgsForSite(rpUrl));
        final SettingsActivity settingsActivity =
                (SettingsActivity)
                        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final SingleWebsiteSettings websitePreferences =
                            (SingleWebsiteSettings) settingsActivity.getMainFragment();
                    final Preference fedCmPreference =
                            websitePreferences.findPreference("federated_identity_api_list");

                    Assert.assertEquals(
                            context.getString(R.string.automatically_blocked),
                            fedCmPreference.getSummary());
                });
        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentDefaultOption() throws Exception {
        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentAskAllow() throws Exception {
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);

        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionTestRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentAskBlocked() throws Exception {
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.ASK);

        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionTestRule.runDenyTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    public void testProtectedContentBlocked() throws Exception {
        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);

        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisabledTest(
            message = "https://crbug.com/40804306,https://crbug.com/40256198,crbug.com/40781540")
    public void testProtectedContentAllowThenBlock() throws Exception {
        initializeUpdateWaiter(/* expectGranted= */ true);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true);

        SiteSettingsTestHelper.setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSetting.BLOCK);

        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html",
                "requestEME()",
                0,
                true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testDesktopSiteWindowSettings() {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(
                        SiteSettingsCategory.Type.REQUEST_DESKTOP_SITE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HistogramWatcher histogramExpectation =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "Android.RequestDesktopSite.WindowSettingChanged", true);
                    SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    // Window setting is only available when the Global Setting is ON.
                    BinaryStatePermissionPreference binaryRadioButton =
                            (BinaryStatePermissionPreference)
                                    preferences.findPreference(
                                            SingleCategorySettings.BINARY_RADIO_BUTTON_KEY);
                    preferences.onPreferenceChange(binaryRadioButton, true);

                    ChromeBaseCheckBoxPreference windowSettingPref =
                            preferences.findPreference(
                                    SingleCategorySettings.DESKTOP_SITE_WINDOW_TOGGLE_KEY);
                    PrefService prefService =
                            UserPrefs.get(SiteSettingsTestHelper.getBrowserContextHandle());
                    preferences.onPreferenceChange(windowSettingPref, true);
                    Assert.assertTrue(
                            "Window setting should be ON.",
                            prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
                    histogramExpectation.assertExpected();

                    preferences.onPreferenceChange(windowSettingPref, false);
                    Assert.assertFalse(
                            "Window setting should be OFF.",
                            prefService.getBoolean(DESKTOP_SITE_WINDOW_SETTING_ENABLED));
                });
        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAutorevokePermissionsSwitch() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SiteSettings.PERMISSION_AUTOREVOCATION_HISTOGRAM_NAME, true);
        // Set the initial toggle state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(SiteSettingsTestHelper.getBrowserContextHandle())
                            .setBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED, false);
                });

        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");

        // Scroll to permission autorevocation preference and click it.
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.scrollTo(
                                hasDescendant(
                                        withText(
                                                R.string.safety_hub_autorevocation_toggle_title))));
        onView(withText(R.string.safety_hub_autorevocation_toggle_title))
                .check(matches(not(isChecked())))
                .perform(click());

        // Verify that the pref has been correctly set.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Unused site permission revocation should be enabled.",
                            UserPrefs.get(SiteSettingsTestHelper.getBrowserContextHandle())
                                    .getBoolean(Pref.UNUSED_SITE_PERMISSIONS_REVOCATION_ENABLED));
                });
        histogramExpectation.assertExpected();

        settingsActivity.finish();
    }

    /** Test case for checking that settings with binary toggles are disabled by policy. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultJavaScriptSetting", string = "2"),
        @Policies.Item(key = "DefaultPopupsSetting", string = "2"),
        @Policies.Item(key = "DefaultGeolocationSetting", string = "2"),
        @Policies.Item(key = "DefaultJavaScriptOptimizerSetting", string = "2")
    })
    public void testAllTwoStateToggleDisabledByPolicy() {
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.POPUPS);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.DEVICE_LOCATION);
        testTwoStateToggleDisabledByPolicy(SiteSettingsCategory.Type.JAVASCRIPT_OPTIMIZER);
        // TODO(crbug.com/40879457): add a test for sensors once crash in the sensors settings page
        // is resolved.
    }
}
