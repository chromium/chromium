// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.base.ThreadUtils;
import org.chromium.support_lib_boundary.ProxyControllerBoundaryInterface;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;

/**
 * Adapter between AwProxyController and ProxyControllerBoundaryInterface.
 */
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
        if (checkNeedsPost()) {
            RuntimeException exception = mRunQueue.runOnUiThreadBlocking(() -> {
                try {
                    mProxyController.setProxyOverride(proxyRules, bypassRules, listener, executor);
                } catch (RuntimeException e) {
                    return e;
                }
                return null;
            });
            maybeThrowUnwrappedException(exception);
        } else {
            mProxyController.setProxyOverride(proxyRules, bypassRules, listener, executor);
        }
    }

    @Override
    public void clearProxyOverride(Runnable listener, Executor executor) {
        if (checkNeedsPost()) {
            RuntimeException exception = mRunQueue.runOnUiThreadBlocking(() -> {
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
