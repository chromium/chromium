// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_COLOR_BLEND_ANIMATION_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubAnimationConstants.PANE_FADE_ANIMATION_DURATION_MS;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.core.widget.TextViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.widget.ButtonCompat;

import java.util.Objects;

/** Holds the current pane's {@link View}. */
public class HubPaneHostView extends FrameLayout {

    // Listens for layout of the snackbar container. Triggers an animation on the floating
    // action button to keep it from overlapping the snackbar.
    private final OnLayoutChangeListener mSnackbarLayoutChangeListener =
            new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(
                        View v,
                        int left,
                        int top,
                        int right,
                        int bottom,
                        int oldLeft,
                        int oldTop,
                        int oldRight,
                        int oldBottom) {
                    mFloatingActionButtonAnimatorHandler.forceFinishAnimation();

                    int height = bottom - top;
                    int oldHeight = oldBottom - oldTop;
                    // If the height is unchanged there is no need to do anything.
                    if (height == oldHeight) return;

                    // Calculate the delta based on the difference of the bottom margin besides
                    // the mOriginalMargin. See #updateFloatingButtonBottomMargin.
                    int delta =
                            Math.max(mEdgeToEdgeBottomInset, height)
                                    - Math.max(mEdgeToEdgeBottomInset, oldHeight);

                    ObjectAnimator animator =
                            ObjectAnimator.ofFloat(mActionButton, View.TRANSLATION_Y, -delta);

                    // Keep the following animation duration and interpolator in sync with
                    // SnackbarView.java.
                    animator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
                    animator.setDuration(mFloatingActionButtonAnimatorDuration);
                    animator.addListener(
                            new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    mActionButton.setTranslationY(0);
                                    mSnackbarHeightForAnimation = height;
                                    updateFloatingButtonBottomMargin();
                                }
                            });

                    mFloatingActionButtonAnimatorHandler.startAnimation(animator);
                }
            };

    private FrameLayout mPaneFrame;
    private ButtonCompat mActionButton;
    private ImageView mHairline;
    private ViewGroup mSnackbarContainer;
    private @Nullable View mCurrentViewRoot;
    private final AnimationHandler mFadeAnimatorHandler;
    private final AnimationHandler mFloatingActionButtonAnimatorHandler;
    private final AnimationHandler mColorBlendAnimatorHandler;
    private final HubColorBlendAnimatorSetHelper mAnimatorSetBuilder;

    private int mFloatingActionButtonAnimatorDuration;
    private int mOriginalMargin;
    private int mEdgeToEdgeBottomInset;
    private int mSnackbarHeightForAnimation;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mFadeAnimatorHandler = new AnimationHandler();
        mFloatingActionButtonAnimatorHandler = new AnimationHandler();
        mColorBlendAnimatorHandler = new AnimationHandler();
        mAnimatorSetBuilder = new HubColorBlendAnimatorSetHelper();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        Resources res = getContext().getResources();

        mPaneFrame = findViewById(R.id.pane_frame);
        mActionButton = findViewById(R.id.host_action_button);
        mHairline = findViewById(R.id.pane_top_hairline);
        mFloatingActionButtonAnimatorDuration =
                res.getInteger(android.R.integer.config_mediumAnimTime);
        mOriginalMargin = res.getDimensionPixelSize(R.dimen.floating_action_button_margin);
        mSnackbarContainer = findViewById(R.id.pane_host_view_snackbar_container);
        mSnackbarContainer.addOnLayoutChangeListener(mSnackbarLayoutChangeListener);

        // ButtonCompat's style Flat removes elevation after calling super so it is overridden. Undo
        // this.
        mActionButton.setElevation(
                res.getDimensionPixelSize(R.dimen.floating_action_button_elevation));

        registerColorBlends();
    }

    void setRootView(@Nullable View newRootView) {
        final View oldRootView = mCurrentViewRoot;
        mCurrentViewRoot = newRootView;

        mFadeAnimatorHandler.forceFinishAnimation();
        mFloatingActionButtonAnimatorHandler.forceFinishAnimation();

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

        @StyleRes
        int textAppearance =
                HubFieldTrial.useAlternativeFabColor()
                        ? HubColors.getTextAppearanceMediumOnPrimaryContainer(newColorScheme)
                        : HubColors.getTextAppearanceMedium(newColorScheme);
        mActionButton.setTextAppearance(textAppearance);

        @ColorInt int hairlineColor = HubColors.getHairlineColor(getContext(), newColorScheme);
        mHairline.setImageTintList(ColorStateList.valueOf(hairlineColor));

        AnimatorSet animatorSet =
                mAnimatorSetBuilder
                        .setNewColorScheme(newColorScheme)
                        .setPreviousColorScheme(prevColorScheme)
                        .build();
        mColorBlendAnimatorHandler.startAnimation(animatorSet);
    }

    private void registerColorBlends() {
        Context context = getContext();

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme -> HubColors.getBackgroundColor(context, colorScheme),
                        mPaneFrame::setBackgroundColor));

        boolean shouldUseAlternativeFabColor = HubFieldTrial.useAlternativeFabColor();
        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                HubColors.getContainerColor(
                                        shouldUseAlternativeFabColor, context, colorScheme),
                        interpolatedColor ->
                                updateActionButtonColorInternal(context, interpolatedColor)));

        mAnimatorSetBuilder.registerBlend(
                new SingleHubViewColorBlend(
                        PANE_COLOR_BLEND_ANIMATION_DURATION_MS,
                        colorScheme ->
                                HubColors.getOnContainerColor(
                                        shouldUseAlternativeFabColor, context, colorScheme),
                        this::updateIconColorInternal));
    }

    void setActionButtonData(@Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, mActionButton);
    }

    private void updateIconColorInternal(@ColorInt int color) {
        ColorStateList interpolatedIconColor = ColorStateList.valueOf(color);
        TextViewCompat.setCompoundDrawableTintList(mActionButton, interpolatedIconColor);
    }

    private void updateActionButtonColorInternal(Context context, @ColorInt int color) {
        ColorStateList buttonColor = HubColors.getContainerColorStateList(context, color);
        mActionButton.setButtonColor(buttonColor);
    }

    void setHairlineVisibility(boolean visible) {
        mHairline.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setFloatingActionButtonConsumer(Callback<Supplier<View>> consumer) {
        consumer.onResult(this::getFloatingActionButton);
    }

    void setSnackbarContainerConsumer(Callback<ViewGroup> consumer) {
        consumer.onResult(mSnackbarContainer);
    }

    void setEdgeToEdgeBottomInsets(int bottomInsets) {
        mEdgeToEdgeBottomInset = bottomInsets;
        mFloatingActionButtonAnimatorHandler.forceFinishAnimation();
        updateFloatingButtonBottomMargin();
    }

    private void updateFloatingButtonBottomMargin() {
        var lp = (MarginLayoutParams) mActionButton.getLayoutParams();
        // TODO(crbug.com/368407436): Use mSnackBarContainer.getHeight().
        lp.bottomMargin =
                mOriginalMargin + Math.max(mEdgeToEdgeBottomInset, mSnackbarHeightForAnimation);
        mActionButton.setLayoutParams(lp);
    }

    private @Nullable View getFloatingActionButton() {
        return mActionButton.getVisibility() == View.VISIBLE ? mActionButton : null;
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

    OnLayoutChangeListener getSnackbarLayoutChangeListenerForTesting() {
        return mSnackbarLayoutChangeListener;
    }
}
