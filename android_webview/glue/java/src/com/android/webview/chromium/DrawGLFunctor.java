// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Canvas;
import android.os.Build;
import android.view.View;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwContents;
import org.chromium.base.annotations.JniIgnoreNatives;

/**
 * Simple Java abstraction and wrapper for the native DrawGLFunctor flow.
 * An instance of this class can be constructed, bound to a single view context (i.e. AwContents)
 * and then drawn and detached from the view tree any number of times (using requestDrawGL and
 * detach respectively).
 */
@JniIgnoreNatives
class DrawGLFunctor implements AwContents.NativeDrawGLFunctor {
    private static final String TAG = DrawGLFunctor.class.getSimpleName();

    // Pointer to native side instance
    private final WebViewDelegate mWebViewDelegate;
    private long mNativeDrawGLFunctor;

    public DrawGLFunctor(long viewContext, WebViewDelegate webViewDelegate) {
        mNativeDrawGLFunctor = nativeCreateGLFunctor(viewContext);
        mWebViewDelegate = webViewDelegate;
    }

    @Override
    public void detach(View containerView) {
        if (mNativeDrawGLFunctor == 0) {
            throw new RuntimeException("detach on already destroyed DrawGLFunctor");
        }
        mWebViewDelegate.detachDrawGlFunctor(containerView, mNativeDrawGLFunctor);
    }

    private static final boolean sSupportFunctorReleasedCallback =
            (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

    @Override
    public boolean requestDrawGL(Canvas canvas, Runnable releasedCallback) {
        if (mNativeDrawGLFunctor == 0) {
            throw new RuntimeException("requestDrawGL on already destroyed DrawGLFunctor");
        }
        assert canvas != null;
        if (sSupportFunctorReleasedCallback) {
            assert releasedCallback != null;
            mWebViewDelegate.callDrawGlFunction(canvas, mNativeDrawGLFunctor, releasedCallback);
        } else {
            assert releasedCallback == null;
            mWebViewDelegate.callDrawGlFunction(canvas, mNativeDrawGLFunctor);
        }
        return true;
    }

    @Override
    public boolean requestInvokeGL(View containerView, boolean waitForCompletion) {
        if (mNativeDrawGLFunctor == 0) {
            throw new RuntimeException("requestInvokeGL on already destroyed DrawGLFunctor");
        }
        if (!sSupportFunctorReleasedCallback
                && !mWebViewDelegate.canInvokeDrawGlFunctor(containerView)) {
            return false;
        }

        mWebViewDelegate.invokeDrawGlFunctor(
                containerView, mNativeDrawGLFunctor, waitForCompletion);
        return true;
    }

    @Override
    public boolean supportsDrawGLFunctorReleasedCallback() {
        return sSupportFunctorReleasedCallback;
    }

    @Override
    public void destroy() {
        assert mNativeDrawGLFunctor != 0;
        nativeDestroyGLFunctor(mNativeDrawGLFunctor);
        mNativeDrawGLFunctor = 0;
    }

    public static void setChromiumAwDrawGLFunction(long functionPointer) {
        nativeSetChromiumAwDrawGLFunction(functionPointer);
    }

    // The Android framework performs manual JNI registration on these methods,
    // so the method signatures cannot change without updating the framework.
    private static native long nativeCreateGLFunctor(long viewContext);
    private static native void nativeDestroyGLFunctor(long functor);
    private static native void nativeSetChromiumAwDrawGLFunction(long functionPointer);
}
