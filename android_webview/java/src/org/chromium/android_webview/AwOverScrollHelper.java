// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.graphics.Canvas;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helper class to manage scroll-related UI feedback, like edge glows. */
@Lifetime.WebView
@NullMarked
public class AwOverScrollHelper {
    private final View mContainerView;
    private final AwScrollOffsetManager mScrollOffsetManager;
    private @Nullable OverScrollGlow mOverScrollGlow;

    public AwOverScrollHelper(View containerView, AwScrollOffsetManager scrollOffsetManager) {
        mContainerView = containerView;
        mScrollOffsetManager = scrollOffsetManager;
    }

    public void setOverScrollMode(int mode, Context context) {
        if (mode == View.OVER_SCROLL_NEVER) {
            mOverScrollGlow = null;
        } else {
            mOverScrollGlow = new OverScrollGlow(context, mContainerView);
        }
    }

    public void onDraw(Canvas canvas) {
        if (mOverScrollGlow == null) return;

        if (mOverScrollGlow.drawEdgeGlows(
                canvas,
                mScrollOffsetManager.computeMaximumHorizontalScrollOffset(),
                mScrollOffsetManager.computeMaximumVerticalScrollOffset())) {
            mContainerView.postInvalidateOnAnimation();
        }
    }

    public void onTouchEvent(MotionEvent event) {
        if (mOverScrollGlow == null) return;

        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mOverScrollGlow.setShouldPull(true);
        } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
            mOverScrollGlow.setShouldPull(false);
            mOverScrollGlow.releaseAll();
        }
    }

    /** Responds to an overscroll event, returns whether an animation results from this. */
    public boolean didOverscroll(
            int deltaX, int deltaY, float velocityX, float velocityY, boolean insideVSync) {
        if (mOverScrollGlow == null) return false;

        mOverScrollGlow.setOverScrollDeltas(deltaX, deltaY);
        final int oldX = mContainerView.getScrollX();
        final int oldY = mContainerView.getScrollY();
        final int x = oldX + deltaX;
        final int y = oldY + deltaY;
        final int scrollRangeX = mScrollOffsetManager.computeMaximumHorizontalScrollOffset();
        final int scrollRangeY = mScrollOffsetManager.computeMaximumVerticalScrollOffset();
        mOverScrollGlow.absorbGlow(
                x,
                y,
                oldX,
                oldY,
                scrollRangeX,
                scrollRangeY,
                (float) Math.hypot(velocityX, velocityY));

        return mOverScrollGlow.isAnimating();
    }

    public void pullGlow(int oldX, int oldY) {
        if (mOverScrollGlow == null) return;

        mOverScrollGlow.pullGlow(
                mContainerView.getScrollX(),
                mContainerView.getScrollY(),
                oldX,
                oldY,
                mScrollOffsetManager.computeMaximumHorizontalScrollOffset(),
                mScrollOffsetManager.computeMaximumVerticalScrollOffset());
    }
}
