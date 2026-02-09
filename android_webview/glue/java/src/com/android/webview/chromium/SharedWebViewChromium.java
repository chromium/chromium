// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Bundle;
import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;

import com.android.webview.chromium.WebViewChromiumAwInit.CallSite;

import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.ScriptHandler;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.js_injection.mojom.DocumentInjectionTime;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class contains the parts of WebViewChromium that should be shared between the webkit-glue
 * layer and the support library glue layer.
 */
public class SharedWebViewChromium {
    private final WebViewChromiumRunQueue mRunQueue;
    private final WebViewChromiumAwInit mAwInit;
    // If set to false, WebViewBuilder configuration may no longer be applied (or, more strictly,
    // cannot begin applying). Non-View method WebView instance APIs (including methods that accept
    // a WebView instance as an argument) will set this to false.
    private final AtomicBoolean mBuilderConfigurationAllowed = new AtomicBoolean(true);
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
        mAwInit.triggerAndWaitForChromiumStarted(CallSite.WEBVIEW_INSTANCE_GET_RENDER_PROCESS);
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

    // Forbids later attempts to begin applying builder configuration on the WebView instance.
    public void forbidBuilderConfiguration() {
        mBuilderConfigurationAllowed.set(false);
    }

    // Returns true iff builder configuration is still permitted, and forbid any subsequent builder
    // configuration.
    public boolean commitToBuilderConfiguration() {
        return mBuilderConfigurationAllowed.getAndSet(false);
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
        mAwInit.triggerAndWaitForChromiumStarted(
                CallSite.WEBVIEW_INSTANCE_CREATE_WEBMESSAGE_CHANNEL);
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

    public void addWebMessageListener(
            final String jsObjectName,
            final String[] allowedOriginRules,
            final WebMessageListener listener,
            final String worldName) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(
                    () ->
                            addWebMessageListener(
                                    jsObjectName, allowedOriginRules, listener, worldName));
            return;
        }
        mAwContents.addWebMessageListener(jsObjectName, allowedOriginRules, listener, worldName);
    }

    public void removeWebMessageListener(final String jsObjectName) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(() -> removeWebMessageListener(jsObjectName));
            return;
        }
        mAwContents.removeWebMessageListener(jsObjectName);
    }

    public void removeWebMessageListener(final String jsObjectName, final String world) {
        if (checkNeedsPost()) {
            mRunQueue.addTask(() -> removeWebMessageListener(jsObjectName, world));
            return;
        }
        mAwContents.removeWebMessageListener(jsObjectName, world);
    }

    public ScriptHandler addDocumentStartJavaScript(
            final String script, final String[] allowedOriginRules) {
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(
                    () -> addDocumentStartJavaScript(script, allowedOriginRules));
        }
        return mAwContents.addDocumentStartJavaScript(script, allowedOriginRules);
    }

    public ScriptHandler addJavaScriptOnEvent(
            final String script,
            final @DocumentInjectionTime.EnumType int event,
            final String[] allowedOriginRules,
            final String world) {
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(
                    () -> addJavaScriptOnEvent(script, event, allowedOriginRules, world));
        }
        return mAwContents.addJavaScriptOnEvent(script, event, allowedOriginRules, world);
    }

    public int getJavaScriptWorld(final String name) {
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(() -> getJavaScriptWorld(name));
        }
        return mAwContents.registerJavaScriptWorld(name);
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
        mAwInit.triggerAndWaitForChromiumStarted(
                CallSite.WEBVIEW_INSTANCE_GET_WEBVIEW_RENDERER_CLIENT_ADAPTER);
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
        return mAwInit.getProfileStore().getProfile(profileName);
    }

    protected boolean checkNeedsPost() {
        RecordHistogram.recordBooleanHistogram(
                "Android.WebView.Startup.CheckNeedsPost.IsChromiumInitialized",
                mAwInit.isChromiumInitialized());
        boolean needsPost = !mAwInit.isChromiumInitialized() || !ThreadUtils.runningOnUiThread();
        if (!needsPost && mAwContents == null) {
            throw new IllegalStateException("AwContents must be created if we are not posting!");
        }
        if (mAwInit.isChromiumInitialized()) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.Startup.CheckNeedsPost.CalledOnUiThread",
                    ThreadUtils.runningOnUiThread());
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
