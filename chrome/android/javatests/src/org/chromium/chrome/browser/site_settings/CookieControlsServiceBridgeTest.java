// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.test.EmbeddedTestServer;

/** Integration tests for CookieControlsServiceBridge. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(CookieControlsBridgeTest.COOKIE_CONTROLS_BATCH_NAME)
public class CookieControlsServiceBridgeTest {
    private class TestCallbackHandler
            implements CookieControlsServiceBridge.CookieControlsServiceObserver {
        private CallbackHelper mHelper;

        public TestCallbackHandler(CallbackHelper helper) {
            mHelper = helper;
        }

        @Override
        public void sendCookieControlsUIChanges(
                boolean checked, @CookieControlsEnforcement int enforcement) {
            mChecked = checked;
            mEnforcement = enforcement;
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
    private CookieControlsServiceBridge mCookieControlsServiceBridge;
    private boolean mChecked;
    private @CookieControlsEnforcement int mEnforcement;

    @Before
    public void setUp() throws Exception {
        mCallbackHelper = new CallbackHelper();
        mCallbackHandler = new TestCallbackHandler(mCallbackHelper);
        mTestServer = sActivityTestRule.getTestServer();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(PrefNames.COOKIE_CONTROLS_MODE);
                });
    }

    private void setCookieControlsMode(@CookieControlsMode int mode) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setInteger(PrefNames.COOKIE_CONTROLS_MODE, mode);
                });
    }

    /** Test changing the bridge triggers callback for correct toggle state. */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testCookieSettingsCheckedChanges() throws Exception {
        setCookieControlsMode(CookieControlsMode.OFF);
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, true); // incognito tab

        int currentCallCount = mCallbackHelper.getCallCount();
        // Create cookie settings bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsServiceBridge =
                            new CookieControlsServiceBridge(tab.getProfile(), mCallbackHandler);
                    mCookieControlsServiceBridge.updateServiceIfNecessary();
                });
        // Initial callback after the bridge is created.
        mCallbackHelper.waitForCallback(currentCallCount, 1);

        // Test that the toggle switches on.
        boolean expectedChecked = true;
        mChecked = false;
        currentCallCount = mCallbackHelper.getCallCount();
        setCookieControlsMode(CookieControlsMode.INCOGNITO_ONLY);
        mCallbackHelper.waitForCallback(currentCallCount, 1);
        Assert.assertEquals(expectedChecked, mChecked);

        // Test that the toggle switches off.
        expectedChecked = false;
        mChecked = true;
        currentCallCount = mCallbackHelper.getCallCount();
        setCookieControlsMode(CookieControlsMode.OFF);
        mCallbackHelper.waitForCallback(currentCallCount, 1);
        Assert.assertEquals(expectedChecked, mChecked);

        // Test that the toggle switches back on and enforced (by settings)
        expectedChecked = true;
        mChecked = false;
        int expectedEnforcement = CookieControlsEnforcement.ENFORCED_BY_COOKIE_SETTING;
        mEnforcement = CookieControlsEnforcement.NO_ENFORCEMENT;
        currentCallCount = mCallbackHelper.getCallCount();
        setCookieControlsMode(CookieControlsMode.BLOCK_THIRD_PARTY);
        mCallbackHelper.waitForCallback(currentCallCount, 1);
        Assert.assertEquals(expectedChecked, mChecked);
        Assert.assertEquals(expectedEnforcement, mEnforcement);
    }

    /** Test the ability to set the cookie controls mode pref through the bridge. */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testCookieBridgeWithTPCookiesDisabled() throws Exception {
        setCookieControlsMode(CookieControlsMode.OFF);
        final String url = mTestServer.getURL("/chrome/test/data/android/cookie.html");
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, true); // incognito tab.

        boolean expectedChecked = true;
        mChecked = false;
        int currentCallCount = mCallbackHelper.getCallCount();
        // Create cookie controls service bridge and wait for desired callbacks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCookieControlsServiceBridge =
                            new CookieControlsServiceBridge(tab.getProfile(), mCallbackHandler);
                    mCookieControlsServiceBridge.updateServiceIfNecessary();

                    mCookieControlsServiceBridge.handleCookieControlsToggleChanged(true);

                    Assert.assertEquals(
                            "CookieControlsMode should be incognito_only",
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                    .getInteger(PrefNames.COOKIE_CONTROLS_MODE),
                            CookieControlsMode.INCOGNITO_ONLY);
                });
        // One initial callback after creation, then another after the toggle change.
        mCallbackHelper.waitForCallback(currentCallCount, 2);
        Assert.assertEquals(expectedChecked, mChecked);
    }
}
