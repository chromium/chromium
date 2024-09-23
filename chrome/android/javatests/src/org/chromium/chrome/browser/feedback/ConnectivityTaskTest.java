// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.chromium.chrome.browser.feedback.ConnectivityCheckerTestRule.TIMEOUT_MS;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.feedback.ConnectivityTask.FeedbackData;
import org.chromium.chrome.browser.feedback.ConnectivityTask.Type;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.net.ConnectionType;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link ConnectivityTask}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ConnectivityTaskTest {
    @Rule
    public ConnectivityCheckerTestRule mConnectivityCheckerTestRule =
            new ConnectivityCheckerTestRule();

    @Rule public ExpectedException thrown = ExpectedException.none();

    private static final int RESULT_CHECK_INTERVAL_MS = 10;

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testNormalCaseShouldWork() {
        final ConnectivityTask task =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<ConnectivityTask>() {
                            @Override
                            public ConnectivityTask call() {
                                // Intentionally make HTTPS-connection fail which should result in
                                // NOT_CONNECTED.
                                ConnectivityChecker.overrideUrlsForTest(
                                        mConnectivityCheckerTestRule.getGenerated204Url(),
                                        mConnectivityCheckerTestRule.getGenerated404Url());
                                // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                                return ConnectivityTask.create(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        TIMEOUT_MS,
                                        null);
                            }
                        });

        CriteriaHelper.pollUiThread(
                () -> {
                    return task.isDone();
                },
                "Should be finished by now.",
                TIMEOUT_MS,
                RESULT_CHECK_INTERVAL_MS);
        FeedbackData feedback = getResult(task);
        verifyConnections(feedback, ConnectivityCheckResult.NOT_CONNECTED);
        Assert.assertEquals("The timeout value is wrong.", TIMEOUT_MS, feedback.getTimeoutMs());
    }

    private static void verifyConnections(FeedbackData feedback, int expectedHttpsValue) {
        Map<Integer, Integer> results = feedback.getConnections();
        Assert.assertEquals("Should have 4 results.", 4, results.size());
        for (Map.Entry<Integer, Integer> result : results.entrySet()) {
            switch (result.getKey()) {
                case Type.CHROME_HTTP:
                case Type.SYSTEM_HTTP:
                    assertResult(ConnectivityCheckResult.CONNECTED, result);
                    break;
                case Type.CHROME_HTTPS:
                case Type.SYSTEM_HTTPS:
                    assertResult(expectedHttpsValue, result);
                    break;
                default:
                    Assert.fail("Failed to recognize type " + result.getKey());
            }
        }
        Assert.assertTrue(
                "The elapsed time should be non-negative.", feedback.getElapsedTimeMs() >= 0);
    }

    private static void assertResult(int expectedValue, Map.Entry<Integer, Integer> actualEntry) {
        Assert.assertEquals(
                "Wrong result for " + actualEntry.getKey(),
                ConnectivityTask.getHumanReadableResult(expectedValue),
                ConnectivityTask.getHumanReadableResult(actualEntry.getValue()));
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testCallbackNormalCaseShouldWork() throws InterruptedException {
        final Semaphore semaphore = new Semaphore(0);
        final AtomicReference<FeedbackData> feedbackRef = new AtomicReference<>();
        final ConnectivityTask.ConnectivityResult callback =
                new ConnectivityTask.ConnectivityResult() {
                    @Override
                    public void onResult(FeedbackData feedbackData) {
                        feedbackRef.set(feedbackData);
                        semaphore.release();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Intentionally make HTTPS-connection fail which should result in
                    // NOT_CONNECTED.
                    ConnectivityChecker.overrideUrlsForTest(
                            mConnectivityCheckerTestRule.getGenerated204Url(),
                            mConnectivityCheckerTestRule.getGenerated404Url());
                    // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                    ConnectivityTask.create(
                            ProfileManager.getLastUsedRegularProfile(), TIMEOUT_MS, callback);
                });
        if (!semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            Assert.fail("Failed to acquire semaphore.");
        }
        FeedbackData feedback = feedbackRef.get();
        verifyConnections(feedback, ConnectivityCheckResult.NOT_CONNECTED);
        Assert.assertEquals("The timeout value is wrong.", TIMEOUT_MS, feedback.getTimeoutMs());
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testCallbackTwoTimeouts() throws InterruptedException {
        final int checkTimeoutMs = 1000;
        final Semaphore semaphore = new Semaphore(0);
        final AtomicReference<FeedbackData> feedbackRef = new AtomicReference<>();
        final ConnectivityTask.ConnectivityResult callback =
                new ConnectivityTask.ConnectivityResult() {
                    @Override
                    public void onResult(FeedbackData feedbackData) {
                        feedbackRef.set(feedbackData);
                        semaphore.release();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Intentionally make HTTPS connections slow which should result in TIMEOUT.
                    ConnectivityChecker.overrideUrlsForTest(
                            mConnectivityCheckerTestRule.getGenerated204Url(),
                            mConnectivityCheckerTestRule.getGeneratedSlowUrl());
                    // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                    ConnectivityTask.create(
                            ProfileManager.getLastUsedRegularProfile(), checkTimeoutMs, callback);
                });
        if (!semaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
            Assert.fail("Failed to acquire semaphore.");
        }
        FeedbackData feedback = feedbackRef.get();
        // In the case of a timeout when using callbacks, the result will be TIMEOUT instead
        // of UNKNOWN.
        verifyConnections(feedback, ConnectivityCheckResult.TIMEOUT);
        Assert.assertEquals("The timeout value is wrong.", checkTimeoutMs, feedback.getTimeoutMs());
    }

    @Test
    @MediumTest
    @Feature({"Feedback"})
    public void testTwoTimeoutsShouldFillInTheRest() {
        final ConnectivityTask task =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<ConnectivityTask>() {
                            @Override
                            public ConnectivityTask call() {
                                // Intentionally make HTTPS connections slow which should result in
                                // UNKNOWN.
                                ConnectivityChecker.overrideUrlsForTest(
                                        mConnectivityCheckerTestRule.getGenerated204Url(),
                                        mConnectivityCheckerTestRule.getGeneratedSlowUrl());
                                // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                                return ConnectivityTask.create(
                                        ProfileManager.getLastUsedRegularProfile(),
                                        TIMEOUT_MS,
                                        null);
                            }
                        });
        thrown.expect(CriteriaHelper.TimeoutException.class);
        CriteriaHelper.pollUiThread(
                () -> {
                    return task.isDone();
                },
                TIMEOUT_MS / 5,
                RESULT_CHECK_INTERVAL_MS);
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testFeedbackDataConversion() {
        Map<Integer, Integer> connectionMap = new HashMap<>();
        connectionMap.put(Type.CHROME_HTTP, ConnectivityCheckResult.NOT_CONNECTED);
        connectionMap.put(Type.CHROME_HTTPS, ConnectivityCheckResult.CONNECTED);
        connectionMap.put(Type.SYSTEM_HTTP, ConnectivityCheckResult.UNKNOWN);
        connectionMap.put(Type.SYSTEM_HTTPS, ConnectivityCheckResult.CONNECTED);

        FeedbackData feedback =
                new FeedbackData(connectionMap, 42, 21, ConnectionType.CONNECTION_WIFI);
        Map<String, String> map = feedback.toMap();

        Assert.assertEquals("Should have 6 entries.", 6, map.size());
        Assert.assertTrue(map.containsKey(ConnectivityTask.CHROME_HTTP_KEY));
        Assert.assertEquals("NOT_CONNECTED", map.get(ConnectivityTask.CHROME_HTTP_KEY));
        Assert.assertTrue(map.containsKey(ConnectivityTask.CHROME_HTTPS_KEY));
        Assert.assertEquals("CONNECTED", map.get(ConnectivityTask.CHROME_HTTPS_KEY));
        Assert.assertTrue(map.containsKey(ConnectivityTask.SYSTEM_HTTP_KEY));
        Assert.assertEquals("UNKNOWN", map.get(ConnectivityTask.SYSTEM_HTTP_KEY));
        Assert.assertTrue(map.containsKey(ConnectivityTask.SYSTEM_HTTPS_KEY));
        Assert.assertEquals("CONNECTED", map.get(ConnectivityTask.SYSTEM_HTTPS_KEY));
        Assert.assertTrue(map.containsKey(ConnectivityTask.CONNECTION_CHECK_ELAPSED_KEY));
        Assert.assertEquals("21", map.get(ConnectivityTask.CONNECTION_CHECK_ELAPSED_KEY));
        Assert.assertTrue(map.containsKey(ConnectivityTask.CONNECTION_TYPE_KEY));
        Assert.assertEquals("WiFi", map.get(ConnectivityTask.CONNECTION_TYPE_KEY));
    }

    private static FeedbackData getResult(final ConnectivityTask task) {
        final FeedbackData result =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<FeedbackData>() {
                            @Override
                            public FeedbackData call() {
                                return task.get();
                            }
                        });
        return result;
    }
}
