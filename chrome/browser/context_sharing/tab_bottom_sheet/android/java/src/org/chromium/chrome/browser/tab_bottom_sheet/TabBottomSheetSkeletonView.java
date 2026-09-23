// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.PathInterpolator;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.ui.animation.AnimationHandler;
import org.chromium.ui.animation.AnimationListeners;

import java.util.ArrayList;
import java.util.List;

/**
 * Custom passive view displaying a skeleton loader for the Tab Bottom Sheet during WebContents
 * resize. Comprises a static peek header at the top and a skeleton group at the bottom, managing
 * header icon configuration, element tinting, parent clipping restoration, per-element proximity
 * fade, and NTP-style staggered wave animation.
 */
@NullMarked
public class TabBottomSheetSkeletonView extends FrameLayout {
    @VisibleForTesting static final int FADE_DURATION_MS = 620;
    @VisibleForTesting static final int FADE_STAGGER_MS = 83;
    @VisibleForTesting static final float HIGH_OPACITY = 1.0f;
    @VisibleForTesting static final float LOW_OPACITY = 0.6f;

    private static final PathInterpolator FADE_CYCLE_CURVE =
            new PathInterpolator(0.33f, 0f, 0.83f, 0.83f);

    @VisibleForTesting
    static class ElementAlphaArbitrator {
        private final View mView;
        private float mWaveAlpha = HIGH_OPACITY;
        private float mResizeAlpha = 1.0f;

        ElementAlphaArbitrator(View view) {
            mView = view;
        }

        View getView() {
            return mView;
        }

        void setWaveAlpha(float waveAlpha) {
            mWaveAlpha = waveAlpha;
            mView.setAlpha(computeAlpha());
        }

        void setResizeAlpha(float resizeAlpha) {
            mResizeAlpha = resizeAlpha;
            mView.setAlpha(computeAlpha());
        }

        float getResizeAlpha() {
            return mResizeAlpha;
        }

        float computeAlpha() {
            return mWaveAlpha * mResizeAlpha;
        }
    }

    private final AnimationHandler mAnimationHandler = new AnimationHandler();
    private final List<ElementAlphaArbitrator> mElementArbitrators = new ArrayList<>();
    private @Nullable FrameLayout mHeaderContainer;
    private @Nullable LinearLayout mBottomGroup;
    private @Nullable TabBottomSheetPeekView mPeekView;
    private @Nullable View mBar1;
    private @Nullable View mBar2;
    private @Nullable View mBar3;
    private @Nullable View mPill;
    private boolean mIsResizing;

    /**
     * Constructor for inflating via XML with attributes.
     *
     * @param context The Android context.
     * @param attrs The XML attributes.
     */
    public TabBottomSheetSkeletonView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHeaderContainer = findViewById(R.id.skeleton_header_container);
        mBottomGroup = findViewById(R.id.skeleton_bottom_group);
        mPeekView = findViewById(R.id.peek_view);
        mBar1 = findViewById(R.id.skeleton_bar_1);
        mBar2 = findViewById(R.id.skeleton_bar_2);
        mBar3 = findViewById(R.id.skeleton_bar_3);
        mPill = findViewById(R.id.skeleton_pill);
        mElementArbitrators.clear();
        if (mBottomGroup != null) {
            mBottomGroup.setGravity(Gravity.BOTTOM);
            for (int i = 0; i < mBottomGroup.getChildCount(); i++) {
                mElementArbitrators.add(new ElementAlphaArbitrator(mBottomGroup.getChildAt(i)));
            }
        }
    }

    /**
     * Sets the header icon drawable resource.
     *
     * @param iconResId The drawable resource ID for the header icon.
     */
    public void setHeaderIcon(@DrawableRes int iconResId) {
        if (mPeekView != null) {
            mPeekView.setPeekIcon(iconResId);
        }
    }

    /**
     * Sets the tint color for all skeleton elements inside the bottom group.
     *
     * @param placeholderElemColor The resolved color for the skeleton bars and pill box.
     */
    public void setPlaceholderElemColor(@ColorInt int placeholderElemColor) {
        ColorStateList tintList = ColorStateList.valueOf(placeholderElemColor);
        if (mBar1 != null) mBar1.setBackgroundTintList(tintList);
        if (mBar2 != null) mBar2.setBackgroundTintList(tintList);
        if (mBar3 != null) mBar3.setBackgroundTintList(tintList);
        if (mPill != null) mPill.setBackgroundTintList(tintList);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (getParent() instanceof ViewGroup parent) {
            parent.setClipChildren(false);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (getParent() instanceof ViewGroup parent) {
            parent.setClipChildren(true);
        }
    }

    /**
     * Sets whether the sheet is actively resizing.
     *
     * @param isResizing True if resizing mode is active, false otherwise.
     */
    public void setIsResizing(boolean isResizing) {
        if (mIsResizing != isResizing) {
            mIsResizing = isResizing;
            updateAnimationState();
        }
    }

    /**
     * Updates the proximity alpha of each skeleton element inside the bottom group sequentially as
     * the header container approaches or moves away from it.
     *
     * @param visibleHeight The current visible height of the skeleton view in pixels.
     * @param bufferPx The distance buffer in pixels before collision over which the first element
     *     fades.
     */
    public void updateChildAlphas(@Px int visibleHeight, @Px int bufferPx) {
        int headerHeight = getHeaderHeight();
        if (mBottomGroup == null || headerHeight == 0 || mElementArbitrators.isEmpty()) return;

        int firstTop = mElementArbitrators.get(0).getView().getTop();
        int lastBottom =
                mElementArbitrators.get(mElementArbitrators.size() - 1).getView().getBottom();
        if (lastBottom <= firstTop) return;

        int baseHeight = headerHeight + (lastBottom - firstTop) + mBottomGroup.getPaddingBottom();
        int maxHeight = baseHeight + bufferPx;
        for (ElementAlphaArbitrator arbitrator : mElementArbitrators) {
            int minHeight = baseHeight - (arbitrator.getView().getTop() - firstTop);
            arbitrator.setResizeAlpha(computeResizeAlpha(visibleHeight, minHeight, maxHeight));
            maxHeight = minHeight;
        }
        updateAnimationState();
    }

    private static float computeResizeAlpha(
            @Px int visibleHeight, @Px int minHeight, @Px int maxHeight) {
        if (visibleHeight <= minHeight) return 0f;
        if (visibleHeight >= maxHeight) return 1f;
        return (float) (visibleHeight - minHeight) / (maxHeight - minHeight);
    }

    private void updateAnimationState() {
        if (mBottomGroup == null || mElementArbitrators.isEmpty()) return;

        boolean hasVisibleChild =
                mElementArbitrators.get(mElementArbitrators.size() - 1).getResizeAlpha() > 0f;
        boolean shouldAnimate =
                mIsResizing && hasVisibleChild && ValueAnimator.areAnimatorsEnabled();
        if (shouldAnimate) {
            if (!mAnimationHandler.isAnimationPresent()) {
                mAnimationHandler.startAnimation(createWaveAnimatorSet());
            }
        } else {
            mAnimationHandler.forceFinishAnimation();
        }
    }

    @VisibleForTesting
    AnimatorSet createWaveAnimatorSet() {
        int count = mElementArbitrators.size();
        List<Animator> animators = new ArrayList<>(count);
        for (int i = 0; i < count; i++) {
            ElementAlphaArbitrator arbitrator = mElementArbitrators.get(i);
            ValueAnimator pulse = ValueAnimator.ofFloat(HIGH_OPACITY, LOW_OPACITY);
            pulse.addUpdateListener(
                    animation -> arbitrator.setWaveAlpha((float) animation.getAnimatedValue()));
            pulse.setStartDelay((long) i * FADE_STAGGER_MS);
            pulse.setDuration(FADE_DURATION_MS);
            pulse.setInterpolator(FADE_CYCLE_CURVE);
            pulse.setRepeatCount(ValueAnimator.INFINITE);
            pulse.setRepeatMode(ValueAnimator.REVERSE);
            animators.add(pulse);
        }

        AnimatorSet animatorSet = new AnimatorSet();
        animatorSet.playTogether(animators);
        animatorSet.addListener(AnimationListeners.onAnimationEnd(this::resetChildAlphas));
        return animatorSet;
    }

    private void resetChildAlphas() {
        for (ElementAlphaArbitrator arbitrator : mElementArbitrators) {
            arbitrator.setWaveAlpha(HIGH_OPACITY);
        }
    }

    /** Returns the measured height of the header container, or 0 if not laid out. */
    @Px
    public int getHeaderHeight() {
        return mHeaderContainer != null ? mHeaderContainer.getHeight() : 0;
    }

    /** Returns the measured height of the bottom skeleton group, or 0 if not laid out. */
    @Px
    public int getBottomGroupHeight() {
        return mBottomGroup != null ? mBottomGroup.getHeight() : 0;
    }

    @Nullable ViewGroup getBottomGroupForTesting() {
        return mBottomGroup;
    }

    @Nullable FrameLayout getHeaderContainerForTesting() {
        return mHeaderContainer;
    }

    AnimationHandler getAnimationHandlerForTesting() {
        return mAnimationHandler;
    }

    @Nullable TabBottomSheetPeekView getPeekViewForTesting() {
        return mPeekView;
    }
}
