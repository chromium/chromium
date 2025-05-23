// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Bundle;
import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;

import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.ScriptHandler;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

/**
 * This class contains the parts of WebViewChromium that should be shared between the webkit-glue
 * layer and the support library glue layer.
 */
public class SharedWebViewChromium {
    private final WebViewChromiumRunQueue mRunQueue;
    private final WebViewChromiumAwInit mAwInit;
    // The WebView wrapper for WebContents and required browser components.
    private AwContents mAwContents;

    private SharedWebViewContentsClientAdapter mContentsClientAdapter;

    // Default WebViewClient used to avoid null checks.
    static final WebViewClient sNullWebViewClient = new WebViewClient();
    // The WebViewClient instance that was passed to WebView.setWebViewClient().
    private WebViewClient mWebViewClient = sNullWebViewClient;
    private WebChromeClient mWebChromeClient;

    public SharedWebViewChromium(WebViewChromiumRunQueue runQueue, WebViewChromiumAwInit awInit) {
        mRunQueue = runQueue;
        mAwInit = awInit;
    }

    void setWebViewClient(WebViewClient client) {
        mWebViewClient = client != null ? client : sNullWebViewClient;
    }

    public WebViewClient getWebViewClient() {
        return mWebViewClient;
    }

    void setWebChromeClient(WebChromeClient client) {
        mWebChromeClient = client;
    }

    public WebChromeClient getWebChromeClient() {
        return mWebChromeClient;
    }

    public AwRenderProcess getRenderProcess() {
        mAwInit.startYourEngines(true);
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(() -> getRenderProcess());
        }
        return mAwContents.getRenderProcess();
    }

    public void init(SharedWebViewContentsClientAdapter contentsClientAdapter) {
        mContentsClientAdapter = contentsClientAdapter;
    }

    public void initForReal(AwContents awContents) {
        assert ThreadUtils.runningOnUiThread();

        if (mAwContents != null) {
            throw new RuntimeException(
                    "Cannot create multiple AwContents for the same SharedWebViewChromium");
        }
        mAwContents = awContents;
    }

    public void insertVisualStateCallback(long requestId, AwContents.VisualStateCallback callback) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            insertVisualStateCallback(requestId, callback);
                        }
                    });
            return;
        }
        mAwContents.insertVisualStateCallback(requestId, callback);
    }

    public MessagePort[] createWebMessageChannel() {
        mAwInit.startYourEngines(true);
        if (checkNeedsPost()) {
            MessagePort[] ret =
                    mRunQueue.runOnUiThreadBlocking(
                            new Callable<MessagePort[]>() {
                                @Override
                                public MessagePort[] call() {
                                    return createWebMessageChannel();
                                }
                            });
            return ret;
        }
        return mAwContents.createMessageChannel();
    }

    public void postMessageToMainFrame(
            final MessagePayload messagePayload,
            final String targetOrigin,
            final MessagePort[] sentPorts) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            postMessageToMainFrame(messagePayload, targetOrigin, sentPorts);
                        }
                    });
            return;
        }
        mAwContents.postMessageToMainFrame(messagePayload, targetOrigin, sentPorts);
    }

    public void addWebMessageListener(
            final String jsObjectName,
            final String[] allowedOriginRules,
            final WebMessageListener listener) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(
                    () -> addWebMessageListener(jsObjectName, allowedOriginRules, listener));
            return;
        }
        mAwContents.addWebMessageListener(jsObjectName, allowedOriginRules, listener);
    }

    public void removeWebMessageListener(final String jsObjectName) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(() -> removeWebMessageListener(jsObjectName));
            return;
        }
        mAwContents.removeWebMessageListener(jsObjectName);
    }

    public ScriptHandler addDocumentStartJavaScript(
            final String script, final String[] allowedOriginRules) {
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(
                    () -> addDocumentStartJavaScript(script, allowedOriginRules));
        }
        return mAwContents.addDocumentStartJavaScript(script, allowedOriginRules);
    }

    public void setWebViewRendererClientAdapter(
            SharedWebViewRendererClientAdapter webViewRendererClientAdapter) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(
                    new Runnable() {
                        @Override
                        public void run() {
                            setWebViewRendererClientAdapter(webViewRendererClientAdapter);
                        }
                    });
            return;
        }
        mContentsClientAdapter.setWebViewRendererClientAdapter(webViewRendererClientAdapter);
    }

    public SharedWebViewRendererClientAdapter getWebViewRendererClientAdapter() {
        mAwInit.startYourEngines(true);
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(
                    new Callable<SharedWebViewRendererClientAdapter>() {
                        @Override
                        public SharedWebViewRendererClientAdapter call() {
                            return getWebViewRendererClientAdapter();
                        }
                    });
        }
        return mContentsClientAdapter.getWebViewRendererClientAdapter();
    }

    public void setProfile(String profileName) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(() -> setProfile(profileName));
            return;
        }
        mAwContents.setBrowserContextForPublicApi(
                AwBrowserContextStore.getNamedContext(profileName, true));
    }

    public Profile getProfile() {
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(this::getProfile);
        }
        String profileName = mAwContents.getBrowserContextForPublicApi().getName();
        return ProfileStore.getInstance().getProfile(profileName);
    }

    protected boolean checkNeedsPost() {
        boolean needsPost = !mAwInit.isChromiumInitialized() || !ThreadUtils.runningOnUiThread();
        if (!needsPost && mAwContents == null) {
            throw new IllegalStateException("AwContents must be created if we are not posting!");
        }
        return needsPost;
    }

    public AwContents getAwContents() {
        return mAwContents;
    }

    public void saveState(Bundle outState, int maxSize, boolean includeForwardState) {
        if (checkNeedsPost()) {
            mRunQueue.runVoidTaskOnUiThreadBlocking(() -> {
                saveState(outState, maxSize, includeForwardState);
            });
            return;
        }

        mAwContents.saveState(outState, maxSize, includeForwardState);
    }

    public List<String> addJavascriptInterfaces(
            List<Object> objects, List<String> names, List<List<String>> originPatterns) {
        // This is called specifically from the WebViewBuilder API which always builds
        // and configures on the UI thread specifically. If we are not on the UI thread,
        // this is an issue and should be reported back.
        // Executing on the UI thread means we can return our validation results
        // synchronously.
        if (!ThreadUtils.runningOnUiThread()) {
            throw new IllegalStateException("WebView must be configured on of UI Thread");
        }
        assert objects.size() == names.size() && names.size() == originPatterns.size();

        // TODO: Add support to JS injection code in content to handle bulk push of
        // patterns. JNIZero currently doesn't support multi dimensional arrays so
        // List<List<String>> is a problem.
        List<String> badPatterns = new ArrayList<>();

        for (int i = 0; i < objects.size(); i++) {
            badPatterns.addAll(
                    mAwContents.addJavascriptInterface(
                            objects.get(i), names.get(i), originPatterns.get(i)));
        }
        return badPatterns;
    }
}
