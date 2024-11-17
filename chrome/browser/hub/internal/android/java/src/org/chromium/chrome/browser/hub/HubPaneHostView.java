// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

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
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.widget.ButtonCompat;

import java.util.Objects;

/** Holds the current pane's {@link View}. */
public class HubPaneHostView extends FrameLayout {
    // Chosen to exactly match the default add/remove animation duration of RecyclerView.
    private static final int FADE_ANIMATION_DURATION_MILLIS = 120;

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
                    endFloatingActionButtonAnimation();

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
                                    mFloatingActionButtonAnimator = null;
                                }
                            });

                    mFloatingActionButtonAnimator = animator;
                    animator.start();
                }
            };

    private FrameLayout mPaneFrame;
    private ButtonCompat mActionButton;
    private ImageView mHairline;
    private ViewGroup mSnackbarContainer;
    private @Nullable View mCurrentViewRoot;
    private @Nullable Animator mCurrentAnimator;
    private @Nullable Animator mFloatingActionButtonAnimator;

    private int mFloatingActionButtonAnimatorDuration;
    private int mOriginalMargin;
    private int mEdgeToEdgeBottomInset;
    private int mSnackbarHeightForAnimation;

    /** Default {@link FrameLayout} constructor called by inflation. */
    public HubPaneHostView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
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
    }

    void setRootView(@Nullable View newRootView) {
        final View oldRootView = mCurrentViewRoot;
        mCurrentViewRoot = newRootView;
        if (mCurrentAnimator != null) {
            mCurrentAnimator.end();
            assert mCurrentAnimator == null;
            endFloatingActionButtonAnimation();
        }

        if (oldRootView != null && newRootView != null) {
            newRootView.setAlpha(0);
            tryAddViewToFrame(newRootView);

            Animator fadeOut = ObjectAnimator.ofFloat(oldRootView, View.ALPHA, 1, 0);
            fadeOut.setDuration(FADE_ANIMATION_DURATION_MILLIS);

            Animator fadeIn = ObjectAnimator.ofFloat(newRootView, View.ALPHA, 0, 1);
            fadeIn.setDuration(FADE_ANIMATION_DURATION_MILLIS);

            AnimatorSet animatorSet = new AnimatorSet();
            animatorSet.playSequentially(fadeOut, fadeIn);
            animatorSet.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            mPaneFrame.removeView(oldRootView);
                            oldRootView.setAlpha(1);
                            mCurrentAnimator = null;
                        }
                    });
            mCurrentAnimator = animatorSet;
            animatorSet.start();
        } else if (newRootView == null) {
            mPaneFrame.removeAllViews();
        } else { // oldRootView == null
            tryAddViewToFrame(newRootView);
        }
    }

    void setActionButtonData(@Nullable FullButtonData buttonData) {
        ApplyButtonData.apply(buttonData, mActionButton);
    }

    void setColorScheme(@HubColorScheme int colorScheme) {
        Context context = getContext();

        @ColorInt int backgroundColor = HubColors.getBackgroundColor(context, colorScheme);
        mPaneFrame.setBackgroundColor(backgroundColor);

        ColorStateList iconColor;
        ColorStateList buttonColor;
        @StyleRes int textAppearance;
        if (HubFieldTrial.useAlternativeFabColor()) {
            iconColor =
                    ColorStateList.valueOf(
                            HubColors.getOnPrimaryContainerColor(context, colorScheme));
            buttonColor = HubColors.getPrimaryContainerColorStateList(context, colorScheme);
            textAppearance = HubColors.getTextAppearanceMediumOnPrimaryContainer(colorScheme);
        } else {
            iconColor =
                    ColorStateList.valueOf(
                            HubColors.getOnSecondaryContainerColor(context, colorScheme));
            buttonColor = HubColors.getSecondaryContainerColorStateList(context, colorScheme);
            textAppearance = HubColors.getTextAppearanceMedium(colorScheme);
        }
        TextViewCompat.setCompoundDrawableTintList(mActionButton, iconColor);
        mActionButton.setButtonColor(buttonColor);
        mActionButton.setTextAppearance(textAppearance);

        @ColorInt int hairlineColor = HubColors.getHairlineColor(context, colorScheme);
        mHairline.setImageTintList(ColorStateList.valueOf(hairlineColor));
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
        endFloatingActionButtonAnimation();
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

    private void endFloatingActionButtonAnimation() {
        if (mFloatingActionButtonAnimator != null) {
            mFloatingActionButtonAnimator.end();
            assert mFloatingActionButtonAnimator == null;
        }
    }

    OnLayoutChangeListener getSnackbarLayoutChangeListenerForTesting() {
        return mSnackbarLayoutChangeListener;
    }
}
