// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.Nullable;

import com.android.webview.chromium.CallbackConverter;
import com.android.webview.chromium.SharedWebViewChromium;
import com.android.webview.chromium.SharedWebViewRendererClientAdapter;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.SpeculativeLoadingParametersBoundaryInterface;
import org.chromium.support_lib_boundary.VisualStateCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationHandler;
import java.util.concurrent.Executor;

/**
 * Support library glue version of WebViewChromium.
 *
 * <p>An instance of this class is created when a WebViewCompat method is called with a WebView
 * instance. WebViewCompat may hold on the new instance until the corresponding WebView is GCed.
 *
 * <p>Do not store state here.
 */
@Lifetime.Temporary
class SupportLibWebViewChromium implements WebViewProviderBoundaryInterface {
    // Use weak references to ensure that caching this object on the client side doesn’t prevent the
    // WebView from being garbage collected.
    private final WeakReference<WebView> mWebView;
    private final WeakReference<SharedWebViewChromium> mSharedWebViewChromium;

    public SupportLibWebViewChromium(WebView webView) {
        mWebView = new WeakReference<>(webView);
        mSharedWebViewChromium =
                new WeakReference<>(WebkitToSharedGlueConverter.getSharedWebViewChromium(webView));
    }

    @Override
    public void insertVisualStateCallback(long requestId, InvocationHandler callbackInvoHandler) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.INSERT_VISUAL_STATE_CALLBACK")) {
            recordApiCall(ApiCall.INSERT_VISUAL_STATE_CALLBACK);
            final VisualStateCallbackBoundaryInterface visualStateCallback =
                    BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                            VisualStateCallbackBoundaryInterface.class, callbackInvoHandler);

            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.insertVisualStateCallback(
                    requestId,
                    new AwContents.VisualStateCallback() {
                        @Override
                        public void onComplete(long requestId) {
                            visualStateCallback.onComplete(requestId);
                        }
                    });
        }
    }

    @Override
    public /* WebMessagePort */ InvocationHandler[] createWebMessageChannel() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.CREATE_WEB_MESSAGE_CHANNEL")) {
            recordApiCall(ApiCall.CREATE_WEB_MESSAGE_CHANNEL);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return SupportLibWebMessagePortAdapter.fromMessagePorts(
                    sharedWebViewChromium.createWebMessageChannel());
        }
    }

    @Override
    public void postMessageToMainFrame(
            /* WebMessage */ InvocationHandler message, Uri targetOrigin) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.POST_MESSAGE_TO_MAIN_FRAME")) {
            recordApiCall(ApiCall.POST_MESSAGE_TO_MAIN_FRAME);
            WebMessageBoundaryInterface messageBoundaryInterface =
                    BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                            WebMessageBoundaryInterface.class, message);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.postMessageToMainFrame(
                    SupportLibWebMessagePayloadAdapter.fromWebMessageBoundaryInterface(
                            messageBoundaryInterface),
                    targetOrigin.toString(),
                    SupportLibWebMessagePortAdapter.toMessagePorts(
                            messageBoundaryInterface.getPorts()));
        }
    }

    @Override
    public void addWebMessageListener(
            String jsObjectName,
            String[] allowedOriginRules,
            /* WebMessageListener */ InvocationHandler listener) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.ADD_WEB_MESSAGE_LISTENER")) {
            recordApiCall(ApiCall.ADD_WEB_MESSAGE_LISTENER);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            WebView webView = mWebView.get();
            if (sharedWebViewChromium == null || webView == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.addWebMessageListener(
                    jsObjectName,
                    allowedOriginRules,
                    new SupportLibWebMessageListenerAdapter(webView, listener));
        }
    }

    @Override
    public void removeWebMessageListener(final String jsObjectName) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.REMOVE_WEB_MESSAGE_LISTENER")) {
            recordApiCall(ApiCall.REMOVE_WEB_MESSAGE_LISTENER);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.removeWebMessageListener(jsObjectName);
        }
    }

    @Override
    public /* ScriptHandler */ InvocationHandler addDocumentStartJavaScript(
            final String script, final String[] allowedOriginRules) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.ADD_DOCUMENT_START_SCRIPT")) {
            recordApiCall(ApiCall.ADD_DOCUMENT_START_SCRIPT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibScriptHandlerAdapter(
                            sharedWebViewChromium.addDocumentStartJavaScript(
                                    script, allowedOriginRules)));
        }
    }

    @Override
    public WebViewClient getWebViewClient() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBVIEW_CLIENT")) {
            recordApiCall(ApiCall.GET_WEBVIEW_CLIENT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return sharedWebViewChromium.getWebViewClient();
        }
    }

    @Override
    public WebChromeClient getWebChromeClient() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBCHROME_CLIENT")) {
            recordApiCall(ApiCall.GET_WEBCHROME_CLIENT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return sharedWebViewChromium.getWebChromeClient();
        }
    }

    @Override
    public /* WebViewRenderer */ InvocationHandler getWebViewRenderer() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBVIEW_RENDERER")) {
            recordApiCall(ApiCall.GET_WEBVIEW_RENDERER);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibWebViewRendererAdapter(sharedWebViewChromium.getRenderProcess()));
        }
    }

    @Override
    public /* WebViewRendererClient */ InvocationHandler getWebViewRendererClient() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBVIEW_RENDERER_CLIENT")) {
            recordApiCall(ApiCall.GET_WEBVIEW_RENDERER_CLIENT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            SharedWebViewRendererClientAdapter webViewRendererClientAdapter =
                    sharedWebViewChromium.getWebViewRendererClientAdapter();
            return webViewRendererClientAdapter != null
                    ? webViewRendererClientAdapter.getSupportLibInvocationHandler()
                    : null;
        }
    }

    @Override
    public void setWebViewRendererClient(
            /* WebViewRendererClient */ InvocationHandler webViewRendererClient) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.SET_WEBVIEW_RENDERER_CLIENT")) {
            recordApiCall(ApiCall.SET_WEBVIEW_RENDERER_CLIENT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.setWebViewRendererClientAdapter(
                    webViewRendererClient != null
                            ? new SupportLibWebViewRendererClientAdapter(webViewRendererClient)
                            : null);
        }
    }

    @Override
    public void setProfile(String profileName) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.SET_WEBVIEW_PROFILE")) {
            recordApiCall(ApiCall.SET_WEBVIEW_PROFILE);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.setProfile(profileName);
        }
    }

    @Override
    public /* Profile */ InvocationHandler getProfile() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBVIEW_PROFILE")) {
            recordApiCall(ApiCall.GET_WEBVIEW_PROFILE);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                    new SupportLibProfile(sharedWebViewChromium.getProfile()));
        }
    }

    @Override
    public void setAudioMuted(boolean muted) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.SET_AUDIO_MUTED")) {
            recordApiCall(ApiCall.SET_AUDIO_MUTED);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.getAwContents().setAudioMuted(muted);
        }
    }

    @Override
    public boolean isAudioMuted() {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.IS_AUDIO_MUTED")) {
            recordApiCall(ApiCall.IS_AUDIO_MUTED);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            return sharedWebViewChromium.getAwContents().isAudioMuted();
        }
    }

    @Override
    public void prerenderUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            ValueCallback<Void> activationCallback,
            ValueCallback<Throwable> errorCallback) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.PRERENDER_URL")) {
            recordApiCall(ApiCall.PRERENDER_URL);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium
                    .getAwContents()
                    .startPrerendering(
                            url,
                            null,
                            cancellationSignal,
                            callbackExecutor,
                            CallbackConverter.fromValueCallback(activationCallback),
                            CallbackConverter.fromValueCallback(errorCallback));
        } catch (Exception e) {
            callbackExecutor.execute(() -> errorCallback.onReceiveValue(e));
        }
    }

    @Override
    public void prerenderUrl(
            String url,
            @Nullable CancellationSignal cancellationSignal,
            Executor callbackExecutor,
            /* SpeculativeLoadingParameters */ InvocationHandler speculativeLoadingParameters,
            ValueCallback<Void> activationCallback,
            ValueCallback<Throwable> errorCallback) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.PRERENDER_URL_WITH_PARAMS")) {
            recordApiCall(ApiCall.PRERENDER_URL_WITH_PARAMS);
            SpeculativeLoadingParametersBoundaryInterface
                    speculativeLoadingParametersBoundaryInterface =
                            BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                                    SpeculativeLoadingParametersBoundaryInterface.class,
                                    speculativeLoadingParameters);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium
                    .getAwContents()
                    .startPrerendering(
                            url,
                            SupportLibSpeculativeLoadingParametersAdapter
                                    .fromSpeculativeLoadingParametersBoundaryInterface(
                                            speculativeLoadingParametersBoundaryInterface)
                                    .toAwPrefetchParams(),
                            cancellationSignal,
                            callbackExecutor,
                            CallbackConverter.fromValueCallback(activationCallback),
                            CallbackConverter.fromValueCallback(errorCallback));
        } catch (Exception e) {
            callbackExecutor.execute(() -> errorCallback.onReceiveValue(e));
        }
    }

    @Override
    public void saveState(Bundle outState, int maxSize, boolean includeForwardState) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.AndroidX.SAVE_STATE")) {
            recordApiCall(ApiCall.SAVE_STATE);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }
            sharedWebViewChromium.saveState(outState, maxSize, includeForwardState);
        }
    }

    @Override
    public void addWebViewNavigationListener(
            Executor executor, /* WebViewNavigationListener */ InvocationHandler listener) {
        assert ThreadUtils.runningOnUiThread();
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.ADD_WEBVIEW_NAVIGATION_LISTENER")) {
            recordApiCall(ApiCall.ADD_NAVIGATION_LISTENER);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }

            if (executor == null || listener == null) {
                throw new NullPointerException(
                        "Executor and WebNavigationListener shouldn't be null");
            }

            // SupportLibWebViewNavigationListenerAdapter implements equals by delegating to the
            // invocation handler which delegates to the wrapped object.
            boolean added =
                    sharedWebViewChromium
                            .getAwContents()
                            .getNavigationClient()
                            .addListener(
                                    new SupportLibWebViewNavigationListenerAdapter(
                                            listener, executor));
            if (!added) {
                throw new IllegalStateException(
                        "The NavigationListener has already been added to this WebView instance.");
            }
        }
    }

    @Override
    public void removeWebViewNavigationListener(
            /* WebViewNavigationListener */ InvocationHandler listener) {
        assert ThreadUtils.runningOnUiThread();
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.REMOVE_WEBVIEW_NAVIGATION_LISTENER")) {
            recordApiCall(ApiCall.REMOVE_NAVIGATION_LISTENER);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }

            // Construct a SupportLibWebViewNavigationListenerAdapter that `equals` any existing
            // one. This is possible since `equals` doesn't take the executor into account.
            sharedWebViewChromium
                    .getAwContents()
                    .getNavigationClient()
                    .removeListener(
                            new SupportLibWebViewNavigationListenerAdapter(
                                    listener, Runnable::run));
        }
    }

    @Override
    public /* WebViewNavigationClient */ InvocationHandler getWebViewNavigationClient() {
        assert ThreadUtils.runningOnUiThread();
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.GET_WEBVIEW_NAVIGATION_CLIENT")) {
            recordApiCall(ApiCall.GET_WEBVIEW_NAVIGATION_CLIENT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }

            if (sharedWebViewChromium.getAwContents().getNavigationClient().getFirstListener()
                    instanceof SupportLibWebViewNavigationClientAdapter navigationClient) {
                return navigationClient.getSupportLibInvocationHandler();
            }

            return null;
        }
    }

    @Override
    public void setWebViewNavigationClient(
            /* WebViewNavigationClient */ InvocationHandler webViewNavigationClient) {
        assert ThreadUtils.runningOnUiThread();
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.SET_WEBVIEW_NAVIGATION_CLIENT")) {
            recordApiCall(ApiCall.SET_WEBVIEW_NAVIGATION_CLIENT);
            SharedWebViewChromium sharedWebViewChromium = mSharedWebViewChromium.get();
            if (sharedWebViewChromium == null) {
                throw new IllegalStateException(
                        "Support lib method called on WebView that no longer exists.");
            }

            if (webViewNavigationClient == null) {
                throw new NullPointerException("WebViewNavigationClient shouldn't be null");
            }

            sharedWebViewChromium
                    .getAwContents()
                    .getNavigationClient()
                    .clearAndSetListener(
                            new SupportLibWebViewNavigationClientAdapter(webViewNavigationClient));
        }
    }
}
