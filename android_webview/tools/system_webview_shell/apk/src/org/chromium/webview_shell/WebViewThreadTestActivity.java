// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.view.WindowManager;
import android.webkit.ConsoleMessage;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewDatabase;

import org.chromium.base.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * This activity is used for running thread tests for
 * WebView.  It creates WebView instances on the UI
 * or a background thread.  It allows tests to interact
 * with the WebView instance.
 */
public class WebViewThreadTestActivity extends Activity {
    private static final String TAG = "WebViewThreadTest";
    private static final int OPERATION_LOAD_DATA = 1;
    private static final int OPERATION_LOAD_URL = 2;
    private static final int OPERATION_REMOVE_VIEW = 3;
    private static final String DATA_KEY = "data";
    private static final String MIME_KEY = "mime";
    private static final String ENCODE_KEY = "encode";
    private static final String URL_KEY = "url";
    private WebView mWebView;
    private Thread mWebViewThread;
    private CountDownLatch mWebviewLatch;
    private CountDownLatch mLoadLatch;
    private Handler mHandler;
    private StringBuilder mStringOutput;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mStringOutput = new StringBuilder();
    }

    private void checkLatch(CountDownLatch latch, boolean isSet) throws IllegalStateException {
        if (isSet) {
            if (latch == null) {
                throw new IllegalStateException("Must have started an operation first");
            }
        } else {
            if (latch != null) {
                throw new IllegalStateException("Must wait for operation to finish first");
            }
        }
    }

    private void checkHandler() throws IllegalStateException {
        if (mHandler == null) {
            throw new IllegalStateException("Must have started webview in non-ui thread");
        }
    }

    /** Create WebView on the main thread, timeout in milliseconds. */
    public boolean createWebViewOnUiThread(long timeout)
            throws IllegalStateException, InterruptedException {
        checkLatch(mWebviewLatch, false);
        if (mWebView != null) {
            throw new IllegalStateException("Webview already created");
        }
        mWebviewLatch = new CountDownLatch(1);

        runOnUiThread(
                () -> {
                    mWebView = new WebView(WebViewThreadTestActivity.this);
                    mWebView.setWebChromeClient(
                            new WebChromeClient() {
                                @Override
                                public boolean onConsoleMessage(ConsoleMessage msg) {
                                    mStringOutput.append(msg.message());
                                    mLoadLatch.countDown();
                                    return true;
                                }
                            });
                    mWebView.getSettings().setJavaScriptEnabled(true);
                    setContentView(mWebView);
                    mWebviewLatch.countDown();
                });
        return waitForWebViewCreated(timeout);
    }

    /** Create WebView on a background thread, timeout in milliseconds. */
    public boolean createWebViewOnNonUiThread(long timeout)
            throws IllegalStateException, InterruptedException {
        checkLatch(mWebviewLatch, false);
        if (mWebView != null) {
            throw new IllegalStateException("Webview already created");
        }
        mWebviewLatch = new CountDownLatch(1);

        mWebViewThread = new Thread(this::webViewThreadMain);
        mWebViewThread.start();
        return waitForWebViewCreated(timeout);
    }

    private void webViewThreadMain() {
        Looper.prepare();
        mHandler =
                new Handler(Looper.myLooper()) {
                    @Override
                    public void handleMessage(Message msg) {
                        switch (msg.what) {
                            case OPERATION_LOAD_DATA:
                                Bundle msgData = msg.getData();
                                mWebView.loadData(
                                        msgData.getString(DATA_KEY),
                                        msgData.getString(MIME_KEY),
                                        msgData.getString(ENCODE_KEY));
                                break;
                            case OPERATION_LOAD_URL:
                                Bundle msgUrl = msg.getData();
                                mWebView.loadUrl(msgUrl.getString(URL_KEY));
                                break;
                            case OPERATION_REMOVE_VIEW:
                                WindowManager wm = (WindowManager) getSystemService(WINDOW_SERVICE);
                                wm.removeViewImmediate(mWebView);
                                mWebView = null;
                                break;
                            default:
                                Log.d(TAG, "Unknown message: " + msg.what);
                                break;
                        }
                    }
                };
        WindowManager wm = (WindowManager) getSystemService(WINDOW_SERVICE);
        mWebView = new WebView(WebViewThreadTestActivity.this);
        mWebView.setWebChromeClient(
                new WebChromeClient() {
                    @Override
                    public boolean onConsoleMessage(ConsoleMessage msg) {
                        mStringOutput.append(msg.message());
                        mLoadLatch.countDown();
                        return true;
                    }
                });
        mWebView.getSettings().setJavaScriptEnabled(true);
        wm.addView(mWebView, new WindowManager.LayoutParams());
        mWebviewLatch.countDown();
        Looper.loop();
    }

    /** Wait for webview to be created, timeout in milliseconds. */
    private boolean waitForWebViewCreated(long timeout)
            throws InterruptedException, IllegalStateException {
        checkLatch(mWebviewLatch, true);
        boolean result = mWebviewLatch.await(timeout, TimeUnit.MILLISECONDS);
        mWebviewLatch = null;
        return result;
    }

    /** Wait for a new console message, timeout in milliseconds. */
    private boolean waitForConsoleMessage(long timeout)
            throws InterruptedException, IllegalStateException {
        checkLatch(mLoadLatch, true);
        boolean result = mLoadLatch.await(timeout, TimeUnit.MILLISECONDS);
        mLoadLatch = null;
        return result;
    }

    /** Get a WebView database. */
    public WebViewDatabase getWebViewDatabase() {
        return WebViewDatabase.getInstance(this);
    }

    /**
     * Call loadData from main thread, timeout in milliseconds to wait for
     * console output.
     */
    public boolean loadDataInUiThread(
            final String data, final String mimeType, final String encoding, long timeout)
            throws IllegalStateException, InterruptedException {
        checkLatch(mLoadLatch, false);
        mLoadLatch = new CountDownLatch(1);
        runOnUiThread(
                new Runnable() {
                    @Override
                    public void run() {
                        mWebView.loadData(data, mimeType, encoding);
                    }
                });
        return waitForConsoleMessage(timeout);
    }

    /**
     * Call loadData from background thread, timeout in milliseconds to wait
     * for console output.
     */
    public boolean loadDataInNonUiThread(
            String data, String mimeType, String encoding, long timeout)
            throws IllegalStateException, InterruptedException {
        checkLatch(mLoadLatch, false);
        checkHandler();
        mLoadLatch = new CountDownLatch(1);
        Bundle bundle = new Bundle();
        bundle.putString(DATA_KEY, data);
        bundle.putString(MIME_KEY, mimeType);
        bundle.putString(ENCODE_KEY, encoding);
        Message msg = new Message();
        msg.what = OPERATION_LOAD_DATA;
        msg.setData(bundle);
        mHandler.sendMessage(msg);
        return waitForConsoleMessage(timeout);
    }

    /**
     * Call loadUrl from background thread, timeout in milliseconds to wait
     * for console output.
     */
    public boolean loadUrlInNonUiThread(String url, long timeout)
            throws IllegalStateException, InterruptedException {
        checkLatch(mLoadLatch, false);
        checkHandler();
        mLoadLatch = new CountDownLatch(1);
        Bundle bundle = new Bundle();
        bundle.putString(URL_KEY, url);
        Message msg = new Message();
        msg.what = OPERATION_LOAD_URL;
        msg.setData(bundle);
        mHandler.sendMessage(msg);
        return waitForConsoleMessage(timeout);
    }

    @Override
    protected void onDestroy() {
        if (mWebViewThread != null) {
            Message msg = new Message();
            msg.what = OPERATION_REMOVE_VIEW;
            mHandler.sendMessage(msg);
            mHandler.getLooper().quitSafely();
            try {
                mWebViewThread.join();
            } catch (Exception e) {
                Log.d(TAG, "While joining mWebViewThread: " + e);
            }
        }
        super.onDestroy();
    }
}
