// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.Intent;
import android.util.AndroidRuntimeException;
import android.util.Base64;
import android.view.ViewGroup;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.net.test.util.TestWebServer;

import java.lang.annotation.Annotation;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.function.Consumer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Custom ActivityTestRunner for WebView instrumentation tests */
public class AwActivityTestRule extends BaseActivityTestRule<AwTestRunnerActivity> {
    public static final long WAIT_TIMEOUT_MS = 15000L;

    // Only use scaled timeout if you are certain it's not being called further up the call stack.
    public static final long SCALED_WAIT_TIMEOUT_MS = ScalableTimeout.scaleTimeout(15000L);

    public static final int CHECK_INTERVAL = 100;

    private static final String TAG = "AwActivityTestRule";

    private static final Pattern MAYBE_QUOTED_STRING = Pattern.compile("^(\"?)(.*)\\1$");

    private static boolean sBrowserProcessStarted;

    /** An interface to call onCreateWindow(AwContents). */
    public interface OnCreateWindowHandler {
        /** This will be called when a new window pops up from the current webview. */
        public boolean onCreateWindow(AwContents awContents);
    }

    private Description mCurrentTestDescription;

    // The browser context needs to be a process-wide singleton.
    private AwBrowserContext mBrowserContext;

    private List<WeakReference<AwContents>> mAwContentsDestroyedInTearDown = new ArrayList<>();

    private Consumer<AwSettings> mMaybeMutateAwSettings;

    public AwActivityTestRule() {
        super(AwTestRunnerActivity.class);
    }

    public AwActivityTestRule(Consumer<AwSettings> mMaybeMutateAwSettings) {
        super(AwTestRunnerActivity.class);
        this.mMaybeMutateAwSettings = mMaybeMutateAwSettings;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        mCurrentTestDescription = description;
        return super.apply(base, description);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        if (needsAwBrowserContextCreated()) {
            createAwBrowserContext();
        }
        if (needsBrowserProcessStarted()) {
            startBrowserProcess();
        } else {
            assert !sBrowserProcessStarted
                    : "needsBrowserProcessStarted false and @Batch are incompatible";
        }
    }

    @Override
    protected void after() {
        if (!needsAwContentsCleanup()) {
            super.after();
            return;
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (WeakReference<AwContents> awContentsRef : mAwContentsDestroyedInTearDown) {
                        AwContents awContents = awContentsRef.get();
                        if (awContents == null) continue;
                        awContents.destroy();
                    }
                });
        // Flush the UI queue since destroy posts again to UI thread.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContentsDestroyedInTearDown.clear();
                });
        super.after();
    }

    public boolean needsHideActionBar() {
        return false;
    }

    private Intent getLaunchIntent() {
        if (needsHideActionBar()) {
            Intent intent = getActivityIntent();
            intent.putExtra(AwTestRunnerActivity.FLAG_HIDE_ACTION_BAR, true);
            return intent;
        }
        return null;
    }

    @Override
    public void launchActivity(Intent intent) {
        if (getActivity() != null) return;
        super.launchActivity(intent);
        ApplicationTestUtils.waitForActivityState(getActivity(), Stage.RESUMED);
    }

    public AwTestRunnerActivity launchActivity() {
        launchActivity(getLaunchIntent());
        return getActivity();
    }

    public AwBrowserContext createAwBrowserContextOnUiThread() {
        // Native pointer is initialized later in startBrowserProcess if needed.
        return new AwBrowserContext(0);
    }

    public TestDependencyFactory createTestDependencyFactory() {
        return new TestDependencyFactory();
    }

    /**
     * Override this to return false if the test doesn't want to create an
     * AwBrowserContext automatically.
     */
    public boolean needsAwBrowserContextCreated() {
        return true;
    }

    /**
     * Override this to return false if the test doesn't want the browser
     * startup sequence to be run automatically.
     *
     * @return Whether the instrumentation test requires the browser process to
     *         already be started.
     */
    public boolean needsBrowserProcessStarted() {
        return true;
    }

    /**
     * Override this to return false if test doesn't need all AwContents to be
     * destroyed explicitly after the test.
     */
    public boolean needsAwContentsCleanup() {
        return true;
    }

    public void createAwBrowserContext() {
        if (mBrowserContext != null) {
            throw new AndroidRuntimeException("There should only be one browser context.");
        }
        launchActivity(); // The Activity must be launched in order to load native code
        ThreadUtils.runOnUiThreadBlocking(
                () -> mBrowserContext = createAwBrowserContextOnUiThread());
    }

    public void startBrowserProcess() {
        doStartBrowserProcess(false);
    }

    public void startBrowserProcessWithVulkan() {
        doStartBrowserProcess(true);
    }

    private void doStartBrowserProcess(boolean useVulkan) {
        // The Activity must be launched in order for proper webview statics to be setup.
        launchActivity();
        if (!sBrowserProcessStarted) {
            sBrowserProcessStarted = true;
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        AwTestContainerView.installDrawFnFunctionTable(useVulkan);
                        AwBrowserProcess.configureChildProcessLauncherForTesting();
                        AwBrowserProcess.start();
                    });
        }
        if (mBrowserContext != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mBrowserContext.setNativePointer(
                                    AwBrowserContext.getDefault()
                                            .getNativeBrowserContextPointer()));
        }
    }

    public static void enableJavaScriptOnUiThread(final AwContents awContents) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.getSettings().setJavaScriptEnabled(true));
    }

    private static boolean getJavaScriptEnabledOnUiThread(final AwContents awContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.getSettings().getJavaScriptEnabled());
    }

    public static void setNetworkAvailableOnUiThread(
            final AwContents awContents, final boolean networkUp) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.setNetworkAvailable(networkUp));
    }

    /** Loads url on the UI thread and blocks until onPageFinished is called. */
    public void loadUrlSync(
            final AwContents awContents, CallbackHelper onPageFinishedHelper, final String url)
            throws Exception {
        loadUrlSync(awContents, onPageFinishedHelper, url, null);
    }

    public void loadUrlSync(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String url,
            final Map<String, String> extraHeaders)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url, extraHeaders);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    public void loadUrlSyncAndExpectError(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            CallbackHelper onReceivedErrorHelper,
            final String url)
            throws Exception {
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();
        int onFinishedCallCount = onPageFinishedHelper.getCallCount();
        loadUrlAsync(awContents, url);
        onReceivedErrorHelper.waitForCallback(
                onReceivedErrorCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        onPageFinishedHelper.waitForCallback(
                onFinishedCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /** Loads url on the UI thread but does not block. */
    public void loadUrlAsync(final AwContents awContents, final String url) {
        loadUrlAsync(awContents, url, null);
    }

    public void loadUrlAsync(
            final AwContents awContents, final String url, final Map<String, String> extraHeaders) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.loadUrl(url, extraHeaders));
    }

    /** Posts url on the UI thread and blocks until onPageFinished is called. */
    public void postUrlSync(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String url,
            byte[] postData)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        postUrlAsync(awContents, url, postData);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /** Loads url on the UI thread but does not block. */
    public void postUrlAsync(final AwContents awContents, final String url, byte[] postData) {
        class PostUrl implements Runnable {
            byte[] mPostData;

            public PostUrl(byte[] postData) {
                mPostData = postData;
            }

            @Override
            public void run() {
                awContents.postUrl(url, mPostData);
            }
        }
        ThreadUtils.runOnUiThreadBlocking(new PostUrl(postData));
    }

    /** Loads data on the UI thread and blocks until onPageFinished is called. */
    public void loadDataSync(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String data,
            final String mimeType,
            final boolean isBase64Encoded)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataAsync(awContents, data, mimeType, isBase64Encoded);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    public void loadHtmlSync(
            final AwContents awContents, CallbackHelper onPageFinishedHelper, final String html)
            throws Throwable {
        final String encodedData = Base64.encodeToString(html.getBytes(), Base64.NO_PADDING);
        loadDataSync(awContents, onPageFinishedHelper, encodedData, "text/html", true);
    }

    public void loadDataSyncWithCharset(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String data,
            final String mimeType,
            final boolean isBase64Encoded,
            final String charset)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        awContents.loadUrl(
                                LoadUrlParams.createLoadDataParams(
                                        data, mimeType, isBase64Encoded, charset)));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /** Loads data on the UI thread but does not block. */
    public void loadDataAsync(
            final AwContents awContents,
            final String data,
            final String mimeType,
            final boolean isBase64Encoded) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.loadData(data, mimeType, isBase64Encoded ? "base64" : null));
    }

    public void loadDataWithBaseUrlSync(
            final AwContents awContents,
            CallbackHelper onPageFinishedHelper,
            final String data,
            final String mimeType,
            final boolean isBase64Encoded,
            final String baseUrl,
            final String historyUrl)
            throws Throwable {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        loadDataWithBaseUrlAsync(awContents, data, mimeType, isBase64Encoded, baseUrl, historyUrl);
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    public void loadDataWithBaseUrlAsync(
            final AwContents awContents,
            final String data,
            final String mimeType,
            final boolean isBase64Encoded,
            final String baseUrl,
            final String historyUrl)
            throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        awContents.loadDataWithBaseURL(
                                baseUrl,
                                data,
                                mimeType,
                                isBase64Encoded ? "base64" : null,
                                historyUrl));
    }

    /** Reloads the current page synchronously. */
    public void reloadSync(final AwContents awContents, CallbackHelper onPageFinishedHelper)
            throws Exception {
        int currentCallCount = onPageFinishedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.getNavigationController().reload(true));
        onPageFinishedHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /** Stops loading on the UI thread. */
    public void stopLoading(final AwContents awContents) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.stopLoading());
    }

    public void waitForVisualStateCallback(final AwContents awContents) throws Exception {
        final CallbackHelper ch = new CallbackHelper();
        final int chCount = ch.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final long requestId = 666;
                    awContents.insertVisualStateCallback(
                            requestId,
                            new AwContents.VisualStateCallback() {
                                @Override
                                public void onComplete(long id) {
                                    Assert.assertEquals(requestId, id);
                                    ch.notifyCalled();
                                }
                            });
                });
        ch.waitForCallback(chCount);
    }

    public void insertVisualStateCallbackOnUIThread(
            final AwContents awContents,
            final long requestId,
            final AwContents.VisualStateCallback callback) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.insertVisualStateCallback(requestId, callback));
    }

    // Waits for the pixel at the center of AwContents to color up into expectedColor.
    // Note that this is a stricter condition that waiting for a visual state callback,
    // as visual state callback only indicates that *something* has appeared in WebView.
    public void waitForPixelColorAtCenterOfView(
            final AwContents awContents,
            final AwTestContainerView testContainerView,
            final int expectedColor) {
        pollUiThread(
                () ->
                        GraphicsTestUtils.getPixelColorAtCenterOfView(awContents, testContainerView)
                                == expectedColor);
    }

    public AwTestContainerView createAwTestContainerView(final AwContentsClient awContentsClient) {
        return createAwTestContainerView(awContentsClient, false, null);
    }

    public AwTestContainerView createAwTestContainerView(
            final AwContentsClient awContentsClient,
            boolean supportsLegacyQuirks,
            final TestDependencyFactory testDependencyFactory) {
        AwTestContainerView testContainerView =
                createDetachedAwTestContainerView(
                        awContentsClient, supportsLegacyQuirks, testDependencyFactory);
        getActivity().addView(testContainerView);
        testContainerView.requestFocus();
        return testContainerView;
    }

    public AwBrowserContext getAwBrowserContext() {
        return mBrowserContext;
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient) {
        return createDetachedAwTestContainerView(awContentsClient, false, null);
    }

    public AwTestContainerView createDetachedAwTestContainerView(
            final AwContentsClient awContentsClient,
            boolean supportsLegacyQuirks,
            TestDependencyFactory testDependencyFactory) {
        if (testDependencyFactory == null) {
            testDependencyFactory = createTestDependencyFactory();
        }
        boolean allowHardwareAcceleration = isHardwareAcceleratedTest();
        final AwTestContainerView testContainerView =
                testDependencyFactory.createAwTestContainerView(
                        getActivity(), allowHardwareAcceleration);

        AwSettings awSettings =
                testDependencyFactory.createAwSettings(getActivity(), supportsLegacyQuirks);
        if (mMaybeMutateAwSettings != null) mMaybeMutateAwSettings.accept(awSettings);
        AwContents awContents =
                testDependencyFactory.createAwContents(
                        mBrowserContext,
                        testContainerView,
                        testContainerView.getContext(),
                        testContainerView.getInternalAccessDelegate(),
                        testContainerView.getNativeDrawFunctorFactory(),
                        awContentsClient,
                        awSettings,
                        testDependencyFactory);
        testContainerView.initialize(awContents);
        mAwContentsDestroyedInTearDown.add(new WeakReference<>(awContents));
        return testContainerView;
    }

    public boolean isHardwareAcceleratedTest() {
        return !testMethodHasAnnotation(DisableHardwareAcceleration.class);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(final AwContentsClient client) {
        return createAwTestContainerViewOnMainSync(client, false, null);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client, final boolean supportsLegacyQuirks) {
        return createAwTestContainerViewOnMainSync(client, supportsLegacyQuirks, null);
    }

    public AwTestContainerView createAwTestContainerViewOnMainSync(
            final AwContentsClient client,
            final boolean supportsLegacyQuirks,
            final TestDependencyFactory testDependencyFactory) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        createAwTestContainerView(
                                client, supportsLegacyQuirks, testDependencyFactory));
    }

    public void destroyAwContentsOnMainSync(final AwContents awContents) {
        if (awContents == null) return;
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.destroy());
    }

    public String getTitleOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getTitle());
    }

    public AwSettings getAwSettingsOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getSettings());
    }

    /**
     * Verify double quotes in both sides of the raw string. Strip the double quotes and returns
     * rest of the string.
     */
    public String maybeStripDoubleQuotes(String raw) {
        Assert.assertNotNull(raw);
        Matcher m = MAYBE_QUOTED_STRING.matcher(raw);
        Assert.assertTrue(m.matches());
        return m.group(2);
    }

    /**
     * Executes the given snippet of JavaScript code within the given ContentView. Returns the
     * result of its execution in JSON format.
     */
    public String executeJavaScriptAndWaitForResult(
            final AwContents awContents, TestAwContentsClient viewClient, final String code)
            throws Exception {
        return executeJavaScriptAndWaitForResult(
                awContents, viewClient, code, /* shouldCheckSettings= */ true);
    }

    /**
     * Like {@link #executeJavaScriptAndWaitForResult} but with a parameter to skip the call to
     * {@link checkJavaScriptEnabled}. This is useful if your test expects JavaScript to be disabled
     * (in which case the underlying executeJavaScriptAndWaitForResult() is expected to be a NOOP).
     */
    public String executeJavaScriptAndWaitForResult(
            final AwContents awContents,
            TestAwContentsClient viewClient,
            final String code,
            boolean shouldCheckSettings)
            throws Exception {
        if (shouldCheckSettings) {
            checkJavaScriptEnabled(awContents);
        }
        return JSUtils.executeJavaScriptAndWaitForResult(
                InstrumentationRegistry.getInstrumentation(), awContents,
                viewClient.getOnEvaluateJavaScriptResultHelper(), code);
    }

    public static void checkJavaScriptEnabled(AwContents awContents) throws Exception {
        boolean javaScriptEnabled = AwActivityTestRule.getJavaScriptEnabledOnUiThread(awContents);
        if (!javaScriptEnabled) {
            throw new IllegalStateException(
                    "JavaScript is disabled in this AwContents; did you forget to call "
                            + "AwActivityTestRule.enableJavaScriptOnUiThread()?");
        }
    }

    /**
     * Executes JavaScript code within the given ContentView to get the text content in
     * document body. Returns the result string without double quotes.
     */
    public String getJavaScriptResultBodyTextContent(
            final AwContents awContents, final TestAwContentsClient viewClient) throws Exception {
        String raw =
                executeJavaScriptAndWaitForResult(
                        awContents, viewClient, "document.body.textContent");
        return maybeStripDoubleQuotes(raw);
    }

    /**
     * Adds a JavaScript interface to the AwContents. Does its work synchronously on the UI thread,
     * and can be called from any thread. All the rules of {@link
     * android.webkit.WebView#addJavascriptInterface} apply to this method (ex. you must call this
     * <b>prior</b> to loading the frame you intend to load the JavaScript interface into).
     *
     * @param awContents the AwContents in which to insert the JavaScript interface.
     * @param objectToInject the JavaScript interface to inject.
     * @param javascriptIdentifier the name with which to refer to {@code objectToInject} from
     *     JavaScript code.
     */
    public static void addJavascriptInterfaceOnUiThread(
            final AwContents awContents,
            final Object objectToInject,
            final String javascriptIdentifier)
            throws Exception {
        checkJavaScriptEnabled(awContents);
        ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.addJavascriptInterface(objectToInject, javascriptIdentifier));
    }

    /**
     * Wrapper around CriteriaHelper.pollInstrumentationThread. This uses AwActivityTestRule-specifc
     * timeouts and treats timeouts and exceptions as test failures automatically.
     */
    public static void pollInstrumentationThread(final Callable<Boolean> callable) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        return callable.call();
                    } catch (Throwable e) {
                        Log.e(TAG, "Exception while polling.", e);
                        return false;
                    }
                },
                WAIT_TIMEOUT_MS,
                CHECK_INTERVAL);
    }

    /**
     * Wrapper around {@link AwActivityTestRule#pollInstrumentationThread()} but runs the callable
     * on the UI thread.
     */
    public void pollUiThread(final Callable<Boolean> callable) {
        pollInstrumentationThread(() -> ThreadUtils.runOnUiThreadBlocking(callable));
    }

    /**
     * Waits for {@code future} and returns its value (or times out). If {@code future} has an
     * associated Exception, this will re-throw that Exception on the instrumentation thread
     * (wrapping with an unchecked Exception if necessary, to avoid requiring callers to declare
     * checked Exceptions).
     *
     * @param future the {@link Future} representing a value of interest.
     * @return the value {@code future} represents.
     */
    public static <T> T waitForFuture(Future<T> future) {
        try {
            return future.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (ExecutionException e) {
            // ExecutionException means this Future has an associated Exception that we should
            // re-throw on the current thread. We throw the cause instead of ExecutionException,
            // since ExecutionException itself isn't interesting, and might mislead those debugging
            // test failures to suspect this method is the culprit (whereas the root cause is from
            // another thread).
            Throwable cause = e.getCause();
            // If the cause is an unchecked Throwable type, re-throw as-is.
            if (cause instanceof Error) throw (Error) cause;
            if (cause instanceof RuntimeException) throw (RuntimeException) cause;
            // Otherwise, wrap this in an unchecked Exception so callers don't need to declare
            // checked Exceptions.
            throw new RuntimeException(cause);
        } catch (InterruptedException | TimeoutException e) {
            // Don't call e.getCause() for either of these. Unlike ExecutionException, these don't
            // wrap the root cause, but rather are themselves interesting. Again, we wrap these
            // checked Exceptions with an unchecked Exception for the caller's convenience.
            //
            // Although we might be tempted to handle InterruptedException by calling
            // Thread.currentThread().interrupt(), this is not correct in this case. The interrupted
            // thread was likely a different thread than the current thread, so there's nothing
            // special we need to do.
            throw new RuntimeException(e);
        }
    }

    /** Takes an element out of the {@link BlockingQueue} (or times out). */
    public static <T> T waitForNextQueueElement(BlockingQueue<T> queue) throws Exception {
        T value = queue.poll(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        if (value == null) {
            // {@code null} is the special value which means {@link BlockingQueue#poll} has timed
            // out (also: there's no risk for collision with real values, because BlockingQueue does
            // not allow null entries). Instead of returning this special value, let's throw a
            // proper TimeoutException.
            throw new TimeoutException(
                    "Timeout while trying to take next entry from BlockingQueue");
        }
        return value;
    }

    /**
     * Clears the resource cache. Note that the cache is per-application, so this will clear the
     * cache for all WebViews used.
     */
    public void clearCacheOnUiThread(final AwContents awContents, final boolean includeDiskFiles) {
        ThreadUtils.runOnUiThreadBlocking(() -> awContents.clearCache(includeDiskFiles));
    }

    /** Returns pure page scale. */
    public float getScaleOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getPageScaleFactor());
    }

    /** Returns page scale multiplied by the screen density. */
    public float getPixelScaleOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.getScale());
    }

    /** Returns whether a user can zoom the page in. */
    public boolean canZoomInOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.canZoomIn());
    }

    /** Returns whether a user can zoom the page out. */
    public boolean canZoomOutOnUiThread(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> awContents.canZoomOut());
    }

    /** Loads the main html then triggers the popup window. */
    public void triggerPopup(
            final AwContents parentAwContents,
            TestAwContentsClient parentAwContentsClient,
            TestWebServer testWebServer,
            String mainHtml,
            String popupHtml,
            String popupPath,
            String triggerScript)
            throws Exception {
        enableJavaScriptOnUiThread(parentAwContents);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    parentAwContents.getSettings().setSupportMultipleWindows(true);
                    parentAwContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
                });

        final String parentUrl = testWebServer.setResponse("/popupParent.html", mainHtml, null);
        if (popupHtml != null) {
            testWebServer.setResponse(popupPath, popupHtml, null);
        } else {
            testWebServer.setResponseWithNoContentStatus(popupPath);
        }

        parentAwContentsClient.getOnCreateWindowHelper().setReturnValue(true);
        loadUrlSync(parentAwContents, parentAwContentsClient.getOnPageFinishedHelper(), parentUrl);

        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                parentAwContentsClient.getOnCreateWindowHelper();
        int currentCallCount = onCreateWindowHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> parentAwContents.evaluateJavaScriptForTests(triggerScript, null));
        onCreateWindowHelper.waitForCallback(
                currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    /**
     * Supplies the popup window with AwContents then waits for the popup window to finish loading.
     * @param parentAwContents Parent webview's AwContents.
     */
    public PopupInfo connectPendingPopup(AwContents parentAwContents) throws Exception {
        PopupInfo popupInfo = createPopupContents(parentAwContents);
        loadPopupContents(parentAwContents, popupInfo, null);
        return popupInfo;
    }

    /** Creates a popup window with AwContents. */
    public PopupInfo createPopupContents(final AwContents parentAwContents) {
        TestAwContentsClient popupContentsClient;
        AwTestContainerView popupContainerView;
        final AwContents popupContents;
        popupContentsClient = new TestAwContentsClient();
        popupContainerView = createAwTestContainerViewOnMainSync(popupContentsClient);
        popupContents = popupContainerView.getAwContents();
        enableJavaScriptOnUiThread(popupContents);
        return new PopupInfo(popupContentsClient, popupContainerView, popupContents);
    }

    /**
     * Waits for the popup window to finish loading.
     * @param parentAwContents Parent webview's AwContents.
     * @param info The PopupInfo.
     * @param onCreateWindowHandler An instance of OnCreateWindowHandler. null if there isn't.
     */
    public void loadPopupContents(
            final AwContents parentAwContents,
            PopupInfo info,
            OnCreateWindowHandler onCreateWindowHandler)
            throws Exception {
        TestAwContentsClient popupContentsClient = info.popupContentsClient;
        final AwContents popupContents = info.popupContents;
        OnPageFinishedHelper onPageFinishedHelper = popupContentsClient.getOnPageFinishedHelper();
        int finishCallCount = onPageFinishedHelper.getCallCount();

        if (onCreateWindowHandler != null) onCreateWindowHandler.onCreateWindow(popupContents);

        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                popupContentsClient.getOnReceivedTitleHelper();
        int titleCallCount = onReceivedTitleHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> parentAwContents.supplyContentsForPopup(popupContents));

        onPageFinishedHelper.waitForCallback(
                finishCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        onReceivedTitleHelper.waitForCallback(
                titleCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private boolean testMethodHasAnnotation(Class<? extends Annotation> clazz) {
        return mCurrentTestDescription.getAnnotation(clazz) != null;
    }

    /**
     * Factory class used in creation of test AwContents instances. Test cases
     * can provide subclass instances to the createAwTest* methods in order to
     * create an AwContents instance with injected test dependencies.
     */
    public static class TestDependencyFactory extends AwContents.DependencyFactory {
        public AwTestContainerView createAwTestContainerView(
                AwTestRunnerActivity activity, boolean allowHardwareAcceleration) {
            return new AwTestContainerView(activity, allowHardwareAcceleration);
        }

        public AwSettings createAwSettings(Context context, boolean supportsLegacyQuirks) {
            return new AwSettings(
                    context,
                    /* isAccessFromFileURLsGrantedByDefault= */ false,
                    supportsLegacyQuirks,
                    /* allowEmptyDocumentPersistence= */ false,
                    /* allowGeolocationOnInsecureOrigins= */ true,
                    /* doNotUpdateSelectionOnMutatingSelectionRange= */ false);
        }

        public AwContents createAwContents(
                AwBrowserContext browserContext,
                ViewGroup containerView,
                Context context,
                InternalAccessDelegate internalAccessAdapter,
                NativeDrawFunctorFactory nativeDrawFunctorFactory,
                AwContentsClient contentsClient,
                AwSettings settings,
                DependencyFactory dependencyFactory) {
            return new AwContents(
                    browserContext,
                    containerView,
                    context,
                    internalAccessAdapter,
                    nativeDrawFunctorFactory,
                    contentsClient,
                    settings,
                    dependencyFactory);
        }
    }

    /** POD object for holding references to helper objects of a popup window. */
    public static class PopupInfo {
        public final TestAwContentsClient popupContentsClient;
        public final AwTestContainerView popupContainerView;
        public final AwContents popupContents;

        public PopupInfo(
                TestAwContentsClient popupContentsClient,
                AwTestContainerView popupContainerView,
                AwContents popupContents) {
            this.popupContentsClient = popupContentsClient;
            this.popupContainerView = popupContainerView;
            this.popupContents = popupContents;
        }
    }
}
