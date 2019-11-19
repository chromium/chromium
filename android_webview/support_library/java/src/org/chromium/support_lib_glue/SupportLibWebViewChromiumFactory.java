// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.net.Uri;
import android.webkit.ValueCallback;
import android.webkit.WebView;

import com.android.webview.chromium.CallbackConverter;
import com.android.webview.chromium.SharedStatics;
import com.android.webview.chromium.SharedTracingControllerAdapter;
import com.android.webview.chromium.WebViewChromiumAwInit;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.android_webview.AwDebug;
import org.chromium.support_lib_boundary.StaticsBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderFactoryBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.util.List;

/**
 * Support library glue version of WebViewChromiumFactoryProvider.
 */
class SupportLibWebViewChromiumFactory implements WebViewProviderFactoryBoundaryInterface {
    // SupportLibWebkitToCompatConverterAdapter
    private final InvocationHandler mCompatConverterAdapter;
    private final WebViewChromiumAwInit mAwInit;
    // clang-format off
    private final String[] mWebViewSupportedFeatures =
            new String[] {
                    Features.VISUAL_STATE_CALLBACK,
                    Features.OFF_SCREEN_PRERASTER,
                    Features.SAFE_BROWSING_ENABLE,
                    Features.DISABLED_ACTION_MODE_MENU_ITEMS,
                    Features.START_SAFE_BROWSING,
                    Features.SAFE_BROWSING_WHITELIST,
                    Features.SAFE_BROWSING_PRIVACY_POLICY_URL,
                    Features.SERVICE_WORKER_BASIC_USAGE,
                    Features.SERVICE_WORKER_CACHE_MODE,
                    Features.SERVICE_WORKER_CONTENT_ACCESS,
                    Features.SERVICE_WORKER_FILE_ACCESS,
                    Features.SERVICE_WORKER_BLOCK_NETWORK_LOADS,
                    Features.SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST,
                    Features.RECEIVE_WEB_RESOURCE_ERROR,
                    Features.RECEIVE_HTTP_ERROR,
                    Features.SAFE_BROWSING_HIT,
                    Features.SHOULD_OVERRIDE_WITH_REDIRECTS,
                    Features.WEB_RESOURCE_REQUEST_IS_REDIRECT,
                    Features.WEB_RESOURCE_ERROR_GET_DESCRIPTION,
                    Features.WEB_RESOURCE_ERROR_GET_CODE,
                    Features.SAFE_BROWSING_RESPONSE_BACK_TO_SAFETY,
                    Features.SAFE_BROWSING_RESPONSE_PROCEED,
                    Features.SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL,
                    Features.WEB_MESSAGE_PORT_POST_MESSAGE,
                    Features.WEB_MESSAGE_PORT_CLOSE,
                    Features.WEB_MESSAGE_PORT_SET_MESSAGE_CALLBACK,
                    Features.CREATE_WEB_MESSAGE_CHANNEL,
                    Features.POST_WEB_MESSAGE,
                    Features.WEB_MESSAGE_CALLBACK_ON_MESSAGE,
                    Features.GET_WEB_VIEW_CLIENT,
                    Features.GET_WEB_CHROME_CLIENT,
                    Features.PROXY_OVERRIDE,
                    Features.SUPPRESS_ERROR_PAGE + Features.DEV_SUFFIX,
                    Features.GET_WEB_VIEW_RENDERER,
                    Features.WEB_VIEW_RENDERER_TERMINATE,
                    Features.TRACING_CONTROLLER_BASIC_USAGE,
                    Features.WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE,
                    Features.MULTI_PROCESS_QUERY,
                    Features.FORCE_DARK,
                    Features.FORCE_DARK_BEHAVIOR + Features.DEV_SUFFIX,
                    Features.WEB_MESSAGE_LISTENER + Features.DEV_SUFFIX,
                    Features.SET_SUPPORT_LIBRARY_VERSION + Features.DEV_SUFFIX,
            };
    // clang-format on

    // Initialization guarded by mAwInit.getLock()
    private InvocationHandler mStatics;
    private InvocationHandler mServiceWorkerController;
    private InvocationHandler mTracingController;
    private InvocationHandler mProxyController;

    public SupportLibWebViewChromiumFactory() {
        mCompatConverterAdapter = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebkitToCompatConverterAdapter());
        mAwInit = WebkitToSharedGlueConverter.getGlobalAwInit();
    }

    @Override
    public /* WebViewProvider */ InvocationHandler createWebView(WebView webView) {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebViewChromium(webView));
    }

    @Override
    public InvocationHandler getWebkitToCompatConverter() {
        return mCompatConverterAdapter;
    }

    private static class StaticsAdapter implements StaticsBoundaryInterface {
        private SharedStatics mSharedStatics;

        public StaticsAdapter(SharedStatics sharedStatics) {
            mSharedStatics = sharedStatics;
        }

        @Override
        public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
            mSharedStatics.initSafeBrowsing(context, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public void setSafeBrowsingWhitelist(List<String> hosts, ValueCallback<Boolean> callback) {
            mSharedStatics.setSafeBrowsingWhitelist(
                    hosts, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public Uri getSafeBrowsingPrivacyPolicyUrl() {
            return mSharedStatics.getSafeBrowsingPrivacyPolicyUrl();
        }

        @Override
        public boolean isMultiProcessEnabled() {
            return mSharedStatics.isMultiProcessEnabled();
        }
    }

    @Override
    public InvocationHandler getStatics() {
        synchronized (mAwInit.getLock()) {
            if (mStatics == null) {
                mStatics = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new StaticsAdapter(
                                WebkitToSharedGlueConverter.getGlobalAwInit().getStatics()));
            }
        }
        return mStatics;
    }

    @Override
    public String[] getSupportedFeatures() {
        return mWebViewSupportedFeatures;
    }

    @Override
    public InvocationHandler getServiceWorkerController() {
        synchronized (mAwInit.getLock()) {
            if (mServiceWorkerController == null) {
                mServiceWorkerController =
                        BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                new SupportLibServiceWorkerControllerAdapter(
                                        mAwInit.getServiceWorkerController()));
            }
        }
        return mServiceWorkerController;
    }

    @Override
    public InvocationHandler getTracingController() {
        synchronized (mAwInit.getLock()) {
            if (mTracingController == null) {
                mTracingController = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibTracingControllerAdapter(new SharedTracingControllerAdapter(
                                mAwInit.getRunQueue(), mAwInit.getAwTracingController())));
            }
        }
        return mTracingController;
    }

    @Override
    public InvocationHandler getProxyController() {
        synchronized (mAwInit.getLock()) {
            if (mProxyController == null) {
                mProxyController = BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibProxyControllerAdapter(
                                mAwInit.getRunQueue(), mAwInit.getAwProxyController()));
            }
        }
        return mProxyController;
    }

    @Override
    public void setSupportLibraryVersion(String version) {
        AwDebug.setSupportLibraryWebkitVersionCrashKey(version);
    }
}
