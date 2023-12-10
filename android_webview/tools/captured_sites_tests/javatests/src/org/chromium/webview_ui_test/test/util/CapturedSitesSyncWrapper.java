// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_ui_test.test.util;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import android.os.Looper;
import android.webkit.ConsoleMessage;
import android.webkit.JavascriptInterface;
import android.webkit.JsResult;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * CapturedSitesSyncWrapper is a WebView wrapper to store the lock and condition provided by the
 * tests and ensure webview signal the tests once page load and javascript load is over.
 */
public class CapturedSitesSyncWrapper {
    // TODO (b/1470289) make this class inherit from WebViewSyncWrapper.
    private static final String JS_BRIDGE = "WEBVIEW_JS_BRIDGE";
    private static final String TAG = "CapturedSitesSyncWrapper";

    private final WebView mWebView;

    private CallbackHelper mPageCallback = new CallbackHelper();
    private CallbackHelper mJsCallback = new CallbackHelper();

    private List<ConsoleMessage> mErrorMessageList =
            Collections.synchronizedList(new ArrayList<ConsoleMessage>());

    public CapturedSitesSyncWrapper(WebView wv) {
        mWebView = wv;
        init();
    }

    /**
     * SyncJavaScriptBridge instance will be used by WebView to allow javascript in WebView can
     * make callbacks to signal that JavaScript is finished loading.
     * <p/>
     * For example, if a test has a long async javascript snippet that needed to be loaded before
     * the test proceed like below.
     * <p/>
     * <pre>
     * <code>
     * window.setTimeout(function() {
     *     document.findElementById("button");
     *     window.WEBVIEW_JS_BRIDGE.onJavaScriptFinished();
     *     }, 10000)
     * };
     * </code>
     * </pre>
     * <p/>
     * Despite that the javascript above takes 10 seconds to finish loading, the test will still
     * wait until javascript is finished loading
     */
    private class SyncJavaScriptBridge {
        @JavascriptInterface
        public void onJavaScriptFinished() {
            mJsCallback.notifyCalled();
        }
    }

    /** A custom WebViewClient that signals tests when onPageStarted and onPageFinished is called */
    private class SyncWebViewClient extends WebViewClient {
        @Override
        public void onPageFinished(WebView view, String url) {
            mPageCallback.notifyCalled();
            super.onPageFinished(view, url);
        }
    }

    private void init() {
        runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        mWebView.getSettings().setJavaScriptEnabled(true);
                        mWebView.setWebViewClient(
                                CapturedSitesSyncWrapper.this.new SyncWebViewClient());
                        mWebView.setWebChromeClient(
                                new WebChromeClient() {
                                    @Override
                                    public boolean onConsoleMessage(ConsoleMessage consoleMessage) {
                                        if (consoleMessage.messageLevel()
                                                == ConsoleMessage.MessageLevel.ERROR) {
                                            mErrorMessageList.add(consoleMessage);
                                        }
                                        return super.onConsoleMessage(consoleMessage);
                                    }

                                    @Override
                                    public boolean onJsAlert(
                                            WebView view,
                                            String url,
                                            String message,
                                            JsResult result) {
                                        mJsCallback.notifyCalled();
                                        return super.onJsAlert(view, url, message, result);
                                    }
                                });
                        mWebView.addJavascriptInterface(
                                CapturedSitesSyncWrapper.this.new SyncJavaScriptBridge(),
                                JS_BRIDGE);
                    }
                });
    }

    private static void runOnUiThread(final Runnable runnable) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new RuntimeException(
                    "Actions in CapturedSitesSyncWrapper is not allowed to be run on "
                            + "UI thread");
        } else {
            getInstrumentation().runOnMainSync(runnable);
        }
    }

    public void loadUrlSync(final String url) {
        mErrorMessageList.clear();
        runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        mWebView.getSettings().setAllowFileAccess(true);
                        mWebView.loadUrl(url);
                    }
                });
    }
}
