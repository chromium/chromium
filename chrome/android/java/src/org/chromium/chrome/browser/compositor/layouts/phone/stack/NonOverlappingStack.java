// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayoutBase;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;

/**
 * The non-overlapping tab stack we use when the HorizontalTabSwitcherAndroid flag is enabled.
 */
public class NonOverlappingStack extends Stack {
    @IntDef({SwitchDirection.LEFT, SwitchDirection.RIGHT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SwitchDirection {
        int LEFT = 0;
        int RIGHT = 1;
    }

    /**
     * The scale the tabs should be shown at when there's exactly one tab open.
     */
    private static final float SCALE_FRACTION_SINGLE_TAB = 0.80f;

    /**
     * The scale the tabs should be shown at when there are two or more tabs open.
     */
    private static final float SCALE_FRACTION_MULTIPLE_TABS = 0.54f;

    /**
     * The percentage of the screen that defines the spacing between tabs by default (no pinch).
     */
    private static final float SPACING_SCREEN = 1.0f;

    /**
     * Controls how far we slide over the (up to) three visible tabs for the switch away and switch
     * to animations (multiple of mSpacing).
     */
    private static final float SWITCH_ANIMATION_SPACING_MULTIPLE = 2.5f;

    /**
     * Duration of the switch away animation (in milliseconds).
     */
    private static final int SWITCH_AWAY_ANIMATION_DURATION = 250;

    /**
     * Duration of the switch to animation (in milliseconds).
     */
    private static final int SWITCH_TO_ANIMATION_DURATION = 250;

    /**
     * Adjustment to add a fixed amount of space between the tabs that's not based on a percentage
     * of the screen (if were 0, the tab borders would actually overlap in the current
     * implementation).
     */
    private static final float EXTRA_SPACE_BETWEEN_TABS_DP = 25.0f;

    /**
     * How much the stack should adjust the y position of each LayoutTab in portrait mode (as a
     * fraction of the amount space that would be above and below the tab if it were centered).
     */
    private static final float STACK_PORTRAIT_Y_OFFSET_PROPORTION = 0.f;

    /**
     * How much the stack should adjust the x position of each LayoutTab in landscape mode (as a
     * fraction of the amount space that would be to the left and right of the tab if it were
     * centered).
     */
    private static final float STACK_LANDSCAPE_START_OFFSET_PROPORTION = 0.f;

    /**
     * How much the stack should adjust the x position of each LayoutTab in portrait mode (as a
     * fraction of the amount space that would be above and below the tab if it were centered).
     */
    private static final float STACK_LANDSCAPE_Y_OFFSET_PROPORTION = 0.f;

    /**
     * Multiplier for adjusting the scrolling friction from the amount provided by
     * ViewConfiguration.
     */
    private static final float FRICTION_MULTIPLIER = 0.6f;

    /**
     * For short scrolls of duration less than this (in milliseconds), we assume the user wants to
     * scroll over to the next tab. If the scroll is longer in duration, we assume they're
     * reconsidering their scroll, so we leave them on the current tab (unless they drag over far
     * enough to center a new tab).
     */
    private static final int SCROLL_BOOST_TIMEOUT_MS = 250;

    /**
     * The minimum fraction of a tab the user has to scroll over by before we apply the boost to
     * scroll them to the next tab.
     */
    private static final float SCROLL_BOOST_THRESHOLD = 0.05f;

    /**
     * Used to prevent mScrollOffset from being changed as a result of clamping during the switch
     * away/switch to animations.
     */
    private boolean mSuppressScrollClamping;

    /**
     * Whether or not the current stack has been "switched away" by having runSwitchAwayAnimation()
     * called. Calling runSwitchToAnimation() resets this back to false. Checking this variable lets
     * us avoid re-playing animations if they're triggered multiple times.
     */
    private boolean mSwitchedAway;

    /** Time at which the last touch down event occurred. */
    private long mLastTouchDownTime;
    /** Index of the tab that was centered when the last touch down event occurred. */
    private int mCenteredTabAtTouchDown;

    /**
     * @param layout The parent layout.
     */
    public NonOverlappingStack(Context context, StackLayoutBase layout) {
        super(context, layout);
    }

    private int getNonDyingTabCount() {
        if (mStackTabs == null) return 0;

        int dyingCount = 0;
        for (int i = 0; i < mStackTabs.length; i++) {
            if (mStackTabs[i].isDying()) dyingCount++;
        }
        return mStackTabs.length - dyingCount;
    }

    @Override
    public float getScaleAmount() {
        if (getNonDyingTabCount() > 1) return SCALE_FRACTION_MULTIPLE_TABS;
        return SCALE_FRACTION_SINGLE_TAB;
    }

    @Override
    protected void finishAnimation(long time) {
        super.finishAnimation(time);
        mSuppressScrollClamping = false;
    }

    @Override
    protected boolean evenOutTabs(float amount, boolean allowReverseDirection) {
        // Nothing to do here; tabs are always a fixed distance apart in NonOverlappingStack (except
        // during tab close/un-close animations)
        return false;
    }

    private void updateScrollSnap() {
        mScroller.setFrictionMultiplier(FRICTION_MULTIPLIER);
        // This is what computeSpacing() returns when there are >= 2 tabs
        final int snapDistance =
                (int) Math.round(getScrollDimensionSize() * SCALE_FRACTION_MULTIPLE_TABS
                        + EXTRA_SPACE_BETWEEN_TABS_DP);
        // Really we're scrolling in the x direction, but the scroller is always wired up to the y
        // direction for both portrait and landscape mode.
        mScroller.setYSnapDistance(snapDistance);
    }

    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);
        updateScrollSnap();
    }

    /**
     * @return The index of the currently centered tab. If we're not currently snapped to a tab
     *         (e.g. we're in the process of animating a scroll or the user is currently dragging),
     *         returns the index of the tab closest to the center.
     */
    public int getCenteredTabIndex() {
        return Math.round(-mScrollOffset / mSpacing);
    }

    @Override
    public void notifySizeChanged(float width, float height, int orientation) {
        super.notifySizeChanged(width, height, orientation);

        int centeredTab = getCenteredTabIndex();
        mSpacing = computeSpacing(0);
        updateScrollOffsets(centeredTab);

        updateScrollSnap();
    }

    @Override
    public void onDown(long time) {
        super.onDown(time);
        mLastTouchDownTime = time;
        mCenteredTabAtTouchDown = getCenteredTabIndex();
        mScroller.setCenteredYSnapIndexAtTouchDown(mCenteredTabAtTouchDown);
    }

    @Override
    public void onLongPress(long time, float x, float y) {
        // Ignore long presses
    }

    @Override
    public void onPinch(long time, float x0, float y0, float x1, float y1, boolean firstEvent) {
        return;
    }

    @Override
    protected void springBack(long time) {
        if (!mScroller.isFinished()) return;

        int offsetAtTouchDown = -mCenteredTabAtTouchDown * mSpacing;
        float scrollFractionToNextTab = (offsetAtTouchDown - mScrollOffset) / mSpacing;

        int newCenteredTab;
        // Make quick, short scrolls go over to the next tab (if a scroll is short but not quick, we
        // assume the user might have decided to stay on the current tab).
        if (time < mLastTouchDownTime + SCROLL_BOOST_TIMEOUT_MS
                && Math.abs(scrollFractionToNextTab) > SCROLL_BOOST_THRESHOLD) {
            newCenteredTab = mCenteredTabAtTouchDown + (int) Math.signum(scrollFractionToNextTab);
        } else {
            newCenteredTab = getCenteredTabIndex();
        }

        int newTarget = -newCenteredTab * mSpacing;
        mScroller.flingYTo((int) mScrollTarget, newTarget, time);
        setScrollTarget(newTarget, false);
        mLayout.requestUpdate();
    }

    @Override
    protected float getSpacingScreen() {
        return SPACING_SCREEN;
    }

    @Override
    protected boolean shouldStackTabsAtTop() {
        return false;
    }

    @Override
    protected boolean shouldStackTabsAtBottom() {
        return false;
    }

    @Override
    protected float getStackPortraitYOffsetProportion() {
        return STACK_PORTRAIT_Y_OFFSET_PROPORTION;
    }

    @Override
    protected float getStackLandscapeStartOffsetProportion() {
        return STACK_LANDSCAPE_START_OFFSET_PROPORTION;
    }

    @Override
    protected float getStackLandscapeYOffsetProportion() {
        return STACK_LANDSCAPE_Y_OFFSET_PROPORTION;
    }

    @Override
    protected void computeTabClippingVisibilityHelper() {
        // Performance optimization: we don't need to draw any tab other than the centered one, the
        // one immediately to the left, and the two immediately to the right (we need the second
        // one for discard animations) since the others can't possibly be on screen.
        int centeredTab = getCenteredTabIndex();
        for (int i = 0; i < mStackTabs.length; i++) {
            LayoutTab layoutTab = mStackTabs[i].getLayoutTab();
            if (i < centeredTab - 1 || i > centeredTab + 2) {
                layoutTab.setVisible(false);
            } else {
                layoutTab.setVisible(true);
            }
        }
    }

    @Override
    protected int computeReferenceIndex() {
        return getCenteredTabIndex();
    }

    @Override
    protected boolean shouldCloseGapsBetweenTabs() {
        return false;
    }

    @Override
    protected float getMinScroll(boolean allowUnderScroll) {
        if (mSuppressScrollClamping) return -Float.MAX_VALUE;

        if (mStackTabs == null) return 0;
        for (int i = mStackTabs.length - 1; i >= 0; i--) {
            // The getScrollOffset() != 0 check avoids a bug when undiscarding the last tab, in
            // which case the tab's scroll offset is initially set to 0, which would cause us to
            // immediately center the first tab. If 0 is the correct offset to return, it's the
            // default value anyway after going through all the tabs.
            if (!mStackTabs[i].isDying() && mStackTabs[i].getScrollOffset() != 0) {
                return -mStackTabs[i].getScrollOffset();
            }
        }

        return 0;
    }

    @Override
    protected boolean allowOverscroll() {
        return false;
    }

    @Override
    protected int computeSpacing(int layoutTabCount) {
        return (int) Math.round(
                getScrollDimensionSize() * getScaleAmount() + EXTRA_SPACE_BETWEEN_TABS_DP);
    }

    /**
     * Updates the overall scroll offset and the scroll offsets for each tab based on the current
     * value of mSpacing so that the tabs are in the proper locations and the specified tab is
     * centered.
     *
     * @param centeredTab The index of the tab that should be centered.
     */
    private void updateScrollOffsets(int centeredTab) {
        // Reset the tabs' scroll offsets.
        if (mStackTabs != null) {
            for (int i = 0; i < mStackTabs.length; i++) {
                mStackTabs[i].setScrollOffset(i * mSpacing);
            }
        }

        // Reset the overall scroll offset.
        mScrollOffset = -centeredTab * mSpacing;
        setScrollTarget(mScrollOffset, false);
    }

    @Override
    protected void resetAllScrollOffset() {
        if (mTabList == null) return;
        updateScrollOffsets(mTabList.index());
    }

    // NonOverlappingStack uses linear scrolling, so screenToScroll() and scrollToScreen() are both
    // just the identity function.
    @Override
    public float screenToScroll(float screenSpace) {
        return screenSpace;
    }

    @Override
    public float scrollToScreen(float scrollSpace) {
        return scrollSpace;
    }

    @Override
    public float getMaxTabHeight() {
        // We want to maintain a constant tab height (via cropping) even as the width is changed as
        // a result of changing the scale.
        if (getNonDyingTabCount() > 1) return mLayout.getHeight();
        return (SCALE_FRACTION_MULTIPLE_TABS / SCALE_FRACTION_SINGLE_TAB) * mLayout.getHeight();
    }

    /**
     * This method sets mSuppressScrollClamping to true to allow an animation to animate the
     * mScrollOffset outside the normal bounds. It will be reset to false when finishAnimation() is
     * called.
     */
    public void suppressScrollClampingForAnimation() {
        mSuppressScrollClamping = true;
    }

    /**
     * Animates the (up to 3) visible tabs sliding off screen.
     * @param direction Whether the tabs should slide off the left or right side of the screen.
     */
    public void runSwitchAwayAnimation(@SwitchDirection int direction) {
        if (mStackTabs == null || mSwitchedAway) {
            mSwitchedAway = true;
            mLayout.onSwitchAwayFinished();
            return;
        }

        mSwitchedAway = true;
        mSuppressScrollClamping = true;

        // Make sure we don't leave any tabs stuck in a partially-discarded state.
        for (int i = 0; i < mStackTabs.length; i++) {
            mStackTabs[i].setDiscardAmount(0);
        }

        // Make sure the tabs are not scrolling so the centered tab does not change between the
        // "switch away" and "switch to" animations.
        forceScrollStop();

        CompositorAnimationHandler handler = mLayout.getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>();

        int centeredTab = getCenteredTabIndex();
        for (int i = centeredTab - 1; i <= centeredTab + 1; i++) {
            if (i < 0 || i >= mStackTabs.length) continue;
            StackTab tab = mStackTabs[i];

            float endOffset;
            if (direction == SwitchDirection.LEFT) {
                endOffset = -SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            } else {
                endOffset = SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            }

            CompositorAnimator animation =
                    CompositorAnimator.ofFloatProperty(handler, tab, StackTab.SCROLL_OFFSET,
                            tab.getScrollOffset(), endOffset, SWITCH_AWAY_ANIMATION_DURATION);
            animationList.add(animation);
        }

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                // If the user pressed the incognito button with one finger while dragging the stack
                // with another, we might not be centered on a tab. We therefore need to enforce
                // this after the animation finishes to avoid odd behavior if/when the user returns
                // to this stack.

                mScrollOffset = Math.round(mScrollOffset / mSpacing) * mSpacing;
                forceScrollStop();
                mLayout.onSwitchAwayFinished();
            }
        });
        set.start();
    }

    /**
     * Animates the (up to 3) tabs were slid off the screen by runSwitchAwayAnimation() back onto
     * the screen.
     * @param direction Whether the tabs should slide in from the left or right side of the screen.
     */
    public void runSwitchToAnimation(@SwitchDirection int direction) {
        if (mStackTabs == null || !mSwitchedAway) {
            mSwitchedAway = false;
            mLayout.onSwitchToFinished();
            return;
        }

        mSwitchedAway = false;
        mSuppressScrollClamping = true;

        CompositorAnimationHandler handler = mLayout.getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>();

        int centeredTab = getCenteredTabIndex();
        for (int i = centeredTab - 1; i <= centeredTab + 1; i++) {
            if (i < 0 || i >= mStackTabs.length) continue;
            StackTab tab = mStackTabs[i];

            float startOffset;
            if (direction == SwitchDirection.LEFT) {
                startOffset = SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            } else {
                startOffset = -SWITCH_ANIMATION_SPACING_MULTIPLE * mSpacing + tab.getScrollOffset();
            }

            CompositorAnimator animation =
                    CompositorAnimator.ofFloatProperty(handler, tab, StackTab.SCROLL_OFFSET,
                            startOffset, i * mSpacing, SWITCH_TO_ANIMATION_DURATION);
            animationList.add(animation);
        }

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        set.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator a) {
                // There are some oddball cases, e.g. a tab was immediately closed before triggering
                // the toggle animation, where the tab offsets might get left in an undesired state.
                // Resetting all the scroll offsets here limits the effect of these bugs to just
                // making the animation look funny vs. leaving the tabs e.g. stuck with gaps between
                // them or overlapping each other.
                if (mStackTabs != null) {
                    for (int i = 0; i < mStackTabs.length; i++) {
                        mStackTabs[i].setScrollOffset(i * mSpacing);
                    }
                }

                mSuppressScrollClamping = false;
                mLayout.onSwitchToFinished();
            }
        });
        set.start();
    }
}
