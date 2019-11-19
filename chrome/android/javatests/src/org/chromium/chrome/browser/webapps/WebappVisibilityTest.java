// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.util.browser.webapps.WebApkInfoBuilder;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;

/**
 * Tests for {@link WebappDelegateFactory}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebappVisibilityTest {
    @Rule
    public NativeLibraryTestRule mActivityTestRule = new NativeLibraryTestRule();

    @Rule
    public UiThreadTestRule mUiThreadTestRule = new UiThreadTestRule();

    private static final String WEBAPP_URL = "http://originalwebsite.com";

    @Before
    public void setUp() {
        mActivityTestRule.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testShouldShowBrowserControls() throws Throwable {
        mUiThreadTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                testCanAutoHideBrowserControls();
                for (@WebappScopePolicy.Type int scopePolicy = WebappScopePolicy.Type.LEGACY;
                        scopePolicy < WebappScopePolicy.Type.NUM_ENTRIES; scopePolicy++) {
                    for (@WebDisplayMode int displayMode : new int[] {WebDisplayMode.STANDALONE,
                                 WebDisplayMode.FULLSCREEN, WebDisplayMode.MINIMAL_UI}) {
                        testShouldShowBrowserControls(scopePolicy, displayMode);
                    }
                }
            }
        });
    }

    private static void testCanAutoHideBrowserControls() {
        // Allow auto-hiding controls unless we're on a dangerous connection.
        Assert.assertTrue(canAutoHideBrowserControls(ConnectionSecurityLevel.NONE));
        Assert.assertTrue(canAutoHideBrowserControls(ConnectionSecurityLevel.SECURE));
        Assert.assertTrue(canAutoHideBrowserControls(ConnectionSecurityLevel.EV_SECURE));
        Assert.assertTrue(canAutoHideBrowserControls(ConnectionSecurityLevel.WARNING));
        Assert.assertFalse(canAutoHideBrowserControls(ConnectionSecurityLevel.DANGEROUS));
    }

    private static void testShouldShowBrowserControls(
            @WebappScopePolicy.Type int scopePolicy, @WebDisplayMode int displayMode) {
        // Show browser controls for out-of-domain URLs.
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://notoriginalwebsite.com",
                ConnectionSecurityLevel.NONE, scopePolicy, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://otherwebsite.com",
                ConnectionSecurityLevel.NONE, scopePolicy, displayMode));

        // Do not show browser controls for subpaths, unless using Minimal-UI.
        Assert.assertEquals(displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL, ConnectionSecurityLevel.NONE,
                        scopePolicy, displayMode));
        Assert.assertEquals(displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/things.html",
                        ConnectionSecurityLevel.NONE, scopePolicy, displayMode));
        Assert.assertEquals(displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/stuff.html",
                        ConnectionSecurityLevel.NONE, scopePolicy, displayMode));

        // For WebAPKs but not Webapps show browser controls for subdomains and private
        // registries that are secure.
        Assert.assertEquals(scopePolicy == WebappScopePolicy.Type.STRICT
                        || displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, "http://sub.originalwebsite.com",
                        ConnectionSecurityLevel.NONE, scopePolicy, displayMode));
        Assert.assertEquals(scopePolicy == WebappScopePolicy.Type.STRICT
                        || displayMode == WebDisplayMode.MINIMAL_UI,
                shouldShowBrowserControls(WEBAPP_URL, "http://thing.originalwebsite.com",
                        ConnectionSecurityLevel.NONE, scopePolicy, displayMode));

        // Do not show browser controls when URL is not available yet.
        Assert.assertFalse(shouldShowBrowserControls(
                WEBAPP_URL, "", ConnectionSecurityLevel.NONE, scopePolicy, displayMode));

        // Show browser controls for Dangerous URLs.
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL,
                ConnectionSecurityLevel.DANGEROUS, scopePolicy, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/stuff.html",
                ConnectionSecurityLevel.DANGEROUS, scopePolicy, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, WEBAPP_URL + "/things.html",
                ConnectionSecurityLevel.DANGEROUS, scopePolicy, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://notoriginalwebsite.com",
                ConnectionSecurityLevel.DANGEROUS, scopePolicy, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://otherwebsite.com",
                ConnectionSecurityLevel.DANGEROUS, scopePolicy, displayMode));
        Assert.assertTrue(shouldShowBrowserControls(WEBAPP_URL, "http://thing.originalwebsite.com",
                ConnectionSecurityLevel.DANGEROUS, scopePolicy, displayMode));
    }

    private static boolean shouldShowBrowserControls(String webappStartUrlOrScopeUrl, String url,
            int securityLevel, @WebappScopePolicy.Type int scopePolicy,
            @WebDisplayMode int displayMode) {
        return WebappBrowserControlsDelegate.shouldShowBrowserControls(scopePolicy,
                createWebappInfo(webappStartUrlOrScopeUrl, scopePolicy, displayMode), url,
                securityLevel);
    }

    private static boolean canAutoHideBrowserControls(int securityLevel) {
        return WebappBrowserControlsDelegate.canAutoHideBrowserControls(securityLevel);
    }

    private static WebappInfo createWebappInfo(String webappStartUrlOrScopeUrl,
            @WebappScopePolicy.Type int scopePolicy, @WebDisplayMode int displayMode) {
        if (scopePolicy == WebappScopePolicy.Type.LEGACY) {
            Intent webappIntent = WebappTestHelper.createMinimalWebappIntent(
                    "" /* id */, webappStartUrlOrScopeUrl);
            webappIntent.putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, displayMode);
            return WebappInfo.create(webappIntent);
        }
        WebApkInfoBuilder webApkInfoBuilder = new WebApkInfoBuilder("random.package", "" /* url */);
        webApkInfoBuilder.setScope(webappStartUrlOrScopeUrl);
        webApkInfoBuilder.setDisplayMode(displayMode);
        return webApkInfoBuilder.build();
    }
}
