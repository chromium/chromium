// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webapk.lib.client.WebApkVersion;

import java.util.HashMap;
import java.util.Map;

/**
 * Tests WebApkUpdateManager. This class contains tests which cannot be done as JUnit tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ChromeSwitches.CHECK_FOR_WEB_MANIFEST_UPDATE_ON_STARTUP})
public class WebApkUpdateManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String WEBAPK_ID = "webapk_id";
    private static final String WEBAPK_MANIFEST_URL =
            "/chrome/test/data/banners/manifest_one_icon.json";

    // Data contained in {@link WEBAPK_MANIFEST_URL}.
    private static final String WEBAPK_START_URL =
            "/chrome/test/data/banners/manifest_test_page.html";
    private static final String WEBAPK_SCOPE_URL = "/chrome/test/data/banners/";
    private static final String WEBAPK_NAME = "Manifest test app";
    private static final String WEBAPK_SHORT_NAME = "Manifest test app";
    private static final String WEBAPK_ICON_URL = "/chrome/test/data/banners/image-512px.png";
    private static final String WEBAPK_ICON_MURMUR2_HASH = "7742433188808797392";
    private static final @WebDisplayMode int WEBAPK_DISPLAY_MODE = WebDisplayMode.STANDALONE;
    private static final int WEBAPK_ORIENTATION = ScreenOrientationValues.LANDSCAPE;
    private static final long WEBAPK_THEME_COLOR = 2147483648L;
    private static final long WEBAPK_BACKGROUND_COLOR = 2147483648L;

    private Tab mTab;

    /**
     * Subclass of {@link WebApkUpdateManager} which notifies the {@link CallbackHelper} passed to
     * the constructor when it has been determined whether an update is needed.
     */
    private static class TestWebApkUpdateManager extends WebApkUpdateManager {
        private CallbackHelper mWaiter;
        private boolean mNeedsUpdate;

        public TestWebApkUpdateManager(CallbackHelper waiter, WebappDataStorage storage) {
            super(storage);
            mWaiter = waiter;
        }

        @Override
        public void onGotManifestData(WebApkInfo fetchedInfo, String primaryIconUrl,
                String badgeIconUrl) {
            super.onGotManifestData(fetchedInfo, primaryIconUrl, badgeIconUrl);
            mWaiter.notifyCalled();
        }

        @Override
        protected void storeWebApkUpdateRequestToFile(String updateRequestPath, WebApkInfo info,
                String primaryIconUrl, String badgeIconUrl, boolean isManifestStale,
                @WebApkUpdateReason int updateReason, Callback<Boolean> callback) {
            mNeedsUpdate = true;
        }

        public boolean needsUpdate() {
            return mNeedsUpdate;
        }
    }

    private static class CreationData {
        public String manifestUrl;
        public String startUrl;
        public String scope;
        public String name;
        public String shortName;
        public Map<String, String> iconUrlToMurmur2HashMap;
        public @WebDisplayMode int displayMode;
        public int orientation;
        public long themeColor;
        public long backgroundColor;
    }

    public CreationData defaultCreationData() {
        CreationData creationData = new CreationData();
        creationData.manifestUrl = mTestServerRule.getServer().getURL(WEBAPK_MANIFEST_URL);
        creationData.startUrl = mTestServerRule.getServer().getURL(WEBAPK_START_URL);
        creationData.scope = mTestServerRule.getServer().getURL(WEBAPK_SCOPE_URL);
        creationData.name = WEBAPK_NAME;
        creationData.shortName = WEBAPK_SHORT_NAME;

        creationData.iconUrlToMurmur2HashMap = new HashMap<String, String>();
        creationData.iconUrlToMurmur2HashMap.put(
                mTestServerRule.getServer().getURL(WEBAPK_ICON_URL), WEBAPK_ICON_MURMUR2_HASH);

        creationData.displayMode = WEBAPK_DISPLAY_MODE;
        creationData.orientation = WEBAPK_ORIENTATION;
        creationData.themeColor = WEBAPK_THEME_COLOR;
        creationData.backgroundColor = WEBAPK_BACKGROUND_COLOR;
        return creationData;
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        RecordHistogram.setDisabledForTests(true);
        mTab = mActivityTestRule.getActivity().getActivityTab();

        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(WEBAPK_ID, callback);
        callback.waitForCallback(0);
    }

    @After
    public void tearDown() throws Exception {
        RecordHistogram.setDisabledForTests(false);
    }

     /** Checks whether a WebAPK update is needed. */
    private boolean checkUpdateNeeded(final CreationData creationData) throws Exception {
        CallbackHelper waiter = new CallbackHelper();
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPK_ID);
        final TestWebApkUpdateManager updateManager = new TestWebApkUpdateManager(waiter, storage);

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                WebApkInfo info = WebApkInfo.create(WEBAPK_ID, "", creationData.scope, null, null,
                        null, creationData.name, creationData.shortName, creationData.displayMode,
                        creationData.orientation, 0, creationData.themeColor,
                        creationData.backgroundColor, "",
                        WebApkVersion.REQUEST_UPDATE_FOR_SHELL_APK_VERSION,
                        creationData.manifestUrl, creationData.startUrl,
                        WebApkInfo.WebApkDistributor.BROWSER, creationData.iconUrlToMurmur2HashMap,
                        null, false /* forceNavigation */, false /* useTransparentSplash */);
                updateManager.updateIfNeeded(mTab, info);
            }
        });
        waiter.waitForCallback(0);

        return updateManager.needsUpdate();
    }

    /**
     * Test that the canonicalized URLs are used in determining whether the fetched Web Manifest
     * data differs from the metadata in the WebAPK's Android Manifest. This is important because
     * the URLs in the Web Manifest have been modified by the WebAPK server prior to being stored in
     * the WebAPK Android Manifest. Chrome and the WebAPK server parse URLs differently.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsIdenticalShouldNotUpgrade() throws Exception {
        // URL canonicalization should replace "%74" with 't'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl = mTestServerRule.getServer().getURL(
                "/chrome/test/data/banners/manifest_%74est_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEBAPK_MANIFEST_URL);
        Assert.assertFalse(checkUpdateNeeded(creationData));
    }

    /**
     * Test that an upgraded WebAPK is requested if the canonicalized "start URLs" are different.
     */
    @Test
    @MediumTest
    @Feature({"WebApk"})
    public void testCanonicalUrlsDifferentShouldUpgrade() throws Exception {
        // URL canonicalization should replace "%62" with 'b'.
        CreationData creationData = defaultCreationData();
        creationData.startUrl = mTestServerRule.getServer().getURL(
                "/chrome/test/data/banners/manifest_%62est_page.html");

        WebappTestPage.navigateToServiceWorkerPageWithManifest(
                mTestServerRule.getServer(), mTab, WEBAPK_MANIFEST_URL);
        Assert.assertTrue(checkUpdateNeeded(creationData));
    }
}
