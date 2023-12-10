// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Canvas;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.Lifetime;

/**
 * Manages state associated with the Android render thread and the draw functor
 * that the WebView uses to render its contents. AwGLFunctor is responsible for
 * managing the lifetime of native RenderThreadManager and HardwareRenderer,
 * ensuring that they continue to exist while the functor remains attached to
 * the render node hierarchy.
 */
@JNINamespace("android_webview")
@Lifetime.WebView
public class AwGLFunctor implements AwFunctor {
    private final long mNativeAwGLFunctor;
    private final AwContents.NativeDrawGLFunctor mNativeDrawGLFunctor;
    private final ViewGroup mContainerView;
    private final Runnable mFunctorReleasedCallback;
    // Counts outstanding requestDrawGL calls as well as window attach count.
    private int mRefCount;

    public AwGLFunctor(
            AwContents.NativeDrawFunctorFactory nativeDrawFunctorFactory, ViewGroup containerView) {
        mNativeAwGLFunctor = AwGLFunctorJni.get().create(this);
        mNativeDrawGLFunctor = nativeDrawFunctorFactory.createGLFunctor(mNativeAwGLFunctor);
        mContainerView = containerView;
        mFunctorReleasedCallback = () -> removeReference();
        addReference();
    }

    @Override
    public void destroy() {
        assert mRefCount > 0;
        AwGLFunctorJni.get()
                .removeFromCompositorFrameProducer(mNativeAwGLFunctor, AwGLFunctor.this);
        removeReference();
    }

    public static long getAwDrawGLFunction() {
        return AwGLFunctorJni.get().getAwDrawGLFunction();
    }

    @Override
    public long getNativeCompositorFrameConsumer() {
        assert mRefCount > 0;
        return AwGLFunctorJni.get()
                .getCompositorFrameConsumer(mNativeAwGLFunctor, AwGLFunctor.this);
    }

    @Override
    public boolean requestDraw(Canvas canvas) {
        assert mRefCount > 0;
        boolean success = mNativeDrawGLFunctor.requestDrawGL(canvas, mFunctorReleasedCallback);
        if (success && mFunctorReleasedCallback != null) {
            addReference();
        }
        return success;
    }

    private void addReference() {
        ++mRefCount;
    }

    private void removeReference() {
        assert mRefCount > 0;
        if (--mRefCount == 0) {
            // When |mRefCount| decreases to zero, the functor is neither attached to a view, nor
            // referenced from the render tree, and so it is safe to delete the HardwareRenderer
            // instance to free up resources because the current state will not be drawn again.
            AwGLFunctorJni.get().deleteHardwareRenderer(mNativeAwGLFunctor, AwGLFunctor.this);
            mNativeDrawGLFunctor.destroy();
            AwGLFunctorJni.get().destroy(mNativeAwGLFunctor);
        }
    }

    @CalledByNative
    private boolean requestInvokeGL(boolean waitForCompletion) {
        return mNativeDrawGLFunctor.requestInvokeGL(mContainerView, waitForCompletion);
    }

    @CalledByNative
    private void detachFunctorFromView() {
        mNativeDrawGLFunctor.detach(mContainerView);
        mContainerView.invalidate();
    }

    @Override
    public void trimMemory() {
        assert mRefCount > 0;
        AwGLFunctorJni.get().deleteHardwareRenderer(mNativeAwGLFunctor, AwGLFunctor.this);
    }

    /**
     * Intended for test code.
     * @return the number of native instances of this class.
     */
    @VisibleForTesting
    public static int getNativeInstanceCount() {
        return AwGLFunctorJni.get().getNativeInstanceCount();
    }

    @NativeMethods
    interface Natives {
        void deleteHardwareRenderer(long nativeAwGLFunctor, AwGLFunctor caller);

        void removeFromCompositorFrameProducer(long nativeAwGLFunctor, AwGLFunctor caller);

        long getCompositorFrameConsumer(long nativeAwGLFunctor, AwGLFunctor caller);

        long getAwDrawGLFunction();

        void destroy(long nativeAwGLFunctor);

        long create(AwGLFunctor javaProxy);

        int getNativeInstanceCount();
    }
}
