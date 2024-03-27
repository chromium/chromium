// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.PathInterpolator;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.appcompat.widget.AppCompatImageView;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

import java.util.ArrayList;
import java.util.List;

/** A {@link LinearLayout} that shows loading placeholder for Feed cards. */
public class FeedPlaceholderLayout extends LinearLayout {
    private static final String TAG = "FeedPlaceholder";

    /** Command line flag to allow rendering tests to disable animation. */
    public static final String DISABLE_ANIMATION_SWITCH = "disable-feed-placeholder-animation";

    private static final int CARD_MARGIN_DP = 12;
    private static final int CARD_TOP_PADDING_DP = 15;
    private static final int IMAGE_PLACEHOLDER_BOTTOM_PADDING_DP = 72;
    private static final int IMAGE_PLACEHOLDER_BOTTOM_PADDING_DENSE_DP = 48;
    private static final int IMAGE_PLACEHOLDER_SIZE_DP = 92;
    private static final int TEXT_CONTENT_HEIGHT_DP = 80;
    private static final int TEXT_PLACEHOLDER_HEIGHT_DP = 20;
    private static final int TEXT_PLACEHOLDER_RADIUS_DP = 12;
    private static final int LARGE_IMAGE_HEIGHT_DP = 207;

    private static final int START_DELAY_MS = 0;
    private static final int FADE_DURATION_MS = 620;
    private static final PathInterpolator INITIAL_FADE_IN_CURVE =
            new PathInterpolator(0.17f, 0.17f, 0.85f, 1f);
    private static final int FADE_STAGGER_MS = 83;
    private static final float HIGH_OPACITY = 1f;
    private static final float LOW_OPACITY = .6f;
    private static final PathInterpolator FADE_CYCLE_CURVE =
            new PathInterpolator(0.33f, 0f, 0.83f, 0.83f);

    private static final int MOVE_UP_DURATION_MS = 1283;
    private static final int MOVE_UP_DP = 33;
    private static final PathInterpolator MOVE_UP_CURVE =
            new PathInterpolator(0.17f, 0.17f, 0f, 1f);

    private final Context mContext;
    private final Resources mResources;
    private long mLayoutInflationCompleteMs;
    private int mScreenWidthDp;
    private boolean mIsFirstCardDense;
    private UiConfig mUiConfig;

    private final List<Animator> mFadeInAndMoveUpAnimators = new ArrayList<>();
    private final List<Animator> mFadeBounceAnimators = new ArrayList<>();
    private AnimatorSet mAllAnimations = new AnimatorSet();
    private final AnimatorSet mFadeInAndMoveUp = new AnimatorSet();
    private final AnimatorSet mFadeBounce = new AnimatorSet();

    public FeedPlaceholderLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mResources = mContext.getResources();
        mScreenWidthDp = mResources.getConfiguration().screenWidthDp;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mUiConfig = new UiConfig(this);
        setPlaceholders();
        mLayoutInflationCompleteMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        mUiConfig.updateDisplayStyle();
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        updateAnimationState(isAttachedToWindow());
    }

    @Override
    protected void onDetachedFromWindow() {
        // isAttachedToWindow() doesn't turn false during onDetachedFromWindow(), so we pass the new
        // attachment state into updateAnimationState() here explicitly.
        updateAnimationState(/* isAttached= */ false);
        super.onDetachedFromWindow();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        updateAnimationState(/* isAttached= */ true);
    }

    private void updateAnimationState(boolean isAttached) {
        // Some Android versions call onVisibilityChanged() during the View's constructor.
        if (mAllAnimations == null) return;

        boolean visible = isShown() && isAttached;
        if (mAllAnimations.isStarted() && !visible) {
            Log.d(TAG, "Canceling animation.");
            mAllAnimations.cancel();
        } else if (!mAllAnimations.isStarted() && visible) {
            Log.d(TAG, "Restarting animation.");
            mAllAnimations.start();
        }
    }

    /**
     * Set the header blank for the placeholder.The header blank should be consistent with the
     * sectionHeaderView of {@link ExploreSurfaceCoordinator.FeedSurfaceController#}
     */
    public void setBlankHeaderHeight(int headerHeight) {
        LinearLayout headerView = findViewById(R.id.feed_placeholder_header);
        ViewGroup.LayoutParams lp = headerView.getLayoutParams();
        lp.height = headerHeight;
        headerView.setLayoutParams(lp);
    }

    private void setPlaceholders() {
        LinearLayout cardsParentView = findViewById(R.id.placeholders_layout);
        cardsParentView.removeAllViews();

        LinearLayout.LayoutParams lp =
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dpToPx(CARD_MARGIN_DP);

        // Set the First placeholder container - an image-right card. If it's in landscape mode, the
        // placeholder should always show in dense mode.
        mIsFirstCardDense =
                getResources().getConfiguration().orientation
                        == Configuration.ORIENTATION_LANDSCAPE;

        // The start delays of views' opacity animations are staggered. fadeStartDelayMs keeps track
        // of what the next view's opacity animation start delay should be.
        int fadeStartDelayMs = setPlaceholders(cardsParentView, true, lp, 0);

        // Set the second and the third placeholder containers - the large image on the top.
        fadeStartDelayMs = setPlaceholders(cardsParentView, false, lp, fadeStartDelayMs);
        setPlaceholders(cardsParentView, false, lp, fadeStartDelayMs);

        mFadeInAndMoveUp.setStartDelay(START_DELAY_MS);
        mFadeInAndMoveUp.playTogether(mFadeInAndMoveUpAnimators);
        mFadeBounce.playTogether(mFadeBounceAnimators);

        // Put animations in order.
        mAllAnimations.play(mFadeInAndMoveUp).before(mFadeBounce);
    }

    private int setPlaceholders(
            LinearLayout parent,
            boolean isSmallCard,
            ViewGroup.LayoutParams lp,
            int fadeStartDelayMs) {
        LinearLayout container = new LinearLayout(mContext);
        container.setLayoutParams(lp);
        container.setOrientation(isSmallCard ? HORIZONTAL : VERTICAL);
        ImageView imagePlaceholder = getImagePlaceholder(isSmallCard);
        ImageView textPlaceholder = getTextPlaceholder(isSmallCard);

        container.addView(
                isSmallCard
                        ? animate(textPlaceholder, fadeStartDelayMs)
                        : animate(imagePlaceholder, fadeStartDelayMs));
        fadeStartDelayMs += FADE_STAGGER_MS;
        container.addView(
                isSmallCard
                        ? animate(imagePlaceholder, fadeStartDelayMs)
                        : animate(textPlaceholder, fadeStartDelayMs));
        fadeStartDelayMs += FADE_STAGGER_MS;

        parent.addView(container);
        return fadeStartDelayMs;
    }

    private View animate(View view, int fadeStartDelayMs) {
        if (CommandLine.getInstance().hasSwitch(DISABLE_ANIMATION_SWITCH)) {
            return view;
        }

        // First, fade in from nothing.
        view.setAlpha(0f);
        view.setVisibility(View.VISIBLE);

        ObjectAnimator initialFadeIn = ObjectAnimator.ofFloat(view, "alpha", 0f, HIGH_OPACITY);
        initialFadeIn.setStartDelay(fadeStartDelayMs);
        initialFadeIn.setDuration(FADE_DURATION_MS);
        initialFadeIn.setInterpolator(INITIAL_FADE_IN_CURVE);
        mFadeInAndMoveUpAnimators.add(initialFadeIn);

        ObjectAnimator moveUp =
                ObjectAnimator.ofFloat(view, "translationY", dpToPx(MOVE_UP_DP), 0f);
        moveUp.setDuration(MOVE_UP_DURATION_MS);
        moveUp.setInterpolator(MOVE_UP_CURVE);
        mFadeInAndMoveUpAnimators.add(moveUp);

        ObjectAnimator pulse = ObjectAnimator.ofFloat(view, "alpha", HIGH_OPACITY, LOW_OPACITY);
        pulse.setStartDelay(fadeStartDelayMs);
        pulse.setDuration(FADE_DURATION_MS);
        pulse.setInterpolator(FADE_CYCLE_CURVE);
        pulse.setRepeatCount(ValueAnimator.INFINITE);
        pulse.setRepeatMode(ValueAnimator.REVERSE);
        mFadeBounceAnimators.add(pulse);

        return view;
    }

    private ImageView getImagePlaceholder(boolean isSmallCard) {
        LinearLayout.LayoutParams imagePlaceholderLp =
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        ImageView imagePlaceholder = new AppCompatImageView(mContext);
        imagePlaceholder.setImageDrawable(
                isSmallCard ? getSmallImageDrawable() : getLargeImageDrawable());
        imagePlaceholder.setLayoutParams(imagePlaceholderLp);
        imagePlaceholder.setScaleType(ImageView.ScaleType.FIT_XY);
        return imagePlaceholder;
    }

    private LayerDrawable getSmallImageDrawable() {
        int imageSize = dpToPx(IMAGE_PLACEHOLDER_SIZE_DP);
        int top = dpToPx(CARD_TOP_PADDING_DP);
        GradientDrawable[] placeholder = getRectangles(1, imageSize, imageSize);
        LayerDrawable layerDrawable = new LayerDrawable(placeholder);
        layerDrawable.setLayerInset(
                0,
                0,
                top,
                0,
                mIsFirstCardDense
                        ? dpToPx(IMAGE_PLACEHOLDER_BOTTOM_PADDING_DENSE_DP)
                        : dpToPx(IMAGE_PLACEHOLDER_BOTTOM_PADDING_DP));
        return layerDrawable;
    }

    private LayerDrawable getLargeImageDrawable() {
        GradientDrawable[] placeholder =
                getRectangles(1, dpToPx(mScreenWidthDp), dpToPx(LARGE_IMAGE_HEIGHT_DP));
        return new LayerDrawable(placeholder);
    }

    private ImageView getTextPlaceholder(boolean isSmallCard) {
        int top = dpToPx(CARD_TOP_PADDING_DP);
        int left = top / 2;
        int height = dpToPx(TEXT_PLACEHOLDER_HEIGHT_DP);
        int width = dpToPx(mScreenWidthDp);
        int contentHeight = dpToPx(TEXT_CONTENT_HEIGHT_DP);

        LinearLayout.LayoutParams textPlaceholderLp =
                isSmallCard
                        ? new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1)
                        : new LinearLayout.LayoutParams(
                                ViewGroup.LayoutParams.WRAP_CONTENT,
                                ViewGroup.LayoutParams.WRAP_CONTENT);

        LayerDrawable layerDrawable =
                isSmallCard
                        ? getSmallTextDrawable(top, width, height, contentHeight)
                        : getLargeTextDrawable(top, left, width, height, contentHeight + 2 * top);

        ImageView textPlaceholder = new AppCompatImageView(mContext);
        textPlaceholder.setImageDrawable(layerDrawable);
        textPlaceholder.setLayoutParams(textPlaceholderLp);
        textPlaceholder.setScaleType(ImageView.ScaleType.FIT_XY);
        return textPlaceholder;
    }

    private LayerDrawable getSmallTextDrawable(int top, int width, int height, int contentHeight) {
        GradientDrawable[] placeholders = getRectangles(4, width, height);
        int cardHeight =
                dpToPx(IMAGE_PLACEHOLDER_SIZE_DP)
                        + dpToPx(CARD_TOP_PADDING_DP)
                        + (mIsFirstCardDense
                                ? dpToPx(IMAGE_PLACEHOLDER_BOTTOM_PADDING_DENSE_DP)
                                : dpToPx(IMAGE_PLACEHOLDER_BOTTOM_PADDING_DP));
        LayerDrawable layerDrawable = new LayerDrawable(placeholders);
        // Title Placeholder
        layerDrawable.setLayerInset(0, 0, top, top, cardHeight - top - height);
        // Content Placeholder
        layerDrawable.setLayerInset(
                1,
                0,
                (contentHeight - height) / 2 + top,
                top,
                cardHeight - top - (height + contentHeight) / 2);
        layerDrawable.setLayerInset(
                2, 0, top + contentHeight - height, top, cardHeight - top - contentHeight);
        // Publisher Placeholder
        layerDrawable.setLayerInset(3, 0, cardHeight - top - height, top * 7, top);
        return layerDrawable;
    }

    private LayerDrawable getLargeTextDrawable(
            int top, int left, int width, int height, int contentHeight) {
        GradientDrawable[] placeholders = getRectangles(3, width, height);
        LayerDrawable layerDrawable = new LayerDrawable(placeholders);
        layerDrawable.setLayerInset(0, left, top, top, contentHeight - top - height);
        layerDrawable.setLayerInset(
                1, left, (contentHeight - height) / 2, top, (contentHeight - height) / 2);
        layerDrawable.setLayerInset(2, left, contentHeight - top - height, top, top);
        return layerDrawable;
    }

    private GradientDrawable[] getRectangles(int num, int width, int height) {
        GradientDrawable[] placeholders = new GradientDrawable[num];
        int radius = dpToPx(TEXT_PLACEHOLDER_RADIUS_DP);
        for (int i = 0; i < num; i++) {
            placeholders[i] = new GradientDrawable();
            placeholders[i].setShape(GradientDrawable.RECTANGLE);
            // The width here is not deterministic to what the rectangle looks like. It may be also
            // affected by layer inset left and right bound and the container padding.
            placeholders[i].setSize(width, height);
            placeholders[i].setCornerRadius(radius);
            placeholders[i].setColor(mContext.getColor(R.color.feed_placeholder_color));
        }
        return placeholders;
    }

    private int dpToPx(int dp) {
        return (int)
                TypedValue.applyDimension(
                        TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics());
    }

    public long getLayoutInflationCompleteMs() {
        return mLayoutInflationCompleteMs;
    }

    void setAnimatorSetForTesting(AnimatorSet animatorSet) {
        mAllAnimations = animatorSet;
    }
}
