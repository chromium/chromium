// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.background_sync.BackgroundSyncBackgroundTaskScheduler.BackgroundSyncTask;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.BackgroundSyncNetworkUtils;
import org.chromium.net.ConnectionType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Instrumentation test for Background Sync. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class BackgroundSyncTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // loadNativeLibraryNoBrowserProcess will access AccountManagerFacade, so we need
    // to mock AccountManagerFacade
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private EmbeddedTestServer mTestServer;
    private static final String BACKGROUND_SYNC_TEST_PAGE =
            "/chrome/test/data/background_sync/background_sync_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(10);
    private static final long WAIT_TIME_MS = scaleTimeout(5000);
    private static final String DISABLE_ANDROID_NETWORK_DETECTION =
            "BackgroundSync.RelyOnAndroidNetworkDetection:rely_on_android_network_detection/false";

    private CountDownLatch mScheduleLatch;
    private CountDownLatch mCancelLatch;

    private BackgroundSyncBackgroundTaskScheduler.Observer mSchedulerObserver;

    @Before
    public void setUp() {
        addSchedulerObserver();

        // This is necessary because our test devices don't have Google Play Services up to date,
        // and BackgroundSync requires that. Remove this once https://crbug.com/514449 has been
        // fixed.
        // Note that this should be done before the startMainActivityOnBlankPage(), because Chrome
        // will otherwise run this check on startup and disable BackgroundSync code.
        if (!ExternalAuthUtils.getInstance().canUseGooglePlayServices()) {
            NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
            disableGooglePlayServicesVersionCheck();
        }

        mActivityTestRule.startMainActivityOnBlankPage();

        // BackgroundSync only works with HTTPS.
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackgroundSyncBackgroundTaskScheduler.getInstance()
                            .removeObserver(mSchedulerObserver);
                });
    }

    @Test
    @MediumTest
    @Feature({"BackgroundSync"})
    @CommandLineFlags.Add({"force-fieldtrial-params=" + DISABLE_ANDROID_NETWORK_DETECTION})
    public void onSyncCalledWithNetworkConnectivity() throws Exception {
        forceConnectionType(ConnectionType.CONNECTION_NONE);

        mActivityTestRule.loadUrl(mTestServer.getURL(BACKGROUND_SYNC_TEST_PAGE));
        runJavaScript("SetupReplyForwardingForTests();");

        // Register Sync.
        runJavaScript("RegisterSyncForTag('tagSucceedsSync');");
        assertTitleBecomes("registered sync");
        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));

        forceConnectionType(ConnectionType.CONNECTION_WIFI);
        assertTitleBecomes("onsync: tagSucceedsSync");

        // Now that sync has completed, browser wakeup should get canceled.
        Assert.assertTrue(mCancelLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"BackgroundSync"})
    public void browserWakeUpScheduledWhenSyncEventFails() throws Exception {
        forceConnectionType(ConnectionType.CONNECTION_NONE);

        mActivityTestRule.loadUrl(mTestServer.getURL(BACKGROUND_SYNC_TEST_PAGE));
        runJavaScript("SetupReplyForwardingForTests();");

        // Register Sync.
        runJavaScript("RegisterSyncForTag('tagFailsSync');");
        assertTitleBecomes("registered sync");
        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        forceConnectionType(ConnectionType.CONNECTION_WIFI);

        // Browser wakeup must not be canceled.
        Assert.assertFalse(mCancelLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
    }

    /** Helper methods. */
    private String runJavaScript(String code) throws TimeoutException {
        return mActivityTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    @SuppressWarnings("MissingFail")
    private void assertTitleBecomes(String expectedTitle) {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            Assert.assertEquals(expectedTitle, tab.getTitle());
        }
    }

    private void forceConnectionType(int connectionType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackgroundSyncNetworkUtils.setConnectionTypeForTesting(connectionType);
                });
    }

    private void disableGooglePlayServicesVersionCheck() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackgroundSyncBackgroundTaskSchedulerJni.get()
                            .setPlayServicesVersionCheckDisabledForTests(/* disabled= */ true);
                });
    }

    private void addSchedulerObserver() {
        mScheduleLatch = new CountDownLatch(1);
        mCancelLatch = new CountDownLatch(1);
        mSchedulerObserver =
                new BackgroundSyncBackgroundTaskScheduler.Observer() {
                    @Override
                    public void oneOffTaskScheduledFor(
                            @BackgroundSyncTask int taskType, long delay) {
                        if (taskType == BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP) {
                            mScheduleLatch.countDown();
                        }
                    }

                    @Override
                    public void oneOffTaskCanceledFor(@BackgroundSyncTask int taskType) {
                        if (taskType == BackgroundSyncTask.ONE_SHOT_SYNC_CHROME_WAKE_UP) {
                            mCancelLatch.countDown();
                        }
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackgroundSyncBackgroundTaskScheduler.getInstance()
                            .addObserver(mSchedulerObserver);
                });
    }
}
