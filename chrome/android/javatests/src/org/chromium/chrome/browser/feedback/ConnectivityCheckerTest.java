// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.chromium.chrome.browser.feedback.ConnectivityCheckerTestRule.TIMEOUT_MS;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link ConnectivityChecker}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ConnectivityCheckerTest {
    @Rule
    public ConnectivityCheckerTestRule mConnectivityCheckerTestRule =
            new ConnectivityCheckerTestRule();

    private static class Callback implements ConnectivityChecker.ConnectivityCheckerCallback {
        private final Semaphore mSemaphore;
        private final AtomicInteger mResult = new AtomicInteger();

        Callback(Semaphore semaphore) {
            mSemaphore = semaphore;
        }

        @Override
        public void onResult(int result) {
            mResult.set(result);
            mSemaphore.release();
        }

        int getResult() {
            return mResult.get();
        }
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testNoContentShouldWorkSystemStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated204Url(),
                ConnectivityCheckResult.CONNECTED,
                TIMEOUT_MS,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testNoContentShouldWorkChromeStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated204Url(),
                ConnectivityCheckResult.CONNECTED,
                TIMEOUT_MS,
                false);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testSlowNoContentShouldNotWorkSystemStack() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(
                mConnectivityCheckerTestRule.getGeneratedSlowUrl(),
                ConnectivityCheckResult.TIMEOUT,
                100,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testSlowNoContentShouldNotWorkChromeStack() throws Exception {
        // Force quick timeout. The server will wait TIMEOUT_MS, so this triggers well before.
        executeTest(
                mConnectivityCheckerTestRule.getGeneratedSlowUrl(),
                ConnectivityCheckResult.TIMEOUT,
                100,
                false);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testHttpOKShouldFailSystemStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated200Url(),
                ConnectivityCheckResult.NOT_CONNECTED,
                TIMEOUT_MS,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testHttpOKShouldFailChromeStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated200Url(),
                ConnectivityCheckResult.NOT_CONNECTED,
                TIMEOUT_MS,
                false);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testMovedTemporarilyShouldFailSystemStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated302Url(),
                ConnectivityCheckResult.NOT_CONNECTED,
                TIMEOUT_MS,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testMovedTemporarilyShouldFailChromeStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated302Url(),
                ConnectivityCheckResult.NOT_CONNECTED,
                TIMEOUT_MS,
                false);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testNotFoundShouldFailSystemStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated404Url(),
                ConnectivityCheckResult.NOT_CONNECTED,
                TIMEOUT_MS,
                true);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testNotFoundShouldFailChromeStack() throws Exception {
        executeTest(
                mConnectivityCheckerTestRule.getGenerated404Url(),
                ConnectivityCheckResult.NOT_CONNECTED,
                TIMEOUT_MS,
                false);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testInvalidURLShouldFailSystemStack() throws Exception {
        executeTest("http:google.com:foo", ConnectivityCheckResult.ERROR, TIMEOUT_MS, true);
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testInvalidURLShouldFailChromeStack() throws Exception {
        executeTest("http:google.com:foo", ConnectivityCheckResult.ERROR, TIMEOUT_MS, false);
    }

    private void executeTest(
            final String url, int expectedResult, final int timeoutMs, final boolean useSystemStack)
            throws Exception {
        Semaphore semaphore = new Semaphore(0);
        final Callback callback = new Callback(semaphore);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (useSystemStack) {
                        ConnectivityChecker.checkConnectivitySystemNetworkStack(
                                url, timeoutMs, callback);
                    } else {
                        // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                        ConnectivityChecker.checkConnectivityChromeNetworkStack(
                                ProfileManager.getLastUsedRegularProfile(),
                                url,
                                timeoutMs,
                                callback);
                    }
                });

        Assert.assertTrue(semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(
                "URL: " + url + ", got " + callback.getResult() + ", want " + expectedResult,
                expectedResult,
                callback.getResult());
    }
}
