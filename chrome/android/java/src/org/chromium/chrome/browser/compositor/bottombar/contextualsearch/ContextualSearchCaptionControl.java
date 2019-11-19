// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelTextViewInflater;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Caption View that is shown at the bottom of the control and used
 * as a dynamic resource.
 */
public class ContextualSearchCaptionControl extends OverlayPanelTextViewInflater {
    private static final float ANIMATION_PERCENTAGE_ZERO = 0.f;
    private static final float ANIMATION_PERCENTAGE_COMPLETE = 1.f;
    private static final float EXPANDED_CAPTION_THRESHOLD = 0.5f;

    /**
     * The resource id for the string to display when the Bar is expanded.
     */
    @VisibleForTesting
    public static final int EXPANED_CAPTION_ID = R.string.contextmenu_open_in_new_tab;

    /**
     * The caption View.
     */
    private TextView mCaption;

    /**
     * The text for the caption when the Bar is peeking.
     */
    private String mPeekingCaptionText;

    /**
     * Whether there is a caption when the Bar is peeking.
     */
    private boolean mHasPeekingCaption;

    /**
     * Whether the caption for the expanded Bar is showing.
     */
    private boolean mShowingExpandedCaption;

    /**
     * Whether the expanded caption should be shown.
     */
    private final boolean mShouldShowExpandedCaption;

    /**
     * The caption visibility.
     */
    private boolean mIsVisible;

    /**
     * The caption animation percentage, which controls how and where to draw. It is
     * ANIMATION_PERCENTAGE_COMPLETE when the Contextual Search bar is peeking and
     * ANIMATION_PERCENTAGE_ZERO when it is expanded.
     */
    private float mAnimationPercentage = ANIMATION_PERCENTAGE_ZERO;

    /** The animator responsible for transitioning the caption. */
    private CompositorAnimator mTransitionAnimator;

    /**
     * Whether a new snapshot has been captured by the system yet - this is false when we have
     * something to show, but cannot yet show it.
     */
    private boolean mDidCapture;

    /**
     * @param panel                     The panel.
     * @param context                   The Android Context used to inflate the View.
     * @param container                 The container View used to inflate the View.
     * @param resourceLoader            The resource loader that will handle the snapshot capturing.
     * @param shouldShowExpandedCaption Whether the "Open in new tab" caption should be shown
     *                                  when the panel is expanded.
     */
    public ContextualSearchCaptionControl(OverlayPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader resourceLoader, boolean shouldShowExpandedCaption) {
        super(panel, R.layout.contextual_search_caption_view, R.id.contextual_search_caption_view,
                context, container, resourceLoader,
                (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)
                                ? R.dimen.contextual_search_end_padding
                                : R.dimen.contextual_search_padded_button_width),
                R.dimen.contextual_search_padded_button_width);
        mShouldShowExpandedCaption = shouldShowExpandedCaption;
    }

    /**
     * Sets the caption to display in the bottom of the control.
     * @param caption The string displayed as a caption to help explain results,
     *        e.g. a Quick Answer.
     */
    public void setCaption(String caption) {
        // If the peeking caption has already been set return early rather than changing it.
        if (mHasPeekingCaption) return;

        mPeekingCaptionText = sanitizeText(caption);
        mHasPeekingCaption = true;

        if (mShowingExpandedCaption) return;

        mDidCapture = false;

        inflate();

        mCaption.setText(sanitizeText(caption));

        invalidate();
        show();
    }

    /**
     * Updates the caption when in transition between peeked to expanded states.
     * @param percentage The percentage to the more opened state.
     */
    @Override
    public void onUpdateFromPeekToExpand(float percentage) {
        super.onUpdateFromPeekToExpand(percentage);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)) {
            if (mHasPeekingCaption) {
                if (mTransitionAnimator != null) mTransitionAnimator.cancel();
                mAnimationPercentage = 1.f - percentage;
            }
            return;
        }

        // ChromeFeatureList.OVERLAY_NEW_LAYOUT not enabled.
        if (!mShouldShowExpandedCaption) {
            if (mHasPeekingCaption) {
                if (mTransitionAnimator != null) mTransitionAnimator.cancel();
                mAnimationPercentage = 1.f - percentage;
            }
            return;
        }

        if (mHasPeekingCaption) {
            if (percentage < EXPANDED_CAPTION_THRESHOLD && mShowingExpandedCaption) {
                // Start showing the peeking caption again.
                mShowingExpandedCaption = false;
                mCaption.setText(mPeekingCaptionText);
                invalidate();
            } else if (percentage >= EXPANDED_CAPTION_THRESHOLD && !mShowingExpandedCaption) {
                // Start showing the expanded caption.
                mShowingExpandedCaption = true;
                mCaption.setText(EXPANED_CAPTION_ID);
                invalidate();
            }

            // If the peeking caption gets set while the bar is expanding, mAnimationPercentage
            // will stop getting updated. Set mAnimationPercentage to its complete value.
            mAnimationPercentage = ANIMATION_PERCENTAGE_COMPLETE;
        } else {
            // If the expanded caption is not showing, set the caption text to the expanded
            // caption.
            if (!mShowingExpandedCaption && percentage > 0.f) {
                mShowingExpandedCaption = true;
                // Inflate the caption view if it has not already been inflated
                if (mCaption == null) {
                    inflate();
                }

                mCaption.setText(EXPANED_CAPTION_ID);
                invalidate();
                show();
            }

            mAnimationPercentage = percentage;
            if (mAnimationPercentage == ANIMATION_PERCENTAGE_ZERO) mShowingExpandedCaption = false;
        }
    }

    /**
     * Hides the caption.
     */
    public void hide() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)) {
            mIsVisible = false;
            mAnimationPercentage = ANIMATION_PERCENTAGE_ZERO;
        } else {
            if (!mShowingExpandedCaption) {
                mIsVisible = false;
                mAnimationPercentage = ANIMATION_PERCENTAGE_ZERO;
            }
        }
        mHasPeekingCaption = false;
    }

    /**
     * Shows the caption.
     */
    private void show() {
        mIsVisible = true;
    }

    /**
     * Controls whether the caption is visible and can be rendered.
     * The caption must be visible in order to draw it and take a snapshot.
     * Even though the caption is visible the user might not be able to see it due to a
     * completely transparent opacity associated with an animation percentage of zero.
     * @return Whether the caption is visible or not.
     */
    public boolean getIsVisible() {
        return mIsVisible;
    }

    /**
     * Gets the animation percentage which controls the drawing of the caption and how high to
     * position it in the Bar.
     * @return The current percentage ranging from 0.0 to 1.0.
     */
    public float getAnimationPercentage() {
        // If we don't yet have a snapshot captured, stay at zero.  See crbug.com/608914.
        if (!mDidCapture) return ANIMATION_PERCENTAGE_ZERO;

        return mAnimationPercentage;
    }

    /**
     * @return The text currently showing in the caption view.
     */
    public CharSequence getCaptionText() {
        return mCaption.getText();
    }

    //========================================================================================
    // OverlayPanelTextViewInflater overrides
    //========================================================================================

    @Override
    protected TextView getTextView() {
        return mCaption;
    }

    //========================================================================================
    // OverlayPanelInflater overrides
    //========================================================================================

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mCaption = (TextView) view.findViewById(R.id.contextual_search_caption);
    }

    @Override
    protected void onCaptureEnd() {
        super.onCaptureEnd();
        if (mDidCapture) return;

        mDidCapture = true;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)) {
            animateTransitionIn();
        } else if (!mShowingExpandedCaption) {
            animateTransitionIn();
        }
    }

    // ============================================================================================
    // Search Caption Animation
    // ============================================================================================

    private void animateTransitionIn() {
        mTransitionAnimator = CompositorAnimator.ofFloat(mOverlayPanel.getAnimationHandler(),
                ANIMATION_PERCENTAGE_ZERO, ANIMATION_PERCENTAGE_COMPLETE,
                OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, null);
        mTransitionAnimator.addUpdateListener(
                animator -> mAnimationPercentage = animator.getAnimatedValue());
        mTransitionAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        mTransitionAnimator.start();
    }
}
