// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;

import org.chromium.build.annotations.NullMarked;

/**
 * Background of the Hub toolbar. While the search box moves with a pane, the toolbar area below its
 * top row is only painted behind the pane that owns the search box, so that the other pane's
 * content stays visible.
 */
@NullMarked
class HubToolbarBackgroundDrawable extends ColorDrawable {
    private boolean mIsSearchRowClipped;
    private int mSearchRowTop;
    private float mSearchRowLeft;
    private float mSearchRowRight;

    /**
     * Clips the painted area below {@code searchRowTop} to the horizontal span from {@code left} to
     * {@code right}.
     */
    void clipSearchRow(int searchRowTop, float left, float right) {
        mIsSearchRowClipped = true;
        mSearchRowTop = searchRowTop;
        mSearchRowLeft = left;
        mSearchRowRight = right;
        invalidateSelf();
    }

    /** Paints the whole toolbar again. */
    void clearSearchRowClip() {
        if (!mIsSearchRowClipped) return;

        mIsSearchRowClipped = false;
        invalidateSelf();
    }

    @Override
    public void draw(Canvas canvas) {
        if (!mIsSearchRowClipped) {
            super.draw(canvas);
            return;
        }

        Rect bounds = getBounds();
        canvas.save();
        canvas.clipRect(bounds.left, bounds.top, bounds.right, mSearchRowTop);
        super.draw(canvas);
        canvas.restore();

        canvas.save();
        canvas.clipRect(mSearchRowLeft, mSearchRowTop, mSearchRowRight, bounds.bottom);
        super.draw(canvas);
        canvas.restore();
    }

    @Override
    public int getOpacity() {
        return mIsSearchRowClipped ? PixelFormat.TRANSLUCENT : super.getOpacity();
    }
}
