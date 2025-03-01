// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.renderer_host;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Integration test for Android OS disabling Javascript Optimizers. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/396239388) Make tests work without {@link ContentSwitches.SITE_PER_PROCESS}.
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.SITE_PER_PROCESS
})
@Batch(Batch.PER_CLASS)
public class JavascriptOptimizerFeatureTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    private static class TestPermissionProvider extends OsAdditionalSecurityPermissionProvider {
        private boolean mIsJavascriptOptimizerPermissionGranted;

        public TestPermissionProvider(boolean isJavascriptOptimizerPermissionGranted) {
            mIsJavascriptOptimizerPermissionGranted = isJavascriptOptimizerPermissionGranted;
        }

        @Override
        public boolean hasJavascriptOptimizerPermission() {
            return mIsJavascriptOptimizerPermissionGranted;
        }
    }

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {
        EmbeddedTestServerRule embeddedTestServerRule =
                mActivityTestRule.getEmbeddedTestServerRule();
        mTestServer = embeddedTestServerRule.getServer();
        mActivityTestRule.startMainActivityOnBlankPage();
        mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
    }

    @After
    public void tearDown() throws TimeoutException {
        OsAdditionalSecurityPermissionUtil.resetForTesting();

        CallbackHelper clearBrowsingCallbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(mProfile)
                            .clearBrowsingData(
                                    clearBrowsingCallbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.SITE_SETTINGS},
                                    TimePeriod.ALL_TIME);
                });
        clearBrowsingCallbackHelper.waitForCallback(0);
    }

    private boolean queryJavascriptOptimizersEnabledForActiveWebContents() {
        WebContents webContents = mActivityTestRule.getWebContents();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return JavascriptOptimizerFeatureTestHelperAndroid
                            .areJavascriptOptimizersEnabledOnWebContents(webContents);
                });
    }

    /** Test that the provider is not queried when the kill switch is on. */
    @Test
    @EnableFeatures({PermissionsAndroidFeatureList.OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH})
    @MediumTest
    public void testKillSwitchOn() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class,
                            new TestPermissionProvider(
                                    /* isJavascriptOptimizerPermissionGranted= */ false));
                });
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /**
     * Test that Javascript optimizers are enabled by default if no {@link
     * OsAdditionalSecurityPermissionProvider} is provided.
     */
    @Test
    @MediumTest
    public void testNoServiceProvider() {
        OsAdditionalSecurityPermissionUtil.resetForTesting();
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /**
     * Test that Javascript optimizers are enabled by default if a {@link
     * OsAdditionalSecurityPermissionProvider} is provided and the OS has granted permission to use
     * Javascript Optimizers.
     */
    @Test
    @MediumTest
    public void testServiceProviderEnablesJavascriptOptimizers() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class,
                            new TestPermissionProvider(
                                    /* isJavascriptOptimizerPermissionGranted= */ true));
                });
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /**
     * Test that Javascript optimizers are disabled by default if a {@link
     * OsAdditionalSecurityPermissionProvider} is provided and the OS has denied permission to use
     * Javascript Optimizers.
     */
    @Test
    @MediumTest
    public void testServiceProviderDisablesJavascriptOptimizers() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class,
                            new TestPermissionProvider(
                                    /* isJavascriptOptimizerPermissionGranted= */ false));
                });
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        assertFalse(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /*
     * Test that specifying an exception in site settings has higher priority than the
     * {@link OsAdditionalSecurityPermissionProvider}-provided setting.
     */
    @Test
    @MediumTest
    public void testCustomExceptionHasHigherPriorityThanService() {
        GURL pageUrl = new GURL(mTestServer.getURL(TEST_PAGE));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class,
                            new TestPermissionProvider(
                                    /* isJavascriptOptimizerPermissionGranted= */ false));

                    GURL pageOrigin = new GURL(pageUrl.getScheme() + "://" + pageUrl.getHost());
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            mProfile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            pageOrigin,
                            pageOrigin,
                            ContentSettingValues.ALLOW);
                });

        mActivityTestRule.loadUrl(pageUrl.getSpec());
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }
}
