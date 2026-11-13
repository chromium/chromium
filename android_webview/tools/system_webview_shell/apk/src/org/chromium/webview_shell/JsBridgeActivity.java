// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.view.MenuItem;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.webkit.JavaScriptReplyProxy;
import androidx.webkit.WebMessageCompat;
import androidx.webkit.WebViewClientCompat;
import androidx.webkit.WebViewCompat;
import androidx.webkit.WebViewFeature;

import org.chromium.base.Log;

import java.io.ByteArrayInputStream;
import java.lang.ref.WeakReference;
import java.nio.charset.StandardCharsets;
import java.util.Collections;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * {@link ManuallyTriggeredStartupActivity} subclass for benchmarking renderer memory across
 * multi-step navigations that exercise both {@link WebView#addJavascriptInterface} and {@link
 * WebViewCompat#addWebMessageListener}.
 *
 * <p>Navigating to {@code https://origina.webviewshell.test/start} drives a deterministic hermetic
 * sequence of {@code N} synthetic HTTPS navigations (default 20, overridable via {@code --es steps
 * <N>}) alternating same-origin and cross-origin transitions, {@code document.open()} same-frame
 * clears, and child {@code <iframe>} attach/detach before settling on {@code about:blank}.
 */
public class JsBridgeActivity extends ManuallyTriggeredStartupActivity {
    private static final String TAG = "WVJsBridgeBench";

    private static final String EXTRA_STEPS = "steps";
    private static final int DEFAULT_STEPS = 20;

    private static final String DOMAIN_SUFFIX = ".webviewshell.test";
    private static final String ORIGIN_A = "https://origina.webviewshell.test";
    private static final String ORIGIN_B = "https://originb.webviewshell.test";
    private static final Set<String> ALLOWED_ORIGIN_RULES =
            Collections.singleton("https://*.webviewshell.test");

    private static final String JS_INTERFACE_NAME = "wvBenchJsInterface";
    private static final String WEB_MESSAGE_LISTENER_NAME = "wvBenchMessageListener";
    private static final long STEP_WATCHDOG_TIMEOUT_MS = 4000L;

    private final Handler mUiHandler = new Handler(Looper.getMainLooper());
    private WeakReference<WebView> mWebViewRef = new WeakReference<>(null);
    private int mTotalSteps = DEFAULT_STEPS;
    private int mCurrentStep;
    private boolean mSequenceRunning;
    private boolean mCompleted;
    private volatile boolean mWebMessageListenerSupported;
    @Nullable private Runnable mStepWatchdog;

    /** Stateless transient token returned across {@code GinJavaBridge} to exercise object wraps. */
    public static final class TransientBridgeToken {
        private final int mStep;

        TransientBridgeToken(int step) {
            mStep = step;
        }

        @JavascriptInterface
        public int getStep() {
            return mStep;
        }
    }

    /**
     * Injected {@code @JavascriptInterface} bridge. Uses a {@link WeakReference} to avoid retaining
     * the {@link JsBridgeActivity} or {@link WebView} across destruction.
     */
    private static final class BenchmarkJsBridge {
        private final WeakReference<JsBridgeActivity> mActivityRef;

        BenchmarkJsBridge(JsBridgeActivity activity) {
            mActivityRef = new WeakReference<>(activity);
        }

        @JavascriptInterface
        public int ping(int step) {
            return step;
        }

        @JavascriptInterface
        public Object createTransientToken(int step) {
            return new TransientBridgeToken(step);
        }

        @JavascriptInterface
        public void notifyFallbackStepDone(int step) {
            JsBridgeActivity activity = mActivityRef.get();
            if (activity != null) {
                activity.mUiHandler.post(() -> activity.onStepCompletedOnUiThread(step));
            }
        }
    }

    @Override
    protected String getBrowserToolbarTitle() {
        return getResources().getString(R.string.title_activity_js_bridge);
    }

    @Override
    @SuppressLint("JavascriptInterface")
    protected void onWebViewCreated(WebView webView) {
        super.onWebViewCreated(webView);
        resetBenchmarkState();
        mWebViewRef = new WeakReference<>(webView);
        mTotalSteps = resolveTotalSteps();

        webView.addJavascriptInterface(new BenchmarkJsBridge(this), JS_INTERFACE_NAME);
        mWebMessageListenerSupported =
                WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER);
        if (mWebMessageListenerSupported) {
            WebViewCompat.addWebMessageListener(
                    webView,
                    WEB_MESSAGE_LISTENER_NAME,
                    ALLOWED_ORIGIN_RULES,
                    this::onBenchmarkWebMessage);
        }

        webView.setWebViewClient(
                new WebViewClientCompat() {
                    @Override
                    public void onPageStarted(WebView view, String url, Bitmap favicon) {
                        updateUrlBar(url);
                    }

                    @Override
                    public void onPageFinished(WebView view, String url) {
                        updateUrlBar(url);
                        if (url == null) {
                            return;
                        }
                        Uri parsed = Uri.parse(url);
                        if (isSyntheticBenchmarkUri(parsed) && "/start".equals(parsed.getPath())) {
                            mUiHandler.removeCallbacksAndMessages(null);
                            mStepWatchdog = null;
                            mSequenceRunning = true;
                            mCompleted = false;
                            advanceToStepOnUiThread(1);
                        } else if (mCompleted && "about:blank".equals(url)) {
                            view.clearHistory();
                            Log.i(
                                    TAG,
                                    "JS bridge sequence settled on about:blank after %d steps",
                                    mTotalSteps);
                        }
                    }

                    @Override
                    public boolean shouldOverrideUrlLoading(
                            @NonNull WebView view, @NonNull WebResourceRequest request) {
                        return false;
                    }

                    @SuppressWarnings("deprecation")
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView view, String url) {
                        return false;
                    }

                    @Override
                    @Nullable
                    public WebResourceResponse shouldInterceptRequest(
                            WebView view, WebResourceRequest request) {
                        Uri uri = request.getUrl();
                        if (!isSyntheticBenchmarkUri(uri)) {
                            return null;
                        }
                        return buildSyntheticResponse(uri);
                    }
                });
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int itemId = item.getItemId();
        if (itemId == R.id.menu_destroy_webview || itemId == R.id.menu_reset_webview) {
            resetBenchmarkState();
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        resetBenchmarkState();
        super.onDestroy();
    }

    private void resetBenchmarkState() {
        mUiHandler.removeCallbacksAndMessages(null);
        mStepWatchdog = null;
        mSequenceRunning = false;
        mCompleted = false;
        mCurrentStep = 0;
        mWebViewRef = new WeakReference<>(null);
    }

    private int resolveTotalSteps() {
        Intent intent = getIntent();
        if (intent == null) {
            return DEFAULT_STEPS;
        }
        int intSteps = intent.getIntExtra(EXTRA_STEPS, -1);
        if (intSteps > 0) {
            return intSteps;
        }
        String strSteps = intent.getStringExtra(EXTRA_STEPS);
        if (strSteps != null) {
            try {
                int parsed = Integer.parseInt(strSteps.trim());
                return parsed > 0 ? parsed : DEFAULT_STEPS;
            } catch (NumberFormatException e) {
                Log.w(TAG, "Invalid steps extra '%s', falling back to %d", strSteps, DEFAULT_STEPS);
            }
        }
        return DEFAULT_STEPS;
    }

    private void onBenchmarkWebMessage(
            @NonNull WebView view,
            @NonNull WebMessageCompat message,
            @NonNull Uri sourceOrigin,
            boolean isMainFrame,
            @NonNull JavaScriptReplyProxy replyProxy) {
        if (!isMainFrame) {
            return;
        }
        String payload = message.getData();
        if (payload == null) {
            return;
        }
        if (payload.startsWith("ready:")) {
            int step = parseStepSuffix(payload, "ready:");
            if (step > 0
                    && WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER)) {
                replyProxy.postMessage("ack:" + step);
            }
        } else if (payload.startsWith("done:")) {
            int step = parseStepSuffix(payload, "done:");
            if (step > 0) {
                mUiHandler.post(() -> onStepCompletedOnUiThread(step));
            }
        }
    }

    private static int parseStepSuffix(String message, String prefix) {
        try {
            return Integer.parseInt(message.substring(prefix.length()).trim());
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    private void advanceToStepOnUiThread(int step) {
        cancelStepWatchdog();
        WebView webView = mWebViewRef.get();
        if (webView == null || !mSequenceRunning) {
            return;
        }
        if (step > mTotalSteps) {
            mSequenceRunning = false;
            mCompleted = true;
            webView.loadUrl("about:blank");
            return;
        }
        mCurrentStep = step;
        // Alternate in pairs (A, A, B, B, A, A, ...) so both same-origin and cross-origin
        // navigations are exercised across consecutive steps.
        String origin = (((step - 1) / 2) % 2 == 0) ? ORIGIN_A : ORIGIN_B;
        String stepUrl = origin + "/page/" + step;
        mStepWatchdog =
                () -> {
                    if (mSequenceRunning && mCurrentStep == step) {
                        Log.w(TAG, "Step %d timed out waiting for JS handshake; advancing", step);
                        advanceToStepOnUiThread(step + 1);
                    }
                };
        mUiHandler.postDelayed(mStepWatchdog, STEP_WATCHDOG_TIMEOUT_MS);
        webView.loadUrl(stepUrl);
    }

    private void onStepCompletedOnUiThread(int step) {
        if (!mSequenceRunning || step != mCurrentStep) {
            return;
        }
        cancelStepWatchdog();
        advanceToStepOnUiThread(step + 1);
    }

    private void cancelStepWatchdog() {
        if (mStepWatchdog != null) {
            mUiHandler.removeCallbacks(mStepWatchdog);
            mStepWatchdog = null;
        }
    }

    private void updateUrlBar(@Nullable String url) {
        if (url == null) {
            return;
        }
        EditText urlField = findViewById(R.id.url_field);
        if (urlField != null) {
            urlField.setText(url, TextView.BufferType.EDITABLE);
        }
    }

    private static boolean isSyntheticBenchmarkUri(@Nullable Uri uri) {
        if (uri == null) {
            return false;
        }
        String scheme = uri.getScheme();
        String host = uri.getHost();
        if (scheme == null || host == null) {
            return false;
        }
        return "https".equalsIgnoreCase(scheme)
                && host.toLowerCase(Locale.US).endsWith(DOMAIN_SUFFIX);
    }

    private static Map<String, String> buildNoStoreHeaders(String contentType) {
        Map<String, String> headers = new HashMap<>();
        headers.put("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        headers.put("Content-Type", contentType);
        return headers;
    }

    private WebResourceResponse buildSyntheticResponse(Uri uri) {
        String path = uri.getPath() != null ? uri.getPath() : "/";
        if ("/favicon.ico".equals(path)) {
            return new WebResourceResponse(
                    "image/x-icon",
                    "UTF-8",
                    204,
                    "No Content",
                    buildNoStoreHeaders("image/x-icon"),
                    new ByteArrayInputStream(new byte[0]));
        }
        String html;
        if ("/start".equals(path)) {
            html =
                    "<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"></head>"
                            + "<body>Starting JS bridge benchmark...</body></html>";
        } else if (path.startsWith("/subframe/")) {
            int step = parseTrailingStep(path, "/subframe/");
            html = buildSubframeHtml(step);
        } else if (path.startsWith("/page/")) {
            int step = parseTrailingStep(path, "/page/");
            html = buildMainPageHtml(step, mWebMessageListenerSupported);
        } else {
            html =
                    "<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"></head>"
                            + "<body>OK</body></html>";
        }
        byte[] utf8 = html.getBytes(StandardCharsets.UTF_8);
        return new WebResourceResponse(
                "text/html",
                "UTF-8",
                200,
                "OK",
                buildNoStoreHeaders("text/html; charset=utf-8"),
                new ByteArrayInputStream(utf8));
    }

    private static int parseTrailingStep(String path, String prefix) {
        try {
            return Math.max(1, Integer.parseInt(path.substring(prefix.length()).trim()));
        } catch (NumberFormatException e) {
            return 1;
        }
    }

    private static String buildSubframeHtml(int step) {
        return "<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"></head><body>"
                + "<script>\n"
                + "(function() {\n"
                + "  const step = "
                + step
                + ";\n"
                + "  const subBuf = new Uint8Array(2 * 1024 * 1024);\n"
                + "  subBuf.fill((step + 97) & 0xff);\n"
                + "  window.__subPayload = subBuf;\n"
                + "  let tok = null;\n"
                + "  if (window.wvBenchJsInterface) {\n"
                + "    tok = window.wvBenchJsInterface.createTransientToken(step);\n"
                + "    window.__subToken = tok;\n"
                + "    window.wvBenchJsInterface.ping(step + tok.getStep());\n"
                + "  }\n"
                + "  if (window.wvBenchMessageListener) {\n"
                + "    window.wvBenchMessageListener.onmessage = function(e) {\n"
                + "      window.__subMsg = subBuf[0] + (tok ? tok.getStep() : 0)"
                + " + String(e.data).length;\n"
                + "    };\n"
                + "    window.wvBenchMessageListener.addEventListener('message', function(e) {\n"
                + "      window.__subEvt = subBuf[subBuf.length - 1] + String(e.data).length;\n"
                + "    });\n"
                + "  }\n"
                + "  parent.postMessage('subframe-ready:' + step, '*');\n"
                + "})();\n"
                + "</script></body></html>";
    }

    private static String buildMainPageHtml(int step, boolean useWebMessageHandshake) {
        return "<!DOCTYPE html><html><head><link rel=\"icon\" href=\"data:,\"></head><body>"
                + "<h3>JS Bridge Step "
                + step
                + "</h3><script>\n"
                + "(function() {\n"
                + "  const step = "
                + step
                + ";\n"
                + "  const useWebMessage = "
                + (useWebMessageHandshake ? "true" : "false")
                + ";\n"
                + "  const mainBuf = new Uint8Array(4 * 1024 * 1024);\n"
                + "  mainBuf.fill(step & 0xff);\n"
                + "  const mainHeap = new Array(131072).fill(step + 0.5);\n"
                + "  window.__benchPayload = mainBuf;\n"
                + "  window.__benchHeap = mainHeap;\n"
                + "  let tok = null;\n"
                + "  if (window.wvBenchJsInterface) {\n"
                + "    tok = window.wvBenchJsInterface.createTransientToken(step);\n"
                + "    window.__benchToken = tok;\n"
                + "    window.wvBenchJsInterface.ping(step + tok.getStep());\n"
                + "  }\n"
                + "  let ackHandled = false;\n"
                + "  if (window.wvBenchMessageListener) {\n"
                + "    window.wvBenchMessageListener.onmessage = function(e) {\n"
                + "      window.__lastOnMessage = mainBuf[0] + mainHeap[0] + (tok ? tok.getStep() :"
                + " 0);\n"
                + "      if (!ackHandled && e.data === 'ack:' + step) {\n"
                + "        ackHandled = true;\n"
                + "        window.wvBenchMessageListener.postMessage('done:' + step);\n"
                + "      }\n"
                + "    };\n"
                + "    window.wvBenchMessageListener.addEventListener('message', function(e) {\n"
                + "      window.__lastEventMessage = mainBuf[mainBuf.length - 1] +"
                + " mainHeap[mainHeap.length - 1] + (tok ? tok.getStep() : 0) +"
                + " String(e.data).length;\n"
                + "    });\n"
                + "  }\n"
                + "  const frame = document.createElement('iframe');\n"
                + "  document.body.appendChild(frame);\n"
                + "  const writeDocOpen = function(sizeBytes, fillVal) {\n"
                + "    if (!frame.contentDocument) return;\n"
                + "    frame.contentDocument.open();\n"
                + "    frame.contentDocument.write('<!DOCTYPE html><script>(function(){'\n"
                + "      + 'const b = new Uint8Array(' + sizeBytes + '); b.fill(' + fillVal +"
                + " ');'\n"
                + "      + 'window.__docOpenPayload = b;'\n"
                + "      + 'if (window.wvBenchJsInterface) {'\n"
                + "      + '  const t = window.wvBenchJsInterface.createTransientToken(' + step +"
                + " '); window.__docOpenTok = t; window.wvBenchJsInterface.ping(t.getStep());'\n"
                + "      + '}'\n"
                + "      + 'if (window.wvBenchMessageListener) {'\n"
                + "      + '  const m = window.wvBenchMessageListener;'\n"
                + "      + '  m.onmessage = function(){return b[0];};'\n"
                + "      + '  m.addEventListener(\"message\",'      + '    function(){return"
                + " b[b.length - 1];});'\n"
                + "      + '}'\n"
                + "      + '})();<\\/script>');\n"
                + "    frame.contentDocument.close();\n"
                + "  };\n"
                + "  window.addEventListener('message', function onSubReady(ev) {\n"
                + "    if (ev.data !== 'subframe-ready:' + step) {\n"
                + "      return;\n"
                + "    }\n"
                + "    window.removeEventListener('message', onSubReady);\n"
                + "    writeDocOpen(1024 * 1024, (step + 79) & 0xff);\n"
                + "    frame.remove();\n"
                + "    if (useWebMessage && window.wvBenchMessageListener) {\n"
                + "      window.wvBenchMessageListener.postMessage('ready:' + step);\n"
                + "      setTimeout(function() {\n"
                + "        if (!ackHandled && window.wvBenchJsInterface) {\n"
                + "          ackHandled = true;\n"
                + "          window.wvBenchJsInterface.notifyFallbackStepDone(step);\n"
                + "        }\n"
                + "      }, 250);\n"
                + "    } else if (window.wvBenchJsInterface) {\n"
                + "      window.wvBenchJsInterface.notifyFallbackStepDone(step);\n"
                + "    }\n"
                + "  });\n"
                + "  setTimeout(function() {\n"
                + "    const initWin = frame.contentWindow;\n"
                + "    if (initWin) {\n"
                + "      const initBuf = new initWin.Uint8Array(6 * 1024 * 1024);\n"
                + "      initBuf.fill((step + 31) & 0xff);\n"
                + "      initWin.__initPayload = initBuf;\n"
                + "      if (initWin.wvBenchJsInterface) {\n"
                + "        const iTok = initWin.wvBenchJsInterface.createTransientToken(step);\n"
                + "        initWin.__initToken = iTok;\n"
                + "        initWin.wvBenchJsInterface.ping(step + iTok.getStep());\n"
                + "      }\n"
                + "      if (initWin.wvBenchMessageListener) {\n"
                + "        initWin.wvBenchMessageListener.onmessage = function() {\n"
                + "          initWin.__m = initBuf[0];\n"
                + "        };\n"
                + "        initWin.wvBenchMessageListener.addEventListener('message', function()"
                + " {\n"
                + "          initWin.__e = initBuf[initBuf.length - 1];\n"
                + "        });\n"
                + "      }\n"
                + "    }\n"
                + "    frame.src = '/subframe/' + step;\n"
                + "  }, 25);\n"
                + "})();\n"
                + "</script></body></html>";
    }
}
