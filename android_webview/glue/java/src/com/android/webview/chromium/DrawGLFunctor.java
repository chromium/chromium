// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Canvas;
import android.view.View;
import android.webkit.WebViewDelegate;

import org.chromium.android_webview.AwContents;
import org.chromium.build.annotations.UsedByReflection;

/**
 * Simple Java abstraction and wrapper for the native DrawGLFunctor flow. An instance of this class
 * can be constructed, bound to a single view context (i.e. AwContents) and then drawn and detached
 * from the view tree any number of times (using requestDrawGL and detach respectively).
 */
class DrawGLFunctor implements AwContents.NativeDrawGLFunctor {
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

    @Override
    public boolean requestDrawGL(Canvas canvas, Runnable releasedCallback) {
        if (mNativeDrawGLFunctor == 0) {
            throw new RuntimeException("requestDrawGL on already destroyed DrawGLFunctor");
        }
        assert canvas != null;
        assert releasedCallback != null;
        mWebViewDelegate.callDrawGlFunction(canvas, mNativeDrawGLFunctor, releasedCallback);
        return true;
    }

    @Override
    public boolean requestInvokeGL(View containerView, boolean waitForCompletion) {
        if (mNativeDrawGLFunctor == 0) {
            throw new RuntimeException("requestInvokeGL on already destroyed DrawGLFunctor");
        }
        mWebViewDelegate.invokeDrawGlFunctor(
                containerView, mNativeDrawGLFunctor, waitForCompletion);
        return true;
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

    // The Android framework performs manual JNI registration on these methods, so the method
    // signatures cannot change without updating the framework. We use @UsedByReflection, while not
    // technically true, as a way to preserve these methods and their names.
    @UsedByReflection("Android framework manual registration")
    private static native long nativeCreateGLFunctor(long viewContext);

    @UsedByReflection("Android framework manual registration")
    private static native void nativeDestroyGLFunctor(long functor);

    @UsedByReflection("Android framework manual registration")
    private static native void nativeSetChromiumAwDrawGLFunction(long functionPointer);
}
