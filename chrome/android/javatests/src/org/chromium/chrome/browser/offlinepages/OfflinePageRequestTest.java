// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge.SavePageCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for offline page request handling. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OfflinePageRequestTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private static final String ABOUT_PAGE = "/chrome/test/data/android/about.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId CLIENT_ID =
            new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, "1234");

    private OfflinePageBridge mOfflinePageBridge;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(true);
        });
        mOfflinePageBridge = OfflineTestUtil.getOfflinePageBridge();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/786233")
    public void testLoadOfflinePageOnDisconnectedNetwork() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);
        String aboutUrl = testServer.getURL(ABOUT_PAGE);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Load and save an offline page.
        savePage(testUrl, CLIENT_ID);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertFalse(isOfflinePage(tab));

        // Load another page.
        mActivityTestRule.loadUrl(aboutUrl);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertFalse(isOfflinePage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(false); });

        // Load the page that has an offline copy. The offline page should be shown.
        mActivityTestRule.loadUrl(testUrl);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertTrue(isOfflinePage(tab));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/786233")
    public void testLoadOfflinePageWithFragmentOnDisconnectedNetwork() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);
        String testUrlWithFragment = testUrl + "#ref";

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Load and save an offline page for the url with a fragment.
        savePage(testUrlWithFragment, CLIENT_ID);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertFalse(isOfflinePage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(false); });

        // Load the URL without the fragment. The offline page should be shown.
        mActivityTestRule.loadUrl(testUrl);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertTrue(isOfflinePage(tab));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/786233")
    public void testLoadOfflinePageFromDownloadsOnDisconnectedNetwork() throws Exception {
        // Specifically tests saving to and loading from Downloads.
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);
        String aboutUrl = testServer.getURL(ABOUT_PAGE);

        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Load and save a persistent offline page using a persistent namespace so that the archive
        // will be published.
        savePage(testUrl, new ClientId(OfflinePageBridge.DOWNLOAD_NAMESPACE, "1234"));
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertFalse(isOfflinePage(tab));

        // Load another page.
        mActivityTestRule.loadUrl(aboutUrl);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertFalse(isOfflinePage(tab));

        // Stop the server and also disconnect the network.
        testServer.stopAndDestroyServer();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(false); });

        // Load the page that has an offline copy. The offline page should be shown.
        mActivityTestRule.loadUrl(testUrl);
        Assert.assertFalse(isErrorPage(tab));
        Assert.assertTrue(isOfflinePage(tab));
    }

    private void savePage(String url, ClientId clientId) throws InterruptedException {
        mActivityTestRule.loadUrl(url);

        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mOfflinePageBridge.savePage(
                    mActivityTestRule.getWebContents(), clientId, new SavePageCallback() {
                        @Override
                        public void onSavePageDone(int savePageResult, String url, long offlineId) {
                            Assert.assertEquals(
                                    "Save failed.", SavePageResult.SUCCESS, savePageResult);
                            semaphore.release();
                        }
                    });
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private boolean isOfflinePage(final Tab tab) {
        final boolean[] isOffline = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { isOffline[0] = OfflinePageUtils.isOfflinePage(tab); });
        return isOffline[0];
    }

    private boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { isShowingError[0] = tab.isShowingErrorPage(); });
        return isShowingError[0];
    }
}
