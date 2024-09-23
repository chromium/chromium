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
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.BackgroundSyncNetworkUtils;
import org.chromium.net.ConnectionType;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

/** Instrumentation test for Periodic Background Sync. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=PeriodicBackgroundSync<BackgroundSync",
    "force-fieldtrials=BackgroundSync/BackgroundSync",
    "force-fieldtrial-params=BackgroundSync.BackgroundSync:"
            + "min_periodic_sync_events_interval_sec/1/"
            + "skip_permissions_check_for_testing/true"
})
public final class PeriodicBackgroundSyncTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    // loadNativeLibraryNoBrowserProcess will access AccountManagerFacade, so we need
    // to mock AccountManagerFacade
    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private EmbeddedTestServer mTestServer;
    private String mTestPage;
    private static final String TEST_PAGE =
            "/chrome/test/data/background_sync/background_sync_test.html";
    private static final int TITLE_UPDATE_TIMEOUT_SECONDS = (int) scaleTimeout(10);
    private static final long WAIT_TIME_MS = scaleTimeout(100);

    private CountDownLatch mScheduleLatch;
    private CountDownLatch mCancelLatch;
    private AtomicInteger mScheduleCount;

    private BackgroundSyncBackgroundTaskScheduler.Observer mSchedulerObserver;

    @Before
    public void setUp() throws InterruptedException, TimeoutException {
        // This is necessary because our test devices don't have Google Play Services up to date,
        // and Periodic Background Sync requires that. Remove this once https://crbug.com/514449 has
        // been fixed.
        // Note that this should be done before the startMainActivityOnBlankPage(), because Chrome
        // will otherwise run this check on startup and disable Periodic Background Sync code.
        if (!ExternalAuthUtils.getInstance().canUseGooglePlayServices()) {
            NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
            disableGooglePlayServicesVersionCheck();
        }

        mActivityTestRule.startMainActivityOnBlankPage();

        // Periodic Background Sync only works with HTTPS.
        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        runJavaScript("SetupReplyForwardingForTests();");
    }

    @After
    public void tearDown() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BackgroundSyncBackgroundTaskScheduler.getInstance()
                            .removeObserver(mSchedulerObserver);
                });
    }

    @Test
    @MediumTest
    @Feature({"PeriodicBackgroundSync"})
    public void registerPeriodicSyncForASiteWithNoEngagement() throws Exception {
        // No schedule or cancel expected.
        // We set scheduleCount and cancelCount to 1 here so ensure that the |await|
        // call doesn't succeed, even after we've waited long enough for it to have happened.
        addSchedulerObserver(/* scheduleCount= */ 1, /* cancelCount= */ 1);

        forceConnectionType(ConnectionType.CONNECTION_NONE);

        resetEngagementForUrl(mTestServer.getURL(TEST_PAGE), 0);

        // Register Periodic Background Sync.
        runJavaScript("RegisterPeriodicSyncForTag('tagSucceedsSync');");
        assertTitleBecomes("registered periodicsync");

        forceConnectionType(ConnectionType.CONNECTION_WIFI);
        Assert.assertFalse(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        Assert.assertFalse(mCancelLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"PeriodicBackgroundSync"})
    public void eventFiredWithNetworkConnectivity() throws Exception {
        // Schedule is expected once after register, once after restoration of connectivity, and
        // once after the periodicSync event completes.
        // Cancel is expected once after the periodicSync event completes.
        addSchedulerObserver(/* scheduleCount= */ 3, /* cancelCount= */ 1);

        forceConnectionType(ConnectionType.CONNECTION_NONE);
        resetEngagementForUrl(mTestServer.getURL(TEST_PAGE), 50);

        // Register Periodic Background Sync.
        runJavaScript("RegisterPeriodicSyncForTag('tagSucceedsSync');");
        assertTitleBecomes("registered periodicsync");
        Assert.assertEquals(1, mScheduleCount.get());

        forceConnectionType(ConnectionType.CONNECTION_WIFI);
        assertTitleBecomes("onperiodicsync: tagSucceedsSync");

        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mCancelLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"PeriodicBackgroundSync"})
    public void browserWakeUpScheduledWhenPeriodicSyncEventFails() throws Exception {
        // Schedule is expected once after register, once after restoration of connectivity, and
        // once after the periodicSync event completes.
        // Cancel is expected once after the periodicSync event completes.
        addSchedulerObserver(/* scheduleCount= */ 3, /* cancelCount= */ 1);

        forceConnectionType(ConnectionType.CONNECTION_NONE);
        resetEngagementForUrl(mTestServer.getURL(TEST_PAGE), 50);

        // Register Periodic Background Sync.
        runJavaScript("RegisterPeriodicSyncForTag('tagFailsSync');");
        assertTitleBecomes("registered periodicsync");
        Assert.assertEquals(1, mScheduleCount.get());

        forceConnectionType(ConnectionType.CONNECTION_WIFI);

        assertTitleBecomes("failed periodicsync: tagFailsSync");
        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
        Assert.assertTrue(mCancelLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
    }

    @Test
    @MediumTest
    @Feature({"PeriodicBackgroundSync"})
    public void unregisterCancelsBrowserWakeup() throws Exception {
        // Schedule and cancel expected once each.
        addSchedulerObserver(/* scheduleCount= */ 1, /* cancelCount= */ 1);

        forceConnectionType(ConnectionType.CONNECTION_NONE);
        resetEngagementForUrl(mTestServer.getURL(TEST_PAGE), 50);

        // Register Periodic Background Sync.
        runJavaScript("RegisterPeriodicSyncForTag('tagSucceedsSync');");
        assertTitleBecomes("registered periodicsync");
        Assert.assertTrue(mScheduleLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));

        // Unregister Periodic Background Sync.
        runJavaScript("UnregisterPeriodicSyncForTag('tagSucceedsSync');");
        assertTitleBecomes("unregistered periodicsync");
        Assert.assertTrue(mCancelLatch.await(WAIT_TIME_MS, TimeUnit.MILLISECONDS));
    }

    /** Helper methods. */
    private String runJavaScript(String code) throws TimeoutException, InterruptedException {
        return mActivityTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    @SuppressWarnings("MissingFail")
    private void assertTitleBecomes(String expectedTitle) throws InterruptedException {
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        TabTitleObserver titleObserver = new TabTitleObserver(tab, expectedTitle);
        try {
            titleObserver.waitForTitleUpdate(TITLE_UPDATE_TIMEOUT_SECONDS);
        } catch (TimeoutException e) {
            // The title is not as expected, this assertion neatly logs what the difference is.
            Assert.assertEquals(expectedTitle, ChromeTabUtils.getTitleOnUiThread(tab));
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

    private void resetEngagementForUrl(final String url, final double engagement) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                    SiteEngagementService.getForBrowserContext(
                                    ProfileManager.getLastUsedRegularProfile())
                            .resetBaseScoreForUrl(url, engagement);
                });
    }

    private void addSchedulerObserver(int scheduleCount, int cancelCount) {
        mScheduleCount = new AtomicInteger();
        mScheduleLatch = new CountDownLatch(scheduleCount);
        mCancelLatch = new CountDownLatch(cancelCount);
        mSchedulerObserver =
                new BackgroundSyncBackgroundTaskScheduler.Observer() {
                    @Override
                    public void oneOffTaskScheduledFor(
                            @BackgroundSyncTask int taskType, long delay) {
                        if (taskType == BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP) {
                            mScheduleCount.incrementAndGet();
                            mScheduleLatch.countDown();
                        }
                    }

                    @Override
                    public void oneOffTaskCanceledFor(@BackgroundSyncTask int taskType) {
                        if (taskType == BackgroundSyncTask.PERIODIC_SYNC_CHROME_WAKE_UP) {
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
