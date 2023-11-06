// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubLayoutConstants.FADE_DURATION_MS;

import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.RectEvaluator;
import android.graphics.Bitmap;
import android.view.View;
import android.view.animation.Interpolator;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.SyncOneshotSupplier;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.ui.interpolators.Interpolators;

import java.lang.ref.WeakReference;

/** {@link HubLayoutAnimatorProvider} for shrink, expand, and new tab animations. */
public class ShrinkExpandHubLayoutAnimatorProvider implements HubLayoutAnimatorProvider {
    /**
     * Utility class for the bitmap callback. This retains weak references to an image view to
     * supply a bitmap to and a runnable to execute once fulfilled. Weak references are necessary in
     * the event this callback somehow gets stuck in native thumbnail capture code and a reference
     * to it is held for an extended duration. If this happens a fallback animator will run and it
     * is desirable for the view and runnable to be available for garabage collection.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static class ImageViewWeakRefBitmapCallback implements Callback<Bitmap> {
        private final WeakReference<ImageView> mViewRef;
        private final WeakReference<Runnable> mOnFinishedRunnableRef;

        ImageViewWeakRefBitmapCallback(ImageView view, Runnable onFinishedRunnable) {
            mViewRef = new WeakReference<ImageView>(view);
            mOnFinishedRunnableRef = new WeakReference<Runnable>(onFinishedRunnable);
        }

        @Override
        public void onResult(Bitmap bitmap) {
            ImageView view = mViewRef.get();

            // If the view is null a fallback animation is already happening we don't need to
            // invoke the runnable.
            if (view == null) return;
            view.setImageBitmap(bitmap);

            Runnable runnable = mOnFinishedRunnableRef.get();
            if (runnable == null) return;
            runnable.run();
        }
    }

    private final @HubLayoutAnimationType int mAnimationType;
    private final @NonNull HubContainerView mHubContainerView;
    private final @NonNull SyncOneshotSupplierImpl<HubLayoutAnimator> mAnimatorSupplier;

    private final @NonNull SyncOneshotSupplier<ShrinkExpandAnimationData> mAnimationDataSupplier;
    private final @Nullable ImageViewWeakRefBitmapCallback mBitmapCallback;
    private final long mDurationMs;

    private @Nullable ShrinkExpandImageView mShrinkExpandImageView;
    private boolean mLayoutSatisfied;

    /**
     * Keep an explicit reference to this because {@link ObjectAnimator} only keeps a {@link
     * WeakReference}. This class will outlive the animation.
     */
    private @Nullable ShrinkExpandAnimator mShrinkExpandAnimator;

    /**
     * Creates a shrink, expand, or new tab animation.
     *
     * @param animationType The {@link HubLayoutAnimationType} of this animation.
     * @param needsBitmap Whether this animation will require a bitmap callback.
     * @param hubContainerView The {@link HubContainerView} to animate.
     * @param animationDataSupplier The supplier for {@link ShrinkExpandAnimationData} to use for
     *     the animation.
     * @param backgroundColor The background color to use for new tab animations or if the thumbnail
     *     doesn't cover the animating area.
     * @param durationMs The duration in milliseconds of the animation.
     */
    public ShrinkExpandHubLayoutAnimatorProvider(
            @HubLayoutAnimationType int animationType,
            boolean needsBitmap,
            @NonNull HubContainerView hubContainerView,
            @NonNull SyncOneshotSupplier<ShrinkExpandAnimationData> animationDataSupplier,
            @ColorInt int backgroundColor,
            long durationMs) {
        mAnimationType = animationType;
        mHubContainerView = hubContainerView;
        mAnimatorSupplier = new SyncOneshotSupplierImpl<HubLayoutAnimator>();
        mAnimationDataSupplier = animationDataSupplier;
        mDurationMs = durationMs;

        mShrinkExpandImageView = new ShrinkExpandImageView(hubContainerView.getContext());
        mShrinkExpandImageView.setVisibility(View.INVISIBLE);
        mShrinkExpandImageView.setBackgroundColor(backgroundColor);
        mHubContainerView.addView(mShrinkExpandImageView, 0);

        mBitmapCallback =
                needsBitmap
                        ? new ImageViewWeakRefBitmapCallback(
                                mShrinkExpandImageView, this::maybeSupplyAnimation)
                        : null;

        mAnimationDataSupplier.onAvailable(this::onAnimationDataAvailable);
    }

    @Override
    public @HubLayoutAnimationType int getPlannedAnimationType() {
        return mAnimationType;
    }

    @Override
    public @NonNull SyncOneshotSupplier<HubLayoutAnimator> getAnimatorSupplier() {
        return mAnimatorSupplier;
    }

    @Override
    public void supplyAnimatorNow() {
        if (mAnimatorSupplier.hasValue()) return;

        supplyFallbackAnimator();
    }

    @Override
    public @Nullable Callback<Bitmap> getThumbnailCallback() {
        return mBitmapCallback;
    }

    public ShrinkExpandImageView getImageViewForTesting() {
        return mShrinkExpandImageView;
    }

    private void onAnimationDataAvailable(ShrinkExpandAnimationData animationData) {
        // Preserve the bitmap because it might have been supplied before the animation data.
        mShrinkExpandImageView.resetKeepingBitmap(animationData.getInitialRect());

        if (animationData.shouldUseFallbackAnimation()) {
            supplyFallbackAnimator();
            return;
        }

        mShrinkExpandImageView.runOnNextLayout(
                () -> {
                    mLayoutSatisfied = true;
                    maybeSupplyAnimation();
                });
    }

    private void maybeSupplyAnimation() {
        boolean bitmapSatisfied =
                mBitmapCallback == null || mShrinkExpandImageView.getBitmap() != null;
        if (!bitmapSatisfied || !mLayoutSatisfied) return;

        supplyAnimator();
    }

    private void supplyFallbackAnimator() {
        if (mAnimationType == HubLayoutAnimationType.EXPAND_NEW_TAB) {
            assert mAnimationDataSupplier.hasValue()
                    : "For new tab animation the data should already be supplied.";
            // This is only possible if layout fails to happen, still try to use the normal
            // animation since after a draw pass things should catch up.
            supplyAnimator();
            return;
        }

        resetState();

        if (mAnimationType == HubLayoutAnimationType.SHRINK_TAB) {
            mAnimatorSupplier.set(
                    FadeHubLayoutAnimationFactory.createFadeInAnimator(
                            mHubContainerView, FADE_DURATION_MS));
        } else if (mAnimationType == HubLayoutAnimationType.EXPAND_TAB) {
            mAnimatorSupplier.set(
                    FadeHubLayoutAnimationFactory.createFadeOutAnimator(
                            mHubContainerView, FADE_DURATION_MS));
        } else {
            assert false : "Not reached.";
            // If in production we somehow get here just skip animating entirely.
            mAnimatorSupplier.set(
                    new HubLayoutAnimator(HubLayoutAnimationType.NONE, new AnimatorSet(), null));
        }
    }

    private void supplyAnimator() {
        assert mAnimationDataSupplier.hasValue();

        ShrinkExpandAnimationData animationData = mAnimationDataSupplier.get();
        mShrinkExpandAnimator =
                new ShrinkExpandAnimator(
                        mShrinkExpandImageView,
                        animationData.getInitialRect(),
                        animationData.getFinalRect());
        mShrinkExpandAnimator.setThumbnailSizeForOffset(animationData.getThumbnailSize());
        mShrinkExpandAnimator.setRect(animationData.getInitialRect());

        ObjectAnimator shrinkExpandAnimator =
                ObjectAnimator.ofObject(
                        mShrinkExpandAnimator,
                        ShrinkExpandAnimator.RECT,
                        new RectEvaluator(),
                        animationData.getInitialRect(),
                        animationData.getFinalRect());
        shrinkExpandAnimator.setInterpolator(getInterpolator(mAnimationType));
        shrinkExpandAnimator.setDuration(mDurationMs);

        // TODO(crbug/1492207): Add the ability to change corner radii of the ShrinkExpandImageView
        // via ShrinkExpandAnimator as part of the animation. For radiii use data supplied through
        // ShrinkExpandAnimationData.
        // * Near circular -> 0 for new tab.
        // * 0 -> TabThumbnailView radii for shrink.
        // * TabThumbnailView radii -> 0 for expand.

        // TODO(crbug/1492207): Fade in or out the toolbar along with this animation.
        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.play(shrinkExpandAnimator);

        HubLayoutAnimationListener listener =
                new HubLayoutAnimationListener() {
                    @Override
                    public void onStart() {
                        mHubContainerView.setVisibility(View.VISIBLE);
                        mShrinkExpandImageView.setVisibility(View.VISIBLE);
                    }

                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        // At this point the mShrinkExpandImageView is located at
                        // animationData#getFinalRect(); however, its layout params still has its
                        // dimensions as those from animationData#getInitialRect(). This is because
                        // the animator applies translate and scale transformations rather than
                        // manipulating the layout parameters of the view. Here we want to
                        // explicitly set the layout params of the view to match getFinalRect().
                        // This is effectively a no-op in production as the view will be removed
                        // before the next layout pass. However, it makes testing easier because
                        // the layout dimensions match the expected final state rather than being
                        // transformed from the initial layout parameters.
                        mShrinkExpandImageView.resetKeepingBitmap(animationData.getFinalRect());
                    }

                    @Override
                    public void afterEnd() {
                        resetState();
                    }
                };

        mAnimatorSupplier.set(new HubLayoutAnimator(mAnimationType, animatorSet, listener));
    }

    /**
     * Cleans up any leftover state that may exist on the {@link HubContainerView} after the
     * animation is finished or the animation is aborted.
     */
    private void resetState() {
        mHubContainerView.removeView(mShrinkExpandImageView);
        mShrinkExpandImageView.setImageBitmap(null);
        mShrinkExpandImageView = null;
    }

    private static Interpolator getInterpolator(@HubLayoutAnimationType int animationType) {
        if (animationType == HubLayoutAnimationType.EXPAND_NEW_TAB) {
            return Interpolators.STANDARD_INTERPOLATOR;
        }
        return Interpolators.EMPHASIZED;
    }
}
