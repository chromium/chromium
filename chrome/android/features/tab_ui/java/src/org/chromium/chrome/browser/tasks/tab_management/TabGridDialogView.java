// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
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

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/** Parent for TabGridDialog component. */
public class TabGridDialogView extends FrameLayout {
    private static final int DIALOG_ANIMATION_DURATION = 400;
    private static final int DIALOG_UNGROUP_ALPHA_ANIMATION_DURATION = 200;
    private static final int DIALOG_ALPHA_ANIMATION_DURATION = DIALOG_ANIMATION_DURATION >> 1;
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

    /** An interface to listen to visibility related changes on this {@link TabGridDialogView}. */
    interface VisibilityListener {
        /** Called when the animation to hide the tab grid dialog is finished. */
        void finishedHidingDialogView();
    }

    private final Context mContext;
    private final float mTabGridCardPadding;
    private FrameLayout mAnimationClip;
    private FrameLayout mToolbarContainer;
    private FrameLayout mRecyclerViewContainer;
    private View mBackgroundFrame;
    private View mAnimationCardView;
    private View mItemView;
    private View mUngroupBar;
    private ViewGroup mSnackBarContainer;
    private ViewGroup mParent;
    private ImageView mHairline;
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
    private int mAppHeaderHeight;
    private int mOrientation;
    private int mParentHeight;
    private int mParentWidth;
    private int mBackgroundDrawableColor;
    private @UngroupBarStatus int mUngroupBarStatus = UngroupBarStatus.HIDE;
    private int mUngroupBarBackgroundColor;
    private int mUngroupBarHoveredBackgroundColor;
    @ColorInt private int mUngroupBarTextColor;
    @ColorInt private int mUngroupBarHoveredTextColor;
    private Integer mBindingToken;

    public TabGridDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mTabGridCardPadding = TabUiThemeProvider.getTabGridCardMargin(mContext);
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

    void forceAnimationToFinish() {
        if (mCurrentDialogAnimator != null) {
            mCurrentDialogAnimator.end();
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            View v = findViewById(R.id.title);
            if (v != null && v.isFocused()) {
                Rect rect = new Rect();
                v.getGlobalVisibleRect(rect);
                if (!rect.contains((int) event.getRawX(), (int) event.getRawY())) {
                    v.clearFocus();
                }
            }
        }
        return super.dispatchTouchEvent(event);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mParent = (ViewGroup) getParent();
        mParentHeight = mParent.getHeight();
        mParentWidth = mParent.getWidth();
        mParentGlobalLayoutListener =
                () -> {
                    // Skip updating the parent view size caused by keyboard showing.
                    if (!KeyboardVisibilityDelegate.getInstance()
                            .isKeyboardShowing(mContext, this)) {
                        mParentWidth = mParent.getWidth();
                        mParentHeight = mParent.getHeight();
                        updateDialogWithOrientation(mOrientation);
                    }
                };
        mParent.getViewTreeObserver().addOnGlobalLayoutListener(mParentGlobalLayoutListener);
        updateDialogWithOrientation(mOrientation);
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
        mContainerParams =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mDialogContainerView = findViewById(R.id.dialog_container_view);
        mDialogContainerView.setLayoutParams(mContainerParams);
        mToolbarContainer = findViewById(R.id.tab_grid_dialog_toolbar_container);
        mRecyclerViewContainer = findViewById(R.id.tab_grid_dialog_recycler_view_container);
        mUngroupBar = findViewById(R.id.dialog_ungroup_bar);
        mUngroupBarTextView = mUngroupBar.findViewById(R.id.dialog_ungroup_bar_text);
        mAnimationClip = findViewById(R.id.dialog_animation_clip);
        mBackgroundFrame = findViewById(R.id.dialog_frame);
        mBackgroundFrame.setLayoutParams(mContainerParams);
        mAnimationCardView = findViewById(R.id.dialog_animation_card_view);
        mSnackBarContainer = findViewById(R.id.dialog_snack_bar_container_view);
        mHairline = findViewById(R.id.tab_grid_dialog_hairline);
        updateDialogWithOrientation(mContext.getResources().getConfiguration().orientation);

        prepareAnimation();
    }

    private void prepareAnimation() {
        mShowDialogAnimation = new AnimatorSet();
        mHideDialogAnimation = new AnimatorSet();
        mBasicFadeInAnimation = new AnimatorSet();
        ObjectAnimator dialogFadeInAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 0f, 1f);
        mBasicFadeInAnimation.play(dialogFadeInAnimator);
        mBasicFadeInAnimation.setInterpolator(Interpolators.EMPHASIZED);
        mBasicFadeInAnimation.setDuration(DIALOG_ANIMATION_DURATION);

        mBasicFadeOutAnimation = new AnimatorSet();
        ObjectAnimator dialogFadeOutAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 1f, 0f);
        mBasicFadeOutAnimation.play(dialogFadeOutAnimator);
        mBasicFadeOutAnimation.setInterpolator(Interpolators.EMPHASIZED);
        mBasicFadeOutAnimation.setDuration(DIALOG_ANIMATION_DURATION);
        mBasicFadeOutAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        updateItemViewAlpha();
                    }
                });

        final int screenHeightPx =
                ViewUtils.dpToPx(
                        getContext(),
                        getContext().getResources().getConfiguration().screenHeightDp);
        final float mDialogInitYPos = mDialogContainerView.getY();
        mYTranslateAnimation =
                ObjectAnimator.ofFloat(
                        mDialogContainerView, View.TRANSLATION_Y, mDialogInitYPos, screenHeightPx);
        mYTranslateAnimation.setInterpolator(Interpolators.EMPHASIZED_ACCELERATE);
        mYTranslateAnimation.setDuration(Y_TRANSLATE_DURATION_MS);
        mYTranslateAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        updateItemViewAlpha();
                        mDialogContainerView.setY(mDialogInitYPos);
                    }
                });

        mShowDialogAnimationListener =
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mCurrentDialogAnimator = null;
                        mDialogContainerView.requestFocus();
                        mDialogContainerView.sendAccessibilityEvent(
                                AccessibilityEvent.TYPE_VIEW_FOCUSED);
                        // TODO(crbug.com/40138401): Move clear/restore accessibility importance
                        // logic to ScrimView so that it can be shared by all components using
                        // ScrimView.
                        clearBackgroundViewAccessibilityImportance();
                    }
                };
        mHideDialogAnimationListener =
                new AnimatorListenerAdapter() {
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
        mUngroupBarShow.setInterpolator(Interpolators.EMPHASIZED);
        mUngroupBarShow.addListener(
                new AnimatorListenerAdapter() {
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
        mUngroupBarHide.setInterpolator(Interpolators.EMPHASIZED);
        mUngroupBarHide.addListener(
                new AnimatorListenerAdapter() {
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
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
                assert mAccessibilityImportanceMap.containsKey(view);
            }
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
            Animator hideAnimator =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                            ? mYTranslateAnimation
                            : mBasicFadeOutAnimation;
            mHideDialogAnimation.play(hideAnimator);
            mHideDialogAnimation.removeAllListeners();
            mHideDialogAnimation.addListener(mHideDialogAnimationListener);

            updateAnimationCardView(null);
            return;
        }

        mItemView = sourceView;
        Rect rect = new Rect();
        mItemView.getGlobalVisibleRect(rect);
        // Offset for status bar (top) and nav bar when landscape (left).
        Rect dialogParentRect = new Rect();
        mParent.getGlobalVisibleRect(dialogParentRect);
        rect.offset(-dialogParentRect.left, -dialogParentRect.top);
        // Setup a stand-in animation card that looks the same as the original tab grid card for
        // animation.
        updateAnimationCardView(mItemView);

        // Calculate dialog size.
        int dialogHeight = mParentHeight - 2 * mTopMargin;
        int dialogWidth = mParentWidth - 2 * mSideMargin;

        // Calculate a clip mask to avoid any source view that is not fully visible from drawing
        // over other UI.
        Rect itemViewParentRect = new Rect();
        ((View) mItemView.getParent()).getGlobalVisibleRect(itemViewParentRect);
        int clipTop = itemViewParentRect.top - dialogParentRect.top;
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mAnimationClip.getLayoutParams();
        params.setMargins(0, clipTop, 0, 0);
        mAnimationClip.setLayoutParams(params);

        // Because the mAnimationCardView is offset by clip top we need to compensate in the
        // opposite direction for its animation only.
        float yClipCompensation = clipTop;

        // If the item view is clipped by being offsceen the height of the visible rect and the
        // item view will differ. This is the `yClip`. If this amount is less than the card's
        // padding we need to still apply the part of the padding that is visible otherwise we
        // can ignore the padding entirely.
        int yClip = mItemView.getHeight() - rect.height();

        // The dialog and mBackgroundFrame are not clipped by the mAnimationClip (the math would be
        // broken due to those object relying on MATCH_PARENT for dimensions). So we need to use the
        // clipped height of the mItemView. Here we apply one side of the mTabGridCardPadding. The
        // other side might be clipped.
        float clippedSourceHeight = rect.height() - mTabGridCardPadding;

        // Apply the remaining tab grid card padding if the `yClip` doesn't result in it being
        // entirely occluded.
        boolean isYClipLessThanPadding = yClip < mTabGridCardPadding;
        if (isYClipLessThanPadding) {
            clippedSourceHeight += yClip - mTabGridCardPadding;
        }

        // Calculate position and size info about the original tab grid card.
        float sourceTop = rect.top;
        float sourceLeft = rect.left + mTabGridCardPadding;
        if (rect.top == clipTop) {
            // If the clipping is off the "top" of the screen i.e. the rect is touching the clip
            // bound. Then we need to add clip compensation when animating the card by starting it
            // in its original position. However, if the clip is less than padding we also need to
            // take whatever top padding is visible into account.
            if (isYClipLessThanPadding) {
                float clipDelta = mTabGridCardPadding - yClip;
                sourceTop += clipDelta;
                yClipCompensation += clipDelta + yClip;
            } else {
                yClipCompensation += yClip;
            }
        } else {
            // If the clipping either doesn't exist or is off the bottom of the screen we can assume
            // the clipping is to the bottom of the card and include the full padding.
            sourceTop += mTabGridCardPadding;
            yClipCompensation += mTabGridCardPadding;
        }
        float unclippedSourceHeight = mItemView.getHeight() - 2 * mTabGridCardPadding;
        float sourceWidth = rect.width() - 2 * mTabGridCardPadding;
        if (sSourceRectCallbackForTesting != null) {
            sSourceRectCallbackForTesting.onResult(
                    new RectF(
                            sourceLeft,
                            sourceTop,
                            sourceLeft + sourceWidth,
                            sourceTop + unclippedSourceHeight));
        }

        // Setup animation position info and scale ratio of the background frame.
        float frameInitYPosition =
                -(dialogHeight / 2 + mTopMargin - clippedSourceHeight / 2 - sourceTop);
        float frameInitXPosition = -(dialogWidth / 2 + mSideMargin - sourceWidth / 2 - sourceLeft);
        float frameScaleY = clippedSourceHeight / dialogHeight;
        float frameScaleX = sourceWidth / dialogWidth;

        // Setup scale ratio of card and dialog. Height and Width for both dialog and card scale at
        // the same rate during scaling animations.
        float cardScale = (float) dialogWidth / rect.width();
        float dialogScale = frameScaleX;

        // Setup animation position info of the animation card.
        float cardScaledYPosition =
                mTopMargin + ((cardScale - 1f) / 2) * unclippedSourceHeight - clipTop;
        float cardScaledXPosition = mSideMargin + ((cardScale - 1f) / 2) * sourceWidth;
        float cardInitYPosition = sourceTop - yClipCompensation;
        float cardInitXPosition = sourceLeft - mTabGridCardPadding;

        // Setup animation position info of the dialog.
        float dialogInitYPosition =
                frameInitYPosition - (clippedSourceHeight - (dialogHeight * dialogScale)) / 2f;
        float dialogInitXPosition = frameInitXPosition;

        // In the first half of the dialog showing animation, the animation card scales up and moves
        // towards where the dialog should be.
        final ObjectAnimator cardZoomOutMoveYAnimator =
                ObjectAnimator.ofFloat(
                        mAnimationCardView,
                        View.TRANSLATION_Y,
                        cardInitYPosition,
                        cardScaledYPosition);
        final ObjectAnimator cardZoomOutMoveXAnimator =
                ObjectAnimator.ofFloat(
                        mAnimationCardView,
                        View.TRANSLATION_X,
                        cardInitXPosition,
                        cardScaledXPosition);
        final ObjectAnimator cardZoomOutScaleXAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_X, 1f, cardScale);
        final ObjectAnimator cardZoomOutScaleYAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_Y, 1f, cardScale);

        AnimatorSet cardZoomOutAnimatorSet = new AnimatorSet();
        cardZoomOutAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        cardZoomOutAnimatorSet.setInterpolator(Interpolators.EMPHASIZED);
        cardZoomOutAnimatorSet
                .play(cardZoomOutMoveYAnimator)
                .with(cardZoomOutMoveXAnimator)
                .with(cardZoomOutScaleYAnimator)
                .with(cardZoomOutScaleXAnimator);

        // In the first half of the dialog showing animation, the animation card fades out as it
        // moves and scales up.
        final ObjectAnimator cardZoomOutAlphaAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.ALPHA, 1f, 0f);
        cardZoomOutAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        cardZoomOutAlphaAnimator.setInterpolator(Interpolators.EMPHASIZED);

        // In the second half of the dialog showing animation, the dialog zooms out from where the
        // card stops at the end of the first half and moves towards where the dialog should be.
        final ObjectAnimator dialogZoomOutMoveYAnimator =
                ObjectAnimator.ofFloat(
                        mDialogContainerView, View.TRANSLATION_Y, dialogInitYPosition, 0f);
        final ObjectAnimator dialogZoomOutMoveXAnimator =
                ObjectAnimator.ofFloat(
                        mDialogContainerView, View.TRANSLATION_X, dialogInitXPosition, 0f);
        final ObjectAnimator dialogZoomOutScaleYAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_Y, dialogScale, 1f);
        final ObjectAnimator dialogZoomOutScaleXAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_X, dialogScale, 1f);

        AnimatorSet dialogZoomOutAnimatorSet = new AnimatorSet();
        dialogZoomOutAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        dialogZoomOutAnimatorSet.setInterpolator(Interpolators.EMPHASIZED);
        dialogZoomOutAnimatorSet
                .play(dialogZoomOutMoveYAnimator)
                .with(dialogZoomOutMoveXAnimator)
                .with(dialogZoomOutScaleYAnimator)
                .with(dialogZoomOutScaleXAnimator);

        // In the second half of the dialog showing animation, the dialog fades in while it moves
        // and scales up.
        final ObjectAnimator dialogZoomOutAlphaAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.ALPHA, 0f, 1f);
        dialogZoomOutAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        dialogZoomOutAlphaAnimator.setStartDelay(DIALOG_ALPHA_ANIMATION_DURATION);
        dialogZoomOutAlphaAnimator.setInterpolator(Interpolators.EMPHASIZED);

        // During the whole dialog showing animation, the frame background scales up and moves so
        // that it looks like the card zooms out and becomes the dialog.
        final ObjectAnimator frameZoomOutMoveYAnimator =
                ObjectAnimator.ofFloat(
                        mBackgroundFrame, View.TRANSLATION_Y, frameInitYPosition, 0f);
        final ObjectAnimator frameZoomOutMoveXAnimator =
                ObjectAnimator.ofFloat(
                        mBackgroundFrame, View.TRANSLATION_X, frameInitXPosition, 0f);
        final ObjectAnimator frameZoomOutScaleYAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_Y, frameScaleY, 1f);
        final ObjectAnimator frameZoomOutScaleXAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_X, frameScaleX, 1f);

        AnimatorSet frameZoomOutAnimatorSet = new AnimatorSet();
        frameZoomOutAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        frameZoomOutAnimatorSet.setInterpolator(Interpolators.EMPHASIZED);
        frameZoomOutAnimatorSet
                .play(frameZoomOutMoveYAnimator)
                .with(frameZoomOutMoveXAnimator)
                .with(frameZoomOutScaleYAnimator)
                .with(frameZoomOutScaleXAnimator);

        // After the dialog showing animation starts, the original card in grid tab switcher fades
        // out.
        final ObjectAnimator tabFadeOutAnimator =
                ObjectAnimator.ofFloat(mItemView, View.ALPHA, 1f, 0f);
        tabFadeOutAnimator.setDuration(CARD_FADE_ANIMATION_DURATION);

        dialogZoomOutAnimatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        // At the beginning of the first half of the showing animation, the white
                        // frame and the animation card should be above the the dialog view, and
                        // their alpha should be set to 1.
                        mBackgroundFrame.bringToFront();
                        mAnimationClip.bringToFront();
                        mAnimationCardView.bringToFront();
                        mDialogContainerView.setAlpha(0f);
                        mBackgroundFrame.setAlpha(1f);
                        mAnimationCardView.setAlpha(1f);
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // At the end of the showing animation, reset the alpha of animation related
                        // views to 0.
                        mBackgroundFrame.setAlpha(0f);
                        mAnimationCardView.setAlpha(0f);
                    }
                });

        dialogZoomOutAlphaAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        // At the beginning of the second half of the showing animation, the dialog
                        // should be above the white frame and the animation card.
                        mDialogContainerView.bringToFront();
                    }
                });

        // Setup the dialog showing animation.
        mShowDialogAnimation = new AnimatorSet();
        mShowDialogAnimation
                .play(cardZoomOutAnimatorSet)
                .with(cardZoomOutAlphaAnimator)
                .with(frameZoomOutAnimatorSet)
                .with(dialogZoomOutAnimatorSet)
                .with(dialogZoomOutAlphaAnimator)
                .with(tabFadeOutAnimator);
        mShowDialogAnimation.addListener(mShowDialogAnimationListener);

        // In the first half of the dialog hiding animation, the dialog scales down and moves
        // towards where the tab grid card should be.
        final ObjectAnimator dialogZoomInMoveYAnimator =
                ObjectAnimator.ofFloat(
                        mDialogContainerView, View.TRANSLATION_Y, 0f, dialogInitYPosition);
        final ObjectAnimator dialogZoomInMoveXAnimator =
                ObjectAnimator.ofFloat(
                        mDialogContainerView, View.TRANSLATION_X, 0f, dialogInitXPosition);
        final ObjectAnimator dialogZoomInScaleYAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_Y, 1f, dialogScale);
        final ObjectAnimator dialogZoomInScaleXAnimator =
                ObjectAnimator.ofFloat(mDialogContainerView, View.SCALE_X, 1f, dialogScale);

        AnimatorSet dialogZoomInAnimatorSet = new AnimatorSet();
        dialogZoomInAnimatorSet
                .play(dialogZoomInMoveYAnimator)
                .with(dialogZoomInMoveXAnimator)
                .with(dialogZoomInScaleYAnimator)
                .with(dialogZoomInScaleXAnimator);
        dialogZoomInAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        dialogZoomInAnimatorSet.setInterpolator(Interpolators.EMPHASIZED);

        dialogZoomInAnimatorSet.addListener(
                new AnimatorListenerAdapter() {
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
        dialogZoomInAlphaAnimator.setInterpolator(Interpolators.EMPHASIZED);

        // In the second half of the dialog hiding animation, the animation card zooms in from where
        // the dialog stops at the end of the first half and moves towards where the card should be.
        final ObjectAnimator cardZoomInMoveYAnimator =
                ObjectAnimator.ofFloat(
                        mAnimationCardView,
                        View.TRANSLATION_Y,
                        cardScaledYPosition,
                        cardInitYPosition);
        final ObjectAnimator cardZoomInMoveXAnimator =
                ObjectAnimator.ofFloat(
                        mAnimationCardView,
                        View.TRANSLATION_X,
                        cardScaledXPosition,
                        cardInitXPosition);
        final ObjectAnimator cardZoomInScaleXAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_X, cardScale, 1f);
        final ObjectAnimator cardZoomInScaleYAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.SCALE_Y, cardScale, 1f);

        AnimatorSet cardZoomInAnimatorSet = new AnimatorSet();
        cardZoomInAnimatorSet
                .play(cardZoomInMoveYAnimator)
                .with(cardZoomInMoveXAnimator)
                .with(cardZoomInScaleXAnimator)
                .with(cardZoomInScaleYAnimator);
        cardZoomInAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        cardZoomInAnimatorSet.setInterpolator(Interpolators.EMPHASIZED);

        // In the second half of the dialog hiding animation, the tab grid card fades in while it
        // scales down and moves.
        final ObjectAnimator cardZoomInAlphaAnimator =
                ObjectAnimator.ofFloat(mAnimationCardView, View.ALPHA, 0f, 1f);
        cardZoomInAlphaAnimator.setDuration(DIALOG_ALPHA_ANIMATION_DURATION);
        cardZoomInAlphaAnimator.setStartDelay(DIALOG_ALPHA_ANIMATION_DURATION);
        cardZoomInAlphaAnimator.setInterpolator(Interpolators.EMPHASIZED);

        cardZoomInAlphaAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        // At the beginning of the second half of the hiding animation, the white
                        // frame and the animation card should be above the the dialog view.
                        mBackgroundFrame.bringToFront();
                        mAnimationClip.bringToFront();
                        mAnimationCardView.bringToFront();
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        // At the end of the hiding animation, reset the alpha of animation related
                        // views to
                        // 0.
                        mBackgroundFrame.setAlpha(0f);
                        mAnimationCardView.setAlpha(0f);
                    }
                });

        // During the whole dialog hiding animation, the frame background scales down and moves so
        // that it looks like the dialog zooms in and becomes the card.
        final ObjectAnimator frameZoomInMoveYAnimator =
                ObjectAnimator.ofFloat(
                        mBackgroundFrame, View.TRANSLATION_Y, 0f, frameInitYPosition);
        final ObjectAnimator frameZoomInMoveXAnimator =
                ObjectAnimator.ofFloat(
                        mBackgroundFrame, View.TRANSLATION_X, 0f, frameInitXPosition);
        final ObjectAnimator frameZoomInScaleYAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_Y, 1f, frameScaleY);
        final ObjectAnimator frameZoomInScaleXAnimator =
                ObjectAnimator.ofFloat(mBackgroundFrame, View.SCALE_X, 1f, frameScaleX);

        AnimatorSet frameZoomInAnimatorSet = new AnimatorSet();
        frameZoomInAnimatorSet
                .play(frameZoomInMoveYAnimator)
                .with(frameZoomInMoveXAnimator)
                .with(frameZoomInScaleYAnimator)
                .with(frameZoomInScaleXAnimator);
        frameZoomInAnimatorSet.setDuration(DIALOG_ANIMATION_DURATION);
        frameZoomInAnimatorSet.setInterpolator(Interpolators.EMPHASIZED);

        frameZoomInAnimatorSet.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        // At the beginning of the hiding animation, the alpha of white frame needs
                        // to be restored to 1.
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
        mHideDialogAnimation
                .play(dialogZoomInAnimatorSet)
                .with(dialogZoomInAlphaAnimator)
                .with(frameZoomInAnimatorSet)
                .with(cardZoomInAnimatorSet)
                .with(cardZoomInAlphaAnimator)
                .with(tabFadeInAnimator);
        mHideDialogAnimation.addListener(mHideDialogAnimationListener);
    }

    @VisibleForTesting
    void updateDialogWithOrientation(int orientation) {
        Resources res = mContext.getResources();
        int minMargin = res.getDimensionPixelSize(R.dimen.tab_grid_dialog_min_margin);
        int maxMargin = res.getDimensionPixelSize(R.dimen.tab_grid_dialog_max_margin);
        int sideMargin;
        int topMargin;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            sideMargin = minMargin;
            topMargin =
                    clampMargin(
                            Math.round(mParentHeight * 0.1f) + mAppHeaderHeight,
                            minMargin,
                            maxMargin);
        } else {
            sideMargin = clampMargin(Math.round(mParentWidth * 0.1f), minMargin, maxMargin);
            topMargin = clampMargin(minMargin + mAppHeaderHeight, minMargin, maxMargin);
        }
        if (mSideMargin != sideMargin || mTopMargin != topMargin) {
            mSideMargin = sideMargin;
            mTopMargin = topMargin;
            mContainerParams.setMargins(mSideMargin, mTopMargin, mSideMargin, mTopMargin);
            // Set params to force requestLayout() to reflect margin immediately.
            mDialogContainerView.setLayoutParams(mContainerParams);
        }
        mOrientation = orientation;
    }

    private int clampMargin(int sizeAdjustedValue, int lowerBound, int upperBound) {
        // In the event the parent isn't laid out yet just default to the upper bound.
        if (sizeAdjustedValue == 0) return upperBound;

        return MathUtils.clamp(sizeAdjustedValue, lowerBound, upperBound);
    }

    void setAppHeaderHeight(int height) {
        mAppHeaderHeight = height;
        updateDialogWithOrientation(mOrientation);
    }

    private void updateAnimationCardView(View view) {
        View animationCard = mAnimationCardView;
        TextView cardTitle = animationCard.findViewById(R.id.tab_title);
        ImageView cardFavicon = animationCard.findViewById(R.id.tab_favicon);
        TabThumbnailView cardThumbnail = animationCard.findViewById(R.id.tab_thumbnail);
        ImageView cardActionButton = animationCard.findViewById(R.id.action_button);
        View cardBackground = animationCard.findViewById(R.id.background_view);
        cardBackground.setBackground(null);

        if (view == null) {
            cardFavicon.setImageDrawable(null);
            cardTitle.setText("");
            cardThumbnail.setImageDrawable(null);
            cardActionButton.setImageDrawable(null);
            return;
        }

        // Update the stand-in animation card view with the actual item view from grid tab switcher
        // recyclerView.
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) animationCard.getLayoutParams();
        params.width = view.getWidth();
        params.height = view.getHeight();
        animationCard.setLayoutParams(params);
        TextView viewTitle = view.findViewById(R.id.tab_title);
        if (viewTitle == null) {
            return;
        }

        // Sometimes we get clip artifacting when sharing a drawable, unclear why, so make a copy.
        Drawable backgroundCopy =
                view.findViewById(R.id.card_view).getBackground().getConstantState().newDrawable();
        animationCard.findViewById(R.id.card_view).setBackground(backgroundCopy);

        Drawable faviconDrawable = ((ImageView) view.findViewById(R.id.tab_favicon)).getDrawable();
        if (faviconDrawable != null) {
            cardFavicon.setImageDrawable(faviconDrawable);
        } else {
            // Draw the tab group color dot to the bitmap and put it in the favicon container as it
            // isn't possible to clone the whole view.
            FrameLayout containerView = view.findViewById(R.id.tab_group_color_view_container);
            int childCount = containerView.getChildCount();
            if (childCount != 0) {
                assert childCount == 1;
                View v = containerView.getChildAt(0);

                Bitmap bitmap =
                        Bitmap.createBitmap(v.getWidth(), v.getHeight(), Bitmap.Config.ARGB_8888);
                Canvas canvas = new Canvas(bitmap);
                v.draw(canvas);
                cardFavicon.setImageBitmap(bitmap);
            }
        }

        cardTitle.setText(viewTitle.getText());
        cardTitle.setTextAppearance(R.style.TextAppearance_TextMediumThick_Primary);
        cardTitle.setTextColor(viewTitle.getTextColors());

        TabThumbnailView originalThumbnailView = view.findViewById(R.id.tab_thumbnail);
        if (originalThumbnailView.isPlaceholder()) {
            cardThumbnail.setImageDrawable(null);
        } else {
            cardThumbnail.setImageDrawable(originalThumbnailView.getDrawable());
            cardThumbnail.setImageMatrix(originalThumbnailView.getImageMatrix());
            cardThumbnail.setScaleType(originalThumbnailView.getScaleType());
        }

        ImageView originalActionButton = view.findViewById(R.id.action_button);
        cardActionButton.setImageDrawable(originalActionButton.getDrawable());
        ImageViewCompat.setImageTintList(
                cardActionButton, ImageViewCompat.getImageTintList(originalActionButton));
    }

    /**
     * Setup the {@link PropertyModel} used to show scrim view.
     *
     * @param scrimClickRunnable The {@link Runnable} that runs when scrim view is clicked.
     */
    void setScrimClickRunnable(Runnable scrimClickRunnable) {
        mScrimPropertyModel =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
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
        mToolbarContainer.removeAllViews();
        mToolbarContainer.addView(toolbarView);
        mRecyclerViewContainer.removeAllViews();
        mRecyclerViewContainer.addView(recyclerView);

        recyclerView.setVisibility(View.VISIBLE);
    }

    void refreshScrim() {
        assert mScrimCoordinator != null && mScrimPropertyModel != null;
        mScrimCoordinator.showScrim(mScrimPropertyModel);
    }

    /** Show {@link PopupWindow} for dialog with animation. */
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

    /** Hide {@link PopupWindow} for dialog with animation. */
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
     *     should be updated to.
     */
    void updateUngroupBar(@UngroupBarStatus int status) {
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

    void updateUngroupBarText(String ungroupBarText) {
        mUngroupBarTextView.setText(ungroupBarText);
    }

    /**
     * Update the dialog container background color.
     *
     * @param backgroundColor The new background color to use.
     */
    void updateDialogContainerBackgroundColor(int backgroundColor) {
        mBackgroundDrawableColor = backgroundColor;
        DrawableCompat.setTint(mDialogContainerView.getBackground(), backgroundColor);
        DrawableCompat.setTint(mBackgroundFrame.getBackground(), backgroundColor);
    }

    void updateHairlineColor(@ColorInt int hairlineColor) {
        mHairline.setImageTintList(ColorStateList.valueOf(hairlineColor));
    }

    void setHairlineVisibility(boolean visible) {
        mHairline.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    /**
     * Updates the background color for the animation card.
     *
     * @param colorInt The new color to use.
     */
    void updateAnimationBackgroundColor(@ColorInt int colorInt) {
        assert TabUiFeatureUtilities.shouldUseListMode();
        updateAnimationCardView(null);
        Drawable animationBackground =
                mAnimationCardView.findViewById(R.id.card_view).getBackground();
        DrawableCompat.setTint(animationBackground, colorInt);
    }

    /**
     * Update the ungroup bar background color.
     *
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

    /** Return the container view for undo closure snack bar. */
    ViewGroup getSnackBarContainer() {
        return mSnackBarContainer;
    }

    void setBindingToken(Integer bindingToken) {
        assert mBindingToken == null || bindingToken == null;
        mBindingToken = bindingToken;
    }

    Integer getBindingToken() {
        return mBindingToken;
    }

    Animator getCurrentDialogAnimatorForTesting() {
        return mCurrentDialogAnimator;
    }

    Animator getCurrentUngroupBarAnimatorForTesting() {
        return mCurrentUngroupBarAnimator;
    }

    int getUngroupBarStatusForTesting() {
        return mUngroupBarStatus;
    }

    AnimatorSet getShowDialogAnimationForTesting() {
        return mShowDialogAnimation;
    }

    int getBackgroundColorForTesting() {
        return mBackgroundDrawableColor;
    }

    int getUngroupBarBackgroundColorForTesting() {
        return mUngroupBarBackgroundColor;
    }

    int getUngroupBarHoveredBackgroundColorForTesting() {
        return mUngroupBarHoveredBackgroundColor;
    }

    int getUngroupBarTextColorForTesting() {
        return mUngroupBarTextColor;
    }

    int getUngroupBarHoveredTextColorForTesting() {
        return mUngroupBarHoveredTextColor;
    }

    static void setSourceRectCallbackForTesting(Callback<RectF> callback) {
        sSourceRectCallbackForTesting = callback;
        ResettersForTesting.register(() -> sSourceRectCallbackForTesting = null);
    }

    ScrimCoordinator getScrimCoordinatorForTesting() {
        return mScrimCoordinator;
    }

    VisibilityListener getVisibilityListenerForTesting() {
        return mVisibilityListener;
    }

    int getAppHeaderHeightForTesting() {
        return mAppHeaderHeight;
    }
}
