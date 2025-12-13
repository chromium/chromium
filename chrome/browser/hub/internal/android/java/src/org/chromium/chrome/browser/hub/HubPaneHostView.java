// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_SLIDE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.animation.AnimationHandler;

import java.util.Objects;

/** Holds the current pane's {@link View}. */
@NullMarked
public class HubPaneHostView extends FrameLayout {
    private FrameLayout mPaneFrame;
    private ViewGroup mSnackbarContainer;
    private @Nullable View mCurrentViewRoot;
    private final AnimationHandler mFadeAnimatorHandler;
    private final AnimationHandler mSlideAnimatorHandler;
    private @Nullable ObservableSupplier<Boolean> mXrSpaceModeObservableSupplier;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mFadeAnimatorHandler = new AnimationHandler();
        mSlideAnimatorHandler = new AnimationHandler();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPaneFrame = findViewById(R.id.pane_frame);
        mSnackbarContainer = findViewById(R.id.pane_host_view_snackbar_container);
    }

    /**
     * Sets the root view for the pane host, animating the transition if both old and new views are
     * non-null.
     *
     * @param newRootView The new root view to display.
     * @param isSlideAnimationLeftToRight Whether the animation should slide from left-to-right
     *     (true) or right-to-left (false), only when slide animation is enabled.
     */
    void setRootView(@Nullable View newRootView, boolean isSlideAnimationLeftToRight) {

        final View oldRootView = mCurrentViewRoot;
        mCurrentViewRoot = newRootView;

        if (oldRootView != null && newRootView != null) {
            if (isSlideAnimationEnabled()) {
                // If width is not available, just swap views without animation.
                if (mPaneFrame.getWidth() == 0) {
                    mPaneFrame.removeAllViews();
                    tryAddViewToFrame(newRootView);
                } else {
                    animateSlideTransition(oldRootView, newRootView, isSlideAnimationLeftToRight);
                }
                return;
            } else {
                animateFadeTransition(oldRootView, newRootView);
            }
        } else if (newRootView == null) {
            mPaneFrame.removeAllViews();
        } else { // oldRootView == null
            tryAddViewToFrame(newRootView);
        }
    }

    private void animateFadeTransition(View oldRootView, View newRootView) {
        mFadeAnimatorHandler.forceFinishAnimation();

        newRootView.setAlpha(0);
        tryAddViewToFrame(newRootView);

        Animator fadeOut = ObjectAnimator.ofFloat(oldRootView, View.ALPHA, 1, 0);
        fadeOut.setDuration(HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS);

        Animator fadeIn = ObjectAnimator.ofFloat(newRootView, View.ALPHA, 0, 1);
        fadeIn.setDuration(HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playSequentially(fadeOut, fadeIn);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mPaneFrame.removeView(oldRootView);
                        oldRootView.setAlpha(1);
                    }
                });
        mFadeAnimatorHandler.startAnimation(animatorSet);
    }

    private void animateSlideTransition(View oldRootView, View newRootView, boolean isLeftToRight) {
        mSlideAnimatorHandler.forceFinishAnimation();
        int containerWidth = mPaneFrame.getWidth();

        // Determine start and end positions based on direction.
        float oldViewEndTranslation = isLeftToRight ? containerWidth : -containerWidth;
        float newViewStartTranslation = isLeftToRight ? -containerWidth : containerWidth;

        // Ensure old view is at its starting position.
        oldRootView.setTranslationX(0);
        // Position new view off-screen.
        newRootView.setTranslationX(newViewStartTranslation);

        // Ensure new view is added before animation starts.
        tryAddViewToFrame(newRootView);

        Animator slideOut =
                ObjectAnimator.ofFloat(oldRootView, View.TRANSLATION_X, 0, oldViewEndTranslation);
        slideOut.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        Animator slideIn =
                ObjectAnimator.ofFloat(newRootView, View.TRANSLATION_X, newViewStartTranslation, 0);
        slideIn.setDuration(PANE_SLIDE_ANIMATION_DURATION_MS);

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(slideOut, slideIn);
        animatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mPaneFrame.removeView(oldRootView);
                        oldRootView.setTranslationX(0);
                        newRootView.setTranslationX(0);
                    }
                });
        mSlideAnimatorHandler.startAnimation(animatorSet);
    }

    void setColorMixer(HubColorMixer mixer) {
        registerColorBlends(mixer);
    }

    private void registerColorBlends(HubColorMixer mixer) {
        Context context = getContext();
        mixer.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> getBackgroundColor(context, colorScheme),
                        mPaneFrame::setBackgroundColor));
    }

    void setSnackbarContainerConsumer(Callback<ViewGroup> consumer) {
        consumer.onResult(mSnackbarContainer);
    }

    private void tryAddViewToFrame(View rootView) {
        ViewParent parent = rootView.getParent();
        if (!Objects.equals(parent, mPaneFrame)) {
            if (parent instanceof ViewGroup viewGroup) {
                viewGroup.removeView(rootView);
            }
            mPaneFrame.addView(rootView);
        }
    }

    private @ColorInt int getBackgroundColor(Context context, @HubColorScheme int colorScheme) {
        boolean isXrFullSpaceMode =
                mXrSpaceModeObservableSupplier != null && mXrSpaceModeObservableSupplier.get();
        return HubColors.getBackgroundColor(context, colorScheme, isXrFullSpaceMode);
    }

    public void setXrSpaceModeObservableSupplier(
            @Nullable ObservableSupplier<Boolean> xrSpaceModeObservableSupplier) {
        mXrSpaceModeObservableSupplier = xrSpaceModeObservableSupplier;
    }

    private boolean isSlideAnimationEnabled() {
        return ChromeFeatureList.sHubSlideAnimation.isEnabled();
    }
}
