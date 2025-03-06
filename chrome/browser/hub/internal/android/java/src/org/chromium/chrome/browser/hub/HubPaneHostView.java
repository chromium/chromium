// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.ui.animation.AnimationHandler;

import java.util.Objects;

/** Holds the current pane's {@link View}. */
public class HubPaneHostView extends FrameLayout {
    private FrameLayout mPaneFrame;
    private ImageView mHairline;
    private ViewGroup mSnackbarContainer;
    private @Nullable View mCurrentViewRoot;
    private final AnimationHandler mFadeAnimatorHandler;
    private final AnimationHandler mColorBlendAnimatorHandler;
    private final HubColorBlendAnimatorSetHelper mAnimatorSetBuilder;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mFadeAnimatorHandler = new AnimationHandler();
        mColorBlendAnimatorHandler = new AnimationHandler();
        mAnimatorSetBuilder = new HubColorBlendAnimatorSetHelper();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mPaneFrame = findViewById(R.id.pane_frame);
        mHairline = findViewById(R.id.pane_top_hairline);
        mSnackbarContainer = findViewById(R.id.pane_host_view_snackbar_container);

        registerColorBlends();
    }

    void setRootView(@Nullable View newRootView) {
        final View oldRootView = mCurrentViewRoot;
        mCurrentViewRoot = newRootView;

        mFadeAnimatorHandler.forceFinishAnimation();

        if (oldRootView != null && newRootView != null) {
            newRootView.setAlpha(0);
            tryAddViewToFrame(newRootView);

            Animator fadeOut = ObjectAnimator.ofFloat(oldRootView, View.ALPHA, 1, 0);
            fadeOut.setDuration(PANE_FADE_ANIMATION_DURATION_MS);

            Animator fadeIn = ObjectAnimator.ofFloat(newRootView, View.ALPHA, 0, 1);
            fadeIn.setDuration(PANE_FADE_ANIMATION_DURATION_MS);

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
        } else if (newRootView == null) {
            mPaneFrame.removeAllViews();
        } else { // oldRootView == null
            tryAddViewToFrame(newRootView);
        }
    }

    void setColorScheme(HubColorSchemeUpdate colorSchemeUpdate) {
        @HubColorScheme int newColorScheme = colorSchemeUpdate.newColorScheme;
        @HubColorScheme int prevColorScheme = colorSchemeUpdate.previousColorScheme;

        @ColorInt int hairlineColor = HubColors.getHairlineColor(getContext(), newColorScheme);
        mHairline.setImageTintList(ColorStateList.valueOf(hairlineColor));

        AnimatorSet animatorSet =
                mAnimatorSetBuilder
                        .setNewColorScheme(newColorScheme)
                        .setPreviousColorScheme(prevColorScheme)
                        .setIsImmediate(!colorSchemeUpdate.animate)
                        .build();
        mColorBlendAnimatorHandler.startAnimation(animatorSet);
    }

    private void registerColorBlends() {
        Context context = getContext();

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        mPaneFrame::setBackgroundColor));
    }

    void setHairlineVisibility(boolean visible) {
        mHairline.setVisibility(visible ? View.VISIBLE : View.GONE);
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
}
