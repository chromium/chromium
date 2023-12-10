// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.ProxyControllerBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;

/** Adapter between AwProxyController and ProxyControllerBoundaryInterface. */
public class SupportLibProxyControllerAdapter implements ProxyControllerBoundaryInterface {
    private final WebViewChromiumRunQueue mRunQueue;
    private final AwProxyController mProxyController;

    public SupportLibProxyControllerAdapter(
            WebViewChromiumRunQueue runQueue, AwProxyController proxyController) {
        mRunQueue = runQueue;
        mProxyController = proxyController;
    }

    @Override
    public void setProxyOverride(
            String[][] proxyRules, String[] bypassRules, Runnable listener, Executor executor) {
        setProxyOverride(proxyRules, bypassRules, listener, executor, false);
    }

    @Override
    public void setProxyOverride(
            String[][] proxyRules,
            String[] bypassRules,
            Runnable listener,
            Executor executor,
            boolean reverseBypass) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SET_PROXY_OVERRIDE_OR_REVERSE_BYPASS")) {
            recordApiCall(
                    reverseBypass
                            ? ApiCall.SET_PROXY_OVERRIDE
                            : ApiCall.SET_PROXY_OVERRIDE_REVERSE_BYPASS);
            if (checkNeedsPost()) {
                RuntimeException exception =
                        mRunQueue.runOnUiThreadBlocking(
                                () -> {
                                    try {
                                        mProxyController.setProxyOverride(
                                                proxyRules,
                                                bypassRules,
                                                listener,
                                                executor,
                                                reverseBypass);
                                    } catch (RuntimeException e) {
                                        return e;
                                    }
                                    return null;
                                });
                maybeThrowUnwrappedException(exception);
            } else {
                mProxyController.setProxyOverride(
                        proxyRules, bypassRules, listener, executor, reverseBypass);
            }
        }
    }

    @Override
    public void clearProxyOverride(Runnable listener, Executor executor) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.CLEAR_PROXY_OVERRIDE")) {
            recordApiCall(ApiCall.CLEAR_PROXY_OVERRIDE);
            if (checkNeedsPost()) {
                RuntimeException exception =
                        mRunQueue.runOnUiThreadBlocking(
                                () -> {
                                    try {
                                        mProxyController.clearProxyOverride(listener, executor);
                                    } catch (RuntimeException e) {
                                        return e;
                                    }
                                    return null;
                                });
                maybeThrowUnwrappedException(exception);
            } else {
                mProxyController.clearProxyOverride(listener, executor);
            }
        }
    }

    private void maybeThrowUnwrappedException(RuntimeException exception) {
        if (exception != null) {
            Throwable cause = exception.getCause();
            if (cause instanceof ExecutionException) cause = cause.getCause();
            if (cause instanceof RuntimeException) throw (RuntimeException) cause;
            throw exception;
        }
    }

    private static boolean checkNeedsPost() {
        return !ThreadUtils.runningOnUiThread();
    }
}
