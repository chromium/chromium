// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.widget.FrameLayout;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.android_webview.shell.ContextManager;
import org.chromium.base.Callback;
import org.chromium.content_public.browser.WebContents;

/**
 * A View used for testing the AwContents internals.
 *
 * This class takes the place android.webkit.WebView would have in the production configuration.
 */
public class AwTestContainerView extends FrameLayout {
    private static HandlerThread sRenderThread;
    private static Handler sRenderThreadHandler;

    private AwContents mAwContents;
    private AwContents.InternalAccessDelegate mInternalAccessDelegate;

    private HardwareView mHardwareView;
    private boolean mAttachedContents;

    private Rect mWindowVisibleDisplayFrameOverride;

    private static final class WaitableEvent {
        private final Object mLock = new Object();
        private boolean mSignaled;

        public void waitForEvent() {
            synchronized (mLock) {
                while (!mSignaled) {
                    try {
                        mLock.wait();
                    } catch (InterruptedException e) {
                        throw new RuntimeException(e);
                    }
                }
            }
        }

        public void signal() {
            synchronized (mLock) {
                assert !mSignaled;
                mSignaled = true;
                mLock.notifyAll();
            }
        }
    }

    public static void installDrawFnFunctionTable(boolean useVulkan) {
        AwDrawFnImpl.setDrawFnFunctionTable(ContextManager.getDrawFnFunctionTable(useVulkan));
    }

    private static class HardwareView extends SurfaceView implements SurfaceHolder.Callback {
        // Only accessed on UI thread.
        private int mWidth;
        private int mHeight;
        private int mLastScrollX;
        private int mLastScrollY;
        private boolean mHaveSurface;
        private Runnable mReadyToRenderCallback;
        private SurfaceView mOverlaysSurfaceView;

        // Only accessed on render thread.
        private final ContextManager mContextManager;

        public HardwareView(Context context) {
            super(context);
            if (sRenderThread == null) {
                sRenderThread = new HandlerThread("RenderThreadInstr");
                sRenderThread.start();
                sRenderThreadHandler = new Handler(sRenderThread.getLooper());
            }
            mContextManager = new ContextManager();
            getHolder().setFormat(PixelFormat.TRANSPARENT);
            getHolder().addCallback(this);

            // Main SurfaceView needs to be positioned above the media content.
            setZOrderMediaOverlay(true);

            mOverlaysSurfaceView = new SurfaceView(context);
            mOverlaysSurfaceView.getHolder().addCallback(this);

            // This SurfaceView is used to present media and must be positioned below main surface.
            mOverlaysSurfaceView.setZOrderMediaOverlay(false);
        }

        public void readbackQuadrantColors(Callback<int[]> callback) {
            sRenderThreadHandler.post(
                    () -> {
                        callback.onResult(
                                mContextManager.draw(
                                        mWidth,
                                        mHeight,
                                        mLastScrollX,
                                        mLastScrollY,
                                        /* readbackQuadrants= */ true));
                    });
        }

        public boolean isReadyToRender() {
            return mHaveSurface;
        }

        public void setReadyToRenderCallback(Runnable runner) {
            assert !isReadyToRender() || runner == null;
            mReadyToRenderCallback = runner;
        }

        public SurfaceView getOverlaysView() {
            return mOverlaysSurfaceView;
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {}

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            if (holder == mOverlaysSurfaceView.getHolder()) {
                Surface surface = holder.getSurface();
                sRenderThreadHandler.post(
                        () -> {
                            mContextManager.setOverlaysSurface(surface);
                        });
                return;
            }

            mWidth = width;
            mHeight = height;
            mHaveSurface = true;

            Surface surface = holder.getSurface();
            sRenderThreadHandler.post(
                    () -> {
                        mContextManager.setSurface(surface, width, height);
                    });

            if (mReadyToRenderCallback != null) {
                mReadyToRenderCallback.run();
                mReadyToRenderCallback = null;
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            if (holder == mOverlaysSurfaceView.getHolder()) {
                WaitableEvent event = new WaitableEvent();
                sRenderThreadHandler.post(
                        () -> {
                            mContextManager.setOverlaysSurface(null);
                            event.signal();
                        });
                event.waitForEvent();
                return;
            }

            mHaveSurface = false;
            WaitableEvent event = new WaitableEvent();
            sRenderThreadHandler.post(
                    () -> {
                        mContextManager.setSurface(null, 0, 0);
                        event.signal();
                    });
            event.waitForEvent();
        }

        public void updateScroll(int x, int y) {
            mLastScrollX = x;
            mLastScrollY = y;
        }

        public void drawWebViewFunctor(int functor) {
            if (!mHaveSurface) {
                return;
            }

            WaitableEvent syncEvent = new WaitableEvent();
            sRenderThreadHandler.post(
                    () -> {
                        drawOnRt(syncEvent, functor, mWidth, mHeight, mLastScrollX, mLastScrollY);
                    });
            syncEvent.waitForEvent();
        }

        private void drawOnRt(
                WaitableEvent syncEvent,
                int functor,
                int width,
                int height,
                int scrollX,
                int scrollY) {
            mContextManager.sync(functor, false);
            syncEvent.signal();
            mContextManager.draw(width, height, scrollX, scrollY, /* readbackQuadrants= */ false);
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
            addView(
                    mHardwareView.getOverlaysView(),
                    new FrameLayout.LayoutParams(
                            FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));
            addView(
                    mHardwareView,
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

    /** Use glReadPixels to get 4 pixels from center of 4 quadrants. Result is in row-major order. */
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

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        attachedContentsInternal();
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mAttachedContents) {
            mAwContents.onDetachedFromWindow();
            mAttachedContents = false;
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
        AccessibilityNodeProvider provider = mAwContents.getAccessibilityNodeProvider();
        return provider == null ? super.getAccessibilityNodeProvider() : provider;
    }

    @Override
    public boolean performAccessibilityAction(int action, Bundle arguments) {
        return mAwContents.performAccessibilityAction(action, arguments);
    }

    @Override
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
        public void overScrollBy(
                int deltaX,
                int deltaY,
                int scrollX,
                int scrollY,
                int scrollRangeX,
                int scrollRangeY,
                int maxOverScrollX,
                int maxOverScrollY,
                boolean isTouchEvent) {
            // We're intentionally not calling super.scrollTo here to make testing easier.
            AwTestContainerView.this.overScrollBy(
                    deltaX,
                    deltaY,
                    scrollX,
                    scrollY,
                    scrollRangeX,
                    scrollRangeY,
                    maxOverScrollX,
                    maxOverScrollY,
                    isTouchEvent);
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
