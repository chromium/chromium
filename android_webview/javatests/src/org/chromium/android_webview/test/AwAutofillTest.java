// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.SystemClock;
import android.util.Pair;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.view.autofill.AutofillValue;
import android.webkit.WebResourceResponse;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.webkit.WebViewAssetLoader;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.autofill.mojom.SubmissionSource;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.components.autofill.AutofillHintsServiceTestHelper;
import org.chromium.components.autofill.AutofillManagerWrapper;
import org.chromium.components.autofill.AutofillPopup;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.autofill.AutofillProviderTestHelper;
import org.chromium.components.autofill.AutofillProviderUMA;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.TestViewStructure;
import org.chromium.components.autofill_public.ViewType;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;

/** Tests for WebView Autofill. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
// Point autofill::switches::kAutofillServerURL at an unreachable local port so tests with
// AutofillServerCommunication enabled fail queries locally instead of hitting the live server.
@CommandLineFlags.Add({"autofill-server-url=http://127.0.0.1:1/"})
public class AwAutofillTest extends AwParameterizedTest {
    public static final boolean DEBUG = false;
    public static final String TAG = "AutofillTest";

    // How long to wait for an expected Autofill event. Generous because it only matters when an
    // event is missing, and the event pipeline runs through the renderer and the IME thread.
    private static final long EVENT_TIMEOUT_MS = ScalableTimeout.scaleTimeout(10000);

    public static final String FILE = "/login.html";
    public static final String ASSET_URL =
            "https://" + WebViewAssetLoader.DEFAULT_DOMAIN + "/assets/autofill.html";

    public static final int AUTOFILL_VIEW_ENTERED = 0;
    public static final int AUTOFILL_VIEW_EXITED = 1;
    public static final int AUTOFILL_VALUE_CHANGED = 2;
    public static final int AUTOFILL_COMMIT = 3;
    public static final int AUTOFILL_CANCEL = 4;
    public static final int AUTOFILL_SESSION_STARTED = 5;
    public static final int AUTOFILL_PREDICTIONS_AVAILABLE = 6;
    public static final int AUTOFILL_EVENT_MAX = 7;

    public static final String[] EVENT = {
        "VIEW_ENTERED",
        "VIEW_EXITED",
        "VALUE_CHANGED",
        "COMMIT",
        "CANCEL",
        "SESSION_STARTED",
        "QUERY_DONE"
    };

    private static final int NO_VIRTUAL_ID = -1;

    // crbug.com/776230: On Android L, declaring variables of unsupported classes causes an error.
    // Wrapped them in a class to avoid it.
    private static class TestValues {
        public TestViewStructure testViewStructure;
        public ArrayList<Pair<Integer, AutofillValue>> changedValues;
    }

    /** A call of WebView's AutofillProvider to the Android AutofillManager. */
    private static final class AutofillEvent {
        public final int type;
        public final int virtualId;

        public AutofillEvent(int type, int virtualId) {
            this.type = type;
            this.virtualId = virtualId;
        }

        @Override
        public String toString() {
            return virtualId == NO_VIRTUAL_ID ? EVENT[type] : EVENT[type] + "(" + virtualId + ")";
        }
    }

    /**
     * The AutofillManager calls of a test in the order in which they happened. Tests consume them
     * from the start, see {@link #waitForCallbackAndVerifyTypes}.
     */
    private static final class AutofillEventLog {
        private static final int MISMATCH = -2;
        private static final int NEEDS_MORE_EVENTS = -1;

        private final List<AutofillEvent> mEvents = new ArrayList<>();
        // The index of the first event that no test step has consumed yet.
        private int mConsumed;

        public synchronized void add(AutofillEvent event) {
            mEvents.add(event);
            notifyAll();
        }

        public synchronized int size() {
            return mEvents.size();
        }

        /** Marks all events so far as consumed and returns their number. */
        public synchronized int consumeAll() {
            mConsumed = mEvents.size();
            return mConsumed;
        }

        public synchronized String describeFrom(int start) {
            return mEvents.subList(Math.min(start, mEvents.size()), mEvents.size()).toString();
        }

        /**
         * Waits until the unconsumed events at index `start` or later match `expectedTypes`,
         * consumes them and returns the index after them. Fails as soon as the events can no longer
         * match, if other events already follow them, or if they don't match within the timeout.
         *
         * <p>AutofillProvider tells the framework about new bounds of the focused field by exiting
         * and re-entering it. Unless the test expects these events, such pairs are skipped: they
         * depend on layout timing, not on what the test does.
         */
        public synchronized int waitForTypes(int start, int[] expectedTypes) {
            start = Math.max(start, mConsumed);
            long deadline = SystemClock.uptimeMillis() + EVENT_TIMEOUT_MS;
            while (true) {
                int matched = match(start, expectedTypes);
                if (matched == MISMATCH) {
                    throw new AssertionError(describeExpectation(start, expectedTypes));
                }
                if (matched != NEEDS_MORE_EVENTS) {
                    int end = skipBoundsUpdates(start + matched);
                    if (end == MISMATCH) {
                        throw new AssertionError(
                                "Unexpected events. " + describeExpectation(start, expectedTypes));
                    }
                    mConsumed = end;
                    return mConsumed;
                }
                if (!waitForNewEvent(deadline)) {
                    throw new AssertionError(
                            "Timed out. " + describeExpectation(start, expectedTypes));
                }
            }
        }

        /**
         * Waits until the unconsumed events contain `expectedTypes` in this order, possibly with
         * other events in between, and consumes the events up to the last expected one. Fails if
         * they don't occur within the timeout.
         */
        public synchronized void waitForSubsequence(int[] expectedTypes) {
            long deadline = SystemClock.uptimeMillis() + EVENT_TIMEOUT_MS;
            int next = mConsumed;
            int found = 0;
            while (found < expectedTypes.length) {
                if (next < mEvents.size()) {
                    if (mEvents.get(next).type == expectedTypes[found]) found++;
                    next++;
                    continue;
                }
                if (!waitForNewEvent(deadline)) {
                    throw new AssertionError(
                            "Timed out. " + describeExpectation(mConsumed, expectedTypes));
                }
            }
            mConsumed = next;
        }

        // Returns how many events from `start` on make up `expectedTypes`, or MISMATCH, or
        // NEEDS_MORE_EVENTS.
        private int match(int start, int[] expectedTypes) {
            int index = start;
            for (int expected = 0; expected < expectedTypes.length; ) {
                if (index >= mEvents.size()) return NEEDS_MORE_EVENTS;
                AutofillEvent event = mEvents.get(index);
                if (event.type == expectedTypes[expected]) {
                    index++;
                    expected++;
                    continue;
                }
                if (event.type != AUTOFILL_VIEW_EXITED) return MISMATCH;
                if (index + 1 >= mEvents.size()) return NEEDS_MORE_EVENTS;
                if (!isBoundsUpdate(index)) return MISMATCH;
                index += 2;
            }
            return index - start;
        }

        // Returns the index after the bounds updates that start at `index`, or MISMATCH if other
        // events follow. A trailing VIEW_EXITED is not skipped since its VIEW_ENTERED may follow.
        private int skipBoundsUpdates(int index) {
            while (index < mEvents.size()) {
                if (mEvents.get(index).type != AUTOFILL_VIEW_EXITED) return MISMATCH;
                if (index + 1 == mEvents.size()) return index;
                if (!isBoundsUpdate(index)) return MISMATCH;
                index += 2;
            }
            return index;
        }

        // Whether the events at `index` and `index + 1` exit and re-enter the same field.
        private boolean isBoundsUpdate(int index) {
            AutofillEvent exited = mEvents.get(index);
            AutofillEvent entered = mEvents.get(index + 1);
            return exited.type == AUTOFILL_VIEW_EXITED
                    && entered.type == AUTOFILL_VIEW_ENTERED
                    && entered.virtualId == exited.virtualId;
        }

        private String describeExpectation(int start, int[] expectedTypes) {
            List<String> expected = new ArrayList<>();
            for (int type : expectedTypes) expected.add(EVENT[type]);
            String actual = describeFrom(start);
            return "Expected events " + expected + " after event #" + start + ", got " + actual;
        }

        // Waits until another event is added. Returns false if none is added before `deadline`.
        private boolean waitForNewEvent(long deadline) {
            int size = mEvents.size();
            while (mEvents.size() == size) {
                long remaining = deadline - SystemClock.uptimeMillis();
                if (remaining <= 0) return false;
                try {
                    wait(remaining);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    throw new RuntimeException(e);
                }
            }
            return true;
        }
    }

    private class TestAutofillManagerWrapper extends AutofillManagerWrapper {
        private boolean mDisabled;
        private boolean mQuerySucceed;

        public TestAutofillManagerWrapper(Context context) {
            super(context);
        }

        public void setDisabled() {
            mDisabled = true;
        }

        @Override
        public boolean isDisabled() {
            return mDisabled;
        }

        @Override
        public boolean isAwGCurrentAutofillService() {
            return sIsAwGCurrentAutofillService;
        }

        public boolean isQuerySucceed() {
            return mQuerySucceed;
        }

        @Override
        public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
            if (DEBUG) Log.i(TAG, "notifyVirtualViewEntered");
            mEvents.add(new AutofillEvent(AUTOFILL_VIEW_ENTERED, childId));
        }

        @Override
        public void notifyVirtualViewExited(View parent, int childId) {
            if (DEBUG) Log.i(TAG, "notifyVirtualViewExited");
            mEvents.add(new AutofillEvent(AUTOFILL_VIEW_EXITED, childId));
        }

        @Override
        public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
            if (DEBUG) Log.i(TAG, "notifyVirtualValueChanged");
            if (mTestValues.changedValues == null) {
                mTestValues.changedValues = new ArrayList<Pair<Integer, AutofillValue>>();
            }
            mTestValues.changedValues.add(new Pair<Integer, AutofillValue>(childId, value));
            mEvents.add(new AutofillEvent(AUTOFILL_VALUE_CHANGED, childId));
        }

        @Override
        public void commit(int submissionSource) {
            if (DEBUG) Log.i(TAG, "commit");
            mSubmissionSource = submissionSource;
            mEvents.add(new AutofillEvent(AUTOFILL_COMMIT, NO_VIRTUAL_ID));
        }

        @Override
        public void cancel() {
            if (DEBUG) Log.i(TAG, "cancel");
            mEvents.add(new AutofillEvent(AUTOFILL_CANCEL, NO_VIRTUAL_ID));
        }

        @Override
        public void notifyNewSessionStarted(boolean hasServerPrediction) {
            if (DEBUG) Log.i(TAG, "notifyNewSessionStarted");
            mEvents.add(new AutofillEvent(AUTOFILL_SESSION_STARTED, NO_VIRTUAL_ID));
        }

        @Override
        public void onServerPredictionsAvailable() {
            mQuerySucceed = true;
            if (DEBUG) Log.i(TAG, "onServerPredictionsAvailable");
            mEvents.add(new AutofillEvent(AUTOFILL_PREDICTIONS_AVAILABLE, NO_VIRTUAL_ID));
        }
    }

    private static class AwAutofillTestClient extends TestAwContentsClient {
        public interface ShouldInterceptRequestImpl {
            WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request);
        }

        private ShouldInterceptRequestImpl mShouldInterceptRequestImpl;

        public void setShouldInterceptRequestImpl(ShouldInterceptRequestImpl impl) {
            mShouldInterceptRequestImpl = impl;
        }

        @Override
        public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
            WebResourceResponseInfo response = null;
            if (mShouldInterceptRequestImpl != null) {
                response = mShouldInterceptRequestImpl.shouldInterceptRequest(request);
            }
            if (response != null) return response;
            return super.shouldInterceptRequest(request);
        }
    }

    private static class AwAutofillSessionUMATestHelper {
        private static final String DATA =
                """
                <html>
                <head></head>
                <body>
                    <form action="a.html" name="formname" id="formid">
                        <label>User Name:</label>
                           <input type="text" id="text1" name="username"
                                  placeholder="placeholder@placeholder.com"
                                  autocomplete="username name" />
                           <input type="submit" />
                    </form>
                    <form><input type="text" id="text2" /></form>
                </body>
                </html>\
                """;

        private static final int TOTAL_CONTROLS = 1; // text1

        private int mCnt;
        private final AwAutofillTest mTest;
        private final TestWebServer mWebServer;

        public AwAutofillSessionUMATestHelper(AwAutofillTest test, TestWebServer webServer) {
            mTest = test;
            mWebServer = webServer;
        }

        public void triggerAutofill() throws Throwable {
            final String url = mWebServer.setResponse(FILE, DATA, null);
            mTest.loadUrlSync(url);
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            mCnt +=
                    mTest.waitForCallbackAndVerifyTypes(
                            mCnt,
                            new Integer[] {
                                AUTOFILL_VIEW_ENTERED,
                                AUTOFILL_SESSION_STARTED,
                                AUTOFILL_VALUE_CHANGED
                            });
        }

        public void simulateServerPredictionBeforeTriggeringAutofill(int serverType)
                throws Throwable {
            final String url = mWebServer.setResponse(FILE, DATA, null);
            mTest.loadUrlSync(url);
            simulateServerPrediction(serverType);
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            mCnt +=
                    mTest.waitForCallbackAndVerifyTypes(
                            mCnt,
                            new Integer[] {
                                AUTOFILL_VIEW_ENTERED,
                                AUTOFILL_SESSION_STARTED,
                                AUTOFILL_VALUE_CHANGED
                            });
        }

        public void simulateServerPrediction(int serverType) throws Throwable {
            mTest.simulateServerPrimaryTypes(new String[] {"text1"}, new int[] {serverType});
        }

        public void simulateUserSelectSuggestion() throws Throwable {
            // Simulate user select suggestion
            TestViewStructure viewStructure = mTest.mTestValues.testViewStructure;
            assertNotNull(viewStructure);
            assertEquals(TOTAL_CONTROLS, viewStructure.getChildCount());

            TestViewStructure child0 = viewStructure.getChild(0);

            // Autofill form and verify filled values.
            SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
            values.append(child0.getId(), AutofillValue.forText("example@example.com"));
            mCnt = mTest.getCallbackCount();
            mTest.clearChangedValues();
            mTest.invokeAutofill(values);
            mCnt +=
                    mTest.waitForCallbackAndVerifyTypes(
                            mCnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        }

        public void submitForm() throws Throwable {
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('formid').submit();");
            mCnt +=
                    mTest.waitForCallbackAndVerifyTypes(
                            mCnt,
                            new Integer[] {
                                AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL
                            });
        }

        public void startNewSession() throws Throwable {
            // Start a new session by moving focus to another form.
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
            mCnt +=
                    mTest.waitForCallbackAndVerifyTypes(
                            mCnt,
                            new Integer[] {
                                AUTOFILL_VIEW_EXITED,
                                AUTOFILL_VIEW_ENTERED,
                                AUTOFILL_SESSION_STARTED,
                                AUTOFILL_VALUE_CHANGED
                            });
        }

        public void simulateUserChangeField() throws Throwable {
            mTest.executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
            mTest.dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
            mCnt +=
                    mTest.waitForCallbackAndVerifyTypes(
                            mCnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        }
    }

    private static boolean sIsAwGCurrentAutofillService;

    @Rule public AwActivityTestRule mRule;

    private TestWebServer mWebServer;
    private EmbeddedTestServer mEmbeddedServer;
    private AwTestContainerView mTestContainerView;
    private AwAutofillTestClient mContentsClient;
    private AwContents mAwContents;
    private final AutofillEventLog mEvents = new AutofillEventLog();
    private final TestValues mTestValues = new TestValues();
    private int mSubmissionSource;
    private TestAutofillManagerWrapper mTestAutofillManagerWrapper;
    private AwAutofillSessionUMATestHelper mUMATestHelper;
    private AutofillProvider mAutofillProvider;

    public AwAutofillTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mEmbeddedServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        doSetUp(/* isAwGCurrentAutofillService= */ true);
    }

    private void doSetUp(boolean isAwGCurrentAutofillService) throws Exception {
        sIsAwGCurrentAutofillService = isAwGCurrentAutofillService;
        AutofillProvider.setAutofillManagerWrapperFactoryForTesting(
                new AutofillProvider.AutofillManagerWrapperFactoryForTesting() {
                    @Override
                    public AutofillManagerWrapper create(Context context) {
                        mTestAutofillManagerWrapper = new TestAutofillManagerWrapper(context);
                        return mTestAutofillManagerWrapper;
                    }
                });
        mUMATestHelper = new AwAutofillSessionUMATestHelper(this, mWebServer);
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newSingleRecordWatcher(
                                    AutofillProviderUMA.UMA_AUTOFILL_CREATION_CONTEXT,
                                    AutofillProviderUMA.AutofillManagerCreationContext
                                            .ACTIVITY_CONTEXT);
                        });
        mContentsClient = new AwAutofillTestClient();
        mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mAutofillProvider = mAwContents.getAutofillProviderForTesting();
        // Don't let the device's IME interact with the page. Whenever a test focuses a field, the
        // real IME restarts its input connection and calls into WebView's InputConnection on the
        // IME thread that WebView shares between all WebViews of the process. These calls can
        // block that thread for good while waiting for the renderer, so that the key events which
        // the tests dispatch are never delivered, neither in this test nor in later tests of the
        // same batch.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ImeAdapter imeAdapter =
                            assertNonNull(ImeAdapter.fromWebContents(mAwContents.getWebContents()));
                    imeAdapter.setInputMethodManagerWrapper(
                            TestInputMethodManagerWrapper.create(imeAdapter));
                });

        ThreadUtils.runOnUiThreadBlocking(() -> histograms.assertExpected());
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
        mAutofillProvider = null;
    }

    public String getAbsoluteTestPageUrl(String relativePageUrl) {
        return mEmbeddedServer.getURL("/android_webview/test/data/autofill/" + relativePageUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.clickNode() which is known to be flaky"
        + " under modified scaling factor, see crbug.com/40840940")
    public void testTouchingFormWithAdjustResize() throws Throwable {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mRule.getActivity()
                            .getWindow()
                            .setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                });
        internalTestTriggerTest();
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.clickNode() which is known to be flaky"
        + " under modified scaling factor, see crbug.com/40840940")
    public void testTouchingFormWithAdjustPan() throws Throwable {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mRule.getActivity()
                            .getWindow()
                            .setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_PAN);
                });
        internalTestTriggerTest();
    }

    private void internalTestTriggerTest() throws Throwable {
        int cnt = 0;
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // Note that we currently depend on keyboard app's behavior.
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] { AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED});
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});

        executeJavaScriptAndWaitForResult("document.getElementById('text1').blur();");
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VIEW_EXITED});
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testBasicAutofill() throws Throwable {
        final String url =
                loadHTML(
                        """
                        <form action='a.html' name='formname'>
                            <label>User Name:</label>
                                <input type='text' id='text1' name='name' maxlength='30'
                                    placeholder='Your name'
                                    autocomplete='name given-name'>
                                <input type='checkbox' id='checkbox1' name='showpassword'>
                                <select id='select1' name='month'>
                                    <option value='1'>Jan</option>
                                    <option value='2'>Feb</option>
                                </select><textarea id='textarea1'></textarea>
                                <div contenteditable id='div1'>hello</div>
                                <input type='submit'>
                                <input type='reset' id='reset1'>
                                <input type='color' id='color1'><input type='file' id='file1'>
                                <input type='image' id='image1'>
                        </form>\
                        """);
        final int totalControls = 3; // text1, select1, textarea1
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(totalControls, viewStructure.getChildCount());

        // Verify form filled correctly in ViewStructure.
        URL pageURL = new URL(url);
        String webDomain =
                new URL(pageURL.getProtocol(), pageURL.getHost(), pageURL.getPort(), "/")
                        .toString();
        assertEquals(webDomain, viewStructure.getWebDomain());
        // WebView shouldn't set class name.
        assertNull(viewStructure.getClassName());
        Bundle extras = viewStructure.getExtras();
        assertEquals("Android WebView", extras.getCharSequence("VIRTUAL_STRUCTURE_PROVIDER_NAME"));
        assertTrue(0 < extras.getCharSequence("VIRTUAL_STRUCTURE_PROVIDER_VERSION").length());
        TestViewStructure.TestHtmlInfo htmlInfoForm = viewStructure.getHtmlInfo();
        assertEquals("form", htmlInfoForm.getTag());
        assertEquals("formname", htmlInfoForm.getAttribute("name"));

        // Verify input text control filled correctly in ViewStructure.
        TestViewStructure child0 = viewStructure.getChild(0);
        assertEquals(View.AUTOFILL_TYPE_TEXT, child0.getAutofillType());
        assertEquals("Your name", child0.getHint());
        assertEquals("name", child0.getAutofillHints()[0]);
        assertEquals("given-name", child0.getAutofillHints()[1]);
        assertFalse(child0.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child0.getDimensScrollX());
        assertEquals(0, child0.getDimensScrollY());
        TestViewStructure.TestHtmlInfo htmlInfo0 = child0.getHtmlInfo();
        assertEquals("text", htmlInfo0.getAttribute("type"));
        assertEquals("text1", htmlInfo0.getAttribute("id"));
        assertEquals("name", htmlInfo0.getAttribute("name"));
        assertEquals("User Name:", htmlInfo0.getAttribute("label"));
        assertEquals("30", htmlInfo0.getAttribute("maxlength"));
        assertEquals("NAME_FIRST", htmlInfo0.getAttribute("ua-autofill-hints"));

        // Verify select control filled correctly in ViewStructure.
        TestViewStructure child1 = viewStructure.getChild(1);
        assertEquals(View.AUTOFILL_TYPE_LIST, child1.getAutofillType());
        assertEquals("", child1.getHint());
        assertNull(child1.getAutofillHints());
        assertFalse(child1.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child1.getDimensScrollX());
        assertEquals(0, child1.getDimensScrollY());
        TestViewStructure.TestHtmlInfo htmlInfo1 = child1.getHtmlInfo();
        assertEquals("month", htmlInfo1.getAttribute("name"));
        assertEquals("select1", htmlInfo1.getAttribute("id"));
        CharSequence[] options = child1.getAutofillOptions();
        assertEquals("Jan", options[0]);
        assertEquals("Feb", options[1]);

        // Verify textarea control is filled correctly in ViewStructure.
        TestViewStructure child2 = viewStructure.getChild(2);
        assertEquals(View.AUTOFILL_TYPE_TEXT, child2.getAutofillType());
        assertEquals("", child2.getHint());
        assertNull(child2.getAutofillHints());
        assertFalse(child2.getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, child2.getDimensScrollX());
        assertEquals(0, child2.getDimensScrollY());
        TestViewStructure.TestHtmlInfo htmlInfo2 = child2.getHtmlInfo();
        assertEquals("textarea1", htmlInfo2.getAttribute("name"));

        // Autofill form and verify filled values.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(child0.getId(), AutofillValue.forText("Juan"));
        values.append(child1.getId(), AutofillValue.forList(1));
        values.append(child2.getId(), AutofillValue.forText("aaa"));
        cnt = getCallbackCount();
        clearChangedValues();
        invokeAutofill(values);

        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED,
                });

        // Verify form filled by Javascript
        String value0 =
                executeJavaScriptAndWaitForResult("document.getElementById('text1').value;");
        assertEquals("\"Juan\"", value0);
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('select1').value;");
        assertEquals("\"2\"", value1);
        String value2 =
                executeJavaScriptAndWaitForResult("document.getElementById('textarea1').value;");
        assertEquals("\"aaa\"", value2);
        ArrayList<Pair<Integer, AutofillValue>> changedValues = getChangedValues();
        assertEquals("Juan", changedValues.get(0).second.getTextValue());
        assertEquals(1, changedValues.get(1).second.getListValue());
    }

    /** Tests that a frame-transcending form is filled correctly. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @DisabledTest(message = "https://crbug.com/1401726")
    public void testCrossFrameAutofill() throws Throwable {
        loadHTML(
                """
                 <form>
                     <input autocomplete=cc-name>
                     <iframe srcdoc='<input autocomplete=cc-number>'></iframe>
                     <iframe srcdoc='<input autocomplete=cc-exp>'></iframe>
                     <iframe srcdoc='<input autocomplete=cc-csc>'></iframe>
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult(
                "window.frames[0].document.body.firstElementChild.select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);

        // Autofill form and verify filled values.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(viewStructure.getChild(0).getId(), AutofillValue.forText("Barack Obama"));
        values.append(viewStructure.getChild(1).getId(), AutofillValue.forText("4444333322221111"));
        values.append(viewStructure.getChild(2).getId(), AutofillValue.forText("12 / 2035"));
        values.append(viewStructure.getChild(3).getId(), AutofillValue.forText("123"));
        invokeAutofill(values);
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_VALUE_CHANGED
                });

        assertEquals(
                "\"Barack Obama\"",
                executeJavaScriptAndWaitForResult("document.forms[0].elements[0].value;"));
        assertEquals(
                "\"4444333322221111\"",
                executeJavaScriptAndWaitForResult(
                        "window.frames[0].document.body.firstElementChild.value;"));
        assertEquals(
                "\"12 / 2035\"",
                executeJavaScriptAndWaitForResult(
                        "window.frames[1].document.body.firstElementChild.value;"));
        assertEquals(
                "\"123\"",
                executeJavaScriptAndWaitForResult(
                        "window.frames[2].document.body.firstElementChild.value;"));
    }

    /**
     * This test is verifying that a user interacting with a form after reloading a webpage triggers
     * a new autofill session rather than continuing a session that was started before the reload.
     * This is necessary to ensure that autofill is properly triggered in this case (see
     * crbug.com/1117563 for details).
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @SkipMutations(
            reason =
                    "This test uses DOMUtils.clickNode() which is known to be flaky"
                            + " under modified scaling factor, see crbug.com/40840940")
    public void testAutofillTriggersAfterReload() throws Throwable {
        int cnt = 0;

        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] { AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED
                        });

        // Reload the page and check that the user clicking on the same form field ends the current
        // autofill session and starts a new session.
        reloadSync();
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_CANCEL,
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED
                        });
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testNotifyVirtualValueChanged() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        // Check if NotifyVirtualValueChanged() called and value is 'a'.
        assertEquals(1, values.size());
        assertEquals("a", values.get(0).second.getTextValue());

        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);

        // Check if NotifyVirtualValueChanged() called again, first value is 'a',
        // second value is 'ab', and both time has the same id.
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        values = getChangedValues();
        assertEquals(2, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("ab", values.get(1).second.getTextValue());
        assertEquals(values.get(0).first, values.get(1).first);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testJavascriptNotTriggerNotifyVirtualValueChanged() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        // Check if NotifyVirtualValueChanged() called and value is 'a'.
        assertEquals(1, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        executeJavaScriptAndWaitForResult("document.getElementById('text1').value='c';");
        // Check no new event occurs, this is best effort checking, the event here could be leaked
        // from previous dispatchDownAndUpKeyEvents().
        assertEquals("Unexpected events " + mEvents.describeFrom(cnt), cnt, getCallbackCount());
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        // Check if NotifyVirtualValueChanged() called one more time and value is 'cb', this
        // means javascript change didn't trigger the NotifyVirtualValueChanged().
        waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        values = getChangedValues();
        assertEquals(2, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("cb", values.get(1).second.getTextValue());
        assertEquals(values.get(0).first, values.get(1).first);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testCommit() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='password' id='passwordid' name='passwordname'>
                    <input type='submit'>
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        // Fill the password.
        executeJavaScriptAndWaitForResult("document.getElementById('passwordid').select();");
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED
                        });
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Submit form.
        executeJavaScriptAndWaitForResult("document.getElementById('formid').submit();");
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL
                });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(2, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("b", values.get(1).second.getTextValue());
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
    }

    // Test that `AutofillManager.commit()` is called after form submission even
    // if the web page dynamically modified the form after the last user interaction.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testCommitWithChangedFormProperties() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='password' id='passwordid' name='passwordname'>
                    <input type='submit'>
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        // Fill the password.
        executeJavaScriptAndWaitForResult("document.getElementById('passwordid').select();");
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED
                        });
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();

        // Change the form name.
        executeJavaScriptAndWaitForResult("document.getElementById('formid').name = 'othername';");

        // The form submission is detected despite the change in form properties.
        executeJavaScriptAndWaitForResult("document.getElementById('formid').submit();");
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL
                });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(2, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("b", values.get(1).second.getTextValue());
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
    }

    /**
     * Tests that when a multi-frame form is submitted in a subframe, we register the submission of
     * the overall form.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @DisabledTest(message = "crbug.com/376351174")
    public void testCrossFrameCommit() throws Throwable {
        // The only reason we use a <form> inside the iframe is that this makes it easiest to
        // trigger a form submission in that frame.
        loadHTML(
                """
                 <form>
                     <input id=name>
                     <iframe srcdoc='<form action=arbitrary.html method=GET>
                         <input id=num></form>'></iframe>
                     <iframe srcdoc='<input id=exp>'></iframe>
                     <iframe srcdoc='<input id=csc>'></iframe>
                </form>\
                """);
        int cnt = 0;
        // Fill name field.
        executeJavaScriptAndWaitForResult("document.forms[0].elements[0].select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        // Fill number field.
        executeJavaScriptAndWaitForResult(
                "window.frames[0].document.forms[0].elements[0].select();");
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED
                        });
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Fill expiration date field.
        executeJavaScriptAndWaitForResult(
                "window.frames[1].document.body.firstElementChild.select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_C);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED
                        });
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Fill CVC field.
        executeJavaScriptAndWaitForResult(
                "window.frames[2].document.body.firstElementChild.select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_D);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED
                        });
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
        clearChangedValues();
        // Submit a form in the subframe.
        executeJavaScriptAndWaitForResult("window.frames[0].document.forms[0].submit();");
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_VALUE_CHANGED,
                    AUTOFILL_COMMIT,
                    AUTOFILL_CANCEL
                });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(4, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        assertEquals("b", values.get(1).second.getTextValue());
        assertEquals("c", values.get(2).second.getTextValue());
        assertEquals("d", values.get(3).second.getTextValue());
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testLoadAssetURL() throws Throwable {
        final WebViewAssetLoader assetLoader =
                new WebViewAssetLoader.Builder()
                        .addPathHandler(
                                "/assets/",
                                new WebViewAssetLoader.AssetsPathHandler(mRule.getActivity()))
                        .build();
        mContentsClient.setShouldInterceptRequestImpl(
                request -> {
                    WebResourceResponse response =
                            assetLoader.shouldInterceptRequest(Uri.parse(request.getUrl()));
                    if (response == null) return null;
                    return new WebResourceResponseInfo(
                            response.getMimeType(),
                            response.getEncoding(),
                            response.getData(),
                            response.getStatusCode(),
                            response.getReasonPhrase(),
                            response.getResponseHeaders());
                });
        int cnt = 0;
        loadUrlSync(ASSET_URL);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        // Cancel called for the first query.
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VIEW_ENTERED,
                    AUTOFILL_SESSION_STARTED,
                    AUTOFILL_VALUE_CHANGED
                });
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testMovingToOtherForm() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='submit'>
                </form>
                <form action='a.html' name='formname' id='formid2'>
                    <input type='text' id='text2' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='submit'>
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        // Move to form2, cancel() should be called again.
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VIEW_EXITED,
                    AUTOFILL_VIEW_ENTERED,
                    AUTOFILL_SESSION_STARTED,
                    AUTOFILL_VALUE_CHANGED
                });
    }

    /** This test is verifying new session starts if frame change. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @DisabledTest(message = "https://crbug.com/340928697")
    public void testSwitchFromIFrame() throws Throwable {
        // we intentionally load main frame and iframe from the same URL and make both have the
        // similar form, so the new session is triggered by frame change
        final String data =
                "<html><head></head><body><form name='formname' id='formid'>"
                        + "<input type='text' id='text1' name='username'"
                        + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                        + "<input type='submit'></form>"
                        + "<iframe id='myframe' src='"
                        + FILE
                        + "'></iframe>"
                        + "</body></html>";
        final String iframeData =
                """
                <html>
                <head></head>
                <body>
                    <form name='formname' id='formid'>
                        <input type='text' id='text1' name='username'
                            placeholder='placeholder@placeholder.com'
                            autocomplete='username name' autofocus>
                        <input type='submit'>
                    </form>
                </body>
                </html>\
                """;
        final String url = mWebServer.setResponse(FILE, data, null);
        mContentsClient.setShouldInterceptRequestImpl(
                new AwAutofillTestClient.ShouldInterceptRequestImpl() {
                    private int mCallCount;

                    @Override
                    public WebResourceResponseInfo shouldInterceptRequest(
                            AwWebResourceRequest request) {
                        try {
                            if (url.equals(request.getUrl())) {
                                // Only intercept the iframe's request.
                                if (mCallCount == 1) {
                                    return new WebResourceResponseInfo(
                                            "text/html",
                                            "UTF-8",
                                            new ByteArrayInputStream(
                                                    iframeData.getBytes(StandardCharsets.UTF_8)));
                                }
                                mCallCount++;
                            }
                            return null;
                        } catch (Exception e) {
                            throw new RuntimeException(e);
                        }
                    }
                });
        loadUrlSync(url);

        // Trigger the autofill in iframe.
        int count = clearEventQueueAndGetCallCount();
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        // Verify autofill session triggered.
        count +=
                waitForCallbackAndVerifyTypes(
                        count,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        // Verify focus is in iframe.
        assertEquals(
                "true",
                executeJavaScriptAndWaitForResult(
                        "document.getElementById('myframe').contentDocument.hasFocus()"));
        // Move focus to the main frame form.
        clearChangedValues();
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        // The new session starts because cancel() has been called.
        waitForCallbackAndVerifyTypes(
                count,
                new Integer[] {
                    AUTOFILL_VIEW_EXITED,
                    AUTOFILL_VIEW_ENTERED,
                    AUTOFILL_SESSION_STARTED,
                    AUTOFILL_VALUE_CHANGED
                });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertEquals("a", values.get(0).second.getTextValue());
        // Verify focus isn't in iframe now.
        assertEquals(
                "false",
                executeJavaScriptAndWaitForResult(
                        "document.getElementById('myframe').contentDocument.hasFocus()"));
    }

    /** This test is verifying new session starts if frame change. */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.clickNode() which is known to be flaky"
        + " under modified scaling factor, see crbug.com/40840940")
    public void testTouchingPasswordFieldTriggerQuery() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='password' id='passwordid'
                        name='passwordname'> <input type='submit'>
                </form>\
                """);
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "passwordid");
        // Note that we currently depend on keyboard app's behavior.
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "passwordid"));
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] { AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED
                        });
    }

    /**
     * This test is verifying that AutofillProvider correctly processes the removal and restoring of
     * focus on a form element.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    @SkipMutations(
        reason = "This test uses DOMUtils.clickNode() which is known to be flaky"
        + " under modified scaling factor, see crbug.com/40840940")
    public void testFocusRemovedAndRestored() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='password' id='passwordid' name='passwordname'>
                </form>\
                """);

        // Start the session by clicking on the username element.
        DOMUtils.waitForNonZeroNodeBounds(mAwContents.getWebContents(), "text1");
        // TODO(changwan): mock out IME interaction.
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] { AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED
                        });

        // Removing focus from this element should cause a notification that the autofill view was
        // exited.
        executeJavaScriptAndWaitForResult("document.getElementById('text1').blur();");
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VIEW_EXITED});

        // Restoring focus on the form element should cause notifications to the autofill framework
        // that the autofill view was entered and value changed (AutofillProvider sends the latter
        // as a safeguard whenever focus changes to a new form element in the current session; it
        // was not sent as part of the first click above because at that point focus didn't change
        // to a *new* form element but was still on the element whose focusing had caused the
        // autofill session to start).
        Assert.assertTrue(DOMUtils.clickNode(mTestContainerView.getWebContents(), "text1"));
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
    }

    /**
     * This test is verifying that a navigation occurring while there is a probably-submitted form
     * will trigger commit of the current autofill session.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    // TODO: Run the test with BFCache after relanding crrev.com/c/5434056
    @CommandLineFlags.Add({"disable-features=WebViewBackForwardCache,AutofillServerCommunication"})
    public void testNavigationAfterProbableSubmitResultsInSessionCommit() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='password' id='passwordid' name='passwordname'>
                </form>
                >\
                """);
        final String success = "<!DOCTYPE html>" + "<html>" + "<body>" + "</body>" + "</html>";
        mWebServer.setResponse("/success.html", success, null);

        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        executeJavaScriptAndWaitForResult("window.location.href = 'success.html'; ");
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL
                });
        assertEquals(SubmissionSource.PROBABLY_FORM_SUBMITTED, mSubmissionSource);
    }

    /**
     * This test is verifying there is no callback if there is no form change between two
     * navigations.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testNoSubmissionWithoutFillingForm() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'
                    placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                    <input type='password' id='passwordid' name='passwordname'>
                </form>\
                """);
        final String success = "<!DOCTYPE html>" + "<html>" + "<body>" + "</body>" + "</html>";
        mWebServer.setResponse("/success.html", success, null);
        executeJavaScriptAndWaitForResult("window.location.href = 'success.html'; ");
        // There is no callback. AUTOFILL_CANCEL shouldn't be invoked.
        assertEquals(0, getCallbackCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'>
                    <select id='color' autofocus>
                        <option value='red'>red</option>
                        <option value='blue' id='blue'>blue</option>
                    </select>
                </form>\
                """);
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        clearChangedValues();
        executeJavaScriptAndWaitForResult("document.getElementById('color').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        // Use key B to select 'blue'.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_VIEW_ENTERED,
                            // onFocusChangeImpl() treats focus changes as value changes.
                            AUTOFILL_VALUE_CHANGED,
                            AUTOFILL_VALUE_CHANGED
                        });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(2, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(0, values.get(0).second.getListValue());
        assertTrue(values.get(1).second.isList());
        assertEquals(1, values.get(1).second.getListValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testSelectControlChangeStartAutofillSession() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <form action='a.html' name='formname' id='formid'>
                    <input type='text' id='text1' name='username'>
                    <select id='color' autofocus>
                        <option value='red'>red</option>
                        <option value='blue' id='blue'>blue</option>
                    </select>
                </form>\
                """);
        // Change select control first shall start autofill session.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        // Use key B to select 'blue'.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());

        // Verify the autofill session started by select control has dimens filled.
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertFalse(viewStructure.getChild(0).getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, viewStructure.getChild(0).getDimensScrollX());
        assertEquals(0, viewStructure.getChild(0).getDimensScrollY());
        assertFalse(viewStructure.getChild(1).getDimensRect().isEmpty());
        // The field has no scroll, should always be zero.
        assertEquals(0, viewStructure.getChild(1).getDimensScrollX());
        assertEquals(0, viewStructure.getChild(1).getDimensScrollY());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testUserInitiatedJavascriptSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <script>
                    function myFunction() {
                        document.getElementById('color').value = 'blue';
                    }
                </script>
                <form action='a.html' name='formname' id='formid'>
                    <button onclick='myFunction();' autofocus>button </button>
                    <select id='color'>
                        <option value='red'>red</option>
                        <option value='blue' id='blue'>blue</option>
                    </select>
                </form>\
                """);
        // Change select control first shall start autofill session.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testJavascriptNotTriggerSelectControlChangeNotification() throws Throwable {
        int cnt = 0;
        loadHTML(
                """
                <script>
                    function myFunction() {
                        document.getElementById('color').value = 'blue';
                    }
                </script>
                <script defer>
                    myFunction();
                </script>
                <form action='a.html' name='formname' id='formid'>
                    <button onclick='myFunction();' autofocus>button </button>
                    <select id='color'>
                        <option value='red'>red</option>
                        <option value='blue' id='blue'>blue</option>
                    </select>
                </form>\
                """);
        // There is no good way to verify no callback occurred, we just simulate user trigger
        // the autofill and verify autofill is only triggered once, then this proves javascript
        // didn't trigger the autofill, since
        // testUserInitiatedJavascriptSelectControlChangeNotification verified user's triggering
        // work.
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_SPACE);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        ArrayList<Pair<Integer, AutofillValue>> values = getChangedValues();
        assertEquals(1, values.size());
        assertTrue(values.get(0).second.isList());
        assertEquals(1, values.get(0).second.getListValue());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @Feature({"AndroidWebView"})
    public void testUaAutofillHints() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <label for=\"frmAddressB\">Address</label>
                    <input name=\"bill-address\" id=\"frmAddressB\">
                    <label for=\"frmCityB\">City</label>
                    <input name=\"bill-city\" id=\"frmCityB\">
                    <label for=\"frmStateB\">State</label>
                    <input name=\"bill-state\" id=\"frmStateB\">
                    <label for=\"frmZipB\">Zip</label>
                    <input name=\"bill-zip\" id=\"frmZipB\">
                    <input type='checkbox' id='checkbox1' name='showpassword'>
                    <label for=\"frmCountryB\">Country</label>
                    <input name=\"bill-country\" id=\"frmCountryB\">
                    <input type='submit'>
                </form>\
                """);
        final int totalControls = 5;
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('frmAddressB').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(totalControls, viewStructure.getChildCount());

        TestViewStructure child0 = viewStructure.getChild(0);
        TestViewStructure.TestHtmlInfo htmlInfo0 = child0.getHtmlInfo();
        assertEquals("ADDRESS_HOME_LINE1", htmlInfo0.getAttribute("ua-autofill-hints"));

        TestViewStructure child1 = viewStructure.getChild(1);
        TestViewStructure.TestHtmlInfo htmlInfo1 = child1.getHtmlInfo();
        assertEquals("ADDRESS_HOME_CITY", htmlInfo1.getAttribute("ua-autofill-hints"));

        TestViewStructure child2 = viewStructure.getChild(2);
        TestViewStructure.TestHtmlInfo htmlInfo2 = child2.getHtmlInfo();
        assertEquals("ADDRESS_HOME_STATE", htmlInfo2.getAttribute("ua-autofill-hints"));

        TestViewStructure child3 = viewStructure.getChild(3);
        TestViewStructure.TestHtmlInfo htmlInfo3 = child3.getHtmlInfo();
        assertEquals("ADDRESS_HOME_ZIP", htmlInfo3.getAttribute("ua-autofill-hints"));

        TestViewStructure child4 = viewStructure.getChild(4);
        TestViewStructure.TestHtmlInfo htmlInfo4 = child4.getHtmlInfo();
        assertEquals("ADDRESS_HOME_COUNTRY", htmlInfo4.getAttribute("ua-autofill-hints"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserChangeFormFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FORM_SUBMISSION)
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                                            AutofillProviderUMA.AWG_HAS_SUGGESTION_AUTOFILLED)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.submitForm();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    /** Tests that the metrics of the ongoing session are recorded on AwContents destruction. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMASessionMetricsRecordedOnAwContentsDestruction() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.simulateUserChangeField();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.destroy();
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectNotSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectAnyRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUGGESTION_TIME)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserChangeFormFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_NOT_SELECT_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FORM_SUBMISSION)
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                                            AutofillProviderUMA.AWG_HAS_SUGGESTION_NO_AUTOFILL)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.submitForm();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserChangeFormNoFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .NO_SUGGESTION_USER_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserChangeFormFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .NO_SUGGESTION_USER_CHANGE_FORM_FORM_SUBMITTED)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FORM_SUBMISSION)
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                                            AutofillProviderUMA.AWG_NO_SUGGESTION)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.simulateUserChangeField();
        mUMATestHelper.submitForm();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FORM_SUBMISSION)
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                                            AutofillProviderUMA.AWG_HAS_SUGGESTION_AUTOFILLED)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.submitForm();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserSelectSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.simulateUserSelectSuggestion();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAUserNotSelectSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .USER_NOT_SELECT_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FORM_SUBMISSION)
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                                            AutofillProviderUMA.AWG_HAS_SUGGESTION_NO_AUTOFILL)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        invokeOnInputUIShown();
        mUMATestHelper.submitForm();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserNotChangeFormNoFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .NO_SUGGESTION_USER_NOT_CHANGE_FORM_NO_FORM_SUBMITTED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectNoRecords(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoSuggestionUserNotChangeFormFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA
                                                    .NO_SUGGESTION_USER_NOT_CHANGE_FORM_FORM_SUBMITTED)
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FORM_SUBMISSION)
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_AWG_SUGGESTION_AVAILABILITY,
                                            AutofillProviderUMA.AWG_NO_SUGGESTION)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        mUMATestHelper.submitForm();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    /**
     * Tests that the proper histograms are reocrded when no virtual structure is provided before
     * session start.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoCallbackFromFramework() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_AUTOFILL_SESSION,
                                            AutofillProviderUMA.NO_STRUCTURE_PROVIDED)
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMANoServerPrediction() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newSingleRecordWatcher(
                                    AutofillProviderUMA.UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                    AutofillProviderUMA.SERVER_PREDICTION_NOT_AVAILABLE);
                        });
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.startNewSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAServerPredictionArriveBeforeSessionStart() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                            AutofillProviderUMA
                                                    .SERVER_PREDICTION_AVAILABLE_ON_SESSION_STARTS)
                                    .expectBooleanRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_VALID_SERVER_PREDICTION,
                                            true)
                                    .build();
                        });
        mUMATestHelper.simulateServerPredictionBeforeTriggeringAutofill(FieldType.USERNAME);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAServerPredictionArriveAfterSessionStart() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_SERVER_PREDICTION_AVAILABILITY,
                                            AutofillProviderUMA
                                                    .SERVER_PREDICTION_AVAILABLE_AFTER_SESSION_STARTS)
                                    .expectBooleanRecord(
                                            AutofillProviderUMA
                                                    .UMA_AUTOFILL_VALID_SERVER_PREDICTION,
                                            false)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        mUMATestHelper.simulateServerPrediction(FieldType.NO_SERVER_DATA);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillDisabled() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectBooleanRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_ENABLED, false)
                                    .build();
                        });
        mTestAutofillManagerWrapper.setDisabled();
        mUMATestHelper.triggerAutofill();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testUMAAutofillEnabled() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectNoRecords(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE)
                                    .expectBooleanRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_ENABLED, true)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    /**
     * Tests recording of the `PROBABLY_FORM_SUBMITTED` bucket for the
     * "Autofill.WebView.SubmissionSource" histogram. This event is fired on a navigation not
     * resulting from a link click (in this case the test uses a reload).
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testUMAFormSubmissionProbablyFormSubmitted() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.PROBABLY_FORM_SUBMITTED)
                                    .build();
                        });
        mUMATestHelper.triggerAutofill();
        invokeOnProvideAutoFillVirtualStructure();
        int cnt = getCallbackCount();
        executeJavaScriptAndWaitForResult("window.location = 'about:blank';");
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL});
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    /**
     * Tests recording of the `FRAME_DETACHED` bucket for the "Autofill.WebView.SubmissionSource"
     * histogram. This event is fired when a non-main frame is detached.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testUMAFormSubmissionFrameDetached() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.FRAME_DETACHED)
                                    .build();
                        });
        loadHTML(
                """
                <div id='parent'>
                    <iframe id='frame' srcdoc='<input id="username">'></iframe>
                </div>\
                """);

        int cnt = 0;
        executeJavaScriptAndWaitForResult(
                """
                var iframe = document.getElementById('frame');
                var frame_doc = iframe.contentDocument;
                frame_doc.getElementById('username').select();\
                """);
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED, AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        executeJavaScriptAndWaitForResult(
                "document.getElementById('parent').removeChild(document.getElementById('frame'));");
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_VALUE_CHANGED,
                            AUTOFILL_COMMIT,
                            AUTOFILL_CANCEL
                        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    /**
     * Tests recording of the `SAME_DOCUMENT_NAVIGATION` bucket for the
     * "Autofill.WebView.SubmissionSource" histogram. This event is fired when clicking a link that
     * jumps through the same document and the tracked element disappears.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testUMAFormSubmissionSameDocumentNavigation() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.SAME_DOCUMENT_NAVIGATION)
                                    .build();
                        });

        loadHTML(
                """
                <input id='username'>
                <a id='link' href='#destination'></a>
                <div id='destination'></div>\
                """);

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('username').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        executeJavaScriptAndWaitForResult(
                """
                document.getElementById('link').click();
                document.getElementById('username').remove();\
                """);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_VALUE_CHANGED,
                            AUTOFILL_COMMIT,
                            AUTOFILL_CANCEL
                        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    /**
     * Tests recording of the `XHR_SUCCEEDED` bucket for the "Autofill.WebView.SubmissionSource"
     * histogram. This event is fired when a successful XHR request occurs and the tracked element
     * disappears.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testUMAFormSubmissionXHRSucceeded() throws Throwable {
        var histograms =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return HistogramWatcher.newBuilder()
                                    .expectIntRecord(
                                            AutofillProviderUMA.UMA_AUTOFILL_SUBMISSION_SOURCE,
                                            AutofillProviderUMA.XHR_SUCCEEDED)
                                    .build();
                        });

        loadHTML("<input id='username'>");

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('username').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();

        final String xhrUrl = mWebServer.setEmptyResponse(FILE);
        executeJavaScriptAndWaitForResult(
                String.format(
                        """
                        document.getElementById('username').remove();
                        const xhr = new XMLHttpRequest();
                        xhr.open('GET', '%s', true);
                        xhr.send(null);\
                        """,
                        xhrUrl));
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_VALUE_CHANGED,
                            AUTOFILL_COMMIT,
                            AUTOFILL_CANCEL
                        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    histograms.assertExpected();
                });
    }

    @Test
    @SmallTest
    @RequiresRestart("https://crbug.com/1422936")
    // TODO: Run the test with BFCache after relanding crrev.com/c/5434056
    @CommandLineFlags.Add({"disable-features=WebViewBackForwardCache,AutofillServerCommunication"})
    public void testUmaFunnelMetrics() throws Throwable {
        HistogramWatcher.Builder histogramWatcherBuilder = HistogramWatcher.newBuilder();

        histogramWatcherBuilder
                .expectBooleanRecord("Autofill.WebView.Funnel.ParsedAsType2.Address", true)
                // Ignore histogram for pages without any forms.
                .allowExtraRecords("Autofill.WebView.Funnel.ParsedAsType2.Address")
                .expectBooleanRecord(
                        "Autofill.WebView.Funnel.InteractionAfterParsedAsType2.Address", true)
                .expectBooleanRecord("Autofill.WebView.Funnel.FillAfterInteraction2.Address", true)
                .expectBooleanRecord("Autofill.WebView.Funnel.SubmissionAfterFill2.Address", true)
                .expectBooleanRecord(
                        "Autofill.WebView.KeyMetrics.FillingCorrectness2.Address", true)
                .expectBooleanRecord("Autofill.WebView.KeyMetrics.FillingAssistance2.Address", true)
                .expectBooleanRecord(
                        "Autofill.WebView.KeyMetrics.FormSubmission.Autofilled2.Address", true);

        histogramWatcherBuilder
                .expectBooleanRecord("Autofill.WebView.Funnel.ParsedAsType2.CreditCard", true)
                // Ignore histogram for pages without any forms.
                .allowExtraRecords("Autofill.WebView.Funnel.ParsedAsType2.CreditCard")
                .expectBooleanRecord(
                        "Autofill.WebView.Funnel.InteractionAfterParsedAsType2.CreditCard", false);
        histogramWatcherBuilder.expectNoRecords(
                "Autofill.WebView.Funnel.SubmissionAfterFill2.CreditCard");

        HistogramWatcher histogramWatcher = histogramWatcherBuilder.build();

        final String url = getAbsoluteTestPageUrl("page_address_credit_card_forms.html");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('address1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        waitForEvents(new Integer[] {AUTOFILL_SESSION_STARTED, AUTOFILL_VALUE_CHANGED});

        invokeOnProvideAutoFillVirtualStructure();
        int address1Id = mTestValues.testViewStructure.getChild(0).getId();
        SparseArray<AutofillValue> autofillValues = new SparseArray<AutofillValue>();
        autofillValues.append(address1Id, AutofillValue.forText("Jane Doe"));
        invokeAutofill(autofillValues);
        executeJavaScriptAndWaitForResult("document.getElementById('addressFormId').submit();");

        // All of the metrics are recorded at the same time, when the submission navigates away
        // from the page. Wait for one of the metrics to be recorded.
        CriteriaHelper.pollUiThread(
                () -> {
                    int numSamples =
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "Autofill.WebView.Funnel.ParsedAsType2.Address",
                                    /* sample= */ 1);
                    return numSamples > 0;
                },
                "Autofill.WebView.Funnel metrics weren't recorded",
                ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG),
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    @DisabledTest(message = "crbug.com/353502929")
    public void testPageScrollTriggerViewExitAndEnter() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'
                        placeholder='placeholder@placeholder.com'
                        autocomplete='username name'>
                </form>
                <p style='height: 100vh'>Hello</p>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        // Moved view, the position change trigger additional AUTOFILL_VIEW_EXITED and
        // AUTOFILL_VIEW_ENTERED.
        scrollToBottom();
        pollJavascriptResultNotEqualTo("document.body.scrollTop;", "0");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        List<Integer> expectedValues = new ArrayList<>();

        // Check if NotifyVirtualValueChanged() called again and with extra AUTOFILL_VIEW_EXITED
        // and AUTOFILL_VIEW_ENTERED
        expectedValues.addAll(
                Arrays.asList(AUTOFILL_VIEW_EXITED, AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED));
        waitForCallbackAndVerifyTypes(cnt, expectedValues.toArray(new Integer[0]));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testMismatchedAutofillValueWontCauseCrash() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_username.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(1, viewStructure.getChildCount());
        TestViewStructure child0 = viewStructure.getChild(0);

        // Autofill form and verify filled values.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        // Append wrong autofill value.
        values.append(child0.getId(), AutofillValue.forToggle(false));
        // If the test fail, the exception shall be thrown in below.
        invokeAutofill(values);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testDatalistSentToAutofillService() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());

        // Verified the datalist has correctly been sent to AutofillService
        TestViewStructure child1 = viewStructure.getChild(1);
        assertEquals(2, child1.getAutofillOptions().length);
        assertEquals("A1", child1.getAutofillOptions()[0]);
        assertEquals("A2", child1.getAutofillOptions()[1]);

        // Simulate autofilling the datalist by the AutofillService.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(child1.getId(), AutofillValue.forText("example@example.com"));
        invokeAutofill(values);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('text2').value;");
        assertEquals("\"example@example.com\"", value1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/424007303")
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testNoEventSentToAutofillServiceForFocusedDatalist() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        // Verify not notifying AUTOFILL_VIEW_ENTERED and AUTOFILL_VALUE_CHANGED events for the
        // datalist.
        cnt +=
                waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_SESSION_STARTED});
        // Verify input accepted.
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('text2').value;");
        assertEquals("\"a\"", value1);
        // Move cursor to text1 and enter something.
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        // Verify no AUTOFILL_VIEW_EXITED sent for datalist and autofill service shall get the
        // events from the change of text1.
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt, new Integer[] {AUTOFILL_VIEW_ENTERED, AUTOFILL_VALUE_CHANGED});
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testDatalistPopup() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        pollDatalistPopupShown(2);
        TouchCommon.singleClickView(
                mAutofillProvider.getDatalistPopupForTesting().getListView().getChildAt(1));
        // Verify the selection accepted by renderer.
        pollJavascriptResult("document.getElementById('text2').value;", "\"A2\"");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testHideDatalistPopup() throws Throwable {
        final String url = getAbsoluteTestPageUrl("form_with_datalist.html");
        loadUrlSync(url);
        executeJavaScriptAndWaitForResult("document.getElementById('text2').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        pollDatalistPopupShown(2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.hideAutofillPopup();
                });
        assertNull(mAutofillProvider.getDatalistPopupForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testVisibility() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'>
                    <input type='text' name='email' id='text2' style='display: none;' />
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        // Verifies the visibility set correctly.
        assertEquals(View.VISIBLE, viewStructure.getChild(0).getVisibility());
        assertEquals(View.INVISIBLE, viewStructure.getChild(1).getVisibility());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testServerPredictionArrivesBeforeAutofillStart() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'>
                    <input type='text' name='email' id='text2' autocomplete='email' />
                </form>\
                """);
        simulateServerPredictions(
                new String[] {"text1", "text2"},
                new int[][] {
                    {
                        FieldType.USERNAME, FieldType.EMAIL_ADDRESS,
                    },
                    {
                        FieldType.EMAIL_ADDRESS,
                    }
                });

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals(
                "USERNAME",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "USERNAME,EMAIL_ADDRESS",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        assertEquals(
                "EMAIL_ADDRESS",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "EMAIL_ADDRESS",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        // Binder will not be set if the prediction already arrives.
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNull(binder);
    }

    /** Tests that server predictions are mapped to the fields of a cross-frame form. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testCrossFrameServerPredictionArrivesBeforeAutofillStart() throws Throwable {
        loadHTML(
                """
                <form>
                    <input id=name>
                    <iframe srcdoc='<form action=arbitrary.html method=GET>
                                <input id=num autocomplete=cc-number></form>' sandbox></iframe>
                    <iframe srcdoc='<input id=exp>'></iframe>
                    <iframe srcdoc='<input id=csc>'></iframe>
                </form>\
                """);
        simulateServerPredictions(
                new String[] {"name", "num", "exp", "csc"},
                new int[][] {
                    {FieldType.CREDIT_CARD_NAME_FULL},
                    {FieldType.CREDIT_CARD_NUMBER},
                    {FieldType.CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR},
                    {FieldType.CREDIT_CARD_VERIFICATION_CODE}
                });

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.forms[0].elements[0].select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(4, viewStructure.getChildCount());
        // Name field.
        assertEquals(
                "CREDIT_CARD_NAME_FULL",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_NAME_FULL",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_NAME_FULL",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        // Number field.
        assertEquals(
                "CREDIT_CARD_NUMBER",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "HTML_TYPE_CREDIT_CARD_NUMBER",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_NUMBER",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        // Expiration date field.
        assertEquals(
                "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
                viewStructure
                        .getChild(2)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
                viewStructure.getChild(2).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR",
                viewStructure
                        .getChild(2)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        // CVC field.
        assertEquals(
                "CREDIT_CARD_VERIFICATION_CODE",
                viewStructure
                        .getChild(3)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_VERIFICATION_CODE",
                viewStructure.getChild(3).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "CREDIT_CARD_VERIFICATION_CODE",
                viewStructure
                        .getChild(3)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        // Binder is not set if the prediction has already arrived.
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNull(binder);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testServerPredictionPrimaryTypeArrivesBeforeAutofillStart() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'>
                    <input type='text' name='email' id='text2' autocomplete='email' />
                </form>\
                """);
        simulateServerPrimaryTypes(
                new String[] {"text1", "text2"},
                new int[] {FieldType.USERNAME, FieldType.EMAIL_ADDRESS});

        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals(
                "USERNAME",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "USERNAME",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "USERNAME",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        assertEquals(
                "EMAIL_ADDRESS",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertEquals(
                "EMAIL_ADDRESS",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        // Binder will not be set if the prediction already arrives.
        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNull(binder);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testServerPredictionArrivesAfterAutofillStart() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'>
                    <input type='text' name='email' id='text2' autocomplete='email' />
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals(
                "NO_SERVER_DATA",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        assertEquals(
                "NO_SERVER_DATA",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));

        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);

        simulateServerPredictions(
                new String[] {"text1", "text2"},
                new int[][] {
                    {FieldType.USERNAME, FieldType.EMAIL_ADDRESS},
                    {FieldType.EMAIL_ADDRESS}
                });

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_PREDICTIONS_AVAILABLE});
        assertTrue(mTestAutofillManagerWrapper.isQuerySucceed());
        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        List<ViewType> viewTypes = autofillHintsServiceTestHelper.getViewTypes();
        assertEquals(2, viewTypes.size());
        assertEquals(viewStructure.getChild(0).getAutofillId(), viewTypes.get(0).mAutofillId);
        assertEquals("USERNAME", viewTypes.get(0).mServerType);
        assertEquals("USERNAME", viewTypes.get(0).mOverallType);
        assertArrayEquals(
                new String[] {"USERNAME", "EMAIL_ADDRESS"},
                viewTypes.get(0).getServerPredictions());
        assertEquals(viewStructure.getChild(1).getAutofillId(), viewTypes.get(1).mAutofillId);
        assertEquals("EMAIL_ADDRESS", viewTypes.get(1).mServerType);
        assertEquals("HTML_TYPE_EMAIL", viewTypes.get(1).mOverallType);
        assertArrayEquals(new String[] {"EMAIL_ADDRESS"}, viewTypes.get(1).getServerPredictions());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testServerPredictionPrimaryTypeArrivesAfterAutofillStart() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'>
                    <input type='text' name='email' id='text2' autocomplete='email' />
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals(
                "NO_SERVER_DATA",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        assertEquals(
                "NO_SERVER_DATA",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));

        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);

        simulateServerPrimaryTypes(
                new String[] {"text1", "text2"},
                new int[] {FieldType.USERNAME, FieldType.EMAIL_ADDRESS});

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_PREDICTIONS_AVAILABLE});
        assertTrue(mTestAutofillManagerWrapper.isQuerySucceed());
        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        List<ViewType> viewTypes = autofillHintsServiceTestHelper.getViewTypes();
        assertEquals(2, viewTypes.size());
        assertEquals(viewStructure.getChild(0).getAutofillId(), viewTypes.get(0).mAutofillId);
        assertEquals("USERNAME", viewTypes.get(0).mServerType);
        assertEquals("USERNAME", viewTypes.get(0).mOverallType);
        assertArrayEquals(new String[] {"USERNAME"}, viewTypes.get(0).getServerPredictions());
        assertEquals(viewStructure.getChild(1).getAutofillId(), viewTypes.get(1).mAutofillId);
        assertEquals("EMAIL_ADDRESS", viewTypes.get(1).mServerType);
        assertEquals("HTML_TYPE_EMAIL", viewTypes.get(1).mOverallType);
        assertArrayEquals(new String[] {"EMAIL_ADDRESS"}, viewTypes.get(1).getServerPredictions());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testServerPredictionArrivesBeforeCallbackRegistered() throws Throwable {
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <input type='text' id='text1' name='username'>
                    <input type='text' name='email' id='text2' autocomplete='email' />
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);

        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });

        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());
        assertEquals(
                "NO_SERVER_DATA",
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "UNKNOWN_TYPE",
                viewStructure.getChild(0).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(
                viewStructure
                        .getChild(0)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));
        assertEquals(
                "NO_SERVER_DATA",
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-autofill-hints"));
        assertEquals(
                "HTML_TYPE_EMAIL",
                viewStructure.getChild(1).getHtmlInfo().getAttribute("computed-autofill-hints"));
        assertNull(
                viewStructure
                        .getChild(1)
                        .getHtmlInfo()
                        .getAttribute("crowdsourcing-predictions-autofill-hints"));

        simulateServerPredictions(
                new String[] {"text1", "text2"},
                new int[][] {
                    {FieldType.USERNAME, FieldType.EMAIL_ADDRESS},
                    {FieldType.EMAIL_ADDRESS}
                });

        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_PREDICTIONS_AVAILABLE});
        assertTrue(mTestAutofillManagerWrapper.isQuerySucceed());

        IBinder binder = viewStructure.getExtras().getBinder("AUTOFILL_HINTS_SERVICE");
        assertNotNull(binder);
        AutofillHintsServiceTestHelper autofillHintsServiceTestHelper =
                new AutofillHintsServiceTestHelper();
        autofillHintsServiceTestHelper.registerViewTypeService(binder);
        autofillHintsServiceTestHelper.waitForCallbackInvoked();
        List<ViewType> viewTypes = autofillHintsServiceTestHelper.getViewTypes();
        assertEquals(2, viewTypes.size());
        assertEquals(viewStructure.getChild(0).getAutofillId(), viewTypes.get(0).mAutofillId);
        assertEquals("USERNAME", viewTypes.get(0).mServerType);
        assertEquals("USERNAME", viewTypes.get(0).mOverallType);
        assertArrayEquals(
                new String[] {"USERNAME", "EMAIL_ADDRESS"},
                viewTypes.get(0).getServerPredictions());
        assertEquals(viewStructure.getChild(1).getAutofillId(), viewTypes.get(1).mAutofillId);
        assertEquals("EMAIL_ADDRESS", viewTypes.get(1).mServerType);
        assertEquals("HTML_TYPE_EMAIL", viewTypes.get(1).mOverallType);
        assertArrayEquals(new String[] {"EMAIL_ADDRESS"}, viewTypes.get(1).getServerPredictions());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testFieldAddedBeforeSuggestionSelected() throws Throwable {
        // This test verifies that form filling works even in the case that the form has been
        // modified (field was added) in the DOM between the decision to fill and executing the
        // fill.
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <label>User Name:</label>
                    <input type='text' id='text1' name='name' />
                    <label>Password:</label>
                    <input type='password' id='pwdid' name='pwd' />
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());

        // Append a field.
        executeJavaScriptAndWaitForResult(
                """
                document.getElementById('pwdid').insertAdjacentHTML(
                    'afterend', '<input type=\"password\" id=\"pwdid2\"/>');\
                """);

        // Autofill the original form.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(
                viewStructure.getChild(0).getId(), AutofillValue.forText("example@example.com"));
        values.append(viewStructure.getChild(1).getId(), AutofillValue.forText("password"));
        cnt = getCallbackCount();
        clearChangedValues();
        invokeAutofill(values);
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});

        String value0 =
                executeJavaScriptAndWaitForResult("document.getElementById('text1').value;");
        assertEquals("\"example@example.com\"", value0);
        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('pwdid').value;");
        assertEquals("\"password\"", value1);
        String value2 =
                executeJavaScriptAndWaitForResult("document.getElementById('pwdid2').value;");
        assertEquals("\"\"", value2);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testFirstFieldRemovedBeforeSuggestionSelected() throws Throwable {
        // This test verifies that form filling works even if an element of the form that was
        // supposed to be filled has been deleted between the time of decision to fill the form and
        // executing the fill.
        loadHTML(
                """
                <form action='a.html' name='formname'>
                    <label>User Name:</label>
                    <input type='text' id='text1' name='name' />
                    <label>Password:</label>
                    <input type='password' id='pwdid' name='pwd' />
                </form>\
                """);
        int cnt = 0;
        // Focus on the second element, since the first one is about to be removed. Removing the
        // element on which the fill was triggered would cancel the filling operation.
        executeJavaScriptAndWaitForResult("document.getElementById('pwdid').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        invokeOnProvideAutoFillVirtualStructure();
        TestViewStructure viewStructure = mTestValues.testViewStructure;
        assertNotNull(viewStructure);
        assertEquals(2, viewStructure.getChildCount());

        // Remove the first field.
        executeJavaScriptAndWaitForResult("document.getElementById('text1').remove()");

        // Autofill the original form.
        SparseArray<AutofillValue> values = new SparseArray<AutofillValue>();
        values.append(
                viewStructure.getChild(0).getId(), AutofillValue.forText("example@example.com"));
        values.append(viewStructure.getChild(1).getId(), AutofillValue.forText("password"));
        cnt = getCallbackCount();
        clearChangedValues();
        invokeAutofill(values);
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_VALUE_CHANGED});

        String value1 =
                executeJavaScriptAndWaitForResult("document.getElementById('pwdid').value;");
        assertEquals("\"password\"", value1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testFrameDetachedOnFormSubmission() throws Throwable {
        final String subFrame =
                """
                <html>
                <body>
                    <script>
                        function send_post() {
                            window.parent.postMessage('SubmitComplete', '*');
                        }
                    </script>
                    <form action='inner_frame_address_form.html' id='deleting_form'
                        onsubmit='send_post(); return false;'>
                        <input type='text' id='address_field' name='address'
                            autocomplete='on'>
                        <input type='submit' id='submit_button'
                            name='submit_button'>
                    </form>
                </body>
                </html>\
                """;
        final String subFrameURL =
                mWebServer.setResponse("/inner_frame_address_form.html", subFrame, null);
        assertTrue(Uri.parse(subFrameURL).getPath().equals("/inner_frame_address_form.html"));
        loadHTML(
                """
                <script>
                    function receiveMessage(event) {
                        var address_iframe = document.getElementById('address_iframe');
                        address_iframe.parentNode.removeChild(address_iframe);
                        setTimeout(delayedUpload, 0);
                    }
                    window.addEventListener('message', receiveMessage, false);
                </script>
                <iframe src='inner_frame_address_form.html' id='address_iframe'
                    name='address_iframe'>
                </iframe>\
                """);

        int cnt = 0;
        pollJavascriptResult(
                """
                var iframe = document.getElementById('address_iframe');
                var frame_doc = iframe.contentDocument;
                frame_doc.getElementById('address_field').focus();
                frame_doc.activeElement.id;\
                """,
                "\"address_field\"");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        executeJavaScriptAndWaitForResult(
                """
                var iframe = document.getElementById('address_iframe');
                var frame_doc = iframe.contentDocument;
                frame_doc.getElementById('submit_button').click();\
                """);
        waitForCallbackAndVerifyTypes(
                cnt, new Integer[] {AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL});
        assertEquals(SubmissionSource.FORM_SUBMISSION, mSubmissionSource);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=AutofillServerCommunication"})
    public void testFrameDetachedOnFormlessSubmission() throws Throwable {
        final String subFrame =
                """
                <html>
                <body>
                    <script>
                        function send_post() {
                            window.parent.postMessage('SubmitComplete', '*');
                        }
                    </script>
                    <input type='text' id='address_field' name='address' autocomplete='on'>
                    <input type='button' id='submit_button' name='submit_button' onclick='send_post()'>
                </body>
                </html>\
                """;
        final String subFrameURL =
                mWebServer.setResponse("/inner_frame_address_formless.html", subFrame, null);
        assertTrue(Uri.parse(subFrameURL).getPath().equals("/inner_frame_address_formless.html"));
        loadHTML(
                """
                <script>
                    function receiveMessage(event) {
                        var address_iframe = document.getElementById('address_iframe');
                        address_iframe.parentNode.removeChild(address_iframe);
                    }
                    window.addEventListener('message', receiveMessage, false);
                </script>
                <iframe src='inner_frame_address_formless.html' id='address_iframe' name='address_iframe'>
                </iframe>\
                """);

        int cnt = 0;
        pollJavascriptResult(
                """
                var iframe = document.getElementById('address_iframe');
                var frame_doc = iframe.contentDocument;
                frame_doc.getElementById('address_field').focus();
                frame_doc.activeElement.id;\
                """,
                "\"address_field\"");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        executeJavaScriptAndWaitForResult(
                """
                var iframe = document.getElementById('address_iframe');
                var frame_doc = iframe.contentDocument;
                frame_doc.getElementById('submit_button').click();\
                """);
        // The additional AUTOFILL_VIEW_EXITED event caused by 'click' of the button.
        waitForCallbackAndVerifyTypes(
                cnt,
                new Integer[] {
                    AUTOFILL_VIEW_EXITED, AUTOFILL_VALUE_CHANGED, AUTOFILL_COMMIT, AUTOFILL_CANCEL
                });
        assertEquals(SubmissionSource.FRAME_DETACHED, mSubmissionSource);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/404869375")
    public void testLabelChange() throws Throwable {
        loadHTML(
                """
                <form action='a.html'>
                    <label id='label_id'> Address </label>
                    <input type='text' id='address' name='address' autocomplete='on' />
                    <p id='p_id'>Address 1</p>
                    <input type='text' name='address1' autocomplete='on' />
                    <input type='submit' id='submit_button' name='submit_button' />
                </form>\
                """);
        int cnt = 0;
        executeJavaScriptAndWaitForResult("document.getElementById('address').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        // Verify label change shall trigger new session.
        executeJavaScriptAndWaitForResult(
                "document.getElementById('label_id').innerHTML='address change';");
        executeJavaScriptAndWaitForResult("document.getElementById('address').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt +=
                waitForCallbackAndVerifyTypes(
                        cnt,
                        new Integer[] {
                            AUTOFILL_VIEW_EXITED,
                            AUTOFILL_VIEW_ENTERED,
                            AUTOFILL_SESSION_STARTED,
                            AUTOFILL_VALUE_CHANGED
                        });
        // Verify inferred label change won't trigger new session.
        executeJavaScriptAndWaitForResult(
                "document.getElementById('p_id').innerHTML='address change';");
        executeJavaScriptAndWaitForResult("document.getElementById('address').focus();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_B);
        cnt += waitForCallbackAndVerifyTypes(cnt, new Integer[] {AUTOFILL_VALUE_CHANGED});
    }

    /**
     * Simulates an Autofill server response with the primary types `fieldTypes[i]` for the fields
     * with id `fieldIds[i]`. Form parsing in the browser is asynchronous, so this waits until the
     * main frame has parsed a form that contains all of `fieldIds`. For a frame-transcending form,
     * this is only the case after the forms of all subframes have been parsed.
     */
    private void simulateServerPrimaryTypes(String[] fieldIds, int[] fieldTypes) {
        CriteriaHelper.pollUiThread(
                () ->
                        AutofillProviderTestHelper
                                .simulateMainFrameAutofillServerResponseForTesting(
                                        mAwContents.getWebContents(), fieldIds, fieldTypes),
                "The browser hasn't parsed a form with the fields " + Arrays.toString(fieldIds),
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Like {@link #simulateServerPrimaryTypes}, but with all server predictions for each field. The
     * first prediction for a field is its primary type.
     */
    private void simulateServerPredictions(String[] fieldIds, int[][] fieldTypes) {
        CriteriaHelper.pollUiThread(
                () ->
                        AutofillProviderTestHelper
                                .simulateMainFramePredictionsAutofillServerResponseForTesting(
                                        mAwContents.getWebContents(), fieldIds, fieldTypes),
                "The browser hasn't parsed a form with the fields " + Arrays.toString(fieldIds),
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void pollJavascriptResult(String script, String expectedResult) throws Throwable {
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        return expectedResult.equals(executeJavaScriptAndWaitForResult(script));
                    } catch (Throwable e) {
                        return false;
                    }
                });
    }

    private void pollJavascriptResultNotEqualTo(String script, String result) throws Throwable {
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        return !result.equals(executeJavaScriptAndWaitForResult(script));
                    } catch (Throwable e) {
                        return false;
                    }
                });
    }

    private void pollDatalistPopupShown(int expectedTotalChildren) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    AutofillPopup popup = mAutofillProvider.getDatalistPopupForTesting();
                    boolean isShown =
                            popup != null
                                    && popup.getListView() != null
                                    && popup.getListView().getChildCount() == expectedTotalChildren;
                    for (int i = 0; i < expectedTotalChildren && isShown; i++) {
                        isShown =
                                popup.getListView().getChildAt(i).getWidth() > 0
                                        && popup.getListView().getChildAt(i).isAttachedToWindow();
                    }
                    return isShown;
                });
    }

    private void scrollToBottom() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestContainerView.scrollTo(0, mTestContainerView.getHeight());
                });
    }

    private void loadUrlSync(String url) throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        done.waitForCallback(callCount);

        ThreadUtils.runOnUiThreadBlocking(
                () -> WebContentsUtils.simulateEndOfPaintHolding(mAwContents.getWebContents()));
    }

    private void reloadSync() throws Exception {
        CallbackHelper done = mContentsClient.getOnPageCommitVisibleHelper();
        int callCount = done.getCallCount();
        mRule.reloadSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper());
        done.waitForCallback(callCount);
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private ArrayList<Pair<Integer, AutofillValue>> getChangedValues() {
        return mTestValues.changedValues;
    }

    private void clearChangedValues() {
        if (mTestValues.changedValues != null) mTestValues.changedValues.clear();
    }

    private void invokeOnProvideAutoFillVirtualStructure() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestValues.testViewStructure = new TestViewStructure();
                    mAwContents.onProvideAutoFillVirtualStructure(mTestValues.testViewStructure, 1);
                });
    }

    private void invokeAutofill(SparseArray<AutofillValue> values) {
        ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.autofill(values));
    }

    private void invokeOnInputUIShown() {
        ThreadUtils.runOnUiThreadBlocking(() -> mTestAutofillManagerWrapper.notifyInputUiChange());
    }

    private int getCallbackCount() {
        return mEvents.size();
    }

    private int clearEventQueueAndGetCallCount() {
        return mEvents.consumeAll();
    }

    /**
     * Waits until the next events are `expectedEventArray` and consumes them.
     *
     * @param currentCallCount The number of events that the test has consumed so far. Events before
     *     this index are ignored even if no earlier step consumed them.
     * @param expectedEventArray The expected event types, in order.
     * @return How many more events have been consumed since `currentCallCount`. This is the length
     *     of `expectedEventArray` plus the number of skipped bounds updates, see {@link
     *     AutofillEventLog#waitForTypes}.
     */
    private int waitForCallbackAndVerifyTypes(int currentCallCount, Integer[] expectedEventArray) {
        int[] expectedTypes = new int[expectedEventArray.length];
        for (int i = 0; i < expectedTypes.length; i++) expectedTypes[i] = expectedEventArray[i];
        return mEvents.waitForTypes(currentCallCount, expectedTypes) - currentCallCount;
    }

    /**
     * Consumes events until {@code expectedEvents} have been observed in this order. Other events
     * are ignored. Fails if they aren't observed within the timeout.
     *
     * @param expectedEvents the events that need to happen.
     */
    private void waitForEvents(Integer[] expectedEvents) {
        int[] expectedTypes = new int[expectedEvents.length];
        for (int i = 0; i < expectedTypes.length; i++) expectedTypes[i] = expectedEvents[i];
        mEvents.waitForSubsequence(expectedTypes);
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        long eventTime = SystemClock.uptimeMillis();
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, code, 0));
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, code, 0));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return mTestContainerView.dispatchKeyEvent(event);
                    }
                });
    }

    /**
     * Loads an HTML snippet which will be used by the test to execute JS commands on. This snippet
     * is loaded on the test web server.
     *
     * @param htmlBody The body of the HTML snippet to be loaded.
     * @return The url where the loaded HTML can be found on the test web server.
     */
    private String loadHTML(String htmlBody) throws Exception {
        final String data =
                String.format(
                        """
                        <html>
                        <head></head>
                        <body>
                        %s
                        </body>
                        </html>
                        """,
                        htmlBody);
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
        return url;
    }
}
