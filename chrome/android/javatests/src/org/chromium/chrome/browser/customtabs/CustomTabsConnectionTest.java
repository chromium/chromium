// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.support.test.InstrumentationRegistry;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsClient;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsServiceConnection;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for CustomTabsConnection. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class CustomTabsConnectionTest {
    private CustomTabsConnection mCustomTabsConnection;
    private static final String URL = "http://www.google.com";
    private static final String URL2 = "https://www.android.com";
    private static final String INVALID_SCHEME_URL = "intent://www.google.com";

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
        mCustomTabsConnection = CustomTabsTestUtils.setUpConnection();
    }

    @After
    public void tearDown() {
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> WarmupManager.getInstance().destroySpareWebContents());
    }

    /**
     * Tests that we can create a new session. Registering with a null callback
     * fails. Registering a session with an {@linkplain CustomTabsSessionToken#equals equal}
     * session token will update the callback for the session.
     */
    @Test
    @SmallTest
    public void testNewSession() {
        Assert.assertFalse(mCustomTabsConnection.newSession(null));
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        // Request to update callback for the session.
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
    }

    /**
     * Tests that we can create several sessions.
     */
    @Test
    @SmallTest
    public void testSeveralSessions() {
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        CustomTabsSessionToken token2 = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token2));
    }

    /**
     * Tests that {@link CustomTabsConnection#warmup(long)} succeeds and can
     * be issued multiple times.
     */
    @Test
    @SmallTest
    @DisabledTest
    public void testCanWarmup() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        CustomTabsTestUtils.warmUpAndWait();
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateSpareRenderer() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        // On UI thread because:
        // 1. takeSpareWebContents needs to be called from the UI thread.
        // 2. warmup() is non-blocking and posts tasks to the UI thread, it ensures proper ordering.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WarmupManager warmupManager = WarmupManager.getInstance();
            Assert.assertTrue(warmupManager.hasSpareWebContents());
            WebContents webContents = warmupManager.takeSpareWebContents(false, false);
            Assert.assertNotNull(webContents);
            Assert.assertFalse(warmupManager.hasSpareWebContents());
            webContents.destroy();
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_LOW_END_DEVICE)
    public void testDoNotCreateSpareRendererOnLowEnd() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        // On UI thread because:
        // 1. takeSpareWebContents needs to be called from the UI thread.
        // 2. warmup() is non-blocking and posts tasks to the UI thread, it ensures proper ordering.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WarmupManager warmupManager = WarmupManager.getInstance();
            Assert.assertFalse(warmupManager.hasSpareWebContents());
        });
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCreateSpareRendererCanBeRecreated() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertSpareWebContentsNotNullAndDestroy();
            Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
        });
        CustomTabsTestUtils.warmUpAndWait();
        TestThreadUtils.runOnUiThreadBlocking(this::assertSpareWebContentsNotNullAndDestroy);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabTakessSpareRenderer() throws Exception {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        mCustomTabsConnection.newSession(token);
        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);
        assertWarmupAndMayLaunchUrl(token, URL, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            String referrer = mCustomTabsConnection.getDefaultReferrerForSession(token).getUrl();
            Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents());
        });
    }

    /*
     * Tests that when the disconnection notification comes from a non-UI thread, Chrome doesn't
     * crash. Non-regression test for crbug.com/623128.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPrerenderAndDisconnectOnOtherThread() throws Exception {
        final CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        final Thread otherThread = new Thread(() -> mCustomTabsConnection.cleanUpSession(token));

        TestThreadUtils.runOnUiThreadBlocking(otherThread::start);
        // Should not crash, hence no assertions below.
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMayLaunchUrlKeepsSpareRendererWithoutHiddenTab() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));

        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);
        mCustomTabsConnection.setCanUseHiddenTabForSession(token, false);
        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));

        TestThreadUtils.runOnUiThreadBlocking(() -> assertSpareWebContentsNotNullAndDestroy());
    }

    @Test
    @SmallTest
    public void testMayLaunchUrlNullOrEmptyUrl() throws Exception {
        assertWarmupAndMayLaunchUrl(null, null, true);
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection); // Resets throttling.
        assertWarmupAndMayLaunchUrl(null, "", true);
    }

    /**
     * Tests that a new mayLaunchUrl() call destroys the previous hidden tab.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testOnlyOneHiddenTab() throws Exception {
        Assert.assertTrue("Failed warmup()", mCustomTabsConnection.warmup(0));
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue("Failed newSession()", mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setCanUseHiddenTabForSession(token, true);

        // First hidden tab, add an observer to check that it's destroyed.
        Assert.assertTrue("Failed first mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        final CallbackHelper tabDestroyedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull("Null speculation, first one",
                    mCustomTabsConnection.getSpeculationParamsForTesting());
            Tab tab = mCustomTabsConnection.getSpeculationParamsForTesting().tab;
            Assert.assertNotNull("No first tab", tab);
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onDestroyed(Tab destroyedTab) {
                    tabDestroyedHelper.notifyCalled();
                }
            });
        });

        // New hidden tab.
        mCustomTabsConnection.resetThrottling(Process.myUid());
        Assert.assertTrue("Failed second mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL2), null, null));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull("Null speculation, new hidden tab",
                    mCustomTabsConnection.getSpeculationParamsForTesting());
            Assert.assertNotNull("No second tab",
                    mCustomTabsConnection.getSpeculationParamsForTesting().tab);
            Assert.assertEquals(URL2, mCustomTabsConnection.getSpeculationParamsForTesting().url);
        });
        tabDestroyedHelper.waitForCallback("The first hidden tab should have been destroyed", 0);

        // Clears the second hidden tab.
        mCustomTabsConnection.resetThrottling(Process.myUid());
        Assert.assertTrue("Failed cleanup mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, null, null, null));
    }

    /**
     * Tests that if the renderer backing a hidden tab is killed, the speculation is
     * canceled.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testKillHiddenTabRenderer() throws Exception {
        Assert.assertTrue("Failed warmup()", mCustomTabsConnection.warmup(0));
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue("Failed newSession()", mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);
        Assert.assertTrue("Failed first mayLaunchUrl()",
                mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        final CallbackHelper tabDestroyedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertNotNull("Null speculation",
                    mCustomTabsConnection.getSpeculationParamsForTesting());
            Tab speculationTab = mCustomTabsConnection.getSpeculationParamsForTesting().tab;
            Assert.assertNotNull("Null speculation tab", speculationTab);
            speculationTab.addObserver(new EmptyTabObserver() {
                @Override
                public void onDestroyed(Tab tab) {
                    tabDestroyedHelper.notifyCalled();
                }
            });
            WebContentsUtils.simulateRendererKilled(speculationTab.getWebContents());
        });
        tabDestroyedHelper.waitForCallback("The speculated tab was not destroyed", 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testUnderstandsLowConfidenceMayLaunchUrl() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        List<Bundle> urls = new ArrayList<>();
        Bundle urlBundle = new Bundle();
        urlBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urls.add(urlBundle);
        mCustomTabsConnection.mayLaunchUrl(token, null, null, urls);

        TestThreadUtils.runOnUiThreadBlocking(this::assertSpareWebContentsNotNullAndDestroy);
    }

    @Test
    @SmallTest
    public void testLowConfidenceMayLaunchUrlOnlyAcceptUris() throws Exception {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        CustomTabsTestUtils.warmUpAndWait();

        final List<Bundle> urlsAsString = new ArrayList<>();
        Bundle urlStringBundle = new Bundle();
        urlStringBundle.putString(CustomTabsService.KEY_URL, URL);
        urlsAsString.add(urlStringBundle);

        final List<Bundle> urlsAsUri = new ArrayList<>();
        Bundle urlUriBundle = new Bundle();
        urlUriBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urlsAsUri.add(urlUriBundle);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(mCustomTabsConnection.lowConfidenceMayLaunchUrl(urlsAsString));
            Assert.assertTrue(mCustomTabsConnection.lowConfidenceMayLaunchUrl(urlsAsUri));
        });
    }

    @Test
    @SmallTest
    public void testLowConfidenceMayLaunchUrlDoesntCrash() throws Exception {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        CustomTabsTestUtils.warmUpAndWait();

        final List<Bundle> invalidBundles = new ArrayList<>();
        Bundle invalidBundle = new Bundle();
        invalidBundle.putParcelable(CustomTabsService.KEY_URL, new Intent());
        invalidBundles.add(invalidBundle);

        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mCustomTabsConnection.lowConfidenceMayLaunchUrl(invalidBundles));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testStillHighConfidenceMayLaunchUrlWithSeveralUrls() {
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);
        List<Bundle> urls = new ArrayList<>();
        Bundle urlBundle = new Bundle();
        urlBundle.putParcelable(CustomTabsService.KEY_URL, Uri.parse(URL));
        urls.add(urlBundle);

        mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, urls);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertNull(
                                WarmupManager.getInstance().takeSpareWebContents(false, false)));
    }

    private void assertSpareWebContentsNotNullAndDestroy() {
        WebContents webContents = WarmupManager.getInstance().takeSpareWebContents(false, false);
        Assert.assertNotNull(webContents);
        webContents.destroy();
    }

    /**
     * Calls warmup() and mayLaunchUrl(), checks for the expected result
     * (success or failure) and returns the result code.
     */
    private CustomTabsSessionToken assertWarmupAndMayLaunchUrl(
            CustomTabsSessionToken token, String url, boolean shouldSucceed) throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        if (token == null) {
            token = CustomTabsSessionToken.createMockSessionTokenForTesting();
            mCustomTabsConnection.newSession(token);
        }
        Uri uri = url == null ? null : Uri.parse(url);
        boolean succeeded = mCustomTabsConnection.mayLaunchUrl(token, uri, null, null);
        Assert.assertEquals(shouldSucceed, succeeded);
        return shouldSucceed ? token : null;
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(
     * CustomTabsSessionToken, Uri, android.os.Bundle, java.util.List)}
     * returns an error when called with an invalid session ID.
     */
    @Test
    @SmallTest
    public void testNoMayLaunchUrlWithInvalidSessionId() throws Exception {
        assertWarmupAndMayLaunchUrl(
                CustomTabsSessionToken.createMockSessionTokenForTesting(), URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * rejects invalid URL schemes.
     */
    @Test
    @SmallTest
    @DisabledTest
    public void testNoMayLaunchUrlWithInvalidScheme() throws Exception {
        assertWarmupAndMayLaunchUrl(null, INVALID_SCHEME_URL, false);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * succeeds.
     */
    @Test
    @SmallTest
    public void testMayLaunchUrl() throws Exception {
        assertWarmupAndMayLaunchUrl(null, URL, true);
    }

    /**
     * Tests that
     * {@link CustomTabsConnection#mayLaunchUrl(CustomTabsSessionToken, Uri, Bundle, List)}
     * can be called several times with the same, and different URLs.
     */
    @Test
    @SmallTest
    public void testMultipleMayLaunchUrl() throws Exception {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        assertWarmupAndMayLaunchUrl(token, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        assertWarmupAndMayLaunchUrl(token, URL2, true);
    }

    /**
     * Tests that sessions are forgotten properly.
     */
    @Test
    @SmallTest
    public void testForgetsSession() throws Exception {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        CustomTabsTestUtils.cleanupSessions(mCustomTabsConnection);
        assertWarmupAndMayLaunchUrl(token, URL, false);
    }

    /**
     * Tests that whether we can detect access rights to /proc/pid/.
     */
    @Test
    @SmallTest
    public void testCanGetSchedulerGroup() {
        Assume.assumeTrue(Build.VERSION.SDK_INT >= Build.VERSION_CODES.M);
        // self is always accessible.
        Assert.assertTrue(CustomTabsConnection.canGetSchedulerGroup(Process.myPid()));
        // PID 1 always exists, yet should never be accessible by regular apps.
        Assert.assertFalse(CustomTabsConnection.canGetSchedulerGroup(1));
    }

    /**
     * Tests that CPU cgroups exist and have the expected values for background and foreground.
     *
     * To make testing easier the test assumes that the Android Framework uses
     * the same cgroup for background processes and background _threads_, which
     * is the the case between LOLLIPOP_MR1 and O.
     */
    @Test
    @SmallTest
    public void testGetSchedulerGroup() throws Exception {
        // After M, /proc is mounted with hidepid=2, so even though the test still passes, it
        // is not useful in practice. See crbug.com/973368 for details.
        Assume.assumeTrue(Build.VERSION.SDK_INT < Build.VERSION_CODES.M);
        Assert.assertNotNull(CustomTabsConnection.getSchedulerGroup(Process.myPid()));
        String cgroup = CustomTabsConnection.getSchedulerGroup(Process.myPid());
        // Tests run in the foreground. Last two are from Android O.
        List<String> foregroundGroups = Arrays.asList("/", "/apps", "/top-app", "/foreground");
        Assert.assertTrue(foregroundGroups.contains(cgroup));

        final AtomicReference<String> backgroundThreadCgroup = new AtomicReference<>();
        Thread backgroundThread = new Thread(() -> {
            int tid = Process.myTid();
            Process.setThreadPriority(tid, Process.THREAD_PRIORITY_BACKGROUND);
            backgroundThreadCgroup.set(CustomTabsConnection.getSchedulerGroup(tid));
        });
        backgroundThread.start();
        backgroundThread.join();
        String threadCgroup = backgroundThreadCgroup.get();
        Assert.assertNotNull(threadCgroup);
        Assert.assertTrue(CustomTabsConnection.BACKGROUND_GROUPS.contains(threadCgroup));
    }

    /**
     * Tests that predictions are throttled.
     */
    @Test
    @SmallTest
    public void testThrottleMayLaunchUrl() throws Exception {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        int successfulRequests = 0;
        // Send a burst of requests instead of checking for precise delays to avoid flakiness.
        while (successfulRequests < 10) {
            if (!mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null)) break;
            successfulRequests++;
        }
        Assert.assertTrue("10 requests in a row should not all succeed.", successfulRequests < 10);
    }

    /**
     * Tests that the mayLaunchUrl() throttling is reset after a long enough wait.
     */
    @Test
    @SmallTest
    public void testThrottlingIsReset() throws Exception {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        // Depending on the timing, the delay should be 100 or 200ms here.
        assertWarmupAndMayLaunchUrl(token, URL, true);
        // assertWarmUpAndMayLaunchUrl() can take longer than the throttling delay.
        Assert.assertFalse(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        // Wait for more than 2 * MAX_POSSIBLE_DELAY to clear the delay
        try {
            Thread.sleep(450); // 2 * MAX_POSSIBLE_DELAY + 50ms
        } catch (InterruptedException e) {
            Assert.fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(token, URL, true);
        // Check that the delay has been reset, by waiting for 100ms.
        try {
            Thread.sleep(150); // MIN_DELAY + 50ms margin
        } catch (InterruptedException e) {
            Assert.fail();
            return;
        }
        assertWarmupAndMayLaunchUrl(token, URL, true);
    }

    /**
     * Tests that throttling applies across sessions.
     */
    @Test
    @SmallTest
    public void testThrottlingAcrossSessions() throws Exception {
        CustomTabsSessionToken token = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        CustomTabsSessionToken token2 = assertWarmupAndMayLaunchUrl(null, URL, true);
        mCustomTabsConnection.resetThrottling(Process.myUid());
        for (int i = 0; i < 10; i++) {
            mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null);
        }
        Assert.assertFalse(mCustomTabsConnection.mayLaunchUrl(token2, Uri.parse(URL), null, null));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBanningWorks() {
        mCustomTabsConnection.ban(Process.myUid());
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        TestThreadUtils.runOnUiThreadBlocking(this::assertSpareWebContentsNotNullAndDestroy);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testBanningDisabledForCellular() {
        mCustomTabsConnection.ban(Process.myUid());
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);

        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertNull(
                                WarmupManager.getInstance().takeSpareWebContents(false, false)));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testCellularPrerenderingDoesntOverrideSettings() throws Exception {
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);
        CustomTabsTestUtils.warmUpAndWait();

        // Needs the browser process to be initialized.
        @PreloadPagesState
        int state = TestThreadUtils.runOnUiThreadBlocking(() -> {
            @PreloadPagesState
            int oldState = PreloadPagesSettingsBridge.getState();
            PreloadPagesSettingsBridge.setState(PreloadPagesState.NO_PRELOADING);
            return oldState;
        });

        try {
            Assert.assertTrue(
                    mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
            TestThreadUtils.runOnUiThreadBlocking(this::assertSpareWebContentsNotNullAndDestroy);
        } finally {
            TestThreadUtils.runOnUiThreadBlocking(() -> PreloadPagesSettingsBridge.setState(state));
        }
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testHiddenTabTakesSpareRenderer() throws Exception {
        CustomTabsSessionToken token = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mCustomTabsConnection.newSession(token));
        mCustomTabsConnection.setShouldSpeculateLoadOnCellularForSession(token, true);
        CustomTabsTestUtils.warmUpAndWait();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertTrue(WarmupManager.getInstance().hasSpareWebContents()));
        Assert.assertTrue(mCustomTabsConnection.mayLaunchUrl(token, Uri.parse(URL), null, null));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(WarmupManager.getInstance().hasSpareWebContents()));
    }

    @Test
    @SmallTest
    public void testWarmupNotificationIsSent() throws Exception {
        final AtomicReference<CustomTabsClient> clientReference = new AtomicReference<>(null);
        final CallbackHelper waitForConnection = new CallbackHelper();
        CustomTabsClient.bindCustomTabsService(InstrumentationRegistry.getContext(),
                InstrumentationRegistry.getTargetContext().getPackageName(),
                new CustomTabsServiceConnection() {
                    @Override
                    public void onServiceDisconnected(ComponentName name) {}

                    @Override
                    public void onCustomTabsServiceConnected(
                            ComponentName name, CustomTabsClient client) {
                        clientReference.set(client);
                        waitForConnection.notifyCalled();
                    }
                });
        waitForConnection.waitForCallback(0);
        CustomTabsClient client = clientReference.get();
        final CallbackHelper warmupWaiter = new CallbackHelper();
        CustomTabsSession session = newSessionWithWarmupWaiter(client, warmupWaiter);
        CustomTabsSession session2 = newSessionWithWarmupWaiter(client, warmupWaiter);

        // Both sessions should be notified.
        Assert.assertTrue(mCustomTabsConnection.warmup(0));
        warmupWaiter.waitForCallback(0, 2);

        // Notifications should be sent even if warmup() has already been called.
        Assert.assertTrue(mCustomTabsConnection.warmup(0));
        warmupWaiter.waitForCallback(2, 2);
    }

    private static CustomTabsSession newSessionWithWarmupWaiter(
            CustomTabsClient client, final CallbackHelper waiter) {
        return client.newSession(new CustomTabsCallback() {
            @Override
            public void extraCallback(String callbackName, Bundle args) {
                if (callbackName.equals(CustomTabsConnection.ON_WARMUP_COMPLETED)) {
                    waiter.notifyCalled();
                }
            }
        });
    }
}
