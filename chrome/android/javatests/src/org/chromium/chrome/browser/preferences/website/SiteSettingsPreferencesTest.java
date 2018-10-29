// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Intent;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceScreen;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.preferences.ChromeBaseCheckBoxPreference;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.LocationSettings;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.Callable;

/**
 * Tests for everything under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
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
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private void setAllowLocation(final boolean enabled) {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(true);
        final Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsCategory.Type.DEVICE_LOCATION);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences websitePreferences = (SingleCategoryPreferences)
                        preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference location =
                        (ChromeSwitchPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.BINARY_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(location, enabled);
                Assert.assertEquals("Location should be " + (enabled ? "allowed" : "blocked"),
                        enabled, LocationSettings.getInstance().areAllLocationSettingsEnabled());
                preferenceActivity.finish();
            }
        });
    }

    private InfoBarTestAnimationListener setInfoBarAnimationListener() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<InfoBarTestAnimationListener>() {
                    @Override
                    public InfoBarTestAnimationListener call() throws Exception {
                        InfoBarContainer container = mActivityTestRule.getInfoBarContainer();
                        InfoBarTestAnimationListener listener =  new InfoBarTestAnimationListener();
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
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS)
    @Feature({"Preferences"})
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
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS)
    @Feature({"Preferences"})
    public void testSetAllowLocationNotEnabled() throws Exception {
        setAllowLocation(false);

        // Launch a page that uses geolocation.
        mActivityTestRule.loadUrl(
                mTestServer.getURL("/chrome/test/data/geolocation/geolocation_on_load.html"));

        // No infobars are expected.
        Assert.assertTrue(mActivityTestRule.getInfoBars().isEmpty());
    }

    private Preferences startSiteSettingsMenu(String category) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SingleCategoryPreferences.EXTRA_CATEGORY, category);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                InstrumentationRegistry.getTargetContext(),
                SiteSettingsPreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        return (Preferences) InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    private Preferences startSiteSettingsCategory(@SiteSettingsCategory.Type int type) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(
                SingleCategoryPreferences.EXTRA_CATEGORY, SiteSettingsCategory.preferenceKey(type));
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                InstrumentationRegistry.getTargetContext(),
                SingleCategoryPreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        return (Preferences) InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    private Preferences startSingleWebsitePreferences(Website site) {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putSerializable(SingleWebsitePreferences.EXTRA_SITE, site);
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                InstrumentationRegistry.getTargetContext(),
                SingleWebsitePreferences.class.getName());
        intent.putExtra(Preferences.EXTRA_SHOW_FRAGMENT_ARGUMENTS, fragmentArgs);
        return (Preferences) InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }

    private void setCookiesEnabled(final Preferences preferenceActivity, final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                final ChromeSwitchPreference cookies =
                        (ChromeSwitchPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.BINARY_TOGGLE_KEY);
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

                Assert.assertEquals("Third-party cookie toggle should be "
                                + (doesAcceptCookies() ? "enabled" : " disabled"),
                        doesAcceptCookies(), thirdPartyCookies.isEnabled());
                websitePreferences.onPreferenceChange(cookies, enabled);
                Assert.assertEquals("Cookies should be " + (enabled ? "allowed" : "blocked"),
                        doesAcceptCookies(), enabled);
            }

            private boolean doesAcceptCookies() {
                return PrefServiceBridge.getInstance().isCategoryEnabled(
                        ContentSettingsType.CONTENT_SETTINGS_TYPE_COOKIES);
            }
        });
    }

    private void setThirdPartyCookiesEnabled(final Preferences preferenceActivity,
            final boolean enabled) {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                final SingleCategoryPreferences websitePreferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                final ChromeBaseCheckBoxPreference thirdPartyCookies =
                        (ChromeBaseCheckBoxPreference) websitePreferences.findPreference(
                                SingleCategoryPreferences.THIRD_PARTY_COOKIES_TOGGLE_KEY);

                websitePreferences.onPreferenceChange(thirdPartyCookies, enabled);
                Assert.assertEquals(
                        "Third-party cookies should be " + (enabled ? "allowed" : "blocked"),
                        PrefServiceBridge.getInstance().isBlockThirdPartyCookiesEnabled(), enabled);
            }
        });
    }

    private void setGlobalToggleForCategory(
            final @SiteSettingsCategory.Type int type, final boolean enabled) {
        final Preferences preferenceActivity = startSiteSettingsCategory(type);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleCategoryPreferences preferences =
                        (SingleCategoryPreferences) preferenceActivity.getFragmentForTest();
                ChromeSwitchPreference toggle = (ChromeSwitchPreference) preferences.findPreference(
                        SingleCategoryPreferences.BINARY_TOGGLE_KEY);
                preferences.onPreferenceChange(toggle, enabled);
            }
        });
        preferenceActivity.finish();
    }

    private void setEnablePopups(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.POPUPS, enabled);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals("Popups should be " + (enabled ? "allowed" : "blocked"),
                        enabled,
                        PrefServiceBridge.getInstance().isCategoryEnabled(
                                ContentSettingsType.CONTENT_SETTINGS_TYPE_POPUPS));
            }
        });
    }

    private void setEnableCamera(final boolean enabled) {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.CAMERA, enabled);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals("Camera should be " + (enabled ? "allowed" : "blocked"),
                        enabled,
                        PrefServiceBridge.getInstance().isCategoryEnabled(
                                ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
            }
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
        final Preferences preferenceActivity = startSiteSettingsCategory(type);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                PreferenceFragment preferenceFragment =
                        (PreferenceFragment) preferenceActivity.getFragmentForTest();
                PreferenceScreen preferenceScreen = preferenceFragment.getPreferenceScreen();
                int preferenceCount = preferenceScreen.getPreferenceCount();

                ArrayList<String> actualKeys = new ArrayList<String>();
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
            }
        });
        preferenceActivity.finish();
    }

    // TODO(finnur): Write test for Autoplay.

    /**
     * Tests that disabling cookies turns off the third-party cookie toggle.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testThirdPartyCookieToggleGetsDisabled() throws Exception {
        Preferences preferenceActivity =
                startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
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
                startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
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
                startSiteSettingsCategory(SiteSettingsCategory.Type.COOKIES);
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
     * Sets Allow Popups Enabled to be false and make sure it is set correctly.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsBlocked() throws Exception {
        setEnablePopups(false);

        // Test that the popup doesn't open.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/popup.html"));

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Assert.assertEquals(1, getTabCount());
    }

    /**
     * Sets Allow Popups Enabled to be true and make sure it is set correctly.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testPopupsNotBlocked() throws Exception {
        setEnablePopups(true);

        // Test that a popup opens.
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/popup.html"));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertEquals(2, getTabCount());
    }

    /**
     * Test that showing the Site Settings menu doesn't crash (crbug.com/610576).
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSiteSettingsMenu() throws Exception {
        final Preferences preferenceActivity = startSiteSettingsMenu("");
        preferenceActivity.finish();
    }

    /**
     * Test the Media Menu.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testMediaMenu() throws Exception {
        final Preferences preferenceActivity =
                startSiteSettingsMenu(SiteSettingsPreferences.MEDIA_KEY);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SiteSettingsPreferences siteSettings = (SiteSettingsPreferences)
                        preferenceActivity.getFragmentForTest();

                SiteSettingsPreference allSites =
                        (SiteSettingsPreference) siteSettings.findPreference(
                                SiteSettingsCategory.preferenceKey(
                                        SiteSettingsCategory.Type.ALL_SITES));
                Assert.assertEquals(null, allSites);

                SiteSettingsPreference autoplay =
                        (SiteSettingsPreference) siteSettings.findPreference(
                                SiteSettingsCategory.preferenceKey(
                                        SiteSettingsCategory.Type.AUTOPLAY));
                Assert.assertFalse(autoplay == null);

                SiteSettingsPreference protectedContent =
                        (SiteSettingsPreference) siteSettings.findPreference(
                                SiteSettingsCategory.preferenceKey(
                                        SiteSettingsCategory.Type.PROTECTED_MEDIA));
                Assert.assertFalse(protectedContent == null);

                preferenceActivity.finish();
            }
        });
    }

    /**
     * Tests that only expected Preferences are shown for a category.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testOnlyExpectedPreferencesShown() throws Exception {
        // TODO: Add tests for other categories. The allow/block group could be tricky.
        checkPreferencesForCategory(SiteSettingsCategory.Type.ALL_SITES, new String[0]);
        checkPreferencesForCategory(SiteSettingsCategory.Type.USE_STORAGE, new String[0]);
    }

    /**
     * Tests that {@link SingleWebsitePreferences#resetSite} doesn't crash
     * (see e.g. the crash on host names in issue 600232).
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testResetDoesntCrash() throws Exception {
        WebsiteAddress address = WebsiteAddress.create("example.com");
        Website website = new Website(address, address);
        final Preferences preferenceActivity = startSingleWebsitePreferences(website);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                SingleWebsitePreferences websitePreferences =
                        (SingleWebsitePreferences) preferenceActivity.getFragmentForTest();
                websitePreferences.resetSite();
            }
        });
        preferenceActivity.finish();
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
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
            "disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS})
    public void testMicBlocked() throws Exception {
        setGlobalToggleForCategory(SiteSettingsCategory.Type.MICROPHONE, false);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse("Mic should be blocked",
                        PrefServiceBridge.getInstance().isCategoryEnabled(
                                ContentSettingsType.CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
            }
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
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
            "disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS})
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
    @CommandLineFlags.Add({ContentSwitches.USE_FAKE_DEVICE_FOR_MEDIA_STREAM,
            "disable-features=" + ChromeFeatureList.MODAL_PERMISSION_PROMPTS})
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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(
                        "Background Sync should be " + (enabled ? "enabled" : "disabled"),
                        PrefServiceBridge.getInstance().isCategoryEnabled(
                                ContentSettingsType.CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC),
                        enabled);
            }
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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals("USB should be " + (enabled ? "enabled" : "disabled"),
                        PrefServiceBridge.getInstance().isCategoryEnabled(
                                ContentSettingsType.CONTENT_SETTINGS_TYPE_USB_GUARD),
                        enabled);
            }
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
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Assert.assertEquals(
                        "Automatic Downloads should be " + (enabled ? "enabled" : "disabled"),
                        PrefServiceBridge.getInstance().isCategoryEnabled(
                                ContentSettingsType.CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS),
                        enabled);
            }
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

    private int getTabCount() {
        return ThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception {
                return mActivityTestRule.getActivity().getTabModelSelector().getTotalTabCount();
            }
        });
    }
}
