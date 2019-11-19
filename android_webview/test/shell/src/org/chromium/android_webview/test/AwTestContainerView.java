// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.shell.DrawFn;
import org.chromium.base.Callback;
import org.chromium.content_public.browser.WebContents;

import java.nio.ByteBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * A View used for testing the AwContents internals.
 *
 * This class takes the place android.webkit.WebView would have in the production configuration.
 */
public class AwTestContainerView extends FrameLayout {
    private AwContents mAwContents;
    private AwContents.InternalAccessDelegate mInternalAccessDelegate;

    private HardwareView mHardwareView;
    private boolean mAttachedContents;

    private Rect mWindowVisibleDisplayFrameOverride;

    private class HardwareView extends GLSurfaceView {
        private static final int MODE_DRAW = 0;
        private static final int MODE_PROCESS = 1;
        private static final int MODE_PROCESS_NO_CONTEXT = 2;
        private static final int MODE_SYNC = 3;

        // mSyncLock is used to synchronized requestRender on the UI thread
        // and drawGL on the rendering thread. The variables following
        // are protected by it.
        private final Object mSyncLock = new Object();
        private int mFunctor;
        private int mLastDrawnFunctor;
        private boolean mSyncDone;
        private boolean mPendingDestroy;
        private int mLastScrollX;
        private int mLastScrollY;
        private Callback<int[]> mQuadrantReadbackCallback;

        // Only used by drawGL on render thread to store the value of scroll offsets at most recent
        // sync for subsequent draws.
        private int mCommittedScrollX;
        private int mCommittedScrollY;

        private boolean mHaveSurface;
        private Runnable mReadyToRenderCallback;
        private Runnable mReadyToDetachCallback;

        public HardwareView(Context context) {
            super(context);
            setEGLContextClientVersion(2); // GLES2
            getHolder().setFormat(PixelFormat.OPAQUE);
            setPreserveEGLContextOnPause(true);
            setRenderer(new Renderer() {
                private int mWidth;
                private int mHeight;

                @Override
                public void onDrawFrame(GL10 gl) {
                    HardwareView.this.onDrawFrame(gl, mWidth, mHeight);
                }

                @Override
                public void onSurfaceChanged(GL10 gl, int width, int height) {
                    gl.glViewport(0, 0, width, height);
                    gl.glScissor(0, 0, width, height);
                    mWidth = width;
                    mHeight = height;
                }

                @Override
                public void onSurfaceCreated(GL10 gl, EGLConfig config) {}
            });

            setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        }

        public void readbackQuadrantColors(Callback<int[]> callback) {
            synchronized (mSyncLock) {
                assert mQuadrantReadbackCallback == null;
                mQuadrantReadbackCallback = callback;
            }
            super.requestRender();
        }

        public boolean isReadyToRender() {
            return mHaveSurface;
        }

        public void setReadyToRenderCallback(Runnable runner) {
            assert !isReadyToRender() || runner == null;
            mReadyToRenderCallback = runner;
        }

        public void setReadyToDetachCallback(Runnable runner) {
            mReadyToDetachCallback = runner;
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            mHaveSurface = true;
            if (mReadyToRenderCallback != null) {
                mReadyToRenderCallback.run();
                mReadyToRenderCallback = null;
            }
            super.surfaceCreated(holder);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            mHaveSurface = false;
            if (mReadyToDetachCallback != null) {
                mReadyToDetachCallback.run();
                mReadyToDetachCallback = null;
            }
            super.surfaceDestroyed(holder);
        }

        public void updateScroll(int x, int y) {
            synchronized (mSyncLock) {
                mLastScrollX = x;
                mLastScrollY = y;
            }
        }

        public void awContentsDetached() {
            synchronized (mSyncLock) {
                super.requestRender();
                assert !mPendingDestroy;
                mPendingDestroy = true;
                try {
                    while (!mPendingDestroy) {
                        mSyncLock.wait();
                    }
                } catch (InterruptedException e) {
                    // ...
                }
            }
        }

        public void drawWebViewFunctor(int functor) {
            synchronized (mSyncLock) {
                super.requestRender();
                assert mFunctor == 0;
                mFunctor = functor;
                mSyncDone = false;
                try {
                    while (!mSyncDone) {
                        mSyncLock.wait();
                    }
                } catch (InterruptedException e) {
                    // ...
                }
            }
        }

        public void onDrawFrame(GL10 gl, int width, int height) {
            int functor;
            int scrollX;
            int scrollY;
            synchronized (mSyncLock) {
                scrollX = mLastScrollX;
                scrollY = mLastScrollY;

                if (mFunctor != 0) {
                    assert !mSyncDone;
                    functor = mFunctor;
                    mLastDrawnFunctor = mFunctor;
                    mFunctor = 0;
                    DrawFn.sync(functor, false);
                    mSyncDone = true;
                    mSyncLock.notifyAll();
                } else {
                    functor = mLastDrawnFunctor;
                    if (mPendingDestroy) {
                        DrawFn.destroyReleased();
                        mPendingDestroy = false;
                        mLastDrawnFunctor = 0;
                        mSyncLock.notifyAll();
                        return;
                    }
                }
            }
            if (functor != 0) {
                DrawFn.drawGL(functor, width, height, scrollX, scrollY);

                Callback<int[]> quadrantReadbackCallback = null;
                synchronized (mSyncLock) {
                    if (mQuadrantReadbackCallback != null) {
                        quadrantReadbackCallback = mQuadrantReadbackCallback;
                        mQuadrantReadbackCallback = null;
                    }
                }
                if (quadrantReadbackCallback != null) {
                    int quadrantColors[] = new int[4];
                    int quarterWidth = width / 4;
                    int quarterHeight = height / 4;
                    ByteBuffer buffer = ByteBuffer.allocate(4);
                    gl.glReadPixels(quarterWidth, quarterHeight * 3, 1, 1, GL10.GL_RGBA,
                            GL10.GL_UNSIGNED_BYTE, buffer);
                    quadrantColors[0] = readbackToColor(buffer);
                    gl.glReadPixels(quarterWidth * 3, quarterHeight * 3, 1, 1, GL10.GL_RGBA,
                            GL10.GL_UNSIGNED_BYTE, buffer);
                    quadrantColors[1] = readbackToColor(buffer);
                    gl.glReadPixels(quarterWidth, quarterHeight, 1, 1, GL10.GL_RGBA,
                            GL10.GL_UNSIGNED_BYTE, buffer);
                    quadrantColors[2] = readbackToColor(buffer);
                    gl.glReadPixels(quarterWidth * 3, quarterHeight, 1, 1, GL10.GL_RGBA,
                            GL10.GL_UNSIGNED_BYTE, buffer);
                    quadrantColors[3] = readbackToColor(buffer);
                    quadrantReadbackCallback.onResult(quadrantColors);
                }
            }
        }

        private int readbackToColor(ByteBuffer buffer) {
            return Color.argb(buffer.get(3) & 0xff, buffer.get(0) & 0xff, buffer.get(1) & 0xff,
                    buffer.get(2) & 0xff);
        }
    }

    private static boolean sCreatedOnce;
    private HardwareView createHardwareViewOnlyOnce(Context context) {
        if (sCreatedOnce) return null;
        sCreatedOnce = true;
        return new HardwareView(context);
    }

    public AwTestContainerView(Context context, boolean allowHardwareAcceleration) {
        super(context);
        if (allowHardwareAcceleration) {
            mHardwareView = createHardwareViewOnlyOnce(context);
        }
        if (isBackedByHardwareView()) {
            addView(mHardwareView,
                    new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        } else {
            setLayerType(LAYER_TYPE_SOFTWARE, null);
        }
        mInternalAccessDelegate = new InternalAccessAdapter();
        setOverScrollMode(View.OVER_SCROLL_ALWAYS);
        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    public void initialize(AwContents awContents) {
        mAwContents = awContents;
        if (isBackedByHardwareView()) {
            AwDrawFnImpl.setDrawFnFunctionTable(DrawFn.getDrawFnFunctionTable());
        }
    }

    public void setWindowVisibleDisplayFrameOverride(Rect rect) {
        mWindowVisibleDisplayFrameOverride = rect;
    }

    @Override
    public void getWindowVisibleDisplayFrame(Rect outRect) {
        if (mWindowVisibleDisplayFrameOverride != null) {
            outRect.set(mWindowVisibleDisplayFrameOverride);
        } else {
            super.getWindowVisibleDisplayFrame(outRect);
        }
    }

    public boolean isBackedByHardwareView() {
        return mHardwareView != null;
    }

    /**
     * Use glReadPixels to get 4 pixels from center of 4 quadrants. Result is in row-major order.
     */
    public void readbackQuadrantColors(Callback<int[]> callback) {
        assert isBackedByHardwareView();
        mHardwareView.readbackQuadrantColors(callback);
    }

    public WebContents getWebContents() {
        return mAwContents.getWebContents();
    }

    public AwContents getAwContents() {
        return mAwContents;
    }

    public AwContents.NativeDrawFunctorFactory getNativeDrawFunctorFactory() {
        return new NativeDrawFunctorFactory();
    }

    public AwContents.InternalAccessDelegate getInternalAccessDelegate() {
        return mInternalAccessDelegate;
    }

    public void destroy() {
        mAwContents.destroy();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mAwContents.onConfigurationChanged(newConfig);
    }

    private void attachedContentsInternal() {
        assert !mAttachedContents;
        mAwContents.onAttachedToWindow();
        mAttachedContents = true;
    }

    private void detachedContentsInternal() {
        assert mAttachedContents;
        mAwContents.onDetachedFromWindow();
        mAttachedContents = false;
        if (mHardwareView != null) {
            mHardwareView.awContentsDetached();
        }
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mHardwareView == null || mHardwareView.isReadyToRender()) {
            attachedContentsInternal();
        } else {
            mHardwareView.setReadyToRenderCallback(() -> attachedContentsInternal());
        }

        if (mHardwareView != null) {
            mHardwareView.setReadyToDetachCallback(() -> detachedContentsInternal());
        }
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mHardwareView == null || mHardwareView.isReadyToRender()) {
            detachedContentsInternal();

            if (mHardwareView != null) {
                mHardwareView.setReadyToRenderCallback(null);
                mHardwareView.setReadyToDetachCallback(null);
            }
        }
    }

    @Override
    public void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
        super.onFocusChanged(focused, direction, previouslyFocusedRect);
        mAwContents.onFocusChanged(focused, direction, previouslyFocusedRect);
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        return mAwContents.onCreateInputConnection(outAttrs);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        return mAwContents.onKeyUp(keyCode, event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        return mAwContents.dispatchKeyEvent(event);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        mAwContents.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    public void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        mAwContents.onSizeChanged(w, h, ow, oh);
    }

    @Override
    public void onOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY) {
        mAwContents.onContainerViewOverScrolled(scrollX, scrollY, clampedX, clampedY);
    }

    @Override
    public void onScrollChanged(int l, int t, int oldl, int oldt) {
        super.onScrollChanged(l, t, oldl, oldt);
        if (mAwContents != null) {
            mAwContents.onContainerViewScrollChanged(l, t, oldl, oldt);
        }
    }

    @Override
    public void computeScroll() {
        mAwContents.computeScroll();
    }

    @Override
    public void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        mAwContents.onVisibilityChanged(changedView, visibility);
    }

    @Override
    public void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        mAwContents.onWindowVisibilityChanged(visibility);
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        super.onTouchEvent(ev);
        return mAwContents.onTouchEvent(ev);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent ev) {
        super.onGenericMotionEvent(ev);
        return mAwContents.onGenericMotionEvent(ev);
    }

    @Override
    public boolean onHoverEvent(MotionEvent ev) {
        super.onHoverEvent(ev);
        return mAwContents.onHoverEvent(ev);
    }

    @Override
    public void onDraw(Canvas canvas) {
        if (isBackedByHardwareView()) {
            mHardwareView.updateScroll(getScrollX(), getScrollY());
        }
        mAwContents.onDraw(canvas);
        super.onDraw(canvas);
    }

    @Override
    public AccessibilityNodeProvider getAccessibilityNodeProvider() {
        AccessibilityNodeProvider provider =
                mAwContents.getAccessibilityNodeProvider();
        return provider == null ? super.getAccessibilityNodeProvider() : provider;
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        return mAwContents.performAccessibilityAction(action, arguments);
    }

    @Override
    @TargetApi(Build.VERSION_CODES.N)
    public boolean onDragEvent(DragEvent event) {
        return mAwContents.onDragEvent(event);
    }

    private class NativeDrawFunctorFactory implements AwContents.NativeDrawFunctorFactory {
        @Override
        public AwContents.NativeDrawGLFunctor createGLFunctor(long context) {
            return null;
        }

        @Override
        public AwDrawFnImpl.DrawFnAccess getDrawFnAccess() {
            return new DrawFnAccess();
        }
    }

    private class DrawFnAccess implements AwDrawFnImpl.DrawFnAccess {
        @Override
        public void drawWebViewFunctor(Canvas canvas, int functor) {
            assert isBackedByHardwareView();
            mHardwareView.drawWebViewFunctor(functor);
        }
    }

    // TODO: AwContents could define a generic class that holds an implementation similar to
    // the one below.
    private class InternalAccessAdapter implements AwContents.InternalAccessDelegate {

        @Override
        public boolean super_onKeyUp(int keyCode, KeyEvent event) {
            return AwTestContainerView.super.onKeyUp(keyCode, event);
        }

        @Override
        public boolean super_dispatchKeyEvent(KeyEvent event) {
            return AwTestContainerView.super.dispatchKeyEvent(event);
        }

        @Override
        public boolean super_onGenericMotionEvent(MotionEvent event) {
            return AwTestContainerView.super.onGenericMotionEvent(event);
        }

        @Override
        public void super_onConfigurationChanged(Configuration newConfig) {
            AwTestContainerView.super.onConfigurationChanged(newConfig);
        }

        @Override
        public void super_scrollTo(int scrollX, int scrollY) {
            // We're intentionally not calling super.scrollTo here to make testing easier.
            AwTestContainerView.this.scrollTo(scrollX, scrollY);
            if (isBackedByHardwareView()) {
                // Undo the scroll that will be applied because of mHardwareView
                // being a child of |this|.
                mHardwareView.setTranslationX(scrollX);
                mHardwareView.setTranslationY(scrollY);
            }
        }

        @Override
        public void overScrollBy(int deltaX, int deltaY,
                int scrollX, int scrollY,
                int scrollRangeX, int scrollRangeY,
                int maxOverScrollX, int maxOverScrollY,
                boolean isTouchEvent) {
            // We're intentionally not calling super.scrollTo here to make testing easier.
            AwTestContainerView.this.overScrollBy(deltaX, deltaY, scrollX, scrollY,
                     scrollRangeX, scrollRangeY, maxOverScrollX, maxOverScrollY, isTouchEvent);
        }

        @Override
        public void onScrollChanged(int l, int t, int oldl, int oldt) {
            AwTestContainerView.super.onScrollChanged(l, t, oldl, oldt);
        }

        @Override
        public void setMeasuredDimension(int measuredWidth, int measuredHeight) {
            AwTestContainerView.super.setMeasuredDimension(measuredWidth, measuredHeight);
        }

        @Override
        public int super_getScrollBarStyle() {
            return AwTestContainerView.super.getScrollBarStyle();
        }

        @Override
        public void super_startActivityForResult(Intent intent, int requestCode) {}
    }
}
