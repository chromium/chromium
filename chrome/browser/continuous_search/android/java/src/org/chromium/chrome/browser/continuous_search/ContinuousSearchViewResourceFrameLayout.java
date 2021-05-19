// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.util.AttributeSet;

import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * A {@link ViewResourceFrameLayout} that specifically handles redraw of the top shadow of the view
 * it represents.
 */
public class ContinuousSearchViewResourceFrameLayout extends ViewResourceFrameLayout {
    /** The height of the shadow sitting at the bottom of the view in px. */
    private final int mShadowHeightPx;

    /** A cached rect to avoid extra allocations. */
    private final Rect mCachedRect = new Rect();

    public ContinuousSearchViewResourceFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mShadowHeightPx = getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);
    }

    @Override
    protected ViewResourceAdapter createResourceAdapter() {
        return new ViewResourceAdapter(this) {
            @Override
            public void onCaptureStart(Canvas canvas, Rect dirtyRect) {
                mCachedRect.set(dirtyRect);
                // Check if the dirty rect contains the shadow of the view
                if (mCachedRect.intersect(
                            0, getHeight() - mShadowHeightPx, getWidth(), getHeight())) {
                    canvas.save();
                    // Clip the canvas to the section of the dirty rect that contains the shadow
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
     * @return The height of the view's shadow in px.
     */
    public int getShadowHeight() {
        return mShadowHeightPx;
    }
}
