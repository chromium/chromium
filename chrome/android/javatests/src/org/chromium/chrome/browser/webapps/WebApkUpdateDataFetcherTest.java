// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.webapps.WebappsIconUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

/** Tests the WebApkUpdateDataFetcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebApkUpdateDataFetcherTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String WEBAPK_START_URL =
            "/chrome/test/data/banners/manifest_test_page.html";

    private static final String WEB_MANIFEST_URL1 = "/chrome/test/data/banners/manifest.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL1}.
    private static final String WEB_MANIFEST_NAME1 = "Manifest test app";

    private static final String WEB_MANIFEST_URL2 =
            "/chrome/test/data/banners/manifest_short_name_only.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL2}.
    private static final String WEB_MANIFEST_NAME2 = "Manifest";

    private static final String WEB_MANIFEST_URL3 =
            "/chrome/test/data/banners/manifest_with_id.json";
    // Name for Web Manifest at {@link WEB_MANIFEST_URL3}.
    private static final String WEB_MANIFEST_NAME3 = "Manifest test app with id specified";

    // Web Manifest with Murmur2 icon hash with value > {@link Long#MAX_VALUE}
    private static final String WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH =
            "/chrome/test/data/banners/manifest_long_icon_murmur2_hash.json";
    // Murmur2 hash of icon at {@link WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH}.
    private static final String LONG_ICON_MURMUR2_HASH = "13495109619211221667";

    private static final String WEB_MANIFEST_URL_MASKABLE =
            "/chrome/test/data/banners/manifest_maskable.json";

    // Scope for {@link WEB_MANIFEST_URL1}, {@link WEB_MANIFEST_URL2} and
    // {@link WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH}.
    private static final String WEB_MANIFEST_SCOPE = "/chrome/test/data";

    private Tab mTab;

    // CallbackHelper which blocks until the {@link ManifestUpgradeDetectorFetcher.Callback}
    // callback is called.
    private static class CallbackWaiter extends CallbackHelper
            implements WebApkUpdateDataFetcher.Observer {
        private boolean mWebApkCompatible;
        private String mName;
        private String mManifestId;
        private String mPrimaryIconMurmur2Hash;
        private boolean mIsPrimaryIconMaskable;

        @Override
        public void onGotManifestData(
                BrowserServicesIntentDataProvider fetchedInfo,
                String primaryIconUrl,
                String splashIconUrl) {
            Assert.assertNull(mName);
            mWebApkCompatible = true;

            WebappExtras fetchedWebappExtras = fetchedInfo.getWebappExtras();
            mName = fetchedWebappExtras.name;
            mManifestId = fetchedInfo.getWebApkExtras().manifestId;
            mPrimaryIconMurmur2Hash =
                    fetchedInfo.getWebApkExtras().iconUrlToMurmur2HashMap.get(primaryIconUrl);
            mIsPrimaryIconMaskable = fetchedWebappExtras.isIconAdaptive;
            notifyCalled();
        }

        public boolean isWebApkCompatible() {
            return mWebApkCompatible;
        }

        public String name() {
            return mName;
        }

        public String manifestId() {
            return mManifestId;
        }

        public String primaryIconMurmur2Hash() {
            return mPrimaryIconMurmur2Hash;
        }

        public boolean isPrimaryIconMaskable() {
            return mIsPrimaryIconMaskable;
        }
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    private WebApkIntentDataProviderBuilder getTestIntentDataProviderBuilder(
            final String manifestUrl) {
        WebApkIntentDataProviderBuilder builder =
                new WebApkIntentDataProviderBuilder(
                        "random.package", mTestServerRule.getServer().getURL(WEBAPK_START_URL));
        builder.setScope(mTestServerRule.getServer().getURL(WEB_MANIFEST_SCOPE));
        builder.setManifestUrl(mTestServerRule.getServer().getURL(manifestUrl));
        return builder;
    }

    /** Creates and starts a WebApkUpdateDataFetcher. */
    private void startWebApkUpdateDataFetcher(
            final WebApkIntentDataProviderBuilder builder,
            final WebApkUpdateDataFetcher.Observer observer) {
        final WebApkUpdateDataFetcher fetcher = new WebApkUpdateDataFetcher();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    fetcher.start(mTab, WebappInfo.create(builder.build()), observer);
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
        startWebApkUpdateDataFetcher(getTestIntentDataProviderBuilder(WEB_MANIFEST_URL1), waiter);
        waiter.waitForCallback(0);

        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME1, waiter.name());
        Assert.assertFalse(waiter.isPrimaryIconMaskable());
    }

    /**
     * Test that WebApkUpdateDataFetcher selects a maskable icon when 1. the manifest has a maskable
     * icon, and 2. the Android version >= 26 (which supports adaptive icon).
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testLaunchWithMaskablePrimaryIconManifestUrl() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL_MASKABLE);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(
                getTestIntentDataProviderBuilder(WEB_MANIFEST_URL_MASKABLE), waiter);
        waiter.waitForCallback(0);

        Assert.assertEquals(
                WebappsIconUtils.doesAndroidSupportMaskableIcons(), waiter.isPrimaryIconMaskable());
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
        startWebApkUpdateDataFetcher(
                getTestIntentDataProviderBuilder(WEB_MANIFEST_WITH_LONG_ICON_MURMUR2_HASH), waiter);
        waiter.waitForCallback(0);

        Assert.assertEquals(LONG_ICON_MURMUR2_HASH, waiter.primaryIconMurmur2Hash());
    }

    /**
     * Test starting WebApkUpdateDataFetcher on page which uses a different manifest URL but same
     * manifest ID than the ManifestUpgradeDetectorFetcher is looking for.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testLaunchWithDifferentManifestUrlSameId() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL1);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(getTestIntentDataProviderBuilder(WEB_MANIFEST_URL2), waiter);

        waiter.waitForOnly();
        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME1, waiter.name());
        Assert.assertNotNull(waiter.manifestId());
    }

    /**
     * Test starting WebApkUpdateDataFetcher on page with different manifest ID. Check that the
     * callback is only called once the user navigates to a page which uses the desired manifest ID.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testLaunchWithDifferentManifestId() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL3);

        CallbackWaiter waiter = new CallbackWaiter();
        startWebApkUpdateDataFetcher(getTestIntentDataProviderBuilder(WEB_MANIFEST_URL1), waiter);

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL2);
        waiter.waitForOnly();
        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME2, waiter.name());
        Assert.assertNotNull(waiter.manifestId());
    }

    /** Test starting WebApkUpdateDataFetcher when current WebAPK has no ID specified. */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testLaunchWithEmptyOldManifestId() throws Exception {
        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEB_MANIFEST_URL3);

        CallbackWaiter waiter = new CallbackWaiter();

        WebApkIntentDataProviderBuilder oldIntentDataProviderBuilder =
                getTestIntentDataProviderBuilder(WEB_MANIFEST_URL1);
        // Set manifestId to empty string.
        oldIntentDataProviderBuilder.setWebApkManifestId("");
        // Set manifestUrl to be the same so update can be trigger
        oldIntentDataProviderBuilder.setManifestUrl(
                mTestServerRule.getServer().getURL(WEB_MANIFEST_URL3));

        startWebApkUpdateDataFetcher(oldIntentDataProviderBuilder, waiter);

        waiter.waitForOnly();
        Assert.assertTrue(waiter.isWebApkCompatible());
        Assert.assertEquals(WEB_MANIFEST_NAME3, waiter.name());
        Assert.assertNotNull(waiter.manifestId());
    }
}
