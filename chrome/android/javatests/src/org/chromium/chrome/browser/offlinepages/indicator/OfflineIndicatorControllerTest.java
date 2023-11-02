// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationState;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.download.home.DownloadActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.net.connectivitydetector.ConnectivityDetector;
import org.chromium.chrome.browser.net.connectivitydetector.ConnectivityDetectorDelegateStub;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/** Unit tests for offline indicator interacting with chrome activity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO(jianli): Add test for disabled feature.
public class OfflineIndicatorControllerTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private static final String TEST_PAGE = "/chrome/test/data/android/test.html";
    private static final int TIMEOUT_MS = 5000;
    private static final ClientId CLIENT_ID =
            new ClientId(OfflinePageBridge.DOWNLOAD_NAMESPACE, "1234");

    private boolean mIsConnected = true;
    private EmbeddedTestServer mTestServer;

    @BeforeClass
    public static void beforeClass() throws Exception {
        // ChromeActivityTestRule disables offline indicator feature. We want to enable it to do
        // our own testing.
        Features.getInstance().enable(ChromeFeatureList.OFFLINE_INDICATOR);
        OfflineIndicatorController.setTimeToWaitForStableOfflineForTesting(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(true);
        });
    }

    @Before
    public void setUp() throws Exception {
        // This test only cares about whether the network is disconnected or not. So there is no
        // need to do http probes to validate the network in ConnectivityDetector.
        ConnectivityDetector.setDelegateForTesting(new ConnectivityDetectorDelegateStub(
                ConnectivityDetector.ConnectionState.NONE, true /*shouldSkipHttpProbes*/));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineIndicatorController.resetInstanceForTesting();
            OfflineIndicatorController.initialize();
            OfflineIndicatorController.getInstance()
                    .getConnectivityDetectorForTesting()
                    .setConnectionState(ConnectivityDetector.ConnectionState.VALIDATED);
        });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        setNetworkConnectivity(true);
    }

    @Test
    @MediumTest
    public void testShowOfflineIndicatorOnNTPWhenOffline() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Load new tab page.
        loadPage(UrlConstants.NTP_URL);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testShowOfflineIndicatorOnRegularPageWhenOffline() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testHideOfflineIndicatorWhenBackToOnline() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should go away.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowSubsequentOfflineIndicatorWhenFlaky() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should go away.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Subsequent offline indicator should not be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);

        // Reconnect the network and keep it for some time before disconnecting it.
        setNetworkConnectivity(true);
        SystemClock.sleep(2000);
        setNetworkConnectivity(false);

        // Subsequent offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnErrorPageWhenOffline() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Stop the server and also disconnect the network.
        mTestServer.shutdownAndWaitUntilComplete();
        setNetworkConnectivity(false);

        // Load an error page.
        loadPage(testUrl);

        // Reconnect the network.
        setNetworkConnectivity(true);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnOfflinePageWhenOffline() throws Exception {
        String testUrl = mTestServer.getURL(TEST_PAGE);
        Tab tab = sActivityTestRule.getActivity().getActivityTab();

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
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnDownloadsWhenOffline() {
        if (sActivityTestRule.getActivity().isTablet()) return;

        DownloadActivity downloadActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), DownloadActivity.class,
                new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                        sActivityTestRule.getActivity(), R.id.downloads_menu_id));

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(downloadActivity, false);
        downloadActivity.finish();
    }

    @Test
    @MediumTest
    public void testDoNotShowOfflineIndicatorOnPageLoadingWhenOffline() {
        String testUrl = mTestServer.getURL("/slow?3");

        // Load a page without waiting it to finish.
        loadPageWithoutWaiting(testUrl, null);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);

        // Wait for the page to finish loading.
        waitForPageLoaded(testUrl);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    public void testReshowOfflineIndicatorWhenResumed() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);

        // Hide offline indicator.
        hideOfflineIndicator(sActivityTestRule.getActivity());

        // Simulate switching to other app and then coming back.
        setApplicationState(ApplicationState.HAS_STOPPED_ACTIVITIES);
        setApplicationState(ApplicationState.HAS_RUNNING_ACTIVITIES);

        // Offline indicator should be shown again.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1211514")
    public void testDoNotShowOfflineIndicatorWhenTemporarilyPaused() {
        String testUrl = mTestServer.getURL(TEST_PAGE);

        // Load a page.
        loadPage(testUrl);

        // Disconnect the network.
        setNetworkConnectivity(false);

        // Offline indicator should be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), true);

        // Hide offline indicator.
        hideOfflineIndicator(sActivityTestRule.getActivity());

        // The paused state can be set when the activity is temporarily covered by another
        // activity's Fragment. So switching to this state temporarily should not bring back
        // the offline indicator.
        setApplicationState(ApplicationState.HAS_PAUSED_ACTIVITIES);
        setApplicationState(ApplicationState.HAS_RUNNING_ACTIVITIES);

        // Offline indicator should not be shown.
        checkOfflineIndicatorVisibility(sActivityTestRule.getActivity(), false);
    }

    private void setNetworkConnectivity(boolean connected) {
        mIsConnected = connected;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NetworkChangeNotifier.forceConnectivityState(connected);
            OfflineIndicatorController.getInstance()
                    .getConnectivityDetectorForTesting()
                    .setConnectionState(connected
                                    ? ConnectivityDetector.ConnectionState.VALIDATED
                                    : ConnectivityDetector.ConnectionState.DISCONNECTED);
        });
    }

    private void setApplicationState(int newState) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            OfflineIndicatorController.getInstance().onApplicationStateChange(newState);
        });
    }

    private void loadPage(String pageUrl) {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();

        sActivityTestRule.loadUrl(pageUrl);
        Assert.assertEquals(pageUrl, ChromeTabUtils.getUrlStringOnUiThread(tab));
        if (mIsConnected) {
            Assert.assertFalse(isErrorPage(tab));
            Assert.assertFalse(isOfflinePage(tab));
        } else {
            Assert.assertTrue(isErrorPage(tab) || isOfflinePage(tab));
        }
    }

    private void loadPageWithoutWaiting(String pageUrl, String headers) {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LoadUrlParams params = new LoadUrlParams(
                    pageUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR);
            if (headers != null) {
                params.setVerbatimHeaders(headers);
            }
            tab.loadUrl(params);
        });
    }

    private void waitForPageLoaded(String pageUrl) {
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, pageUrl);
        ChromeTabUtils.waitForInteractable(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void savePage(String url) throws InterruptedException {
        sActivityTestRule.loadUrl(url);

        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            OfflinePageBridge offlinePageBridge = OfflinePageBridge.getForProfile(profile);
            offlinePageBridge.savePage(sActivityTestRule.getWebContents(), CLIENT_ID,
                    new OfflinePageBridge.SavePageCallback() {
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

    private static void checkOfflineIndicatorVisibility(
            SnackbarManageable activity, boolean visible) {
        CriteriaHelper.pollUiThread(() -> {
            if (OfflineIndicatorController.isUsingTopSnackbar()) {
                TopSnackbarManager snackbarManager =
                        OfflineIndicatorController.getInstance().getTopSnackbarManagerForTesting();
                Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(visible));
            } else {
                SnackbarManager snackbarManager = activity.getSnackbarManager();
                Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(visible));
                if (visible) {
                    Criteria.checkThat(
                            snackbarManager.getCurrentSnackbarForTesting().getController(),
                            Matchers.is(OfflineIndicatorController.getInstance()));
                }
            }
        });
    }

    private static void hideOfflineIndicator(ChromeActivity activity) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { OfflineIndicatorController.getInstance().hideOfflineIndicator(activity); });
    }

    private static boolean isErrorPage(final Tab tab) {
        final boolean[] isShowingError = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { isShowingError[0] = tab.isShowingErrorPage(); });
        return isShowingError[0];
    }

    private static boolean isOfflinePage(final Tab tab) {
        final boolean[] isOffline = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { isOffline[0] = OfflinePageUtils.isOfflinePage(tab); });
        return isOffline[0];
    }
}
