// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.HashMap;

/**
 * Tests the WebApkUpdateDataFetcher.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkUpdateDataFetcherTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String WEB_MANIFEST_URL1 = "/chrome/test/data/banners/manifest.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL1}.
    private static final String WEB_MANIFEST_NAME1 = "Manifest test app";

    private static final String WEB_MANIFEST_URL2 =
            "/chrome/test/data/banners/manifest_short_name_only.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL2}.
    private static final String WEB_MANIFEST_NAME2 = "Manifest";

    // Web Manifest with Murmur2 icon hash with value > {@link Long#MAX_VALUE}
    private static final String WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH =
            "/chrome/test/data/banners/manifest_long_icon_murmur2_hash.json";
    // Murmur2 hash of icon at {@link WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH}.
    private static final String LONG_ICON_MURMUR2_HASH = "13495109619211221667";

    // Scope for {@link WEB_MANIFEST_URL1}, {@link WEB_MANIFEST_URL2} and
    // {@link WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH}.
    private static final String WEB_MANIFEST_SCOPE = "/chrome/test/data";

    private Tab mTab;

    // CallbackHelper which blocks until the {@link ManifestUpgradeDetectorFetcher.Callback}
    // callback is called.
    private static class CallbackWaiter
            extends CallbackHelper implements WebApkUpdateDataFetcher.Observer {
        private boolean mWebApkCompatible;
        private String mName;
        private String mPrimaryIconMurmur2Hash;

        @Override
        public void onGotManifestData(
                WebApkInfo fetchedInfo, String primaryIconUrl, String badgeIconUrl) {
            Assert.assertNull(mName);
            mWebApkCompatible = true;
            mName = fetchedInfo.name();
            mPrimaryIconMurmur2Hash = fetchedInfo.iconUrlToMurmur2HashMap().get(primaryIconUrl);
            notifyCalled();
        }

        public boolean isWebApkCompatible() {
            return mWebApkCompatible;
        }

        public String name() {
            return mName;
        }

        public String primaryIconMurmur2Hash() {
            return mPrimaryIconMurmur2Hash;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    /** Creates and starts a WebApkUpdateDataFetcher. */
    private void startWebApkUpdateDataFetcher(final String scopeUrl,
            final String manifestUrl, final WebApkUpdateDataFetcher.Observer observer) {
        final WebApkUpdateDataFetcher fetcher = new WebApkUpdateDataFetcher();
        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                WebApkInfo oldInfo = WebApkInfo.create("", "", scopeUrl, null, null, null, null,
                        null, -1, -1, -1, -1, -1, "random.package", -1, manifestUrl, "",
                        WebApkInfo.WebApkDistributor.BROWSER, new HashMap<String, String>(), null,
                        false /* forceNavigation */, false /* useTransparentSplash */);
                fetcher.start(mTab, oldInfo, observer);
            }
        });
    }

    /**
     * Test starting WebApkUpdateDataFetcher while a page with the desired manifest URL is loading.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testLaunchWithDesiredManifestUrl() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL1);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServerRule.getServer().getURL(WEB_MANIFEST_SCOPE),
                mTestServerRule.getServer().getURL(WEB_MANIFEST_URL1), waiter);
        waiter.waitForCallback(0);

        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME1, waiter.name());
    }

    /**
     * Test starting WebApkUpdateDataFetcher on page which uses a different manifest URL than the
     * ManifestUpgradeDetectorFetcher is looking for. Check that the callback is only called once
     * the user navigates to a page which uses the desired manifest URL.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testLaunchWithDifferentManifestUrl() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL1);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServerRule.getServer().getURL(WEB_MANIFEST_SCOPE),
                mTestServerRule.getServer().getURL(WEB_MANIFEST_URL2), waiter);

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL2);
        waiter.waitForCallback(0);
        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME2, waiter.name());
    }

    /**
     * Test that large icon murmur2 hashes are correctly plumbed to Java. The hash can take on
     * values up to 2^64 - 1 which is greater than {@link Long#MAX_VALUE}.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testLargeIconMurmur2Hash() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(mTestServerRule.getServer().getURL(WEB_MANIFEST_SCOPE),
                mTestServerRule.getServer().getURL(WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH),
                waiter);
        waiter.waitForCallback(0);

        Assert.assertEquals(LONG_ICON_MURMUR2_HASH, waiter.primaryIconMurmur2Hash());
    }
}
