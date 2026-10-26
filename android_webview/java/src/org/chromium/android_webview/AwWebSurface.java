// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * Presentation surface helper for rendering and interacting with {@link AwContents} without
 * requiring intermediate Android View hierarchies.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
@NullMarked
public class AwWebSurface {
    /**
     * Listener for surface invalidation and overscroll events. Scroll values are in physical
     * pixels, or physical pixels per second for velocities, and follow the finger movement like
     * Blink and Compose (positive means the finger moves right/down).
     */
    public interface EventListener {
        /** Called when the surface needs to be redrawn. */
        void onInvalidate();

        /** Notified of overscroll. The velocity is zero unless it comes from a fling. */
        void onOverScroll(float deltaX, float deltaY, float velocityX, float velocityY);
    }

    private final EventListener mEventListener;
    private final AwWebContent.SurfaceBindingListener mSurfaceBindingListener;
    @Nullable private AwWebContent mWebContent;
    @Nullable private AwContents mAwContents;
    private int mWidth;
    private int mHeight;

    public AwWebSurface(EventListener eventListener) {
        assert eventListener != null : "EventListener cannot be null.";
        mEventListener = eventListener;
        mSurfaceBindingListener =
                awContents -> {
                    if (awContents == null) {
                        mWebContent = null;
                    }
                    setAwContents(awContents);
                };
    }

    public void setWebContent(@Nullable AwWebContent webContent) {
        if (mWebContent == webContent) {
            return;
        }
        if (mWebContent != null) {
            mWebContent.bindSurface(null);
        }
        if (webContent != null) {
            webContent.bindSurface(mSurfaceBindingListener);
            mWebContent = webContent;
        }
    }

    public void setAwContents(@Nullable AwContents awContents) {
        if (mAwContents != null && mAwContents != awContents) {
            mAwContents.setWebSurface(null);
            mAwContents.setInvalidateListener(null);
            mAwContents.getSettings().setForceFullscreenUnsupported(false);
            AwDisplayCutoutController cutoutController = mAwContents.getDisplayCutoutController();
            if (cutoutController != null) {
                cutoutController.setInsetViewportBelowWindow(true);
            }
            WebContents webContents = mAwContents.getWebContents();
            if (webContents != null) {
                AwWebSurfaceJni.get().setForWebContents(webContents, null);
            }
        }
        mAwContents = awContents;
        if (mAwContents != null) {
            mAwContents.setWebSurface(this);
            mAwContents.setInvalidateListener(mEventListener::onInvalidate);
            mAwContents.getSettings().setForceFullscreenUnsupported(true);
            // Don't shrink the page's viewport when the surface extends off the bottom of the
            // screen (e.g. inside a LazyColumn), so the page hits its bottom edge and overscrolls
            // to let the parent scroll the surface into view.
            AwDisplayCutoutController cutoutController = mAwContents.getDisplayCutoutController();
            if (cutoutController != null) {
                cutoutController.setInsetViewportBelowWindow(false);
            }
            mAwContents.setOverScrollMode(View.OVER_SCROLL_NEVER);
            onWebContentsChanged();
            if (mWidth > 0 && mHeight > 0) {
                mAwContents.getViewMethods().onSizeChanged(mWidth, mHeight, 0, 0);
            }
        }
        mEventListener.onInvalidate();
    }

    /** Binds the native peer to the current WebContents, which AwContents swaps for popups. */
    void onWebContentsChanged() {
        WebContents webContents = mAwContents == null ? null : mAwContents.getWebContents();
        if (webContents != null) {
            AwWebSurfaceJni.get().setForWebContents(webContents, this);
        }
    }

    public void draw(Canvas canvas) {
        if (mAwContents == null || !mAwContents.isAttachedToWindow()) return;

        mAwContents.getViewMethods().computeScroll();
        int scrollX = mAwContents.getScrollX();
        int scrollY = mAwContents.getScrollY();
        int saveCount = canvas.save();
        if (scrollX != 0 || scrollY != 0) {
            // View#draw applies a scroll translation that WebView undoes before drawing.
            // We simulate that here so that we can just use the same onDraw call.
            // TODO(b/556847619): We should start tracking AwContents as "attached" to a View
            // separately and then move these details there.
            canvas.translate(-scrollX, -scrollY);
        }
        mAwContents.getViewMethods().onDraw(canvas);
        canvas.restoreToCount(saveCount);
    }

    public void setSize(int width, int height) {
        mWidth = width;
        mHeight = height;
        if (mAwContents == null) return;
        mAwContents.getViewMethods().onSizeChanged(width, height, 0, 0);
    }

    public boolean onTouchEvent(MotionEvent event) {
        if (mAwContents == null) return false;
        return mAwContents.getViewMethods().onTouchEvent(event);
    }

    public void destroy() {
        setWebContent(null);
    }

    @CalledByNative
    private void onOverScroll(float deltaX, float deltaY, float velocityX, float velocityY) {
        mEventListener.onOverScroll(deltaX, deltaY, velocityX, velocityY);
    }

    @NativeMethods
    interface Natives {
        void setForWebContents(
                @JniType("content::WebContents*") WebContents webContents,
                @Nullable AwWebSurface surface);
    }
}
