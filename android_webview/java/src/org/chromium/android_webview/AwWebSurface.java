// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Presentation surface helper for rendering and interacting with {@link AwContents} without
 * requiring intermediate Android View hierarchies.
 */
@NullMarked
public class AwWebSurface {
    private final Runnable mInvalidateListener;
    @Nullable private AwContents mAwContents;
    private int mWidth;
    private int mHeight;

    public AwWebSurface(Runnable invalidateListener) {
        assert invalidateListener != null : "InvalidateListener cannot be null.";
        mInvalidateListener = invalidateListener;
    }

    public void setAwContents(@Nullable AwContents awContents) {
        if (mAwContents != null && mAwContents != awContents) {
            mAwContents.setInvalidateListener(null);
        }
        mAwContents = awContents;
        if (mAwContents != null) {
            mAwContents.setInvalidateListener(mInvalidateListener);
            mAwContents.setOverScrollMode(View.OVER_SCROLL_NEVER);
            if (mWidth > 0 && mHeight > 0) {
                mAwContents.getViewMethods().onSizeChanged(mWidth, mHeight, 0, 0);
            }
        }
        mInvalidateListener.run();
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
}
