// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;

import static org.chromium.components.permissions.PermissionUtil.getGeolocationType;

import androidx.test.espresso.contrib.RecyclerViewActions;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.site_settings.GeolocationSetting;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;

import java.util.concurrent.TimeoutException;

/** Tests for location permissions and settings in Site Settings. */
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
public class SiteSettingsLocationTest {

    private static final String[]
            BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_OS_WARNING_EXTRA_AND_INFO_TEXT =
                    new String[] {
                        "info_text",
                        "binary_radio_button",
                        "os_permissions_warning",
                        "os_permissions_warning_extra"
                    };

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

    /** Sets Allow Location Enabled to be true and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    public void testSetAllowLocationEnabled() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Location",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        true)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "Location should be allowed.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        SiteSettingsTestHelper.getBrowserContextHandle())));

        initializeUpdateWaiter(/* expectGranted= */ true);

        // Launch a page that uses geolocation and make sure a permission prompt shows up.
        mPermissionTestRule.runAllowTest(
                mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation.html",
                "initiate_geolocation()",
                0,
                true);
    }

    /** Sets Allow Location Enabled to be false and make sure it is set correctly. */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSetAllowLocationNotEnabled() throws Exception {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Location",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertFalse(
                                "Location should be blocked.",
                                WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                                        SiteSettingsTestHelper.getBrowserContextHandle())));

        // Launch a page that uses geolocation. No permission prompt is expected.
        initializeUpdateWaiter(/* expectGranted= */ false);
        mPermissionTestRule.runNoPromptTest(
                mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation.html",
                "initiate_geolocation()",
                0,
                true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableFeatures(ChromeFeatureList.PERMISSION_DEDICATED_CPSS_SETTING_ANDROID)
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testOnlyExpectedPreferencesDeviceLocation() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        SiteSettingsTestHelper.testExpectedPreferences(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_AND_INFO_TEXT);

        // Disable system location setting and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ true);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_OS_WARNING_EXTRA_AND_INFO_TEXT);

        // Disable android location permission and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_INFO_TEXT);

        // Disable android fine location permission and check for the right preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ true,
                /* androidEnabled= */ true,
                /* androidFineEnabled= */ false);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                SiteSettingsTestHelper.BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_INFO_TEXT);

        // Disable system location setting and android location permission and check for the right
        // preferences.
        LocationSettingsTestUtil.setSystemAndAndroidLocationSettings(
                /* systemEnabled= */ false,
                /* androidEnabled= */ false,
                /* androidFineEnabled= */ false);
        SiteSettingsTestHelper.checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION,
                BINARY_RADIO_BUTTON_WITH_OS_WARNING_AND_OS_WARNING_EXTRA_AND_INFO_TEXT);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowGeolocation() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Geolocation",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        true)
                .withExpectedPrefKeys(SingleCategorySettings.LOCATION_TRI_STATE_PREF_KEY)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockGeolocation() {
        new SiteSettingsTestHelper.TwoStatePermissionTestCaseWithRadioButton(
                        "Geolocation",
                        SiteSettingsCategory.Type.DEVICE_LOCATION,
                        getGeolocationType(),
                        false)
                .withExpectedPrefKeysAtStart(SingleCategorySettings.INFO_TEXT_KEY)
                .run();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testRemoveGeolocationWithOptions() {
        String url = "https://example.com";
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        var allowSetting = new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        setGeolocationSetting(url, allowSetting);
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);
        assertEquals(allowSetting, getGeolocationSetting(url));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Remove")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.ASK, ContentSetting.ASK),
                getGeolocationSetting(url));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testChangeGeolocationWithOptions() {
        String url = "https://example.com";
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        var allowSetting = new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW);
        setGeolocationSetting(url, allowSetting);
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);
        assertEquals(allowSetting, getGeolocationSetting(url));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Approximate")).perform(click());
        onView(withText("Confirm")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.BLOCK),
                getGeolocationSetting(url));

        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Block")).perform(click());
        onView(withText("Confirm")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK),
                getGeolocationSetting(url));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testChangeGeolocationWithOptionsRadioButtonsEnabledState() {
        String url = "https://example.com";
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        var blockSetting = new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK);
        setGeolocationSetting(url, blockSetting);
        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);
        assertEquals(blockSetting, getGeolocationSetting(url));

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(url)).check(matches(isDisplayed())).perform(click());
        onView(withText("Edit")).perform(click());

        // Verify that the radio buttons for location precision are disabled when 'Block' is
        // selected.
        onView(withText("Precise")).check(matches(not(isEnabled())));
        onView(withText("Approximate")).check(matches(not(isEnabled())));

        // Click 'Allow' and verify that the radio buttons for location precision are enabled.
        onView(withText("Allow")).perform(click());
        onView(withText("Precise")).check(matches(isEnabled()));
        onView(withText("Approximate")).check(matches(isEnabled()));

        // Click 'Block' again and verify that the radio buttons for location precision are
        // disabled.
        onView(withText("Block")).perform(click());
        onView(withText("Precise")).check(matches(not(isEnabled())));
        onView(withText("Approximate")).check(matches(not(isEnabled())));
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures(PermissionsAndroidFeatureList.APPROXIMATE_GEOLOCATION_PERMISSION)
    public void testEmbargoedGeolocationWithOptions() throws TimeoutException {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        final String url = mPermissionTestRule.getURL("/chrome/test/data/geolocation/simple.html");
        final String origin = Origin.create(url).toString();
        triggerEmbargoForOrigin(url);
        assertEquals(
                new GeolocationSetting(ContentSetting.BLOCK, ContentSetting.BLOCK),
                getGeolocationSetting(url));

        SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);

        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText("Automatically blocked")).check(matches(isDisplayed()));
        onView(withText(origin)).perform(click());
        onView(withText("Edit")).perform(click());
        onView(withText("Allow")).perform(click());
        onView(withText("Confirm")).perform(click());
        assertEquals(
                new GeolocationSetting(ContentSetting.ALLOW, ContentSetting.ALLOW),
                getGeolocationSetting(url));
    }

    void setGeolocationSetting(String url, GeolocationSetting setting) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebsitePreferenceBridgeJni.get()
                                .setGeolocationSettingForOrigin(
                                        SiteSettingsTestHelper.getBrowserContextHandle(),
                                        ContentSettingsType.GEOLOCATION_WITH_OPTIONS,
                                        url,
                                        "*",
                                        setting.mApproximate,
                                        setting.mPrecise));
    }

    GeolocationSetting getGeolocationSetting(String url) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        WebsitePreferenceBridge.getGeolocationSettingForOrigin(
                                SiteSettingsTestHelper.getBrowserContextHandle(),
                                url,
                                "https://example.com"));
    }
}
