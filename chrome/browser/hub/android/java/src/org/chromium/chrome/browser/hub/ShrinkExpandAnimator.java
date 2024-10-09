// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.util.Size;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import org.chromium.build.annotations.UsedByReflection;

/** Animator for scaling a {@link ShrinkExpandImageView} from one rect to another. */
// TODO(crbug.com/40286625): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
public class ShrinkExpandAnimator {
    /**
     * Tag for the {@link ObjectAnimator#ofObject()} {@code propertyName} param. Using this triggers
     * invocation of {@link #setRect(Rect)} during animation steps. An {@link RectEvaluator} is used
     * to interpolate between {@link Rect}s based on animation steps.
     */
    public static final String RECT = "rect";

    private final ShrinkExpandImageView mView;
    private final Rect mInitialRect;
    private final Rect mFinalRect;
    private final Matrix mImageMatrix;
    private Size mThumbnailSize;

    /**
     * Create an animator that scales and translates a view from one rect to another.
     *
     * @param view the ShrinkExpandImageView to apply the translation and scaling to. It should have
     *     a default scale of 1.0f and translation of 0.0f and be the size of {@code initialRect}.
     * @param initialRect the initial rect that view encompasses in global coordinates.
     * @param finalRect the final rect that view will encompass in global coordinates.
     */
    public ShrinkExpandAnimator(
            @NonNull ShrinkExpandImageView view,
            @NonNull Rect initialRect,
            @NonNull Rect finalRect) {
        mView = view;
        assert mView.getScaleX() == 1.0f;
        assert mView.getScaleY() == 1.0f;
        assert mView.getTranslationX() == 0.0f;
        assert mView.getTranslationY() == 0.0f;

        // Copy these rects to ensure they aren't re-used.
        mInitialRect = new Rect(initialRect);
        mFinalRect = new Rect(finalRect);
        mImageMatrix = new Matrix();
    }

    /**
     * Set the size of the thumbnail for top offset computation. Only necessary when expanding.
     * @param thumbnailSize The size of the thumbnail in the tab grid.
     */
    public void setThumbnailSizeForOffset(Size thumbnailSize) {
        mThumbnailSize = thumbnailSize;
    }

    /**
     * Adjusts {@link mView} to be the size of {@code rect}. Called by an {@link ObjectAnimator}
     * for each frame using a {@link RectEvaluator} to interpolate between {@link mInitialRect} and
     * {@link mFinalRect}.
     * @param rect The rect to scale the view to for the current frame.
     */
    @UsedByReflection("Used in ObjectAnimator")
    public void setRect(@NonNull Rect rect) {
        final float scaleX = (float) rect.width() / mInitialRect.width();
        final float scaleY = (float) rect.height() / mInitialRect.height();

        // Use view properties to animate the change without needing to re-layout the view. This
        // makes the animation efficient and smooth compared to manually resizing.
        mView.setScaleX(scaleX);
        mView.setScaleY(scaleY);
        mView.setTranslationX(
                (float) rect.left
                        - Math.round(
                                mInitialRect.left + (1.0 - scaleX) * mInitialRect.width() / 2.0));
        mView.setTranslationY(
                (float) rect.top
                        - Math.round(
                                mInitialRect.top + (1.0 - scaleY) * mInitialRect.height() / 2.0));

        // If there is no image we don't need to do anything else.
        mImageMatrix.reset();
        Bitmap bitmap = mView.getBitmap();
        if (bitmap == null) return;

        // Scale image to fill the width of the screen. Normalize the scale against the scaling of
        // the view to ensure the image appears as if it is scaling with the view. Scaling the
        // view by itself will not change the image size which would lead to the wrong appearance.
        final float scale = (float) rect.width() / bitmap.getWidth();
        final float xFactor = scale / scaleX;
        final float yFactor = scale / scaleY;
        mImageMatrix.setScale(xFactor, yFactor);

        // This section handles y-offset of the image so that a view that is initially partially
        // offscreen at the top correctly "crops" the top of the image throughout the animation.
        // This is only necessary when expanding the rect.
        if (mThumbnailSize != null
                && mInitialRect.top == mFinalRect.top
                && mInitialRect.height() < mThumbnailSize.getHeight()) {
            // Y translation offset shifts in line with the scaling of the rectangle. It should
            // progress from initialYOffset -> 0 as the scaling progresses.
            //
            // Use scaleX here as it changes linearly (unlike scaleY) and will ensure the bitmap
            // maintains a fixed aspect ratio as the image should fill the width of the screen.
            //
            // initialYOffset: the initial offset of the image off the screen.
            // scaleX: the current scale value goes from 0 to finalScaleX.
            // finalScaleX: the final expected scale.
            //
            // Multiply the initialYOffset by a linear function that follows the progression from
            // initialYOffset to 0 as a function of how the scaling has progressed. This is applied
            // using preTranslate because it isn't affected by scale.
            final float finalScaleX = (float) mFinalRect.width() / mInitialRect.width();
            final int initialYOffset = mInitialRect.height() - mThumbnailSize.getHeight();
            final int yOffset =
                    (int)
                            Math.round(
                                    (float) initialYOffset
                                            * ((1.0 - scaleX) / (finalScaleX - 1.0) + 1.0));
            mImageMatrix.preTranslate(0, yOffset);
        }

        // Center the image on the x-axis of the view. This is applied postTranslate because it is
        // affected by the scaling of the view.
        final int xOffset =
                Math.round((mInitialRect.width() - (bitmap.getWidth() * xFactor)) / 2.0f);
        mImageMatrix.postTranslate(xOffset, 0);

        mView.setScaleType(ImageView.ScaleType.MATRIX);
        mView.setImageMatrix(mImageMatrix);
    }
}
