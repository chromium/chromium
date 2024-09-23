// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieBlocking3pcdStatus;
import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.CookieControlsObserver;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Integration tests for CookieControlsBridge. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/344669865): Failing when batched, batch this again.
public class CookieControlsBridgeTest {
    public static final String COOKIE_CONTROLS_BATCH_NAME = "cookie_controls";

    private class TestCallbackHandler implements CookieControlsObserver {
        private CallbackHelper mHelper;

        public TestCallbackHandler(CallbackHelper helper) {
            mHelper = helper;
        }

        @Override
        public void onStatusChanged(
                boolean controlsVisible,
                boolean protectionsOn,
                @CookieControlsEnforcement int enforcement,
                @CookieBlocking3pcdStatus int blockingStatus,
                long expiration) {
            mCookieControlsVisible = controlsVisible;
            mThirdPartyCookiesBlocked = protectionsOn;
            mEnforcement = enforcement;
            mExpiration = expiration;
            mHelper.notifyCalled();
        }

        @Override
        public void onHighlightCookieControl(boolean shouldHighlight) {
            mShouldHighlight = shouldHighlight;
            mHelper.notifyCalled();
        }
    }

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private EmbeddedTestServer mTestServer;
    private CallbackHelper mCallbackHelper;
    private TestCallbackHandler mCallbackHandler;
    private CookieControlsBridge mCookieControlsBridge;
    private boolean mCookieControlsVisible;
    private boolean mThirdPartyCookiesBlocked;
    private int mEnforcement;
    private long mExpiration;
    private boolean mShouldHighlight;

    @Before
    public void setUp() throws Exception {
        mCallbackHelper = new CallbackHelper();
        mCallbackHandler = new TestCallbackHandler(mCallbackHelper);
        mTestServer = sActivityTestRule.getTestServer();
        mCookieControlsVisible = false;
        mThirdPartyCookiesBlocked = false;
        mExpiration = -1;
        mShouldHighlight = false;
    }

    @After
    public void tearDown() throws TimeoutException {
        // Reset cookies and cookie settings.
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    UserPrefs.get(profile).clearPref(PrefNames.COOKIE_CONTROLS_MODE);
                    WebsitePreferenceBridge.setDefaultContentSetting(
                            profile, ContentSettingsType.COOKIES, ContentSettingValues.DEFAULT);
                    BrowsingDataBridge.getForProfile(profile)
                            .clearBrowsingData(
                                    helper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_DATA},
                                    TimePeriod.ALL_TIME);
                });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    // This test will become obsolete when 3PCD is rolled out.
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testCookieBridgeWithTPCookiesDisabledUserBypass() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Set CookieControlsMode Pref to Off
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setInteger(PrefNames.COOKIE_CONTROLS_MODE, CookieControlsMode.OFF);
                });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsBridge =
                            new CookieControlsBridge(mCallbackHandler, tab.getWebContents(), null);
                });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(false, mCookieControlsVisible);
        assertEquals(false, mThirdPartyCookiesBlocked);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
    }

    @Test
    @SmallTest
    public void testCookieBridgeWith3PCookiesEnabledUserBypass() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setInteger(
                                    PrefNames.COOKIE_CONTROLS_MODE,
                                    CookieControlsMode.BLOCK_THIRD_PARTY);
                });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsBridge =
                            new CookieControlsBridge(mCallbackHandler, tab.getWebContents(), null);
                });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(true, mCookieControlsVisible);
        assertEquals(true, mThirdPartyCookiesBlocked);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
    }

    @Test
    @SmallTest
    @DisabledTest(
            message = "TODO(crbug.com/40068942): Cookies need to be set in third-party context.")
    public void testCookieBridgeWithChangingAllowedCookiesCountUserBypass() throws Exception {
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsBridge =
                            new CookieControlsBridge(mCallbackHandler, tab.getWebContents(), null);
                });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(false, mCookieControlsVisible);
        assertEquals(false, mThirdPartyCookiesBlocked);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);

        // Try to set a cookie on the page when cookies are allowed.
        currentCallCount = mCallbackHelper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        mCallbackHelper.waitForCallback(currentCallCount, 1);
    }

    @Test
    @SmallTest
    @DisabledTest(
            message = "TODO(crbug.com/40068942): Cookies need to be set in third-party context.")
    public void testCookieBridgeWithChangingBlockedCookiesCountUserBypass() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setInteger(
                                    PrefNames.COOKIE_CONTROLS_MODE,
                                    CookieControlsMode.BLOCK_THIRD_PARTY);
                    // Block all cookies
                    WebsitePreferenceBridge.setCategoryEnabled(
                            ProfileManager.getLastUsedRegularProfile(),
                            ContentSettingsType.COOKIES,
                            false);
                });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsBridge =
                            new CookieControlsBridge(mCallbackHandler, tab.getWebContents(), null);
                });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(true, mCookieControlsVisible);
        assertEquals(true, mThirdPartyCookiesBlocked);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);

        // Try to set a cookie on the page when cookies are blocked.
        currentCallCount = mCallbackHelper.getCallCount();
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        mCallbackHelper.waitForCallback(currentCallCount, 1);
    }

    @Test
    @SmallTest
    // This test will become obsolete when 3PCD is rolled out.
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testCookieBridgeWithIncognitoSettingUserBypass() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Set CookieControlsMode Pref to IncognitoOnly
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setInteger(
                                    PrefNames.COOKIE_CONTROLS_MODE,
                                    CookieControlsMode.INCOGNITO_ONLY);
                });
        int currentCallCount = mCallbackHelper.getCallCount();

        // Navigate to a normal page
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, false);

        // Create cookie bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsBridge =
                            new CookieControlsBridge(mCallbackHandler, tab.getWebContents(), null);
                });

        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(false, mCookieControlsVisible);
        assertEquals(false, mThirdPartyCookiesBlocked);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);

        // Make new incognito page now
        Tab incognitoTab = sActivityTestRule.loadUrlInNewTab(url, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsBridge =
                            new CookieControlsBridge(
                                    mCallbackHandler,
                                    incognitoTab.getWebContents(),
                                    incognitoTab.getProfile().getOriginalProfile());
                });
        mCallbackHelper.waitForCallback(currentCallCount, 2);
        assertEquals(true, mCookieControlsVisible);
        assertEquals(true, mThirdPartyCookiesBlocked);
        assertEquals(CookieControlsEnforcement.NO_ENFORCEMENT, mEnforcement);
    }
}
