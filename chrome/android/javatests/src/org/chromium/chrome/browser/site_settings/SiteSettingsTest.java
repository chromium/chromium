// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge.SITE_WILDCARD;
import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.util.Pair;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.notifications.channels.SiteChannelsManager;
import org.chromium.chrome.browser.permissions.PermissionTestRule;
import org.chromium.chrome.browser.permissions.PermissionTestRule.PermissionUpdateWaiter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference;
import org.chromium.components.browser_ui.site_settings.FourStateCookieSettingsPreference.CookieSettingsState;
import org.chromium.components.browser_ui.site_settings.R;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.TriStateSiteSettingsPreference;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.nfc.NfcSystemLevelSetting;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.concurrent.TimeoutException;

/**
 * Tests for everything under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1", "ignore-certificate-errors"})
@Batch(SiteSettingsTest.SITE_SETTINGS_BATCH_NAME)
public class SiteSettingsTest {
    public static final String SITE_SETTINGS_BATCH_NAME = "site_settings";

    @ClassRule
    public static PermissionTestRule mPermissionRule = new PermissionTestRule(true);

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mPermissionRule, false);

    private PermissionUpdateWaiter mPermissionUpdateWaiter;

    @After
    public void tearDown() throws TimeoutException {
        if (mPermissionUpdateWaiter != null) {
            mPermissionRule.getActivity().getActivityTab().removeObserver(mPermissionUpdateWaiter);
        }

        // Clean up default content setting and system settings.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int t = 0; t < SiteSettingsCategory.Type.NUM_ENTRIES; t++) {
                if (SiteSettingsCategory.contentSettingsType(t) >= 0) {
                    WebsitePreferenceBridge.setContentSetting(getBrowserContextHandle(),
                            SiteSettingsCategory.contentSettingsType(t),
                            ContentSettingValues.DEFAULT);
                }
            }
        });
        LocationUtils.setFactory(null);
        LocationProviderOverrider.setLocationProviderImpl(null);
        NfcSystemLevelSetting.resetNfcForTesting();

        // Clean up cookies and permissions.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.COOKIES, BrowsingDataType.SITE_SETTINGS},
                    TimePeriod.ALL_TIME);
        });
        helper.waitForCallback(0);
    }

    private void initializeUpdateWaiter(final boolean expectGranted) {
        if (mPermissionUpdateWaiter != null) {
            mPermissionRule.getActivity().getActivityTab().removeObserver(mPermissionUpdateWaiter);
        }
        Tab tab = mPermissionRule.getActivity().getActivityTab();

        mPermissionUpdateWaiter = new PermissionUpdateWaiter(
                expectGranted ? "Granted" : "Denied", mPermissionRule.getActivity());
        tab.addObserver(mPermissionUpdateWaiter);
    }

    private BrowserContextHandle getBrowserContextHandle() {
        return Profile.getLastUsedRegularProfile();
    }

    private void setAllowLocation(final boolean enabled) {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        LocationProviderOverrider.setLocationProviderImpl(new MockLocationProvider());
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings websitePreferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            ChromeSwitchPreference location =
                    (ChromeSwitchPreference) websitePreferences.findPreference(
                            SingleCategorySettings.BINARY_TOGGLE_KEY);

            websitePreferences.onPreferenceChange(location, enabled);
            Assert.assertEquals("Location should be " + (enabled ? "allowed" : "blocked"), enabled,
                    WebsitePreferenceBridge.areAllLocationSettingsEnabled(
                            getBrowserContextHandle()));
            settingsActivity.finish();
        });
    }

    private void triggerEmbargoForOrigin(String url) throws TimeoutException {
        // Ignore notification request 4 times to enter embargo. 5th one ensures that notifications
        // are blocked by actually causing a deny-by-embargo.
        for (int i = 0; i < 5; i++) {
            mPermissionRule.loadUrl(url);
            mPermissionRule.runJavaScriptCodeInCurrentTab("requestPermissionAndRespond()");
        }
    }

    /**
     * Sets Allow Location Enabled to be true and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSetAllowLocationEnabled() throws Exception {
        setAllowLocation(true);

        initializeUpdateWaiter(true /* expectGranted */);

        // Launch a page that uses geolocation and make sure a permission prompt shows up.
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html", "", 0, false, true);
    }

    /**
     * Sets Allow Location Enabled to be false and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSetAllowLocationNotEnabled() throws Exception {
        setAllowLocation(false);

        // Launch a page that uses geolocation. No permission prompt is expected.
        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/chrome/test/data/geolocation/geolocation_on_load.html", "", 0, false, true);
    }

    private void setCookiesEnabled(final SettingsActivity settingsActivity, final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategorySettings websitePreferences =
                        (SingleCategorySettings) settingsActivity.getMainFragment();
                final FourStateCookieSettingsPreference cookies =
                        (FourStateCookieSettingsPreference) websitePreferences.findPreference(
                                SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(
                        cookies, enabled ? CookieSettingsState.ALLOW : CookieSettingsState.BLOCK);
                Assert.assertEquals("Cookies should be " + (enabled ? "allowed" : "blocked"),
                        doesAcceptCookies(), enabled);
            }

            private boolean doesAcceptCookies() {
                return WebsitePreferenceBridge.isCategoryEnabled(
                        getBrowserContextHandle(), ContentSettingsType.COOKIES);
            }
        });
    }

    private void setBlockCookiesSiteException(final SettingsActivity settingsActivity,
            final String hostname, final boolean thirdPartiesOnly) {
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategorySettings websitePreferences =
                        (SingleCategorySettings) settingsActivity.getMainFragment();

                Assert.assertTrue(doesAcceptCookies());
                if (thirdPartiesOnly) {
                    websitePreferences.onAddSite(SITE_WILDCARD, hostname);
                } else {
                    websitePreferences.onAddSite(hostname, SITE_WILDCARD);
                }
            }

            private boolean doesAcceptCookies() {
                return WebsitePreferenceBridge.isCategoryEnabled(
                        getBrowserContextHandle(), ContentSettingsType.COOKIES);
            }

        });
    }

    /**
     * Checks if the button representing the given state matches the managed expectation.
     */
    private void checkFourStateCookieToggleButtonEnabled(final SettingsActivity settingsActivity,
            final CookieSettingsState state, final boolean expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            FourStateCookieSettingsPreference fourStateCookieToggle =
                    (FourStateCookieSettingsPreference) preferences.findPreference(
                            SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);
            Assert.assertEquals(state + " button should be " + (expected ? "enabled" : "disabled"),
                    expected, fourStateCookieToggle.isButtonEnabledForTesting(state));
        });
    }

    private void checkDefaultCookiesSettingManaged(boolean expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Default Cookie Setting should be " + (expected ? "managed" : "unmanaged"),
                    expected,
                    WebsitePreferenceBridge.isContentSettingManaged(
                            getBrowserContextHandle(), ContentSettingsType.COOKIES));
        });
    }

    private void checkThirdPartyCookieBlockingManaged(boolean expected) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Third Party Cookie Blocking should be " + (expected ? "managed" : "unmanaged"),
                    expected,
                    UserPrefs.get(Profile.getLastUsedRegularProfile())
                            .isManagedPreference(COOKIE_CONTROLS_MODE));
        });
    }

    private void setGlobalToggleForCategory(
            final @SiteSettingsCategory.Type int type, final boolean enabled) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            ChromeSwitchPreference toggle = (ChromeSwitchPreference) preferences.findPreference(
                    SingleCategorySettings.BINARY_TOGGLE_KEY);
            preferences.onPreferenceChange(toggle, enabled);
        });
        settingsActivity.finish();
    }

    private void setGlobalTriStateToggleForCategory(
            final @SiteSettingsCategory.Type int type, final int newValue) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            TriStateSiteSettingsPreference triStateToggle =
                    (TriStateSiteSettingsPreference) preferences.findPreference(
                            SingleCategorySettings.TRI_STATE_TOGGLE_KEY);
            preferences.onPreferenceChange(triStateToggle, newValue);
        });
        settingsActivity.finish();
    }

    private void setFourStateCookieToggle(CookieSettingsState newState) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategorySettings preferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            FourStateCookieSettingsPreference fourStateCookieToggle =
                    (FourStateCookieSettingsPreference) preferences.findPreference(
                            SingleCategorySettings.FOUR_STATE_COOKIE_TOGGLE_KEY);
            preferences.onPreferenceChange(fourStateCookieToggle, newState);
        });
        settingsActivity.finish();
    }

    private void setEnablePopups(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.POPUPS, enabled);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Popups should be " + (enabled ? "allowed" : "blocked"), enabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.POPUPS));
        });
    }

    private void setEnableCamera(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.CAMERA, enabled);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Camera should be " + (enabled ? "allowed" : "blocked"), enabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.MEDIASTREAM_CAMERA));
        });
    }

    /**
     * Tests that the Preferences designated by keys in |expectedKeys|, and only
     * these preferences, will be shown for the category specified by |type|. The
     * order of Preferences matters.
     */
    private void checkPreferencesForCategory(
            final @SiteSettingsCategory.Type int type, String[] expectedKeys) {
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceFragmentCompat preferenceFragment =
                    (PreferenceFragmentCompat) settingsActivity.getMainFragment();
            PreferenceScreen preferenceScreen = preferenceFragment.getPreferenceScreen();
            int preferenceCount = preferenceScreen.getPreferenceCount();

            ArrayList<String> actualKeys = new ArrayList<>();
            for (int index = 0; index < preferenceCount; index++) {
                Preference preference = preferenceScreen.getPreference(index);
                String key = preference.getKey();
                // Not all Preferences have keys. For example, the list of websites below the
                // toggles, which are dynamically added. Ignore those.
                if (key != null) actualKeys.add(key);
            }

            Assert.assertTrue(
                    actualKeys.toString() + " should match " + Arrays.toString(expectedKeys),
                    Arrays.equals(actualKeys.toArray(), expectedKeys));
        });
        settingsActivity.finish();
    }

    /**
     * Allows cookies to be set and ensures that they are.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesNotBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(settingsActivity, true);
        settingsActivity.finish();

        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie still is set.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Blocks cookies from being set and ensures that no cookies can be set.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(settingsActivity, false);
        settingsActivity.finish();

        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Blocks specific sites from setting cookies and ensures that no cookies can be set.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    // TODO(eokoyomon) figure out how to set and test third party cookie setting in this test
    public void testSiteExceptionCookiesBlocked() throws Exception {
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(settingsActivity, true);
        settingsActivity.finish();

        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Check cookies can be set for this website when there is no rule.
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Set specific rule to block site and ensure it cannot set cookies.
        mPermissionRule.loadUrl(url);
        mPermissionRule.runJavaScriptCodeInCurrentTab("clearCookie()");
        settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setBlockCookiesSiteException(settingsActivity, url, false);
        settingsActivity.finish();
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set a cookie and check that it is removed when a site is cleared.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testClearCookies() throws Exception {
        final String url = mPermissionRule.getURL("/chrome/test/data/android/cookie.html");

        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        resetSite(WebsiteAddress.create(url));

        // Load the page again and ensure the cookie is gone.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set cookies for domains and check that they are removed when a site is cleared.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @FlakyTest(message = "https://crbug.com/1112409")
    public void testClearDomainCookies() throws Exception {
        final String url = mPermissionRule.getURLWithHostName(
                "test.example.com", "/chrome/test/data/android/cookie.html");

        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mPermissionRule.runJavaScriptCodeInCurrentTab("setCookie(\".test.example.com\")");
        Assert.assertEquals("\"Foo=Bar; Foo=Bar\"",
                mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        resetSite(WebsiteAddress.create("test.example.com"));

        // Load the page again and ensure the cookie is gone.
        mPermissionRule.loadUrl(url);
        Assert.assertEquals("\"\"", mPermissionRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set the cookie content setting to allow through policy and ensure the correct radio buttons
     * are enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "1") })
    public void testDefaultCookiesSettingManagedAllow() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to ALLOW) while ThirdPartyCookieBlocking is not
        // managed. This means that every button other than BLOCK is enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.ALLOW, true);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, true);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY, true);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.BLOCK, false);
        settingsActivity.finish();
    }

    /**
     * Set the cookie content setting to block through a policy and ensure the correct radio buttons
     * are enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "DefaultCookiesSetting", string = "2") })
    public void testDefaultCookiesSettingManagedBlock() {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting is managed (and set to BLOCK) while ThirdPartyCookieBlocking is not
        // managed. This means cookies should always be blocked, so the user cannot choose any other
        // options and all buttons except the active one should be disabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.ALLOW, false);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, false);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY, false);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.BLOCK, true);
        settingsActivity.finish();
    }

    /**
     * Enable third-party cookie blocking through policy and ensure the correct radio buttons are
     * enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "BlockThirdPartyCookies", string = "true") })
    public void
    testBlockThirdPartyCookiesManagedTrue() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to true) while the ContentSetting is not
        // managed. This means a user can choose only between BLOCK_THIRD_PARTY and BLOCK, so only
        // these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.ALLOW, false);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, false);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY, true);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.BLOCK, true);
        settingsActivity.finish();
    }

    /**
     * Disable third-party cookie blocking through policy and ensure the correct radio buttons are
     * enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({ @Policies.Item(key = "BlockThirdPartyCookies", string = "false") })
    public void
    testBlockThirdPartyCookiesManagedFalse() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(true);
        // ThirdPartyCookieBlocking is managed (and set to false) while the ContentSetting is not
        // managed. This means a user can only choose to ALLOW all cookies or BLOCK all cookies, so
        // only these should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.ALLOW, true);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, false);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY, false);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.BLOCK, true);
        settingsActivity.finish();
    }

    /**
     * Set both the cookie content setting and third-party cookie blocking through policy and ensure
     * the correct radio buttons are enabled.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @Policies.Add({
        @Policies.Item(key = "DefaultCookiesSetting", string = "1")
        , @Policies.Item(key = "BlockThirdPartyCookies", string = "false")
    })
    public void
    testAllCookieSettingsManaged() throws Exception {
        checkDefaultCookiesSettingManaged(true);
        checkThirdPartyCookieBlockingManaged(true);
        // The ContentSetting and ThirdPartyCookieBlocking are managed. This means a user has a
        // fixed setting for cookies that they cannot change. Therefore, all buttons except the
        // selected one should be disabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.ALLOW, true);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, false);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY, false);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.BLOCK, false);
        settingsActivity.finish();
    }

    /**
     * Ensure no radio buttons are enforced when cookie settings are unmanaged.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testNoCookieSettingsManaged() throws Exception {
        checkDefaultCookiesSettingManaged(false);
        checkThirdPartyCookieBlockingManaged(false);
        // The ContentSetting and ThirdPartyCookieBlocking are unmanaged. This means all buttons
        // should be enabled.
        SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.ALLOW, true);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO, true);
        checkFourStateCookieToggleButtonEnabled(
                settingsActivity, CookieSettingsState.BLOCK_THIRD_PARTY, true);
        checkFourStateCookieToggleButtonEnabled(settingsActivity, CookieSettingsState.BLOCK, true);
        settingsActivity.finish();
    }

    private void resetSite(WebsiteAddress address) {
        Website website = new Website(address, address);
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();
            websitePreferences.resetSite();
        });
        settingsActivity.finish();
    }

    /**
     * Sets Allow Popups Enabled to be false and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsBlocked() {
        setEnablePopups(false);

        // Test that the popup doesn't open.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(1, getTabCount());
    }

    /**
     * Sets Allow Popups Enabled to be true and make sure it is set correctly.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsNotBlocked() {
        setEnablePopups(true);

        // Test that a popup opens.
        mPermissionRule.setUpUrl("/chrome/test/data/android/popup.html");
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(2, getTabCount());
    }

    /**
     * Test that showing the Site Settings menu doesn't crash (crbug.com/610576).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenu() {
        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        settingsActivity.finish();
    }

    /**
     * Tests that only expected Preferences are shown for a category.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @EnableFeatures("QuietNotificationPrompts")
    @DisabledTest(message = "Flaky. crbug.com/1030218")
    public void testOnlyExpectedPreferencesShown() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        NfcSystemLevelSetting.setNfcSettingForTesting(true);

        // If you add a category in the SiteSettings UI, please add a test for it below.
        Assert.assertEquals(22, SiteSettingsCategory.Type.NUM_ENTRIES);

        String[] nullArray = new String[0];
        String[] binaryToggle = new String[] {"binary_toggle"};
        String[] binaryToggleWithException = new String[] {"binary_toggle", "add_exception"};
        String[] binaryToggleWithAllowed = new String[] {"binary_toggle", "allowed_group"};
        String[] binaryToggleWithOsWarningExtra =
                new String[] {"binary_toggle", "os_permissions_warning_extra"};
        String[] cookie =
                new String[] {"cookie_info_text", "four_state_cookie_toggle", "add_exception"};
        String[] protectedMedia = new String[] {"tri_state_toggle", "protected_content_learn_more"};
        String[] notifications_enabled;
        String[] notifications_disabled;
        // The "notifications_vibrate" option has been removed in Android O but is present in
        // earlier versions.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            notifications_enabled = new String[] {"binary_toggle", "notifications_quiet_ui",
                    "notifications_vibrate", "allowed_group"};
            notifications_disabled =
                    new String[] {"binary_toggle", "notifications_vibrate", "allowed_group"};
        } else {
            notifications_enabled =
                    new String[] {"binary_toggle", "notifications_quiet_ui", "allowed_group"};
            notifications_disabled = binaryToggleWithAllowed;
        }

        HashMap<Integer, Pair<String[], String[]>> testCases =
                new HashMap<Integer, Pair<String[], String[]>>();
        testCases.put(SiteSettingsCategory.Type.ADS, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.ALL_SITES, new Pair<>(nullArray, nullArray));
        testCases.put(SiteSettingsCategory.Type.AUGMENTED_REALITY,
                new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                new Pair<>(binaryToggleWithException, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.BACKGROUND_SYNC,
                new Pair<>(binaryToggleWithException, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.CAMERA, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.CLIPBOARD, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.COOKIES, new Pair<>(cookie, cookie));
        testCases.put(SiteSettingsCategory.Type.DEVICE_LOCATION,
                new Pair<>(binaryToggleWithAllowed, binaryToggleWithAllowed));
        testCases.put(SiteSettingsCategory.Type.IDLE_DETECTION,
                new Pair<>(binaryToggleWithAllowed, binaryToggleWithAllowed));
        testCases.put(SiteSettingsCategory.Type.JAVASCRIPT,
                new Pair<>(binaryToggleWithException, binaryToggleWithException));
        testCases.put(SiteSettingsCategory.Type.MICROPHONE, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.NFC, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.NOTIFICATIONS,
                new Pair<>(notifications_disabled, notifications_enabled));
        testCases.put(SiteSettingsCategory.Type.POPUPS, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.SENSORS, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.SOUND,
                new Pair<>(binaryToggleWithException, binaryToggleWithException));
        testCases.put(SiteSettingsCategory.Type.USB, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.USE_STORAGE, new Pair<>(nullArray, nullArray));
        testCases.put(
                SiteSettingsCategory.Type.VIRTUAL_REALITY, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.BLUETOOTH, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                new Pair<>(binaryToggle, binaryToggle));

        for (@SiteSettingsCategory.Type int key = 0; key < SiteSettingsCategory.Type.NUM_ENTRIES;
                ++key) {
            // Protected media has a tri-state global toggle so it needs to be handled slightly
            // differently.
            if (key == SiteSettingsCategory.Type.PROTECTED_MEDIA) {
                setGlobalTriStateToggleForCategory(key, ContentSettingValues.ALLOW);
                checkPreferencesForCategory(key, protectedMedia);
                setGlobalTriStateToggleForCategory(key, ContentSettingValues.ASK);
                checkPreferencesForCategory(key, protectedMedia);
                setGlobalTriStateToggleForCategory(key, ContentSettingValues.BLOCK);
                checkPreferencesForCategory(key, protectedMedia);
                continue;
            }

            // Cookies has a four-state radio preference so it needs to be handled slightly
            // differently.
            if (key == SiteSettingsCategory.Type.COOKIES) {
                setFourStateCookieToggle(CookieSettingsState.ALLOW);
                checkPreferencesForCategory(key, cookie);
                setFourStateCookieToggle(CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO);
                checkPreferencesForCategory(key, cookie);
                setFourStateCookieToggle(CookieSettingsState.BLOCK_THIRD_PARTY);
                checkPreferencesForCategory(key, cookie);
                setFourStateCookieToggle(CookieSettingsState.BLOCK);
                checkPreferencesForCategory(key, cookie);
                continue;
            }

            Pair<String[], String[]> values = testCases.get(key);

            if (key == SiteSettingsCategory.Type.ALL_SITES
                    || key == SiteSettingsCategory.Type.USE_STORAGE) {
                checkPreferencesForCategory(key, values.first);
                continue;
            }

            // Disable the category and check for the right preferences.
            setGlobalToggleForCategory(key, false);
            checkPreferencesForCategory(key, values.first);
            // Re-enable the category and check for the right preferences.
            setGlobalToggleForCategory(key, true);
            checkPreferencesForCategory(key, values.second);
        }

        // Disable system location setting and check for the right preferences.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        checkPreferencesForCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION, binaryToggleWithOsWarningExtra);
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);

        // Disable system nfc setting and check for the right preferences.
        NfcSystemLevelSetting.setNfcSettingForTesting(false);
        checkPreferencesForCategory(SiteSettingsCategory.Type.NFC, binaryToggleWithOsWarningExtra);
        NfcSystemLevelSetting.setNfcSettingForTesting(null);
    }

    /**
     * Tests system NFC support in Preferences.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSystemNfcSupport() {
        String[] binaryToggleWithOsWarningExtra =
                new String[] {"binary_toggle", "os_permissions_warning_extra"};

        // Disable system nfc support and check for the right preferences.
        NfcSystemLevelSetting.setNfcSupportForTesting(false);
        checkPreferencesForCategory(SiteSettingsCategory.Type.NFC, binaryToggleWithOsWarningExtra);
    }

    /**
     * Tests that {@link SingleWebsiteSettings#resetSite} doesn't crash
     * (see e.g. the crash on host names in issue 600232).
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDoesntCrash() {
        WebsiteAddress address = WebsiteAddress.create("example.com");
        resetSite(address);
    }

    /**
     * Sets Allow Camera Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add(ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM)
    public void testCameraBlocked() throws Exception {
        setEnableCamera(false);

        // Test that the camera permission doesn't get requested.
        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: true, audio: false});", 0, false, true);
    }

    /**
     * Sets Allow Mic Enabled to be false and make sure it is set correctly.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testMicBlocked() throws Exception {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.MICROPHONE, false);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Mic should be blocked",
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.MEDIASTREAM_MIC));
        });

        // Test that the microphone permission doesn't get requested.
        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: false, audio: true});", 0, true, true);
    }

    /**
     * Sets Allow Camera Enabled to be true and make sure it is set correctly.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testCameraNotBlocked() throws Exception {
        setEnableCamera(true);

        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: true, audio: false});", 0, false, true);
    }

    /**
     * Sets Allow Mic Enabled to be true and make sure it is set correctly.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    public void testMicNotBlocked() throws Exception {
        setEnableCamera(true);

        // Launch a page that uses the microphone and make sure a permission prompt shows up.
        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/content/test/data/media/getusermedia.html",
                "getUserMediaAndStop({video: false, audio: true});", 0, true, true);
    }

    /**
     * Helper function to test allowing and blocking background sync.
     * @param enabled true to test enabling background sync, false to test disabling the feature.
     */
    private void doTestBackgroundSyncPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.BACKGROUND_SYNC, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Background Sync should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.BACKGROUND_SYNC),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBackgroundSync() {
        doTestBackgroundSyncPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBackgroundSync() {
        doTestBackgroundSyncPermission(false);
    }

    /**
     * Helper function to test allowing and blocking the USB chooser.
     * @param enabled true to test enabling the USB chooser, false to test disabling the feature.
     */
    private void doTestUsbGuardPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.USB, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("USB should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.USB_GUARD),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowUsb() {
        doTestUsbGuardPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockUsb() {
        doTestUsbGuardPermission(false);
    }

    /**
     * Helper function to test allowing and blocking automatic downloads.
     * @param enabled true to test enabling automatic downloads, false to test disabling the
     * feature.
     */
    private void doTestAutomaticDownloadsPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Automatic Downloads should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.AUTOMATIC_DOWNLOADS),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAutomaticDownloads() {
        doTestAutomaticDownloadsPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAutomaticDownloads() {
        doTestAutomaticDownloadsPermission(false);
    }

    /**
     * Helper function to test allowing and blocking the Bluetooth scanning.
     * @param enabled true to test enabling the Bluetooth scanning, false to test disabling the
     *         feature.
     */
    private void doTestBluetoothScanningPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.BLUETOOTH_SCANNING, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Bluetooth scanning should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.BLUETOOTH_SCANNING),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBluetoothScanning() {
        doTestBluetoothScanningPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBluetoothScanning() {
        doTestBluetoothScanningPermission(false);
    }

    /**
     * Helper function to test allowing and blocking the Bluetooth chooser.
     * @param enabled true to test enabling the Bluetooth chooser, false to test disabling the
     *         feature.
     */
    private void doTestBluetoothGuardPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.BLUETOOTH, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Bluetooth scanning should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.BLUETOOTH_GUARD),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowBluetoothGuard() {
        doTestBluetoothScanningPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockBluetoothGuard() {
        doTestBluetoothScanningPermission(false);
    }

    /**
     * Helper function to test allowing and blocking NFC feature.
     * @param enabled true to test enabling NFC feature, false to test disabling the
     *         feature.
     */
    private void doTestNfcPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.NFC, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("NFC should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.NFC),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowNfc() {
        doTestNfcPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockNfc() {
        doTestNfcPermission(false);
    }

    /**
     * Helper function to test allowing and blocking AUGMENTED_REALITY feature.
     * @param enabled true to test enabling AUGMENTED_REALITY feature, false to test disabling the
     *         feature.
     */
    private void doTestArPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.AUGMENTED_REALITY, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("AR should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.AR),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowAr() {
        doTestArPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockAr() {
        doTestArPermission(false);
    }

    /**
     * Helper function to test allowing and blocking VIRTUAL_REALITY feature.
     * @param enabled true to test enabling VIRTUAL_REALITY feature, false to test disabling the
     *         feature.
     */
    private void doTestVrPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.VIRTUAL_REALITY, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("VR should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.VR),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowVr() {
        doTestVrPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockVr() {
        doTestVrPermission(false);
    }

    private int getTabCount() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mPermissionRule.getActivity().getTabModelSelector().getTotalTabCount());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.
    Build(message = "Flaky, see crbug.com/1170671", sdk_is_less_than = Build.VERSION_CODES.Q)
    public void testEmbargoedNotificationSiteSettings() throws Exception {
        final String url = mPermissionRule.getURLWithHostName(
                "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(url);

        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Context context = InstrumentationRegistry.getTargetContext();
        Intent intent = settingsLauncher.createSettingsActivityIntent(context,
                SingleWebsiteSettings.class.getName(),
                SingleWebsiteSettings.createFragmentArgsForSite(url));
        final SettingsActivity settingsActivity =
                (SettingsActivity) InstrumentationRegistry.getInstrumentation().startActivitySync(
                        intent);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();

            final Preference notificationPreference =
                    websitePreferences.findPreference("push_notifications_list");

            Assert.assertEquals(context.getString(R.string.automatically_blocked),
                    notificationPreference.getSummary());

            websitePreferences.launchOsChannelSettingsFromPreference(notificationPreference);

            // Ensure that a proper separate channel has indeed been created to allow the user to
            // alter the setting.
            Assert.assertNotEquals(ChromeChannelDefinitions.ChannelId.SITES,
                    SiteChannelsManager.getInstance().getChannelIdForOrigin(
                            Origin.createOrThrow(url).toString()));
        });

        settingsActivity.finish();
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "https://crbug.com/1094934")
    public void testEmbargoedNotificationCategorySiteSettings() throws Exception {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;
        final String urlToEmbargo = mPermissionRule.getURLWithHostName(
                "example.com", "/chrome/test/data/notifications/notification_tester.html");

        triggerEmbargoForOrigin(urlToEmbargo);

        final String urlToBlock = mPermissionRule.getURLWithHostName(
                "exampleToBlock.com", "/chrome/test/data/notifications/notification_tester.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridgeJni.get().setSettingForOrigin(getBrowserContextHandle(),
                    ContentSettingsType.NOTIFICATIONS, urlToBlock, urlToBlock,
                    ContentSettingValues.BLOCK);
        });

        final SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.NOTIFICATIONS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean blockedByEmbargo =
                    WebsitePreferenceBridgeJni.get().isNotificationEmbargoedForOrigin(
                            getBrowserContextHandle(), urlToEmbargo);
            Assert.assertTrue(blockedByEmbargo);

            final String blockedGroupKey = "blocked_group";
            // Click on Blocked group in Category Settings. By default Blocked is closed, to be able
            // to find any origins inside, Blocked should be opened.
            SingleCategorySettings websitePreferences =
                    (SingleCategorySettings) settingsActivity.getMainFragment();
            websitePreferences.findPreference(blockedGroupKey).performClick();

            // After triggering onClick on Blocked group, all UI will be discarded and reinitialized
            // from scratch. Init all variables again, otherwise it will use stale information.
            websitePreferences = (SingleCategorySettings) settingsActivity.getMainFragment();
            ExpandablePreferenceGroup blockedGroup =
                    (ExpandablePreferenceGroup) websitePreferences.findPreference(blockedGroupKey);

            Assert.assertTrue(blockedGroup.isExpanded());
            // Only |url| has been added under embargo.
            Assert.assertEquals(2, blockedGroup.getPreferenceCount());

            Assert.assertEquals(InstrumentationRegistry.getTargetContext().getString(
                                        R.string.automatically_blocked),
                    blockedGroup.getPreference(0).getSummary());

            // Blocked origin should has no summary.
            Assert.assertNull(blockedGroup.getPreference(1).getSummary());
        });
        settingsActivity.finish();
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(message = "EME not working before M", sdk_is_less_than = Build.VERSION_CODES.M)
    public void testProtectedContentDefaultOption() throws Exception {
        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(message = "EME not working before M", sdk_is_less_than = Build.VERSION_CODES.M)
    public void testProtectedContentAskAllow() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.ASK);

        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runAllowTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(message = "EME not working before M", sdk_is_less_than = Build.VERSION_CODES.M)
    public void testProtectedContentAskBlocked() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.ASK);

        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runDenyTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(message = "EME not working before M", sdk_is_less_than = Build.VERSION_CODES.M)
    public void testProtectedContentBlocked() throws Exception {
        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.BLOCK);

        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    @Test
    @MediumTest
    @Feature({"Preferences"})
    @DisableIf.Build(message = "EME not working before M", sdk_is_less_than = Build.VERSION_CODES.M)
    public void testProtectedContentAllowThenBlock() throws Exception {
        initializeUpdateWaiter(true /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);

        setGlobalTriStateToggleForCategory(
                SiteSettingsCategory.Type.PROTECTED_MEDIA, ContentSettingValues.BLOCK);

        initializeUpdateWaiter(false /* expectGranted */);
        mPermissionRule.runNoPromptTest(mPermissionUpdateWaiter,
                "/content/test/data/android/eme_permissions.html", "requestEME()", 0, true, true);
    }

    /**
     * Helper function to test allowing and blocking IDLE_DETECTION feature.
     * @param enabled true to test enabling IDLE_DETECTION feature, false to test disabling the
     *         feature.
     */
    private void doTestIdleDetectionPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.IDLE_DETECTION, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Idle detection should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(
                            getBrowserContextHandle(), ContentSettingsType.IDLE_DETECTION),
                    enabled);
        });
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testAllowIdleDetection() {
        doTestIdleDetectionPermission(true);
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testBlockIdleDetection() {
        doTestIdleDetectionPermission(false);
    }
}
