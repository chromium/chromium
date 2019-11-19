// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import com.google.common.collect.BiMap;
import com.google.common.collect.HashBiMap;

import org.chromium.base.Log;

/**
 * A main activity to handle WPT requests.
 *
 * This is currently implemented to support minimum viable implementation such that
 * multi-window and JavaScript are enabled by default.
 * Note: multi-window support in this shell is implemented, but due to a bug
 *       in WebView (https://crbug.com/100272), it does not work properly unless a delay is given.
 */
public class WebPlatformTestsActivity extends Activity {
    private static final String TAG = "WPTActivity";
    private static final boolean DEBUG = false;

    /**
     * A callback for testing.
     */
    @VisibleForTesting
    public interface TestCallback {
        /** Called after child layout is added. */
        void onChildLayoutAdded(WebView webView);
        /** Called after child layout is removed. */
        void onChildLayoutRemoved();
    }

    private BiMap<ViewGroup, WebView> mLayoutToWebViewBiMap = HashBiMap.create();

    private LayoutInflater mLayoutInflater;
    private RelativeLayout mRootLayout;
    private WebView mWebView;
    private TestCallback mTestCallback;

    private class MultiWindowWebChromeClient extends WebChromeClient {
        @Override
        public boolean onCreateWindow(
                WebView parentWebView, boolean isDialog, boolean isUserGesture, Message resultMsg) {
            if (DEBUG) Log.i(TAG, "onCreateWindow");
            WebView childWebView = createChildLayoutAndGetNewWebView(parentWebView);
            WebSettings settings = childWebView.getSettings();
            setUpWebSettings(settings);
            childWebView.setWebViewClient(new WebViewClient() {
                @Override
                public void onPageFinished(WebView childWebView, String url) {
                    if (DEBUG) Log.i(TAG, "onPageFinished");
                    // Once the view has loaded, display its title for debugging.
                    ViewGroup childLayout = mLayoutToWebViewBiMap.inverse().get(childWebView);
                    TextView childTitleText = childLayout.findViewById(R.id.childTitleText);
                    childTitleText.setText(childWebView.getTitle());
                }
            });
            childWebView.setWebChromeClient(new MultiWindowWebChromeClient());
            // Tell the transport about the new view
            WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
            transport.setWebView(childWebView);
            resultMsg.sendToTarget();
            if (mTestCallback != null) mTestCallback.onChildLayoutAdded(childWebView);
            return true;
        }

        @Override
        public void onCloseWindow(WebView webView) {
            ViewGroup childLayout = mLayoutToWebViewBiMap.inverse().get(webView);
            if (childLayout == mRootLayout) {
                Log.w(TAG, "Ignoring onCloseWindow() on the top-level webview.");
            } else {
                closeChild(childLayout);
            }
        }
    }

    /** Remove and destroy a webview if it exists. */
    private void removeAndDestroyWebView(WebView webView) {
        if (webView == null) return;
        ViewGroup parent = (ViewGroup) webView.getParent();
        if (parent != null) parent.removeView(webView);
        webView.destroy();
    }

    private String getUrlFromIntent() {
        if (getIntent() == null) return null;
        return getIntent().getDataString();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        WebView.setWebContentsDebuggingEnabled(true);
        mLayoutInflater = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        setContentView(R.layout.activity_web_platform_tests);
        mRootLayout = findViewById(R.id.rootLayout);
        mWebView = mRootLayout.findViewById(R.id.rootWebView);
        mLayoutToWebViewBiMap.put(mRootLayout, mWebView);

        String url = getUrlFromIntent();
        if (url == null) {
            // This is equivalent to Chrome's WPT setup.
            setUpMainWebView("about:blank");
        } else {
            Log.w(TAG,
                    "Handling a non-empty intent. This should only be used for testing. URL: "
                            + url);
            setUpMainWebView(url);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        removeAndDestroyWebView(mWebView);
        mWebView = null;
    }

    private WebView createChildLayoutAndGetNewWebView(WebView parentWebView) {
        // Add all the child layouts to the root layout such that we can remove
        // a child layout without affecting any grand child layout.
        final ViewGroup parentLayout = mRootLayout;
        // Provide parent such that MATCH_PARENT layout params can work. Ignore the return value
        // which is parentLayout.
        mLayoutInflater.inflate(R.layout.activity_web_platform_tests_child, parentLayout);
        // Choose what has just been added.
        LinearLayout childLayout =
                (LinearLayout) parentLayout.getChildAt(parentLayout.getChildCount() - 1);
        Button childCloseButton = childLayout.findViewById(R.id.childCloseButton);
        childCloseButton.setOnClickListener((View v) -> { closeChild(childLayout); });
        WebView childWebView = childLayout.findViewById(R.id.childWebView);
        mLayoutToWebViewBiMap.put(childLayout, childWebView);
        return childWebView;
    }

    private void setUpWebSettings(WebSettings settings) {
        // Required by WPT.
        settings.setJavaScriptEnabled(true);
        // Enable multi-window.
        settings.setJavaScriptCanOpenWindowsAutomatically(true);
        settings.setSupportMultipleWindows(true);
        // Respect "viewport" HTML meta tag. This is false by default, but set to false to be clear.
        settings.setUseWideViewPort(false);
        settings.setDomStorageEnabled(true);
    }

    private void setUpMainWebView(String url) {
        setUpWebSettings(mWebView.getSettings());
        mWebView.setWebChromeClient(new MultiWindowWebChromeClient());
        mWebView.loadUrl(url);
    }

    private void closeChild(ViewGroup childLayout) {
        if (DEBUG) Log.i(TAG, "closeChild");
        ViewGroup parent = (ViewGroup) childLayout.getParent();
        if (parent != null) parent.removeView(childLayout);
        WebView childWebView = mLayoutToWebViewBiMap.get(childLayout);
        removeAndDestroyWebView(childWebView);
        mLayoutToWebViewBiMap.remove(childLayout);
        if (mTestCallback != null) mTestCallback.onChildLayoutRemoved();
    }

    @VisibleForTesting
    public void setTestCallback(TestCallback testDelegate) {
        mTestCallback = testDelegate;
    }

    @VisibleForTesting
    public WebView getTestRunnerWebView() {
        return mWebView;
    }
}
