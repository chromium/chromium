// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.RelativeLayout;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/**
 * Parent for TabGridDialog component.
 */
public class TabGridDialogView extends FrameLayout {
    private static final int DIALOG_ANIMATION_DURATION = 300;
    private static final int DIALOG_UNGROUP_ALPHA_ANIMATION_DURATION = 200;
    private static final int DIALOG_ALPHA_ANIMATION_DURATION = 150;
    private static final int CARD_FADE_ANIMATION_DURATION = 50;
    private static final int Y_TRANSLATE_DURATION_MS = 300;
    private static final int SCRIM_FADE_DURATION_MS = 350;

    private static Callback<RectF> sSourceRectCallbackForTesting;

    @IntDef({UngroupBarStatus.SHOW, UngroupBarStatus.HIDE, UngroupBarStatus.HOVERED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UngroupBarStatus {
        int SHOW = 0;
        int HIDE = 1;
        int HOVERED = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * An interface to listen to visibility related changes on this {@link TabGridDialogView}.
     */
    interface VisibilityListener {
        /**
         * Called when the animation to hide the tab grid dialog is finished.
         */
        void finishedHidingDialogView();
    }

    private final Context mContext;
    private final int mToolbarHeight;
    private final float mTabGridCardPadding;
    private View mBackgroundFrame;
    private View mAnimationCardView;
    private View mItemView;
    private View mUngroupBar;
    private ViewGroup mSnackBarContainer;
    private ViewGroup mParent;
    private TextView mUngroupBarTextView;
    private RelativeLayout mDialogContainerView;
    private PropertyModel mScrimPropertyModel;
    private ScrimCoordinator mScrimCoordinator;
    private FrameLayout.LayoutParams mContainerParams;
    private ViewTreeObserver.OnGlobalLayoutListener mParentGlobalLayoutListener;
    private VisibilityListener mVisibilityListener;
    private Animator mCurrentDialogAnimator;
    private Animator mCurrentUngroupBarAnimator;
    private AnimatorSet mBasicFadeInAnimation;
    private AnimatorSet mBasicFadeOutAnimation;
    private ObjectAnimator mYTranslateAnimation;
    private ObjectAnimator mUngroupBarShow;
    private ObjectAnimator mUngroupBarHide;
    private AnimatorSet mShowDialogAnimation;
    private AnimatorSet mHideDialogAnimation;
    private AnimatorListenerAdapter mShowDialogAnimationListener;
    private AnimatorListenerAdapter mHideDialogAnimationListener;
    private Map<View, Integer> mAccessibilityImportanceMap = new HashMap<>();
    private int mSideMargin;
    private int mTopMargin;
    private int mOrientation;
    private int mParentHeight;
    private int mParentWidth;
    private int mBackgroundDrawableColor;
    private int mUngroupBarStatus = UngroupBarStatus.HIDE;
    private int mUngroupBarBackgroundColor;
    private int mUngroupBarHoveredBackgroundColor;
    @ColorInt
    private int mUngroupBarTextColor;
    @ColorInt
    private int mUngroupBarHoveredTextColor;

    public TabGridDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mTabGridCardPadding = TabUiThemeProvider.getTabGridCardMargin(mContext);
        mToolbarHeight =
                (int) mContext.getResources().getDimension(R.dimen.tab_group_toolbar_height);
        mBackgroundDrawableColor =
                ContextCompat.getColor(mContext, R.color.tab_grid_dialog_background_color);

        mUngroupBarTextColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarTextColor(mContext, false);
        mUngroupBarHoveredTextColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarHoveredTextColor(mContext, false);

        mUngroupBarBackgroundColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarBackgroundColor(mContext, false);
        mUngroupBarHoveredBackgroundColor =
                TabUiThemeProvider.getTabGridDialogUngroupBarHoveredBackgroundColor(
                        mContext, false);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mParent = (ViewGroup) getParent();
        mParentHeight = mParent.getHeight();
        mParentWidth = mParent.getWidth();
        mParentGlobalLayoutListener = () -> {
            // Skip updating the parent view size caused by keyboard showing.
            if (KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(mContext, this)) return;
            mParentWidth = mParent.getWidth();
            mParentHeight = mParent.getHeight();
        };
        mParent.getViewTreeObserver().addOnGlobalLayoutListener(mParentGlobalLayoutListener);
        setVisibility(GONE);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mParent != null) {
            mParent.getViewTreeObserver().removeOnGlobalLayoutListener(mParentGlobalLayoutListener);
        }
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        updateDialogWithOrientation(newConfig.orientation);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mContainerParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mDialogContainerView = findViewById(R.id.dialog_container_view);
        mDialogContainerView.setLayoutParams(mContainerParams);
        mUngroupBar = findViewById(R.id.dialog_ungroup_bar);
        mUngroupBarTextView = mUngroupBar.findViewById(R.id.dialog_ungroup_bar_text);
        mBackgroundFrame = findViewById(R.id.dialog_frame);
        mBackgroundFrame.setLayoutParams(mContainerParams);
        mAnimationCardView = findViewById(R.id.dialog_animation_card_view);
        mSnackBarContainer = findViewById(R.id.dialog_snack_bar_container_view);
        updateDialogWithOrientation(mContext.getResources().getConfiguration().orientation);

        prepareAnimation();
        mDialogContainerView.setClipToOutline(true);
    }

    private void prepareAnimation() {
        mShowDialogAnimation = new AnimatorSet();
        mHideDialogAnimation = new AnimatorSet();
        mBasicFadeInAnimation = new AnimatorSet();
        ObjectAnimator dialogFadeInAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 0f, 1f);
        mBasicFadeInAnimation.play(dialogFadeInAnimator);
        mBasicFadeInAnimation.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mBasicFadeInAnimation.setDuration(DIALOG_ANIMATION_DURATION);

        mBasicFadeOutAnimation = new AnimatorSet();
        ObjectAnimator dialogFadeOutAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 1f, 0f);
        mBasicFadeOutAnimation.play(dialogFadeOutAnimator);
        mBasicFadeOutAnimation.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mBasicFadeOutAnimation.setDuration(DIALOG_ANIMATION_DURATION);
        mBasicFadeOutAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                updateItemViewAlpha();
            }
        });

        final int screenHeightPx = ViewUtils.dpToPx(
                getContext(), getContext().getResources().getConfiguration().screenHeightDp);
        final float mDialogInitYPos = mDialogContainerView.getY();
        mYTranslateAnimation = ObjectAnimator.ofFloat(
                mDialogContainerView, View.TRANSLATION_Y, mDialogInitYPos, screenHeightPx);
        mYTranslateAnimation.setInterpolator(Interpolators.EMPHASIZED_ACCELERATE);
        mYTranslateAnimation.setDuration(Y_TRANSLATE_DURATION_MS);
        mYTranslateAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                updateItemViewAlpha();
                mDialogContainerView.setY(mDialogInitYPos);
            }
        });

        mShowDialogAnimationListener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentDialogAnimator = null;
                mDialogContainerView.requestFocus();
                mDialogContainerView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                // TODO(crbug.com/1101561): Move clear/restore accessibility importance logic to
                // ScrimView so that it can be shared by all components using ScrimView.
                clearBackgroundViewAccessibilityImportance();
            }
        };
        mHideDialogAnimationListener = new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                setVisibility(View.GONE);
                mCurrentDialogAnimator = null;
                mDialogContainerView.clearFocus();
                restoreBackgroundViewAccessibilityImportance();
                if (mVisibilityListener != null) {
                    mVisibilityListener.finishedHidingDialogView();
                }
            }
        };

        mUngroupBarShow = ObjectAnimator.ofFloat(mUngroupBar, View.ALPHA, 0f, 1f);
        mUngroupBarShow.setDuration(DIALOG_UNGROUP_ALPHA_ANIMATION_DURATION);
        mUngroupBarShow.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        mUngroupBarShow.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                if (mCurrentUngroupBarAnimator != null) {
                    mCurrentUngroupBarAnimator.end();
                }
                mCurrentUngroupBarAnimator = mUngroupBarShow;
                mUngroupBar.setVisibility(View.VISIBLE);
                mUngroupBar.setAlpha(0f);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mCurrentUngroupBarAnimator = null;
            }
        });

        mUngroupBarHide = ObjectAnimator.ofFloat(mUngroupBar, View.ALPHA, 1f, 0f);
        mUngroupBarHide.setDuration(DIALOG_UNGROUP_ALPHA_ANIMATION_DURATION);
        mUngroupBarHide.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mUngroupBarHide.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                if (mCurrentUngroupBarAnimator != null) {
                    mCurrentUngroupBarAnimator.end();
                }
                mCurrentUngroupBarAnimator = mUngroupBarHide;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mUngroupBar.setVisibility(View.INVISIBLE);
                mCurrentUngroupBarAnimator = null;
            }
        });
    }

    private void updateItemViewAlpha() {
        // Restore the original card.
        if (mItemView == null) return;
        mItemView.setAlpha(1f);
    }

    private void clearBackgroundViewAccessibilityImportance() {
        assert mAccessibilityImportanceMap.size() == 0;

        ViewGroup parent = (ViewGroup) getParent();
        for (int i = 0; i < parent.getChildCount(); i++) {
            View view = parent.getChildAt(i);
            if (view == TabGridDialogView.this) {
                continue;
            }
            mAccessibilityImportanceMap.put(view, view.getImportantForAccessibility());
            view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }
    }

    private void restoreBackgroundViewAccessibilityImportance() {
        ViewGroup parent = (ViewGroup) getParent();
        for (int i = 0; i < parent.getChildCount(); i++) {
            View view = parent.getChildAt(i);
            if (view == TabGridDialogView.this) {
                continue;
            }
            assert mAccessibilityImportanceMap.containsKey(view);
            Integer importance = mAccessibilityImportanceMap.get(view);
            view.setImportantForAccessibility(
                    importance == null ? IMPORTANT_FOR_ACCESSIBILITY_AUTO : importance);
        }
        mAccessibilityImportanceMap.clear();
    }

    void setVisibilityListener(VisibilityListener visibilityListener) {
        mVisibilityListener = visibilityListener;
    }

    void setupDialogAnimation(View sourceView) {
        // In case where user jumps to a new page from dialog, clean existing animations in
        // mHideDialogAnimation and play basic fade out instead of zooming back to corresponding tab
        // grid card.
        if (sourceView == null) {
            mShowDialogAnimation = new AnimatorSet();
            mShowDialogAnimation.play(mBasicFadeInAnimation);
            mShowDialogAnimation.removeAllListeners();
            mShowDialogAnimation.addListener(mShowDialogAnimationListener);

            mHideDialogAnimation = new AnimatorSet();
            Animator hideAnimator = TabUiFeatureUtilities.isTabletTabGroupsEnabled(getContext())
                    ? mYTranslateAnimation
                    : mBasicFadeOutAnimation;
            mHideDialogAnimation.play(hideAnimator);
            mHideDialogAnimation.removeAllListeners();
            mHideDialogAnimation.addListener(mHideDialogAnimationListener);

            if (ChromeFeatureList.sDiscardOccludedBitmaps.isEnabled()) {
                updateAnimationCardView(null);
            }
            return;
        }

        mItemView = sourceView;
        Rect rect = new Rect();
        mItemView.getGlobalVisibleRect(rect);
        // Offset by CompositeViewHolder top offset.
        Rect parentRect = new Rect();
        mParent.getGlobalVisibleRect(parentRect);
        rect.offset(0, -parentRect.top);
        // Setup a dummy animation card that looks the same as the original tab grid card for
        // animation.
        updateAnimationCardView(mItemView);

        // Calculate dialog size.
        int dialogHeight = mParentHeight - 2 * mTopMargin;
        int dialogWidth = mParentWidth - 2 * mSideMargin;

        // Calculate position and size info about the original tab grid card.
        float sourceLeft = rect.left + mTabGridCardPadding;
        float sourceTop = rect.top + mTabGridCardPadding;
        float sourceHeight = rect.height() - 2 * mTabGridCardPadding;
        float sourceWidth = rect.width() - 2 * mTabGridCardPadding;
        if (sSourceRectCallbackForTesting != null) {
            sSourceRectCallbackForTesting.onResult(new RectF(
                    sourceLeft, sourceTop, sourceLeft + sourceWidth, sourceTop + sourceHeight));
        }

        // Setup animation position info and scale ratio of the background frame.
        float frameInitYPosition = -(dialogHeight / 2 + mTopMargin - sourceHeight / 2 - sourceTop);
        float frameInitXPosition = -(dialogWidth / 2 + mSideMargin - sourceWidth / 2 - sourceLeft);
        float frameScaleY = sourceHeight / dialogHeight;
        float frameScaleX = sourceWidth / dialogWidth;

        // Setup scale ratio of card and dialog. Height and Width for both dialog and card scale at
        // the same rate during scaling animations.
        float cardScale = mOrientation == Configuration.ORIENTATION_PORTRAIT
                ? (float) dialogWidth / rect.width()
                : (float) dialogHeight / rect.height();
        float dialogScale = frameScaleX;

        // Setup animation position info of the animation card.
        float cardScaledYPosition = mTopMargin + ((cardScale - 1f) / 2) * sourceHeight;
        float cardScaledXPosition = mSideMargin + ((cardScale - 1f) / 2) * sourceWidth;
        float cardInitYPosition = sourceTop - mTabGridCardPadding;
        float cardInitXPosition = sourceLeft - mTabGridCardPadding;

        // Setup animation position info of the dialog.
        float dialogInitYPosition =
                frameInitYPosition - (sourceHeight - (dialogHeight * dialogScale)) / 2f;
        float dialogInitXPosition = frameInitXPosition;

        // In the first half of the dialog showing animation, the animation card scales up and moves
        // towards where the dialog should be.
        final ObjectAnimator cardZoomOutMoveYAnimator = ObjectAnimator.ofFloat(
                mAnimationCardView, View.TRANSLATION_Y, cardInitYPosition, cardScaledYPosition);
        final ObjectAnimator cardZoomOutMoveXAnimator = ObjectAnimator.ofFloat(
                mAnimationCardView, View.TRANSLATION_X, cardInitXPosition, cardScaledXPosition);
        final ObjectAnimator cardZoomOutScaleXAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_X, 1f, cardScale);
        final ObjectAnimator cardZoomOutScaleYAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_Y, 1f, cardScale);

        AnimatorSet cardZoomOutAnimatorSet = new AnimatorSet();
        cardZoomOutAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        cardZoomOutAnimatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        cardZoomOutAnimatorSet.play(cardZoomOutMoveYAnimator)
                .with(cardZoomOutMoveXAnimator)
                .with(cardZoomOutScaleYAnimator)
                .with(cardZoomOutScaleXAnimator);

        // In the first half of the dialog showing animation, the animation card fades out as it
        // moves and scales up.
        final ObjectAnimator cardZoomOutAlphaAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.ALPHA, 1f, 0f);
        cardZoomOutAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        cardZoomOutAlphaAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // In the second half of the dialog showing animation, the dialog zooms out from where the
        // card stops at the end of the first half and moves towards where the dialog should be.
        final ObjectAnimator dialogZoomOutMoveYAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.TRANSLATION_Y, dialogInitYPosition, 0f);
        final ObjectAnimator dialogZoomOutMoveXAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.TRANSLATION_X, dialogInitXPosition, 0f);
        final ObjectAnimator dialogZoomOutScaleYAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_Y, dialogScale, 1f);
        final ObjectAnimator dialogZoomOutScaleXAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_X, dialogScale, 1f);

        AnimatorSet dialogZoomOutAnimatorSet = new AnimatorSet();
        dialogZoomOutAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        dialogZoomOutAnimatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        dialogZoomOutAnimatorSet.play(dialogZoomOutMoveYAnimator)
                .with(dialogZoomOutMoveXAnimator)
                .with(dialogZoomOutScaleYAnimator)
                .with(dialogZoomOutScaleXAnimator);

        // In the second half of the dialog showing animation, the dialog fades in while it moves
        // and scales up.
        final ObjectAnimator dialogZoomOutAlphaAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 0f, 1f);
        dialogZoomOutAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        dialogZoomOutAlphaAnimator.setStartDelay(DIALOG_ALPHA_ANIMATION_DURATION);
        dialogZoomOutAlphaAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // During the whole dialog showing animation, the frame background scales up and moves so
        // that it looks like the card zooms out and becomes the dialog.
        final ObjectAnimator frameZoomOutMoveYAnimator = ObjectAnimator.ofFloat(
                mBackgroundFrame, View.TRANSLATION_Y, frameInitYPosition, 0f);
        final ObjectAnimator frameZoomOutMoveXAnimator = ObjectAnimator.ofFloat(
                mBackgroundFrame, View.TRANSLATION_X, frameInitXPosition, 0f);
        final ObjectAnimator frameZoomOutScaleYAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_Y, frameScaleY, 1f);
        final ObjectAnimator frameZoomOutScaleXAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_X, frameScaleX, 1f);

        AnimatorSet frameZoomOutAnimatorSet = new AnimatorSet();
        frameZoomOutAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        frameZoomOutAnimatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        frameZoomOutAnimatorSet.play(frameZoomOutMoveYAnimator)
                .with(frameZoomOutMoveXAnimator)
                .with(frameZoomOutScaleYAnimator)
                .with(frameZoomOutScaleXAnimator);

        // After the dialog showing animation starts, the original card in grid tab switcher fades
        // out.
        final ObjectAnimator tabFadeOutAnimator =
                ObjectAnimator.ofFloat(mItemView, View.ALPHA, 1f, 0f);
        tabFadeOutAnimator.setDuration(CARD_FADE_ANIMATION_DURATION);

        dialogZoomOutAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                // At the beginning of the first half of the showing animation, the white frame and
                // the animation card should be above the the dialog view, and their alpha should be
                // set to 1.
                mBackgroundFrame.bringToFront();
                mAnimationCardView.bringToFront();
                mDialogContainerView.setAlpha(0f);
                mBackgroundFrame.setAlpha(1f);
                mAnimationCardView.setAlpha(1f);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                // At the end of the showing animation, reset the alpha of animation related views
                // to 0.
                mBackgroundFrame.setAlpha(0f);
                mAnimationCardView.setAlpha(0f);
            }
        });

        dialogZoomOutAlphaAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                // At the beginning of the second half of the showing animation, the dialog should
                // be above the white frame and the animation card.
                mDialogContainerView.bringToFront();
            }
        });

        // Setup the dialog showing animation.
        mShowDialogAnimation = new AnimatorSet();
        mShowDialogAnimation.play(cardZoomOutAnimatorSet)
                .with(cardZoomOutAlphaAnimator)
                .with(frameZoomOutAnimatorSet)
                .with(dialogZoomOutAnimatorSet)
                .with(dialogZoomOutAlphaAnimator)
                .with(tabFadeOutAnimator);
        mShowDialogAnimation.addListener(mShowDialogAnimationListener);

        // In the first half of the dialog hiding animation, the dialog scales down and moves
        // towards where the tab grid card should be.
        final ObjectAnimator dialogZoomInMoveYAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.TRANSLATION_Y, 0f, dialogInitYPosition);
        final ObjectAnimator dialogZoomInMoveXAnimator = ObjectAnimator.ofFloat(
                mDialogContainerView, View.TRANSLATION_X, 0f, dialogInitXPosition);
        final ObjectAnimator dialogZoomInScaleYAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_Y, 1f, dialogScale);
        final ObjectAnimator dialogZoomInScaleXAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_X, 1f, dialogScale);

        AnimatorSet dialogZoomInAnimatorSet = new AnimatorSet();
        dialogZoomInAnimatorSet.play(dialogZoomInMoveYAnimator)
                .with(dialogZoomInMoveXAnimator)
                .with(dialogZoomInScaleYAnimator)
                .with(dialogZoomInScaleXAnimator);
        dialogZoomInAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        dialogZoomInAnimatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        dialogZoomInAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mDialogContainerView.setTranslationX(0f);
                mDialogContainerView.setTranslationY(0f);
                mDialogContainerView.setScaleX(1f);
                mDialogContainerView.setScaleY(1f);
            }
        });

        // In the first half of the dialog hiding animation, the dialog fades out while it moves and
        // scales down.
        final ObjectAnimator dialogZoomInAlphaAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 1f, 0f);
        dialogZoomInAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        dialogZoomInAlphaAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        // In the second half of the dialog hiding animation, the animation card zooms in from where
        // the dialog stops at the end of the first half and moves towards where the card should be.
        final ObjectAnimator cardZoomInMoveYAnimator = ObjectAnimator.ofFloat(
                mAnimationCardView, View.TRANSLATION_Y, cardScaledYPosition, cardInitYPosition);
        final ObjectAnimator cardZoomInMoveXAnimator = ObjectAnimator.ofFloat(
                mAnimationCardView, View.TRANSLATION_X, cardScaledXPosition, cardInitXPosition);
        final ObjectAnimator cardZoomInScaleXAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_X, cardScale, 1f);
        final ObjectAnimator cardZoomInScaleYAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_Y, cardScale, 1f);

        AnimatorSet cardZoomInAnimatorSet = new AnimatorSet();
        cardZoomInAnimatorSet.play(cardZoomInMoveYAnimator)
                .with(cardZoomInMoveXAnimator)
                .with(cardZoomInScaleXAnimator)
                .with(cardZoomInScaleYAnimator);
        cardZoomInAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        cardZoomInAnimatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        // In the second half of the dialog hiding animation, the tab grid card fades in while it
        // scales down and moves.
        final ObjectAnimator cardZoomInAlphaAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.ALPHA, 0f, 1f);
        cardZoomInAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        cardZoomInAlphaAnimator.setStartDelay(DIALOG_ALPHA_ANIMATION_DURATION);
        cardZoomInAlphaAnimator.setInterpolator(Interpolators.LINEAR_INTERPOLATOR);

        cardZoomInAlphaAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                // At the beginning of the second half of the hiding animation, the white frame and
                // the animation card should be above the the dialog view.
                mBackgroundFrame.bringToFront();
                mAnimationCardView.bringToFront();
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                // At the end of the hiding animation, reset the alpha of animation related views to
                // 0.
                mBackgroundFrame.setAlpha(0f);
                mAnimationCardView.setAlpha(0f);
            }
        });

        // During the whole dialog hiding animation, the frame background scales down and moves so
        // that it looks like the dialog zooms in and becomes the card.
        final ObjectAnimator frameZoomInMoveYAnimator = ObjectAnimator.ofFloat(
                mBackgroundFrame, View.TRANSLATION_Y, 0f, frameInitYPosition);
        final ObjectAnimator frameZoomInMoveXAnimator = ObjectAnimator.ofFloat(
                mBackgroundFrame, View.TRANSLATION_X, 0f, frameInitXPosition);
        final ObjectAnimator frameZoomInScaleYAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_Y, 1f, frameScaleY);
        final ObjectAnimator frameZoomInScaleXAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_X, 1f, frameScaleX);

        AnimatorSet frameZoomInAnimatorSet = new AnimatorSet();
        frameZoomInAnimatorSet.play(frameZoomInMoveYAnimator)
                .with(frameZoomInMoveXAnimator)
                .with(frameZoomInScaleYAnimator)
                .with(frameZoomInScaleXAnimator);
        frameZoomInAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        frameZoomInAnimatorSet.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        frameZoomInAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                // At the beginning of the hiding animation, the alpha of white frame needs to be
                // restored to 1.
                mBackgroundFrame.setAlpha(1f);
            }
        });

        // At the end of the dialog hiding animation, the original tab grid card fades in.
        final ObjectAnimator tabFadeInAnimator =
                ObjectAnimator.ofFloat(mItemView, View.ALPHA, 0f, 1f);
        tabFadeInAnimator.setDuration(CARD_FADE_ANIMATION_DURATION);
        tabFadeInAnimator.setStartDelay(DIALOG_ANIMATION_DURATION - CARD_FADE_ANIMATION_DURATION);

        // Setup the dialog hiding animation.
        mHideDialogAnimation = new AnimatorSet();
        mHideDialogAnimation.play(dialogZoomInAnimatorSet)
                .with(dialogZoomInAlphaAnimator)
                .with(frameZoomInAnimatorSet)
                .with(cardZoomInAnimatorSet)
                .with(cardZoomInAlphaAnimator)
                .with(tabFadeInAnimator);
        mHideDialogAnimation.addListener(mHideDialogAnimationListener);
    }

    @VisibleForTesting
    void updateDialogWithOrientation(int orientation) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mSideMargin =
                    (int) mContext.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
            mTopMargin =
                    (int) mContext.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        } else {
            mSideMargin =
                    (int) mContext.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
            mTopMargin =
                    (int) mContext.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
        }
        mContainerParams.setMargins(mSideMargin, mTopMargin, mSideMargin, mTopMargin);
        mOrientation = orientation;
    }

    private void updateAnimationCardView(View view) {
        if (view == null) {
            ((ImageView) mAnimationCardView.findViewById(R.id.tab_favicon)).setImageDrawable(null);
            ((TextView) (mAnimationCardView.findViewById(R.id.tab_title))).setText("");
            ((ImageView) (mAnimationCardView.findViewById(R.id.tab_thumbnail)))
                    .setImageDrawable(null);
            ((ImageView) mAnimationCardView.findViewById(R.id.action_button))
                    .setImageDrawable(null);
            mAnimationCardView.findViewById(R.id.background_view).setBackground(null);
            return;
        }

        // Update the dummy animation card view with the actual item view from grid tab switcher
        // recyclerView.
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mAnimationCardView.getLayoutParams();
        params.width = view.getWidth();
        params.height = view.getHeight();
        if (view.findViewById(R.id.tab_title) == null) return;

        mAnimationCardView.findViewById(R.id.card_view)
                .setBackground(view.findViewById(R.id.card_view).getBackground());

        ImageView sourceCardFavicon = view.findViewById(R.id.tab_favicon);
        ImageView animationCardFavicon = mAnimationCardView.findViewById(R.id.tab_favicon);
        if (sourceCardFavicon.getDrawable() != null) {
            int padding = (int) TabUiThemeProvider.getTabCardTopFaviconPadding(mContext);
            animationCardFavicon.setPadding(padding, padding, padding, padding);
            animationCardFavicon.setImageDrawable(sourceCardFavicon.getDrawable());
        } else {
            animationCardFavicon.setImageDrawable(null);
        }

        ((TextView) (mAnimationCardView.findViewById(R.id.tab_title)))
                .setText(((TextView) (view.findViewById(R.id.tab_title))).getText());
        ApiCompatibilityUtils.setTextAppearance(
                (TextView) (mAnimationCardView.findViewById(R.id.tab_title)),
                R.style.TextAppearance_TextMediumThick_Primary);
        ((TextView) (mAnimationCardView.findViewById(R.id.tab_title)))
                .setTextColor(((TextView) (view.findViewById(R.id.tab_title))).getTextColors());

        ((ImageView) (mAnimationCardView.findViewById(R.id.tab_thumbnail)))
                .setImageDrawable(
                        ((ImageView) (view.findViewById(R.id.tab_thumbnail))).getDrawable());

        ImageView actionButton = mAnimationCardView.findViewById(R.id.action_button);
        actionButton.setImageDrawable(
                ((ImageView) (view.findViewById(R.id.action_button))).getDrawable());
        ApiCompatibilityUtils.setImageTintList(actionButton,
                ImageViewCompat.getImageTintList((view.findViewById(R.id.action_button))));

        mAnimationCardView.findViewById(R.id.background_view).setBackground(null);
    }

    /**
     * Setup the {@link PropertyModel} used to show scrim view.
     *
     * @param scrimClickRunnable The {@link Runnable} that runs when scrim view is clicked.
     */
    void setScrimClickRunnable(Runnable scrimClickRunnable) {
        mScrimPropertyModel = new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                                      .with(ScrimProperties.ANCHOR_VIEW, mDialogContainerView)
                                      .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                                      .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                                      .with(ScrimProperties.TOP_MARGIN, 0)
                                      .with(ScrimProperties.CLICK_DELEGATE, scrimClickRunnable)
                                      .with(ScrimProperties.AFFECTS_NAVIGATION_BAR, true)
                                      .build();
    }

    void setupScrimCoordinator(ScrimCoordinator scrimCoordinator) {
        mScrimCoordinator = scrimCoordinator;
    }

    /**
     * Reset the dialog content with {@code toolbarView} and {@code recyclerView}.
     *
     * @param toolbarView The toolbarview to be added to dialog.
     * @param recyclerView The recyclerview to be added to dialog.
     */
    void resetDialog(View toolbarView, View recyclerView) {
        mDialogContainerView.removeAllViews();
        mDialogContainerView.addView(toolbarView);
        mDialogContainerView.addView(recyclerView);
        mDialogContainerView.addView(mUngroupBar);
        mDialogContainerView.addView(mSnackBarContainer);
        RelativeLayout.LayoutParams params =
                (RelativeLayout.LayoutParams) recyclerView.getLayoutParams();
        params.setMargins(0, mToolbarHeight, 0, 0);
        recyclerView.setVisibility(View.VISIBLE);
    }

    /**
     * Show {@link PopupWindow} for dialog with animation.
     */
    void showDialog() {
        if (mCurrentDialogAnimator != null && mCurrentDialogAnimator != mShowDialogAnimation) {
            mCurrentDialogAnimator.end();
        }
        mCurrentDialogAnimator = mShowDialogAnimation;
        assert mScrimCoordinator != null && mScrimPropertyModel != null;
        mScrimCoordinator.showScrim(mScrimPropertyModel);
        setVisibility(View.VISIBLE);
        mShowDialogAnimation.start();
    }

    /**
     * Hide {@link PopupWindow} for dialog with animation.
     */
    void hideDialog() {
        // Skip the hideDialog call caused by initializing the dialog visibility as false.
        if (getVisibility() != VISIBLE) return;
        assert mScrimCoordinator != null && mScrimPropertyModel != null;
        if (mCurrentDialogAnimator != null && mCurrentDialogAnimator != mHideDialogAnimation) {
            mCurrentDialogAnimator.end();
        }
        mCurrentDialogAnimator = mHideDialogAnimation;
        if (mScrimCoordinator.isShowingScrim()) {
            if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
                mScrimCoordinator.hideScrim(true, SCRIM_FADE_DURATION_MS);
            } else {
                mScrimCoordinator.hideScrim(true);
            }
        }
        mHideDialogAnimation.start();
    }

    /**
     * Update the ungroup bar based on {@code status}.
     *
     * @param status The status in {@link TabGridDialogView.UngroupBarStatus} that the ungroup bar
     *         should be updated to.
     */
    void updateUngroupBar(int status) {
        if (status == mUngroupBarStatus) return;
        switch (status) {
            case UngroupBarStatus.SHOW:
                updateUngroupBarTextView(false);
                if (mUngroupBarStatus == UngroupBarStatus.HIDE) {
                    mUngroupBarShow.start();
                }
                break;
            case UngroupBarStatus.HIDE:
                mUngroupBarHide.start();
                break;
            case UngroupBarStatus.HOVERED:
                updateUngroupBarTextView(true);
                if (mUngroupBarStatus == UngroupBarStatus.HIDE) {
                    mUngroupBarShow.start();
                }
                break;
            default:
                assert false;
        }
        mUngroupBarStatus = status;
    }

    private void updateUngroupBarTextView(boolean isHovered) {
        assert mUngroupBarTextView.getBackground() instanceof GradientDrawable;
        mUngroupBar.bringToFront();
        GradientDrawable background = (GradientDrawable) mUngroupBarTextView.getBackground();
        background.setColor(
                isHovered ? mUngroupBarHoveredBackgroundColor : mUngroupBarBackgroundColor);
        mUngroupBarTextView.setTextColor(
                isHovered ? mUngroupBarHoveredTextColor : mUngroupBarTextColor);
    }

    /**
     * Update the dialog container background color.
     * @param backgroundColor The new background color to use.
     */
    void updateDialogContainerBackgroundColor(int backgroundColor) {
        mBackgroundDrawableColor = backgroundColor;
        DrawableCompat.setTint(mDialogContainerView.getBackground(), backgroundColor);
        DrawableCompat.setTint(mBackgroundFrame.getBackground(), backgroundColor);
    }

    /**
     * Update the ungroup bar background color.
     * @param colorInt The new background color to use when ungroup bar is visible.
     */
    void updateUngroupBarBackgroundColor(int colorInt) {
        mUngroupBarBackgroundColor = colorInt;
    }

    /**
     * Update the ungroup bar background color when the ungroup bar is hovered.
     * @param colorInt The new background color to use when ungroup bar is visible and hovered.
     */
    void updateUngroupBarHoveredBackgroundColor(int colorInt) {
        mUngroupBarHoveredBackgroundColor = colorInt;
    }

    /**
     * Update the ungroup bar text color when the ungroup bar is visible but not hovered.
     * @param colorInt The new text color to use when ungroup bar is visible.
     */
    void updateUngroupBarTextColor(int colorInt) {
        mUngroupBarTextColor = colorInt;
    }

    /**
     * Update the ungroup bar text color when the ungroup bar is hovered.
     * @param colorInt The new text color to use when ungroup bar is visible and hovered.
     */
    void updateUngroupBarHoveredTextColor(int colorInt) {
        mUngroupBarHoveredTextColor = colorInt;
    }

    /**
     * Return the container view for undo closure snack bar.
     */
    ViewGroup getSnackBarContainer() {
        return mSnackBarContainer;
    }

    @VisibleForTesting
    Animator getCurrentDialogAnimatorForTesting() {
        return mCurrentDialogAnimator;
    }

    @VisibleForTesting
    Animator getCurrentUngroupBarAnimatorForTesting() {
        return mCurrentUngroupBarAnimator;
    }

    @VisibleForTesting
    int getUngroupBarStatusForTesting() {
        return mUngroupBarStatus;
    }

    @VisibleForTesting
    AnimatorSet getShowDialogAnimationForTesting() {
        return mShowDialogAnimation;
    }

    @VisibleForTesting
    int getBackgroundColorForTesting() {
        return mBackgroundDrawableColor;
    }

    @VisibleForTesting
    int getUngroupBarBackgroundColorForTesting() {
        return mUngroupBarBackgroundColor;
    }

    @VisibleForTesting
    int getUngroupBarHoveredBackgroundColorForTesting() {
        return mUngroupBarHoveredBackgroundColor;
    }

    @VisibleForTesting
    int getUngroupBarTextColorForTesting() {
        return mUngroupBarTextColor;
    }

    @VisibleForTesting
    int getUngroupBarHoveredTextColorForTesting() {
        return mUngroupBarHoveredTextColor;
    }

    @VisibleForTesting
    static void setSourceRectCallbackForTesting(Callback<RectF> callback) {
        sSourceRectCallbackForTesting = callback;
    }

    @VisibleForTesting
    ScrimCoordinator getScrimCoordinatorForTesting() {
        return mScrimCoordinator;
    }

    @VisibleForTesting
    VisibilityListener getVisibilityListenerForTesting() {
        return mVisibilityListener;
    }
}
