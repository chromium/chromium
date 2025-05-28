// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.renderer_host;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.AdvancedProtectionTestRule;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

/** Integration test for Android OS disabling Javascript Optimizers. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@DoNotBatch(reason = "Tests manipulate global profile state")
public class JavascriptOptimizerFeatureTest {
    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";

    private EmbeddedTestServer mTestServer;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @ClassRule
    public static AdvancedProtectionTestRule sAdvancedProtectionRule =
            new AdvancedProtectionTestRule();

    @Before
    public void setUp() {
        EmbeddedTestServerRule embeddedTestServerRule =
                mActivityTestRule.getEmbeddedTestServerRule();
        mTestServer = embeddedTestServerRule.getServer();
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    private boolean queryJavascriptOptimizersEnabledForActiveWebContents() {
        WebContents webContents = mActivityTestRule.getWebContents();
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return JavascriptOptimizerFeatureTestHelperAndroid
                            .areJavascriptOptimizersEnabledOnWebContents(webContents);
                });
    }

    /**
     * Test that Javascript optimizers are enabled by default when the operating system does not
     * request advanced protection.
     */
    @Test
    @MediumTest
    public void testOsDoesNotRequestAdvancedProtection() {
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /**
     * Test that Javascript optimizers are disabled by default when the operating system requests
     * advanced protection.
     */
    @Test
    @MediumTest
    public void testServiceProviderRequestsAdvancedProtection() {
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);
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
        GURL pageUrl = new GURL(mTestServer.getURLWithHostName("allowed.test", TEST_PAGE));
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GURL pageOrigin = new GURL(pageUrl.getScheme() + "://" + pageUrl.getHost());
                    Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
                    WebsitePreferenceBridge.setContentSettingDefaultScope(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            pageOrigin,
                            pageOrigin,
                            ContentSettingValues.ALLOW);
                });

        mActivityTestRule.loadUrl(pageUrl.getSpec());
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /*
     * Test that specifying an exception in site settings has higher priority than the
     * default setting.
     */
    @Test
    @MediumTest
    public void testCustomAllowExceptionHasHigherPriorityThanDefaultBlockSetting() {
        GURL pageUrl = new GURL(mTestServer.getURLWithHostName("allowed.test", TEST_PAGE));
        // Ensure that APM is disabled, because APM actuates site-per-process.
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GURL pageOrigin = new GURL(pageUrl.getScheme() + "://" + pageUrl.getHost());
                    Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
                    WebsitePreferenceBridge.setDefaultContentSetting(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            ContentSettingValues.BLOCK);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            pageOrigin.getHost(),
                            "*",
                            ContentSettingValues.ALLOW);
                });

        mActivityTestRule.loadUrl(pageUrl.getSpec());
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /*
     * Test that specifying an exception in site settings has higher priority than the
     * default setting.
     */
    @Test
    @MediumTest
    public void testCustomAllowExceptionForSiteAlsoAppliesToSubdomain() {
        GURL pageUrl = new GURL(mTestServer.getURLWithHostName("www.allowed.test", TEST_PAGE));
        // Ensure that APM is disabled, because APM actuates site-per-process.
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
                    WebsitePreferenceBridge.setDefaultContentSetting(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            ContentSettingValues.BLOCK);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            "http://[*.]allowed.test",
                            "*",
                            ContentSettingValues.ALLOW);
                });

        mActivityTestRule.loadUrl(pageUrl.getSpec());
        assertTrue(queryJavascriptOptimizersEnabledForActiveWebContents());
    }

    /*
     * Test that specifying an exception in site settings has higher priority than the
     * default setting.
     */
    @Test
    @MediumTest
    public void testCustomBlockExceptionHasHigherPriorityThanDefaultAllowSetting() {
        GURL pageUrl = new GURL(mTestServer.getURLWithHostName("blocked.test", TEST_PAGE));
        // Ensure that APM is disabled, because APM actuates site-per-process.
        sAdvancedProtectionRule.setIsAdvancedProtectionRequestedByOs(false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GURL pageOrigin = new GURL(pageUrl.getScheme() + "://" + pageUrl.getHost());
                    Profile profile = mActivityTestRule.getProfile(/* incognito= */ false);
                    WebsitePreferenceBridge.setDefaultContentSetting(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            ContentSettingValues.ALLOW);
                    WebsitePreferenceBridge.setContentSettingCustomScope(
                            profile,
                            ContentSettingsType.JAVASCRIPT_OPTIMIZER,
                            pageOrigin.getHost(),
                            "*",
                            ContentSettingValues.BLOCK);
                });

        mActivityTestRule.loadUrl(pageUrl.getSpec());
        assertFalse(queryJavascriptOptimizersEnabledForActiveWebContents());
    }
}
