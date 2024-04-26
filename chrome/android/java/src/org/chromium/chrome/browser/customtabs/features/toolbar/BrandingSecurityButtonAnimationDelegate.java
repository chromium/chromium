// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.RotateDrawable;
import android.graphics.drawable.TransitionDrawable;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.interpolators.Interpolators;

/**
 * Animation Delegate for CCT Toolbar security icon. Show a cross-fade + rotation transitioning from
 * an existing icon to a new icon resource. The transition animation is referencing {@link
 * org.chromium.chrome.browser.omnibox.status.StatusView}.<br>
 * <br>
 * <div>
 *
 * <p>How does the rotation security animation work? 0. The rotation transition only works when
 * image view is visible and displaying a |existing drawable|, and needs to be updated to a new
 * |target drawable|;<br>
 * 1. The |existing drawable| will perform a 180-degree rotation from <b>regular position</b>, at
 * the same time opacity transitioning from 100% -> 0%;<br>
 * 2. The |target drawable| will start perform a 180-degree rotation from <b>up-side-down</b>, at
 * the same time opacity transitioning from 0% -> 100%. </div>
 */
// TODO(crbug.com/40859231): Share more code with StatusView.java.
class BrandingSecurityButtonAnimationDelegate {
    public static final int ICON_ANIMATION_DURATION_MS = 250;
    private static final int ICON_ROTATION_DEGREES = 180;

    private boolean mIsAnimationInProgress;

    /** The current drawable resource set through {@link #updateDrawableResource(int)} */
    private @DrawableRes int mCurrentDrawableResource;

    private final ImageView mImageView;

    /**
     * The animation delegate that will apply a rotation transition for image view.
     * @param imageView The image view that the animation will performed on.
     */
    BrandingSecurityButtonAnimationDelegate(@NonNull ImageView imageView) {
        mImageView = imageView;
    }

    /**
     * Update the image view into a new drawable resource with a rotation transition, if the
     * image view is visible and has a drawable showing; otherwise, no transition will be applied.
     * @param newResourceId The new drawable resource image view will get updated into.
     */
    void updateDrawableResource(@DrawableRes int newResourceId) {
        if (mCurrentDrawableResource == newResourceId) return;
        mCurrentDrawableResource = newResourceId;

        if (mImageView.getVisibility() == View.VISIBLE && mImageView.getDrawable() != null) {
            updateWithTransitionalDrawable(newResourceId);
        } else {
            mImageView.setImageResource(newResourceId);
        }
    }

    private void updateWithTransitionalDrawable(int resourceId) {
        if (mIsAnimationInProgress) {
            resetAnimationStatus();
        }

        Drawable targetDrawable =
                ApiCompatibilityUtils.getDrawable(
                        mImageView.getContext().getResources(), resourceId);
        Drawable existingDrawable = mImageView.getDrawable();

        // If the drawable is a transitional drawable, this means previous transition is in place.
        // We should start the transition with the previous target drawable.
        if (existingDrawable instanceof TransitionDrawable transitionDrawable
                && transitionDrawable.getNumberOfLayers() == 2) {
            existingDrawable = ((TransitionDrawable) existingDrawable).getDrawable(1);
        }

        // 1. Add padding to the smaller drawable so it has the same size as the bigger drawable.
        // This is necessary to maintain the original icon size, otherwise TransitionDrawable will
        // scale the smaller drawable to be at the same size as larger drawable;
        // 2. Convert the drawable to Bitmap drawable. This is necessary for the cross fade to work
        // for the TransitionDrawable.
        Resources resources = mImageView.getResources();
        int targetX =
                Math.max(targetDrawable.getIntrinsicWidth(), existingDrawable.getIntrinsicWidth());
        int targetY =
                Math.max(
                        targetDrawable.getIntrinsicHeight(), existingDrawable.getIntrinsicHeight());
        targetDrawable = resizeToBitmapDrawable(resources, targetDrawable, targetX, targetY);
        existingDrawable = resizeToBitmapDrawable(resources, existingDrawable, targetX, targetY);
        TransitionDrawable transitionDrawable =
                new TransitionDrawable(
                        new Drawable[] {existingDrawable, getRotatedIcon(targetDrawable)});
        transitionDrawable.setCrossFadeEnabled(true);
        mImageView.setImageDrawable(transitionDrawable);

        mIsAnimationInProgress = true;
        mImageView
                .animate()
                .setDuration(ICON_ANIMATION_DURATION_MS)
                .rotationBy(ICON_ROTATION_DEGREES)
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                .withStartAction(
                        () -> transitionDrawable.startTransition(ICON_ANIMATION_DURATION_MS))
                .withEndAction(
                        () -> {
                            mIsAnimationInProgress = false;
                            mImageView.setRotation(0);
                            // Only update security icon if it is still the current icon.
                            if (mCurrentDrawableResource == resourceId) {
                                mImageView.setImageResource(resourceId);
                            }
                        })
                .start();
    }

    private void resetAnimationStatus() {
        mIsAnimationInProgress = false;
        if (mImageView.getDrawable() instanceof TransitionDrawable transitionDrawable) {
            transitionDrawable.resetTransition();
        }
    }

    /** Returns whether an animation is currently running. */
    boolean isInAnimation() {
        return mIsAnimationInProgress;
    }

    /**
     * Add padding around the |drawable| until |targetWidth| and |targetHeight|, and convert it
     * to a {@link BitmapDrawable}.
     */
    @VisibleForTesting
    static BitmapDrawable resizeToBitmapDrawable(
            Resources resource, @NonNull Drawable drawable, int targetWidth, int targetHeight)
            throws IllegalArgumentException {
        int width = drawable.getIntrinsicWidth();
        int height = drawable.getIntrinsicHeight();
        if (height > targetHeight || width > targetWidth) {
            throw new IllegalArgumentException(
                    "The input drawable has a larger size than the target width / height.");
        }

        Bitmap bitmap = Bitmap.createBitmap(targetWidth, targetHeight, Config.ARGB_8888);
        Canvas c = new Canvas(bitmap);

        int paddingX = (targetWidth - width) / 2;
        int paddingY = (targetHeight - height) / 2;
        drawable.setBounds(paddingX, paddingY, targetWidth - paddingX, targetHeight - paddingY);
        drawable.draw(c);

        return new BitmapDrawable(resource, bitmap);
    }

    /** Returns a rotated version of the icon passed in. */
    // TODO(crbug.com/40859231): Share more code with StatusView.java.
    private static Drawable getRotatedIcon(Drawable icon) {
        RotateDrawable rotated = new RotateDrawable();
        rotated.setDrawable(icon);
        rotated.setToDegrees(ICON_ROTATION_DEGREES);
        rotated.setLevel(10000); // Max value for #setLevel.
        return rotated;
    }
}
