// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.View;

import androidx.annotation.IntDef;

import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.RenderFrameHostTestExt;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Class containing the core framework for all XR (VR and AR) testing, which requires
 * back-and-forth communication with JavaScript running in the browser. Feature-specific behavior
 * can be found in the *TestFramework subclasses. Additional utility methods that don't relate
 * to the core framework can be found in the util/ directory.
 *
 * The general test flow is:
 * - Load the HTML file containing the test, which:
 *   - Loads the WebVR boilerplate code and some test functions
 *   - Sets up common elements like the canvas and synchronization variable
 *   - Sets up any steps that need to be triggered by the Java code
 * - Check if any VRDisplay objects were found and fail the test if it doesn't
 *       match what we expect for that test
 * - Repeat:
 *   - Run any necessary Java-side code, e.g. trigger a user action
 *   - Trigger the next JavaScript test step and wait for it to finish
 *
 * The JavaScript code will automatically process test results once all
 * testharness.js tests are done, just like in layout tests. Once the results
 * are processed, the JavaScript code will automatically signal the Java code,
 * which can then grab the results and pass/fail the instrumentation test.
 */
public abstract class XrTestFramework {
    public static final int PAGE_LOAD_TIMEOUT_S = 10;
    public static final int POLL_CHECK_INTERVAL_SHORT_MS = 50;
    public static final int POLL_CHECK_INTERVAL_LONG_MS = 100;
    public static final int POLL_TIMEOUT_SHORT_MS = 1000;
    public static final int POLL_TIMEOUT_LONG_MS = 10000;
    public static final boolean DEBUG_LOGS = false;

    // The "3" corresponds to the "Mobile Bookmarks" folder - omitting a particular folder
    // automatically redirects to that folder, and not having it in the URL causes issues with the
    // URL we expect to be loaded being different than the actual URL.
    public static final String[] NATIVE_URLS_OF_INTEREST = {UrlConstants.BOOKMARKS_FOLDER_URL + "3",
            UrlConstants.BOOKMARKS_UNCATEGORIZED_URL, UrlConstants.BOOKMARKS_URL,
            UrlConstants.DOWNLOADS_URL, UrlConstants.NATIVE_HISTORY_URL, UrlConstants.NTP_URL,
            UrlConstants.RECENT_TABS_URL};

    private static final String TAG = "XrTestFramework";
    static final String TEST_DIR = "chrome/test/data/xr/e2e_test_files";

    // Test status enum
    @IntDef({TestStatus.RUNNING, TestStatus.PASSED, TestStatus.FAILED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TestStatus {
        int RUNNING = 0;
        int PASSED = 1;
        int FAILED = 2;
    }

    private ChromeActivityTestRule mRule;

    /**
     * Gets the file:// URL to the test file.
     *
     * @param testName The name of the test whose file will be retrieved.
     * @return The file:// URL to the specified test file.
     */
    public static String getFileUrlForHtmlTestFile(String testName) {
        return "file://" + UrlUtils.getIsolatedTestFilePath(TEST_DIR) + "/html/" + testName
                + ".html";
    }

    /**
     * Checks whether a request for the given permission would trigger a permission prompt.
     *
     * @param permission The name of the permission to check.
     * @param webContents The WebContents to run the JavaScript in.
     * @return True if the permission request would trigger a prompt, false otherwise.
     */
    public static boolean permissionRequestWouldTriggerPrompt(
            String permission, WebContents webContents) {
        runJavaScriptOrFail("checkPermissionRequestWouldTriggerPrompt('" + permission + "')",
                POLL_TIMEOUT_SHORT_MS, webContents);
        pollJavaScriptBooleanOrFail("wouldPrompt !== null", POLL_TIMEOUT_SHORT_MS, webContents);
        return Boolean.valueOf(
                runJavaScriptOrFail("wouldPrompt", POLL_TIMEOUT_SHORT_MS, webContents));
    }

    /**
     * Helper function to run the given JavaScript, return the return value, and fail if a
     * timeout/interrupt occurs so we don't have to catch or declare exceptions all the time.
     *
     * @param js The JavaScript to run.
     * @param timeout The timeout in milliseconds before a failure.
     * @param webContents The WebContents object to run the JavaScript in.
     * @return The return value of the JavaScript.
     */
    public static String runJavaScriptOrFail(String js, int timeout, WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "runJavaScriptOrFail " + js);
        try {
            String ret = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                    webContents, js, timeout, TimeUnit.MILLISECONDS);
            if (DEBUG_LOGS) Log.i(TAG, "runJavaScriptOrFail result=" + ret);
            return ret;
        } catch (TimeoutException e) {
            Assert.fail("Fatal interruption or timeout running JavaScript '" + js
                    + "': " + e.toString());
        }
        return "Not reached";
    }

    /**
     * Runs the given JavaScript in the focused frame, failing if a timeout/interrupt occurs.
     *
     * @param js The JavaScript to run.
     * @param timeout The timeout in milliseconds before failure.
     * @param webContents The WebContents object to get the focused frame from.
     * @return The return value of the JavaScript.
     */
    public static String runJavaScriptInFrameOrFail(
            String js, int timeout, final WebContents webContents) {
        return runJavaScriptInFrameInternal(js, timeout, webContents, true /* failOnTimeout */);
    }

    /**
     * Polls the provided JavaScript boolean expression until the timeout is reached or the boolean
     * is true.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @param webContents The WebContents to run the JavaScript through.
     * @return True if the boolean evaluated to true, false if timed out.
     */
    public static boolean pollJavaScriptBoolean(
            final String boolExpression, int timeoutMs, final WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "pollJavaScriptBoolean " + boolExpression);
        try {
            CriteriaHelper.pollInstrumentationThread(() -> {
                String result = "false";
                try {
                    result = JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents,
                            boolExpression, POLL_CHECK_INTERVAL_SHORT_MS, TimeUnit.MILLISECONDS);
                    if (DEBUG_LOGS) {
                        Log.i(TAG, "pollJavaScriptBoolean " + boolExpression + " => " + result);
                    }
                } catch (TimeoutException e) {
                    // Expected to happen regularly, do nothing
                }
                return Boolean.parseBoolean(result);
            }, "Polling timed out", timeoutMs, POLL_CHECK_INTERVAL_LONG_MS);
        } catch (AssertionError e) {
            Log.d(TAG, "pollJavaScriptBoolean() timed out: " + e.toString());
            return false;
        }
        return true;
    }

    /**
     * Polls the provided JavaScript boolean expression in the focused frame until the timeout is
     * reached or the boolean is true.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @param webContents The WebContents to get the focused frame from.
     * @return True if the boolean evaluated to true, false if timed out.
     */
    public static boolean pollJavaScriptBooleanInFrame(
            final String boolExpression, int timeoutMs, final WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "pollJavaScriptBooleanInFrame " + boolExpression);
        try {
            CriteriaHelper.pollInstrumentationThread(() -> {
                String result = "false";
                result = runJavaScriptInFrameInternal(boolExpression, POLL_CHECK_INTERVAL_SHORT_MS,
                        webContents, false /* failOnTimeout */);
                if (DEBUG_LOGS) {
                    Log.i(TAG, "pollJavaScriptBooleanInFrame " + boolExpression + " => " + result);
                }
                return Boolean.parseBoolean(result);
            }, "Polling timed out", timeoutMs, POLL_CHECK_INTERVAL_LONG_MS);
        } catch (AssertionError e) {
            Log.d(TAG, "pollJavaScriptBooleanInFrame() timed out: " + e.toString());
            return false;
        }
        return true;
    }

    /**
     * Polls the provided JavaScript boolean expression, failing the test if it does not evaluate
     * to true within the provided timeout.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @param webContents The Webcontents to run the JavaScript through.
     */
    public static void pollJavaScriptBooleanOrFail(
            String boolExpression, int timeoutMs, WebContents webContents) {
        Assert.assertTrue("Timed out polling JavaScript boolean expression: " + boolExpression,
                pollJavaScriptBoolean(boolExpression, timeoutMs, webContents));
    }

    /**
     * Polls the provided JavaScript boolean expression in the focused frame, failing the test if
     * it does not evaluate to true within the provided timeout.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @param webContents The WebContents to get the focused frame from.
     */
    public static void pollJavaScriptBooleanInFrameOrFail(
            String boolExpression, int timeoutMs, WebContents webContents) {
        Assert.assertTrue("Timed out polling JavaScript boolean expression in focused frame: "
                        + boolExpression,
                pollJavaScriptBooleanInFrame(boolExpression, timeoutMs, webContents));
    }

    /**
     * Executes a JavaScript step function using the given WebContents.
     *
     * @param stepFunction The JavaScript step function to call.
     * @param webContents The WebContents for the tab the JavaScript is in.
     */
    public static void executeStepAndWait(String stepFunction, WebContents webContents) {
        // Run the step and block
        if (DEBUG_LOGS) Log.i(TAG, "executeStepAndWait " + stepFunction);
        JavaScriptUtils.executeJavaScript(webContents, stepFunction);
        if (DEBUG_LOGS) Log.i(TAG, "executeStepAndWait ...wait");
        waitOnJavaScriptStep(webContents);
        if (DEBUG_LOGS) Log.i(TAG, "executeStepAndWait ...done");
    }

    /**
     * Waits for a JavaScript step to finish, asserting that the step finished instead of timing
     * out.
     *
     * @param webContents The WebContents for the tab the JavaScript step is in.
     */
    public static void waitOnJavaScriptStep(WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "waitOnJavaScriptStep");
        // Make sure we aren't trying to wait on a JavaScript test step without the code to do so.
        Assert.assertTrue("Attempted to wait on a JavaScript step without the code to do so. You "
                        + "either forgot to import webxr_e2e.js or are incorrectly using a "
                        + "Java method.",
                Boolean.parseBoolean(runJavaScriptOrFail("typeof javascriptDone !== 'undefined'",
                        POLL_TIMEOUT_SHORT_MS, webContents)));

        // Actually wait for the step to finish
        boolean success =
                pollJavaScriptBoolean("javascriptDone", POLL_TIMEOUT_LONG_MS, webContents);

        // Check what state we're in to make sure javascriptDone wasn't called because the test
        // failed.
        @TestStatus
        int testStatus = checkTestStatus(webContents);
        if (!success || testStatus == TestStatus.FAILED) {
            // Failure states: Either polling failed or polling succeeded, but because the test
            // failed.
            String reason;
            if (!success) {
                reason = "Timed out waiting for JavaScript step to finish.";
            } else {
                reason = "JavaScript testharness reported failure while waiting for JavaScript "
                        + "step to finish";
            }
            String resultString =
                    runJavaScriptOrFail("resultString", POLL_TIMEOUT_SHORT_MS, webContents);
            if (resultString.equals("\"\"")) {
                reason += " Did not obtain specific failure reason from JavaScript testharness.";
            } else {
                reason += " JavaScript testharness reported failure reason: " + resultString;
            }
            Assert.fail(reason);
        }

        // Reset the synchronization boolean
        runJavaScriptOrFail("javascriptDone = false", POLL_TIMEOUT_SHORT_MS, webContents);
    }

    /**
     * Retrieves the current status of the JavaScript test and returns an enum corresponding to it.
     *
     * @param webContents The WebContents for the tab to check the status in.
     * @return A TestStatus integer corresponding to the current state of the JavaScript test.
     */
    @TestStatus
    public static int checkTestStatus(WebContents webContents) {
        String resultString =
                runJavaScriptOrFail("resultString", POLL_TIMEOUT_SHORT_MS, webContents);
        boolean testPassed = Boolean.parseBoolean(
                runJavaScriptOrFail("testPassed", POLL_TIMEOUT_SHORT_MS, webContents));
        if (testPassed) {
            return TestStatus.PASSED;
        } else if (!testPassed && resultString.equals("\"\"")) {
            return TestStatus.RUNNING;
        } else {
            // !testPassed && !resultString.equals("\"\"")
            return TestStatus.FAILED;
        }
    }

    /**
     * Helper function to end the test harness test and assert that it passed, setting the failure
     * reason as the description if it didn't.
     *
     * @param webContents The WebContents for the tab to check test results in.
     */
    public static void endTest(WebContents webContents) {
        switch (checkTestStatus(webContents)) {
            case TestStatus.PASSED:
                break;
            case TestStatus.FAILED:
                String resultString =
                        runJavaScriptOrFail("resultString", POLL_TIMEOUT_SHORT_MS, webContents);
                Assert.fail("JavaScript testharness failed with reason: " + resultString);
                break;
            case TestStatus.RUNNING:
                Assert.fail("Attempted to end test in Java without finishing in JavaScript.");
                break;
            default:
                Assert.fail("Received unknown test status.");
        }
    }

    /**
     * Helper function to make sure that the JavaScript test harness did not detect any failures.
     * Similar to endTest, but does not fail if the test is still detected as running. This is
     * useful because not all tests make use of the test harness' test/assert features (particularly
     * simple enter/exit tests), but may still want to ensure that no unexpected JavaScript errors
     * were encountered.
     *
     * @param webContents The Webcontents for the tab to check for failures in.
     */
    public static void assertNoJavaScriptErrors(WebContents webContents) {
        if (checkTestStatus(webContents) == TestStatus.FAILED) {
            String resultString =
                    runJavaScriptOrFail("resultString", POLL_TIMEOUT_SHORT_MS, webContents);
            Assert.fail("JavaScript testharness failed with reason: " + resultString);
        }
    }

    private static String runJavaScriptInFrameInternal(
            String js, int timeout, final WebContents webContents, boolean failOnTimeout) {
        RenderFrameHostTestExt rfh = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> new RenderFrameHostTestExt(WebContentsUtils.getFocusedFrame(webContents)));
        Assert.assertTrue("Did not get a focused frame", rfh != null);
        final CountDownLatch latch = new CountDownLatch(1);
        final AtomicReference<String> result = new AtomicReference<String>();
        // The JS execution needs to be started on the UI thread to avoid hitting a DCHECK.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            rfh.executeJavaScript(js, (String r) -> {
                result.set(r);
                latch.countDown();
            });
        });
        try {
            if (!latch.await(timeout, TimeUnit.MILLISECONDS) && failOnTimeout) {
                Assert.fail("Timed out running JavaScript in focused frame: " + js);
            }
        } catch (InterruptedException e) {
            Assert.fail("Waiting for latch was interrupted: " + e.toString());
        }
        return result.get();
    }

    /**
     * Must be constructed after the rule has been applied (e.g. in whatever method is
     * tagged with @Before).
     */
    public XrTestFramework(ChromeActivityTestRule rule) {
        mRule = rule;
    }

    /**
     * Gets the URL that loads the given test file from the embedded test server.
     *
     * @param testName The name of the test whose file will be retrieved.
     */
    public String getEmbeddedServerUrlForHtmlTestFile(String testName) {
        return mRule.getTestServer().getURL("/" + TEST_DIR + "/html/" + testName + ".html");
    }

    /**
     * Loads the given URL with the given timeout then waits for JavaScript to
     * signal that it's ready for testing.
     *
     * @param url The URL of the page to load.
     * @param timeoutSec The timeout of the page load in seconds.
     * @return The return value of ChromeActivityTestRule.loadUrl().
     */
    public int loadUrlAndAwaitInitialization(String url, int timeoutSec) {
        int result = mRule.loadUrl(url, timeoutSec);
        Assert.assertTrue("Timed out waiting for JavaScript test initialization",
                pollJavaScriptBoolean("isInitializationComplete()", POLL_TIMEOUT_LONG_MS,
                        mRule.getWebContents()));
        return result;
    }

    /**
     * Helper method to run permissionRequestWouldTriggerPrompt with the current tab's WebContents.
     *
     * @param permission The name of the permission to check.
     * @return True if the permission request would trigger a prompt, false otherwise.
     */
    public boolean permissionRequestWouldTriggerPrompt(String permission) {
        return permissionRequestWouldTriggerPrompt(permission, getCurrentWebContents());
    }

    /**
     * Helper method to run runJavaScriptOrFail with the current tab's WebContents.
     *
     * @param js The JavaScript to run.
     * @param timeout The timeout in milliseconds before a failure.
     * @return The return value of the JavaScript.
     */
    public String runJavaScriptOrFail(String js, int timeout) {
        return runJavaScriptOrFail(js, timeout, getCurrentWebContents());
    }

    /**
     * Helper method to run runJavaScriptInFrameOrFail with the current tab's WebContents.
     *
     * @param js The JavaScript to run.
     * @param timeout The timeout in milliseconds before a failure.
     * @return The return value of the JavaScript.
     */
    public String runJavaScriptInFrameOrFail(String js, int timeout) {
        return runJavaScriptInFrameOrFail(js, timeout, getCurrentWebContents());
    }

    /**
     * Helper function to run pollJavaScriptBoolean with the current tab's WebContents.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @return True if the boolean evaluated to true, false if timed out.
     */
    public boolean pollJavaScriptBoolean(String boolExpression, int timeoutMs) {
        return pollJavaScriptBoolean(boolExpression, timeoutMs, getCurrentWebContents());
    }

    /**
     * Helper function to run pollJavaScriptBooleanInFrame with the current tab's WebContents.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     * @return True if the boolean evaluated to true, false if timed out.
     */
    public boolean pollJavaScriptInFrameBoolean(String boolExpression, int timeoutMs) {
        return pollJavaScriptBooleanInFrame(boolExpression, timeoutMs, getCurrentWebContents());
    }

    /**
     * Helper function to run pollJavaScriptBooleanOrFail with the current tab's WebContents.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     */
    public void pollJavaScriptBooleanOrFail(String boolExpression, int timeoutMs) {
        pollJavaScriptBooleanOrFail(boolExpression, timeoutMs, getCurrentWebContents());
    }

    /**
     * Helper function to run pollJavaScriptBooleanInFrameOrFail with the current tab's WebContents.
     *
     * @param boolExpression The JavaScript boolean expression to poll.
     * @param timeoutMs The polling timeout in milliseconds.
     */
    public void pollJavaScriptBooleanInFrameOrFail(String boolExpression, int timeoutMs) {
        pollJavaScriptBooleanInFrameOrFail(boolExpression, timeoutMs, getCurrentWebContents());
    }

    /**
     * Helper function to run executeStepAndWait using the current tab's WebContents.
     *
     * @param stepFunction The JavaScript step function to call.
     */
    public void executeStepAndWait(String stepFunction) {
        executeStepAndWait(stepFunction, getCurrentWebContents());
    }

    /**
     * Helper function to run waitOnJavaScriptStep with current current tab's WebContents.
     */
    public void waitOnJavaScriptStep() {
        waitOnJavaScriptStep(getCurrentWebContents());
    }

    /**
     * Helper method to run checkTestSTatus with the current tab's WebContents.
     *
     * @return A TestStatus integer corresponding to the current state of the JavaScript test.
     */
    @TestStatus
    public int checkTestStatus() {
        return checkTestStatus(getCurrentWebContents());
    }

    /**
     * Helper function to run endTest with the current tab's WebContents.
     */
    public void endTest() {
        endTest(getCurrentWebContents());
    }

    /**
     * Helper function to run assertNoJavaScriptErrors with the current tab's WebContents.
     */
    public void assertNoJavaScriptErrors() {
        assertNoJavaScriptErrors(getCurrentWebContents());
    }

    public View getCurrentContentView() {
        return mRule.getActivity().getActivityTab().getContentView();
    }

    public WebContents getCurrentWebContents() {
        return mRule.getWebContents();
    }

    public ChromeActivityTestRule getRule() {
        return mRule;
    }

    public void simulateRendererKilled() {
        final Tab tab = getRule().getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeTabUtils.simulateRendererKilledForTesting(tab, true));

        CriteriaHelper.pollUiThread(
                () -> SadTab.isShowing(tab), "Renderer killed, but sad tab not shown");
    }

    public void openIncognitoTab(final String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRule.getActivity()
                    .getTabCreator(true /* incognito */)
                    .launchUrl(url, TabLaunchType.FROM_LINK);
        });
    }
}
