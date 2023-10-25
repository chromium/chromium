// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.Nullable;

/** {@link ImageView} for the Shrink, Expand, and New Tab animations. */
// TODO(crbug/1495731): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
public class ShrinkExpandImageView extends ImageView implements RunOnNextLayout {
    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

    /**
     * @param context The Android {@link Context} for constructing the view.
     */
    public ShrinkExpandImageView(Context context) {
        super(context);
        mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
    }

    /**
     * Reset the view from a previous animation.
     *
     * @param layoutRect The {@link Rect} to position the view. The top and left will be used to set
     *     margins and position the view. The width and height will be used to set the dimensions of
     *     the view.
     */
    public void reset(@Nullable Rect layoutRect) {
        if (layoutRect != null) {
            FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
            if (layoutParams != null) {
                layoutParams.width = layoutRect.width();
                layoutParams.height = layoutRect.height();
                layoutParams.setMargins(layoutRect.left, layoutRect.top, 0, 0);
                setLayoutParams(layoutParams);
            }
        }
        setImageBitmap(null);
        setImageMatrix(new Matrix());
        setScaleX(1.0f);
        setScaleY(1.0f);
        setTranslationX(0.0f);
        setTranslationY(0.0f);
    }

    /** Returns the bitmap contained in this image view or null if one is not set. */
    public @Nullable Bitmap getBitmap() {
        Drawable drawable = getDrawable();
        if (drawable instanceof BitmapDrawable bitmapDrawable) {
            return bitmapDrawable.getBitmap();
        } else {
            return null;
        }
    }

    @Override
    public void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        runOnNextLayoutRunnables();
    }

    @Override
    public void runOnNextLayout(Runnable runnable) {
        mRunOnNextLayoutDelegate.runOnNextLayout(runnable);
    }

    @Override
    public void runOnNextLayoutRunnables() {
        mRunOnNextLayoutDelegate.runOnNextLayoutRunnables();
    }
}
