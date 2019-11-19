// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceScreen;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.concurrent.Callable;

/**
 * Tests for everything under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
})
public class SiteSettingsPreferencesTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    private void setAllowLocation(final boolean enabled) {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        final Preferences preferenceActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.DEVICE_LOCATION);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategoryPreferences websitePreferences =
                    (SingleCategoryPreferences) preferenceActivity.getMainFragment();
            ChromeSwitchPreference location =
                    (ChromeSwitchPreference) websitePreferences.findPreference(
                            SingleCategoryPreferences.BINARY_TOGGLE_KEY);

            websitePreferences.onPreferenceChange(location, enabled);
            Assert.assertEquals("Location should be " + (enabled ? "allowed" : "blocked"), enabled,
                    LocationSettings.getInstance().areAllLocationSettingsEnabled());
            preferenceActivity.finish();
        });
    }

    private InfoBarTestAnimationListener setInfoBarAnimationListener() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<InfoBarTestAnimationListener>() {
                    @Override
                    public InfoBarTestAnimationListener call() {
                        InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
                        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
                        container.addAnimationListener(listener);
                        return listener;
                    }
                });
    }

    /**
     * Sets Allow Location Enabled to be true and make sure it is set correctly.
     *
     * TODO(timloh): Update this test once modals are enabled everywhere.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testSetAllowLocationEnabled() throws Exception {
        setAllowLocation(true);
        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses geolocation and make sure an infobar shows up.
        mActivityTestRule.loadUrl(
                mTestServer.getURL("/chrome/test/data/geolocation/geolocation_on_load.html"));
        listener.addInfoBarAnimationFinished("InfoBar not added.");

        Assert.assertEquals("Wrong infobar count", 1, mActivityTestRule.getInfoBars().size());
    }

    /**
     * Sets Allow Location Enabled to be false and make sure it is set correctly.
     *
     * TODO(timloh): Update this test once modals are enabled everywhere.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testSetAllowLocationNotEnabled() {
        setAllowLocation(false);

        // Launch a page that uses geolocation.
        mActivityTestRule.loadUrl(
                mTestServer.getURL("/chrome/test/data/geolocation/geolocation_on_load.html"));

        // No infobars are expected.
        Assert.assertTrue(mActivityTestRule.getInfoBars().isEmpty());
    }

    private void setCookiesEnabled(final Preferences preferenceActivity, final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getMainFragment();
                final ChromeSwitchPreference cookies =
                        (ChromeSwitchPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.BINARY_TOGGLE_KEY);
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

                if (thirdPartyCookies != null) {
                    Assert.assertEquals("Third-party cookie toggle should be "
                                    + (doesAcceptCookies() ? "enabled" : " disabled"),
                            doesAcceptCookies(), thirdPartyCookies.isEnabled());
                }
                websitePreferences.onPreferenceChange(cookies, enabled);
                Assert.assertEquals("Cookies should be " + (enabled ? "allowed" : "blocked"),
                        doesAcceptCookies(), enabled);
            }

            private boolean doesAcceptCookies() {
                return WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.COOKIES);
            }
        });
    }

    private void setThirdPartyCookiesEnabled(final Preferences preferenceActivity,
            final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleCategoryPreferences websitePreferences =
                    (SingleCategoryPreferences) preferenceActivity.getMainFragment();
            final ChromeBaseCheckBoxPreference thirdPartyCookies =
                    (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                            SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

            websitePreferences.onPreferenceChange(thirdPartyCookies, enabled);
            Assert.assertEquals(
                    "Third-party cookies should be " + (enabled ? "allowed" : "blocked"),
                    PrefServiceBridge.getInstance().getBoolean(Pref.BLOCK_THIRD_PARTY_COOKIES),
                    enabled);
        });
    }

    private void setGlobalToggleForCategory(
            final @SiteSettingsCategory.Type int type, final boolean enabled) {
        final Preferences preferenceActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleCategoryPreferences preferences =
                    (SingleCategoryPreferences) preferenceActivity.getMainFragment();
            ChromeSwitchPreference toggle = (ChromeSwitchPreference) preferences.findPreference(
                    SingleCategoryPreferences.BINARY_TOGGLE_KEY);
            preferences.onPreferenceChange(toggle, enabled);
        });
        preferenceActivity.finish();
    }

    private void setEnablePopups(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.POPUPS, enabled);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Popups should be " + (enabled ? "allowed" : "blocked"), enabled,
                    WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.POPUPS));
        });
    }

    private void setEnableCamera(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.CAMERA, enabled);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Camera should be " + (enabled ? "allowed" : "blocked"), enabled,
                    WebsitePreferenceBridge.isCategoryEnabled(
                            ContentSettingsType.MEDIASTREAM_CAMERA));
        });
    }

    /**
     * Tests that the Preferences designated by keys in |expectedKeys|, and only
     * these preferences, will be shown for the category specified by |type|. The
     * order of Preferences matters.
     *
     * @throws Exception
     */
    private void checkPreferencesForCategory(
            final @SiteSettingsCategory.Type int type, String[] expectedKeys) {
        final Preferences preferenceActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(type);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceFragmentCompat preferenceFragment =
                    (PreferenceFragmentCompat) preferenceActivity.getMainFragment();
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
        preferenceActivity.finish();
    }

    // TODO(finnur): Write test for Autoplay.

    /**
     * Tests that disabling cookies turns off the third-party cookie toggle.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testThirdPartyCookieToggleGetsDisabled() {
        Preferences preferenceActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(preferenceActivity, true);
        setThirdPartyCookiesEnabled(preferenceActivity, false);
        setThirdPartyCookiesEnabled(preferenceActivity, true);
        setCookiesEnabled(preferenceActivity, false);
        preferenceActivity.finish();
    }

    /**
     * Allows cookies to be set and ensures that they are.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesNotBlocked() throws Exception {
        Preferences preferenceActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(preferenceActivity, true);
        preferenceActivity.finish();

        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mActivityTestRule.loadUrl(url + "#clear");
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie still is set.
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals(
                "\"Foo=Bar\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Blocks cookies from being set and ensures that no cookies can be set.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testCookiesBlocked() throws Exception {
        Preferences preferenceActivity =
                SiteSettingsTestUtils.startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
        setCookiesEnabled(preferenceActivity, false);
        preferenceActivity.finish();

        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");

        // Load the page and clear any set cookies.
        mActivityTestRule.loadUrl(url + "#clear");
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        // Load the page again and ensure the cookie remains unset.
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set a cookie and check that it is removed when a site is cleared.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testClearCookies() throws Exception {
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");

        mActivityTestRule.loadUrl(url);
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("setCookie()");
        Assert.assertEquals(
                "\"Foo=Bar\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        resetSite(WebsiteAddress.create(url));

        // Load the page again and ensure the cookie is gone.
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    /**
     * Set cookies for domains and check that they are removed when a site is cleared.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testClearDomainCookies() throws Exception {
        final String url = mTestServer.getURLWithHostName(
                "test.example.com", "/chrome/test/data/android/cookie.html");

        mActivityTestRule.loadUrl(url);
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".example.com\")");
        mActivityTestRule.runJavaScriptCodeInCurrentTab("setCookie(\".test.example.com\")");
        Assert.assertEquals("\"Foo=Bar; Foo=Bar\"",
                mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));

        resetSite(WebsiteAddress.create("test.example.com"));

        // Load the page again and ensure the cookie is gone.
        mActivityTestRule.loadUrl(url);
        Assert.assertEquals("\"\"", mActivityTestRule.runJavaScriptCodeInCurrentTab("getCookie()"));
    }

    private void resetSite(WebsiteAddress address) {
        Website website = new Website(address, address);
        final Preferences preferenceActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SingleWebsitePreferences websitePreferences =
                    (SingleWebsitePreferences) preferenceActivity.getMainFragment();
            websitePreferences.resetSite();
        });
        preferenceActivity.finish();
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
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/popup.html"));
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
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/popup.html"));
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
        final Preferences preferenceActivity = SiteSettingsTestUtils.startSiteSettingsMenu("");
        preferenceActivity.finish();
    }

    /**
     * Test the Media Menu.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testMediaMenu() {
        final Preferences preferenceActivity =
                SiteSettingsTestUtils.startSiteSettingsMenu(SiteSettingsPreferences.MEDIA_KEY);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SiteSettingsPreferences siteSettings =
                    (SiteSettingsPreferences) preferenceActivity.getMainFragment();

            SiteSettingsPreference allSites = (SiteSettingsPreference) siteSettings.findPreference(
                    SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ALL_SITES));
            Assert.assertEquals(null, allSites);

            SiteSettingsPreference autoplay = (SiteSettingsPreference) siteSettings.findPreference(
                    SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.AUTOPLAY));
            Assert.assertFalse(autoplay == null);

            SiteSettingsPreference protectedContent =
                    (SiteSettingsPreference) siteSettings.findPreference(
                            SiteSettingsCategory.preferenceKey(
                                    SiteSettingsCategory.Type.PROTECTED_MEDIA));
            Assert.assertFalse(protectedContent == null);

            preferenceActivity.finish();
        });
    }

    /**
     * Tests that only expected Preferences are shown for a category.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesShown() {
        // If you add a category in the SiteSettings UI, please add a test for it below.
        Assert.assertEquals(20, SiteSettingsCategory.Type.NUM_ENTRIES);

        String[] nullArray = new String[0];
        String[] binaryToggle = new String[] {"binary_toggle"};
        String[] binaryToggleWithException = new String[] {"binary_toggle", "add_exception"};
        String[] binaryToggleWithAllowed = new String[] {"binary_toggle", "allowed_group"};
        String[] cookie = new String[] {"binary_toggle", "third_party_cookies", "add_exception"};
        String[] protectedMedia = new String[] {"tri_state_toggle", "protected_content_learn_more"};

        HashMap<Integer, Pair<String[], String[]>> testCases =
                new HashMap<Integer, Pair<String[], String[]>>();
        testCases.put(SiteSettingsCategory.Type.ADS, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.ALL_SITES, new Pair<>(nullArray, nullArray));
        testCases.put(SiteSettingsCategory.Type.AUTOMATIC_DOWNLOADS,
                new Pair<>(binaryToggleWithException, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.AUTOPLAY,
                new Pair<>(binaryToggleWithException, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.BACKGROUND_SYNC,
                new Pair<>(binaryToggleWithException, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.CAMERA, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.CLIPBOARD, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.COOKIES, new Pair<>(cookie, cookie));
        testCases.put(SiteSettingsCategory.Type.DEVICE_LOCATION,
                new Pair<>(binaryToggleWithAllowed, binaryToggleWithAllowed));
        testCases.put(SiteSettingsCategory.Type.JAVASCRIPT,
                new Pair<>(binaryToggleWithException, binaryToggleWithException));
        testCases.put(SiteSettingsCategory.Type.MICROPHONE, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.NFC, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.NOTIFICATIONS,
                new Pair<>(binaryToggleWithAllowed, binaryToggleWithAllowed));
        testCases.put(SiteSettingsCategory.Type.POPUPS, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.PROTECTED_MEDIA,
                new Pair<>(protectedMedia, protectedMedia));
        testCases.put(SiteSettingsCategory.Type.SENSORS, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.SOUND,
                new Pair<>(binaryToggleWithException, binaryToggleWithException));
        testCases.put(SiteSettingsCategory.Type.USB, new Pair<>(binaryToggle, binaryToggle));
        testCases.put(SiteSettingsCategory.Type.USE_STORAGE, new Pair<>(nullArray, nullArray));
        testCases.put(SiteSettingsCategory.Type.BLUETOOTH_SCANNING,
                new Pair<>(binaryToggle, binaryToggle));

        for (@SiteSettingsCategory.Type int key = 0; key < SiteSettingsCategory.Type.NUM_ENTRIES;
                ++key) {
            Pair<String[], String[]> values = testCases.get(key);

            if (key == SiteSettingsCategory.Type.ALL_SITES
                    || key == SiteSettingsCategory.Type.USE_STORAGE) {
                checkPreferencesForCategory(key, values.first);
                return;
            }

            // Disable the category and check for the right preferences.
            setGlobalToggleForCategory(key, false);
            checkPreferencesForCategory(key, values.first);
            // Re-enable the category and check for the right preferences.
            setGlobalToggleForCategory(key, true);
            checkPreferencesForCategory(key, values.second);
        }

        // Location is not the only system-managed permission, but having one test for a
        // system-managed permission has been shown to catch stray permissons appearing where they
        // should not.
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        checkPreferencesForCategory(SiteSettingsCategory.Type.DEVICE_LOCATION, binaryToggle);
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
    }

    /**
     * Tests that {@link SingleWebsitePreferences#resetSite} doesn't crash
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
        mActivityTestRule.loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "getUserMediaAndStop({video: true, audio: false});");

        // No infobars are expected.
        Assert.assertTrue(mActivityTestRule.getInfoBars().isEmpty());
    }

    /**
     * Sets Allow Mic Enabled to be false and make sure it is set correctly.
     *
     * TODO(timloh): Update this test once modals are enabled everywhere.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testMicBlocked() throws Exception {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.MICROPHONE, false);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse("Mic should be blocked",
                    WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.MEDIASTREAM_MIC));
        });

        // Test that the microphone permission doesn't get requested.
        mActivityTestRule.loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "getUserMediaAndStop({video: false, audio: true});");

        // No infobars are expected.
        Assert.assertTrue(mActivityTestRule.getInfoBars().isEmpty());
    }

    /**
     * Sets Allow Camera Enabled to be true and make sure it is set correctly.
     *
     * TODO(timloh): Update this test once modals are enabled everywhere.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testCameraNotBlocked() throws Exception {
        setEnableCamera(true);

        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses camera and make sure an infobar shows up.
        mActivityTestRule.loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "getUserMediaAndStop({video: true, audio: false});");

        listener.addInfoBarAnimationFinished("InfoBar not added.");
        Assert.assertEquals("Wrong infobar count", 1, mActivityTestRule.getInfoBars().size());
    }

    /**
     * Sets Allow Mic Enabled to be true and make sure it is set correctly.
     *
     * TODO(timloh): Update this test once modals are enabled everywhere.
     *
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM})
    @DisabledTest(message = "Modals are now enabled and test needs to be reworked crbug.com/935900")
    public void testMicNotBlocked() throws Exception {
        setEnableCamera(true);

        InfoBarTestAnimationListener listener = setInfoBarAnimationListener();

        // Launch a page that uses the microphone and make sure an infobar shows up.
        mActivityTestRule.loadUrl(mTestServer.getURL("/content/test/data/media/getusermedia.html"));
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "getUserMediaAndStop({video: false, audio: true});");

        listener.addInfoBarAnimationFinished("InfoBar not added.");
        Assert.assertEquals("Wrong infobar count", 1, mActivityTestRule.getInfoBars().size());
    }

    /**
     * Helper function to test allowing and blocking background sync.
     * @param enabled true to test enabling background sync, false to test disabling the feature.
     */
    private void doTestBackgroundSyncPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.BACKGROUND_SYNC, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Background Sync should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.BACKGROUND_SYNC),
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
                    WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.USB_GUARD),
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
                            ContentSettingsType.AUTOMATIC_DOWNLOADS),
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
                            ContentSettingsType.BLUETOOTH_SCANNING),
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
     * Helper function to test allowing and blocking NFC feature.
     * @param enabled true to test enabling NFC feature, false to test disabling the
     *         feature.
     */
    private void doTestNfcPermission(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.NFC, enabled);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("NFC should be " + (enabled ? "enabled" : "disabled"),
                    WebsitePreferenceBridge.isCategoryEnabled(ContentSettingsType.NFC), enabled);
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

    private int getTabCount() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount());
    }
}
