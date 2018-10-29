// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.download.DownloadActivity;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for offline indicator interacting with chrome activity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(jianli): Add test for disabled feature.
public class OfflineIndicatorControllerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId CLIENT_ID =
            new ClientId(OfflinePageBridge.DOWNLOAD_NAMESPACE, "1234");

    private boolean mIsConnected = true;

    @Before
    public void setUp() throws Exception {
        // ChromeActivityTestRule disables offline indicator feature. We want to enable it to do
        // our own testing.
        Features.getInstance().enable(ChromeFeatureList.OFFLINE_INDICATOR);
        OfflineIndicatorController.setTimeToWaitForStableOfflineForTesting(1);
        // This test only cares about whether the network is disconnected or not. So there is no
        // need to do http probes to validate the network in ConnectivityDetector.
        ConnectivityDetector.setDelegateForTesting(new ConnectivityDetectorDelegateStub(
                ConnectivityDetector.ConnectionState.NONE, true /*shouldSkipHttpProbes*/));
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(true);
            OfflineIndicatorController.initialize();
            OfflineIndicatorController.getInstance()
                    .getConnectivityDetectorForTesting()
                    .setConnectionState(ConnectivityDetector.ConnectionState.VALIDATED);
        });
    }

    @Test
    @MediumTest
    public void testShowOfflineIndicatorOnNTPWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load new tab page.
        loadPage(UrlConstants.NTP_URL);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testShowOfflineIndicatorOnRegularPageWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testHideOfflineIndicatorWhenBackToOnline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should go away.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowSubsequentOfflineIndicatorWhenFlaky() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should go away.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Subsequent offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);

        // Reconnect the network and keep it for some time before disconnecting it.
        setNetworkConnectivity(true);
        SystemClock.sleep(2000);
        setNetworkConnectivity(false);

        // Subsequent offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnErrorPageWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);

        // Stop the server and also disconnect the network.
        testServer.shutdownAndWaitUntilComplete();
        setNetworkConnectivity(false);

        // Load an error page.
        loadPage(testUrl);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnOfflinePageWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL(TEST_PAGE);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Save an offline page.
        savePage(testUrl);
        Assert.assertFalse(isOfflinePage(tab));

        // Force to load the offline page.
        loadPageWithoutWaiting(testUrl, "X-Chrome-offline: reason=download");
        waitForPageLoaded(testUrl);
        Assert.assertTrue(isOfflinePage(tab));

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnDownloadsWhenOffline() throws Exception {
        if (mActivityTestRule.getActivity().isTablet()) return;

        DownloadActivity downloadActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), DownloadActivity.class,
                new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                        mActivityTestRule.getActivity(), R.id.downloads_menu_id));

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(downloadActivity, false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnPageLoadingWhenOffline() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/slow?1");

        // Load a page without waiting it to finish.
        loadPageWithoutWaiting(testUrl, null);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), false);

        // Wait for the page to finish loading.
        waitForPageLoaded(testUrl);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(mActivityTestRule.getActivity(), true);
    }

    private void setNetworkConnectivity(boolean connected) {
        mIsConnected = connected;
        ThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineIndicatorController.getInstance()
                    .getConnectivityDetectorForTesting()
                    .setConnectionState(connected
                                    ? ConnectivityDetector.ConnectionState.VALIDATED
                                    : ConnectivityDetector.ConnectionState.DISCONNECTED);
        });
    }

    private void loadPage(String pageUrl) throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        mActivityTestRule.loadUrl(pageUrl);
        Assert.assertEquals(pageUrl, tab.getUrl());
        if (mIsConnected) {
            Assert.assertFalse(isErrorPage(tab));
            Assert.assertFalse(isOfflinePage(tab));
        } else {
            Assert.assertTrue(isErrorPage(tab) || isOfflinePage(tab));
        }
    }

    private void loadPageWithoutWaiting(String pageUrl, String headers) throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            LoadUrlParams params = new LoadUrlParams(
                    pageUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR);
            if (headers != null) {
                params.setVerbatimHeaders(headers);
            }
            tab.loadUrl(params);
        });
    }

    private void waitForPageLoaded(String pageUrl) throws Exception {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, pageUrl);
        ChromeTabUtils.waitForInteractable(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void savePage(String url) throws InterruptedException {
        mActivityTestRule.loadUrl(url);

        final Semaphore semaphore = new Semaphore(0);
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                Profile profile = Profile.getLastUsedProfile();
                OfflinePageBridge offlinePageBridge = OfflinePageBridge.getForProfile(profile);
                offlinePageBridge.savePage(mActivityTestRule.getWebContents(), CLIENT_ID,
                        new OfflinePageBridge.SavePageCallback() {
                            @Override
                            public void onSavePageDone(
                                    int savePageResult, String url, long offlineId) {
                                Assert.assertEquals(
                                        "Save failed.", SavePageResult.SUCCESS, savePageResult);
                                semaphore.release();
                            }
                        });
            }
        });
        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private static void checkOfflineIndicatorVisibility(
            SnackbarManageable activity, boolean visible) {
        CriteriaHelper.pollUiThread(
                new Criteria(visible ? "Offline indicator not shown" : "Offline indicator shown") {
                    @Override
                    public boolean isSatisfied() {
                        return visible == isShowingOfflineIndicator();
                    }

                    private boolean isShowingOfflineIndicator() {
                        if (OfflineIndicatorController.isUsingTopSnackbar()) {
                            TopSnackbarManager snackbarManager =
                                    OfflineIndicatorController.getInstance()
                                            .getTopSnackbarManagerForTesting();
                            return snackbarManager.isShowing();
                        } else {
                            SnackbarManager snackbarManager = activity.getSnackbarManager();
                            if (!snackbarManager.isShowing()) return false;
                            return snackbarManager.getCurrentSnackbarForTesting().getController()
                                    == OfflineIndicatorController.getInstance();
                        }
                    }
                });
    }

    private static boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(() -> { isShowingError[0] = tab.isShowingErrorPage(); });
        return isShowingError[0];
    }

    private static boolean isOfflinePage(final Tab tab) {
        final boolean[] isOffline = new boolean[1];
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                isOffline[0] = OfflinePageUtils.isOfflinePage(tab);
            }
        });
        return isOffline[0];
    }
}
