// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.contextualsearch.SwipeRecognizer;
import org.chromium.chrome.browser.ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * A {@link ViewResourceFrameLayout} that specifically handles redraw of the top shadow of the view
 * it represents.
 */
public class ScrollingBottomViewResourceFrameLayout extends ViewResourceFrameLayout {
    /** A cached rect to avoid extra allocations. */
    private final Rect mCachedRect = new Rect();

    /** The height of the shadow sitting above the bottom view in px. */
    private final int mTopShadowHeightPx;

    /** A swipe recognizer for handling swipe gestures. */
    private SwipeRecognizer mSwipeRecognizer;

    public ScrollingBottomViewResourceFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mTopShadowHeightPx = getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);
    }

    /**
     * Set the swipe handler for this view and set {@link #isClickable()} to true to allow motion
     * events to be intercepted by the view itself.
     * @param handler A handler for swipe events on this view.
     */
    public void setSwipeDetector(EdgeSwipeHandler handler) {
        mSwipeRecognizer = new SwipeRecognizer(getContext());
        mSwipeRecognizer.setSwipeHandler(handler);

        // TODO(mdjones): This line of code makes it impossible to scroll through the bottom
        // toolbar. If the user accidentally swipes up on this view, the scroll no longer goes
        // through to the web contents. We should figure out how to make this work while also
        // supporting the toolbar swipe behavior.
        setClickable(true);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        boolean handledEvent = false;
        if (mSwipeRecognizer != null) handledEvent = mSwipeRecognizer.onTouchEvent(event);
        return handledEvent || super.onInterceptTouchEvent(event);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        boolean handledEvent = false;
        if (mSwipeRecognizer != null) handledEvent = mSwipeRecognizer.onTouchEvent(event);
        return handledEvent || super.onTouchEvent(event);
    }

    @Override
    protected ViewResourceAdapter createResourceAdapter() {
        return new ViewResourceAdapter(this) {
            @Override
            public void onCaptureStart(Canvas canvas, Rect dirtyRect) {
                mCachedRect.set(dirtyRect);
                if (mCachedRect.intersect(0, 0, getWidth(), mTopShadowHeightPx)) {
                    canvas.save();

                    // Clip the canvas to only the section of the dirty rect that contains the top
                    // shadow of the view.
                    canvas.clipRect(mCachedRect);

                    // Clear the shadow so redrawing does not make it progressively darker.
                    canvas.drawColor(0, PorterDuff.Mode.CLEAR);

                    canvas.restore();
                }

                super.onCaptureStart(canvas, dirtyRect);
            }
        };
    }

    /**
     * @return The height of the view's top shadow in px.
     */
    public int getTopShadowHeight() {
        return mTopShadowHeightPx;
    }
}
