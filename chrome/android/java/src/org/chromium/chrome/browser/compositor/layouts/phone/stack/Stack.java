// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.animation.FloatProperty;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayoutBase;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackAnimation.OverviewAnimationType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * Handles all the drawing and events of a stack of stackTabs.
 *
 * @VisibleForTesting
 */
public abstract class Stack {
    public static final int MAX_NUMBER_OF_STACKED_TABS_TOP = 3;
    public static final int MAX_NUMBER_OF_STACKED_TABS_BOTTOM = 3;

    private static final float STACK_PORTRAIT_Y_OFFSET_PROPORTION = -0.8f;
    private static final float STACK_LANDSCAPE_START_OFFSET_PROPORTION = -0.7f;
    private static final float STACK_LANDSCAPE_Y_OFFSET_PROPORTION = -0.5f;

    @IntDef({DragLock.NONE, DragLock.SCROLL, DragLock.DISCARD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragLock {
        int NONE = 0;
        int SCROLL = 1;
        int DISCARD = 2;
    }

    /**
     * The percentage of the screen to cover for the discarded tab to be fully transparent.
     */
    public static final float DISCARD_RANGE_SCREEN = 0.7f;

    /**
     * The percentage the tab need to be dragged to actually discard the card.
     */
    private static final float DISCARD_COMMIT_THRESHOLD = 0.4f;

    /**
     * The percentage of the side of the tab that is inactive to swipe to discard. As this is
     * a distance computed from both edges, meaningful value ranges in [0 ... 0.5].
     */
    private static final float DISCARD_SAFE_SELECTION_PCTG = 0.1f;

    /**
     * The minimum scale the tab can reach when being discarded by a click.
     */
    private static final float DISCARD_END_SCALE_CLICK = 0.7f;

    /**
     * The minimum scale the tab can reach when being discarded by a swipe.
     */
    private static final float DISCARD_END_SCALE_SWIPE = 0.5f;

    /**
     * The delta time applied on the velocity from the fling. This is to compute the kick to
     * help discarding a card.
     */
    private static final float DISCARD_FLING_DT = 1.0f / 45.0f;

    /**
     * The maximum contribution of the fling. This is in percentage of the range.
     */
    private static final float DISCARD_FLING_MAX_CONTRIBUTION = 0.4f;

    /**
     * How much to scale the max overscroll angle when tabs are tilting backwards.
     */
    private static final float BACKWARDS_TILT_SCALE = 0.5f;

    /**
     * When overscrolling towards the top or left of the screen, what portion of
     * the overscroll should be devoted to sliding the tabs together. The rest
     * of the overscroll is used for tilting.
     */
    private static final float OVERSCROLL_TOP_SLIDE_PCTG = 0.25f;

    /**
     * Scale max under/over scroll by this amount when flinging.
     */
    private static final float MAX_OVER_FLING_SCALE = 0.5f;

    /**
     * mMaxUnderScroll is determined by multing mMaxOverScroll with
     * MAX_UNDER_SCROLL_SCALE
     */
    private static final float MAX_UNDER_SCROLL_SCALE = 2.0f;

    /**
     * Drags that are mostly horizontal (within 30 degrees) signal that
     * a user is discarding a tab.
     */
    private static final float DRAG_ANGLE_THRESHOLD = (float) Math.tan(Math.toRadians(30.0));

    /**
     * Reset the scroll mode after this number of milliseconds of inactivity or small motions.
     */
    private static final long DRAG_TIME_THRESHOLD = 400;

    /**
     * Minimum motion threshold to lock the scroll mode.
     */
    private static final float DRAG_MOTION_THRESHOLD_DP = 1.25f;

    /**
     * The number of attempt to get the full roll overscroll animation.
     */
    private static final int OVERSCROLL_FULL_ROLL_TRIGGER = 5;

    /**
     * Percentage of the screen to wrap the scroll space.
     */
    private static final float SCROLL_WARP_PCTG = 0.4f;

    /**
     * Percentage of the screen a swipe gesture must traverse before it is allowed to be
     * canceled.
     */
    private static final float SWIPE_LANDSCAPE_THRESHOLD = 0.19f;

    /**
     * How far to place the tab to the left of the user's finger when swiping in dp.  This keeps
     * the tab under the user's finger.
     */
    private static final float LANDSCAPE_SWIPE_DRAG_TAB_OFFSET_DP = 40.f;

    // External References
    protected TabList mTabList;

    // True when the stack is still visible for animation but it is going to be empty.
    private boolean mIsDying;

    // Screen State Variables
    protected int mSpacing;
    protected StackTab[] mStackTabs; // mStackTabs can be null if there are no tabs

    // Overscroll
    protected StackScroller mScroller;
    private float mOverScrollOffset;
    private int mOverScrollDerivative;
    private int mOverScrollCounter;
    private float mMaxOverScroll; // This will be updated from dimens.xml
    protected float mMaxUnderScroll;
    protected float mMaxOverScrollAngle; // This will be updated from values.xml
    private float mMaxOverScrollSlide;

    // Drag Lock
    private @DragLock int mDragLock = DragLock.NONE;
    private long mLastScrollUpdate;
    private float mMinScrollMotion;

    // Scrolling Variables
    protected float mScrollTarget;
    protected float mScrollOffset;
    private float mScrollOffsetForDyingTabs;
    protected float mCurrentScrollDirection;
    protected StackTab mScrollingTab;

    // Swipe Variables
    private float mSwipeUnboundScrollOffset;
    private float mSwipeBoundedScrollOffset;
    private boolean mSwipeIsCancelable;
    private boolean mSwipeCanScroll;
    protected boolean mInSwipe;

    // Discard
    protected StackTab mDiscardingTab;

    // We can't initialize mDiscardDirection here using LocalizationUtils.isRtl() because it
    // will involve a jni call. Instead, mDiscardDirection will be initialized in Show().
    private float mDiscardDirection = Float.NaN;

    private int mReferenceOrderIndex = -1;

    // Orientation Variables
    protected @Orientation int mCurrentMode = Orientation.PORTRAIT;

    // Animation Variables
    protected @OverviewAnimationType int mOverviewAnimationType = OverviewAnimationType.NONE;
    private StackAnimation mAnimationFactory;
    private StackViewAnimation mViewAnimationFactory;

    // Running set of animations applied to tabs.
    private Pair<AnimatorSet, ArrayList<FloatProperty>> mAnimatorSetTabAnimations;
    private Animator mViewAnimations;

    // The parent Layout
    protected final StackLayoutBase mLayout;

    // Border values
    protected float mBorderTransparentTop;
    protected float mBorderTransparentSide;
    // TODO(dtrainor): Expose 9-patch padding from resource manager.
    protected float mBorderTopPadding;
    private float mBorderLeftPadding;

    private boolean mIsStackForCurrentTabList;

    private final AnimatorListenerAdapter mViewAnimatorListener = new AnimatorListenerAdapter() {
        @Override
        public void onAnimationCancel(Animator animation) {
            mLayout.requestUpdate();
        }

        @Override
        public void onAnimationEnd(Animator animation) {
            mLayout.requestUpdate();
        }
    };

    /**
     * @param layout The parent layout.
     */
    public Stack(Context context, StackLayoutBase layout) {
        mLayout = layout;
        contextChanged(context);
    }

    /**
     * @return Animation handler associated with this stack.
     */
    public CompositorAnimationHandler getAnimationHandler() {
        return mLayout.getAnimationHandler();
    }

    /**
     * @param tabList The list to attach to this stack.
     */
    public void setTabList(TabList tabList) {
        mTabList = tabList;
    }

    /**
     * @return The TabList associated with this stack.
     */
    public TabList getTabList() {
        return mTabList;
    }

    /**
     * @return The {@link StackTab}s currently being rendered by the tab stack.
     * @VisibleForTesting
     */
    public StackTab[] getTabs() {
        return mStackTabs;
    }

    /**
     * @return The number of tabs in the tab stack.
     * @VisibleForTesting
     */
    public int getCount() {
        return mStackTabs != null ? mStackTabs.length : 0;
    }

    /**
     * @return The number of visible tabs in the tab stack.
     */
    public int getVisibleCount() {
        int visibleCount = 0;
        if (mStackTabs != null) {
            for (int i = 0; i < mStackTabs.length; ++i) {
                if (mStackTabs[i].getLayoutTab().isVisible()) visibleCount++;
            }
        }
        return visibleCount;
    }

    /**
     * The scale the tabs should be currently shown at (may change based on how many are open).
     */
    public abstract float getScaleAmount();

    /*
     * Main Interaction Methods for the rest of the application
     *
     *
     * These methods are the main entry points for the model to tell the
     * view that something has changed.  The rest of the application can
     * alert this class that something in the tab stack has changed or that
     * the user has decided to enter the tab switcher.
     *
     */

    /**
     * Triggers the closing motions.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the tab that get closed.
     */
    public void tabClosingEffect(long time, int id) {
        if (mStackTabs == null) return;

        // |id| cannot be used to access the particular tab in the model.
        // The tab is already gone from the model by this point.

        int newIndex = 0;
        boolean needAnimation = false;
        for (int i = 0; i < mStackTabs.length; ++i) {
            if (mStackTabs[i].getId() == id) {
                // Mark the {@link StackTab} as dying so that when the animation is
                // finished we can clear it out of the stack. This supports
                // multiple {@link StackTab} deletions.
                needAnimation |= !mStackTabs[i].isDying();
                mStackTabs[i].setDying(true);
            } else {
                // Update the {@link StackTab} with a new index here.  This makes sure the
                // {@link LayoutTab} end up in the proper place.
                mStackTabs[i].setNewIndex(newIndex++);
            }
        }

        if (needAnimation) {
            mScrollOffsetForDyingTabs = mScrollOffset;
            mSpacing = computeSpacing(newIndex);

            startAnimation(time, OverviewAnimationType.DISCARD);
        }

        if (newIndex == 0) mIsDying = true;
    }

    /**
     * @return True if we should put the close button on the right side of the tab, or false if
     *              we should put it on the left. This method already accounts for RTL flipping.
     */
    private boolean isCloseButtonOnRight() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)) {
            return !LocalizationUtils.isLayoutRtl();
        }

        return mCurrentMode == Orientation.PORTRAIT ^ LocalizationUtils.isLayoutRtl();
    }

    /**
     * Animates all the tabs closing at once.
     *
     * @param time The current time of the app in ms.
     */
    public void tabsAllClosingEffect(long time) {
        boolean needAnimation = false;

        if (mStackTabs != null) {
            for (int i = 0; i < mStackTabs.length; ++i) {
                needAnimation |= !mStackTabs[i].isDying();
                mStackTabs[i].setDying(true);
            }
        } else {
            // This needs to be set to true to handle the case where both the normal and
            // incognito tabs are being closed.
            needAnimation = true;
        }

        if (needAnimation) {
            mScrollOffsetForDyingTabs = mScrollOffset;
            mSpacing = computeSpacing(0);

            if (mStackTabs != null) {
                for (int i = 0; i < mStackTabs.length; i++) {
                    StackTab tab = mStackTabs[i];
                    tab.setDiscardOriginY(0.f);
                    tab.setDiscardOriginX(isCloseButtonOnRight()
                                    ? tab.getLayoutTab().getOriginalContentWidth()
                                    : 0.f);
                    tab.setDiscardFromClick(true);
                }
            }
            startAnimation(time, OverviewAnimationType.DISCARD_ALL);
        }

        mIsDying = true;
    }

    /**
     * Animates a new tab opening.
     *
     * @param time The current time of the app in ms.
     * @param id The id of the new tab to animate.
     */
    public void tabCreated(long time, int id) {
        if (!createTabHelper(id)) return;
        mIsDying = false;

        finishAnimation(time);
        startAnimation(time, OverviewAnimationType.NEW_TAB_OPENED,
                TabModelUtils.getTabIndexById(mTabList, id), TabList.INVALID_TAB_INDEX, false);
    }

    /**
     * Animates the closing of the stack. Focusing on the selected tab.
     *
     * @param time The current time of the app in ms.
     * @param id   The id of the tab to select.
     */
    public void tabSelectingEffect(long time, int id) {
        int index = TabModelUtils.getTabIndexById(mTabList, id);
        startAnimation(time, OverviewAnimationType.TAB_FOCUSED, index, -1, false);
    }

    /**
     * Called set up the tab stack to the initial state when it is entered.
     *
     * @param time The current time of the app in ms.
     * @param focused Whether or not the stack was focused when entering.
     */
    public void stackEntered(long time, boolean focused) {
        // Don't request new thumbnails until the animation is over. We should
        // have cached the visible ones already.
        boolean finishImmediately = !focused;
        mSpacing = computeSpacing(mStackTabs != null ? mStackTabs.length : 0);
        resetAllScrollOffset();
        startAnimation(time, OverviewAnimationType.ENTER_STACK, finishImmediately);
    }

    /**
     * @return Whether or not the TabList represented by this TabStackState should be displayed.
     */
    public boolean isDisplayable() {
        return !mTabList.isIncognito() || (!mIsDying && mTabList.getCount() > 0);
    }

    private float getDefaultDiscardDirection() {
        return (mCurrentMode == Orientation.LANDSCAPE && LocalizationUtils.isLayoutRtl()) ? -1.0f
                                                                                          : 1.0f;
    }

    /**
     * show is called to set up the initial variables, and must always be called before
     * displaying the stack.
     * @param isStackForCurrentTabList Whether this {@link Stack} is for the current tab list.
     */
    public void show(boolean isStackForCurrentTabList) {
        mIsStackForCurrentTabList = isStackForCurrentTabList;

        mDiscardDirection = getDefaultDiscardDirection();

        // Reinitialize the roll over counter for each tabswitcher session.
        mOverScrollCounter = 0;

        // TODO: Recreating the stack {@link StackTab} here might be overkill.  Will these
        // already exist in the cache?  Check to make sure it makes sense.
        createStackTabs(false);
    }

    /*
     * Animation Start and Finish Methods
     *
     * This method kicks off animations by using the
     * TabSwitcherAnimationFactory to create an AnimatorSet.
     */

    /**
     * Starts an animation on the stack.
     *
     * @param time The current time of the app in ms.
     * @param type The type of the animation to start.
     */
    protected void startAnimation(long time, @OverviewAnimationType int type) {
        startAnimation(time, type, TabList.INVALID_TAB_INDEX, false);
    }

    /**
     * Starts an animation on the stack.
     *
     * @param time The current time of the app in ms.
     * @param type The type of the animation to start.
     * @param finishImmediately Whether the animation jumps straight to the end.
     */
    private void startAnimation(
            long time, @OverviewAnimationType int type, boolean finishImmediately) {
        startAnimation(time, type, TabList.INVALID_TAB_INDEX, finishImmediately);
    }

    /**
     * Starts an animation on the stack.
     *
     * @param time The current time of the app in ms.
     * @param type The type of the animation to start.
     * @param sourceIndex The source index needed by some animation types.
     * @param finishImmediately Whether the animation jumps straight to the end.
     */
    protected void startAnimation(long time, @OverviewAnimationType int type, int sourceIndex,
            boolean finishImmediately) {
        startAnimation(time, type, mTabList.index(), sourceIndex, finishImmediately);
    }

    private void startAnimation(long time, @OverviewAnimationType int type, int focusIndex,
            int sourceIndex, boolean finishImmediately) {
        if (!canUpdateAnimation(time, type, sourceIndex, finishImmediately)) {
            // We need to finish animations started earlier before we start
            // off a new one.
            finishAnimation(time);
            // Stop movement while the animation takes place.
            stopScrollingMovement(time);
        }

        if (mAnimationFactory != null && mViewAnimationFactory != null) {
            mOverviewAnimationType = type;

            // First try to build a View animation.  Then fallback to the compositor animation
            // if one isn't created.
            mViewAnimations = mViewAnimationFactory.createAnimatorForType(
                    type, mStackTabs, mLayout.getViewContainer(), mTabList, focusIndex);

            if (mViewAnimations != null) {
                mViewAnimations.addListener(mViewAnimatorListener);
            } else {
                // Build the AnimatorSet using the TabSwitcherAnimationFactory.
                // This will give us the appropriate AnimatorSet based on the current
                // state of the tab switcher and the OverviewAnimationType specified.
                mAnimatorSetTabAnimations = mAnimationFactory.createAnimatorSetForType(type, this,
                        mStackTabs, focusIndex, sourceIndex, mSpacing, getDiscardRange());
            }

            if (mAnimatorSetTabAnimations != null) mAnimatorSetTabAnimations.first.start();
            if (mViewAnimations != null) mViewAnimations.start();
            if (mAnimatorSetTabAnimations != null || mViewAnimations != null) {
                mLayout.onStackAnimationStarted();
            }

            if ((mAnimatorSetTabAnimations == null && mViewAnimations == null)
                    || finishImmediately) {
                finishAnimation(time);
            }
        }

        mLayout.requestUpdate();
    }

    /**
     * Performs the necessary actions to finish the current animation.
     *
     * @param time The current time of the app in ms.
     */
    protected void finishAnimation(long time) {
        if (mAnimatorSetTabAnimations != null) mAnimatorSetTabAnimations.first.end();
        if (mViewAnimations != null) mViewAnimations.end();
        if (mAnimatorSetTabAnimations != null || mViewAnimations != null) {
            mLayout.onStackAnimationFinished();
        }

        switch (mOverviewAnimationType) {
            case OverviewAnimationType.ENTER_STACK:
                mLayout.uiDoneEnteringStack();
                break;
            case OverviewAnimationType.FULL_ROLL:
                for (int i = 0; i < mStackTabs.length; i++) {
                    mStackTabs[i].getLayoutTab().setTiltX(0, 0);
                    mStackTabs[i].getLayoutTab().setTiltY(0, 0);
                }
                springBack(time);
                break;
            case OverviewAnimationType.TAB_FOCUSED:
            // Purposeful fall through
            case OverviewAnimationType.NEW_TAB_OPENED:
                // Nothing to do.
                break;
            case OverviewAnimationType.DISCARD_ALL:
                mLayout.uiDoneClosingAllTabs(mTabList.isIncognito());
                cleanupStackTabState();
                break;
            case OverviewAnimationType.UNDISCARD:
            // Purposeful fall through because if UNDISCARD animation updated DISCARD animation,
            // DISCARD animation clean up below is not called so UNDISCARD is responsible for
            // cleaning it up.
            case OverviewAnimationType.DISCARD:
                // Remove all dying tabs from mStackTabs.
                if (mStackTabs != null) {
                    // Request for the model to be updated.
                    for (int i = 0; i < mStackTabs.length; ++i) {
                        StackTab tab = mStackTabs[i];
                        if (tab.isDying()) {
                            mLayout.uiDoneClosingTab(
                                    time, tab.getId(), true, mTabList.isIncognito());
                        }
                    }
                }
                cleanupStackTabState();
                break;
            default:
                break;
        }

        // sync the scrollTarget and scrollOffset. For ENTER_STACK animation, don't sync to
        // ensure the tab can tilt back.
        if (mOverviewAnimationType != OverviewAnimationType.NONE
                && mOverviewAnimationType != OverviewAnimationType.ENTER_STACK
                && mScroller.isFinished()) {
            setScrollTarget(mScrollOffset, true);
        }
        mOverviewAnimationType = OverviewAnimationType.NONE;

        mAnimatorSetTabAnimations = null;
        mViewAnimations = null;
    }

    private void cleanupStackTabState() {
        if (mStackTabs != null) {
            // First count the number of tabs that are still alive.
            int nNumberOfLiveTabs = 0;
            for (int i = 0; i < mStackTabs.length; ++i) {
                if (mStackTabs[i].isDying()) {
                    mLayout.releaseTabLayout(mStackTabs[i].getLayoutTab());
                } else {
                    nNumberOfLiveTabs++;
                }
            }

            if (nNumberOfLiveTabs == 0) {
                // We have no more live {@link StackTab}. Just clean all tab related states.
                cleanupTabs();
            } else if (nNumberOfLiveTabs < mStackTabs.length) {
                // If any tabs have died, we need to remove them from mStackTabs.

                StackTab[] oldTabs = mStackTabs;
                mStackTabs = new StackTab[nNumberOfLiveTabs];

                int newIndex = 0;
                for (int i = 0; i < oldTabs.length; ++i) {
                    if (!oldTabs[i].isDying()) {
                        mStackTabs[newIndex] = oldTabs[i];
                        mStackTabs[newIndex].setNewIndex(newIndex);
                        newIndex++;
                    }
                }
                assert newIndex == nNumberOfLiveTabs;
            }
        }

        mDiscardDirection = getDefaultDiscardDirection();
    }

    /**
     * Ensure that there are no dying tabs by finishing the current animation.
     *
     * @param time The current time of the app in ms.
     */
    public void ensureCleaningUpDyingTabs(long time) {
        finishAnimation(time);
    }

    /**
     * Decide if the animation can be started without cleaning up the current animation.
     * @param time              The current time of the app in ms.
     * @param type              The type of the animation to start.
     * @param sourceIndex       The source index needed by some animation types.
     * @param finishImmediately Whether the animation jumps straight to the end.
     * @return                  true, if we can start the animation without cleaning up the
     *                          current animation.
     */
    private boolean canUpdateAnimation(long time, @OverviewAnimationType int type, int sourceIndex,
            boolean finishImmediately) {
        if (mAnimationFactory != null) {
            if ((mOverviewAnimationType == OverviewAnimationType.DISCARD
                        || mOverviewAnimationType == OverviewAnimationType.UNDISCARD
                        || mOverviewAnimationType == OverviewAnimationType.DISCARD_ALL)
                    && (type == OverviewAnimationType.DISCARD
                            || type == OverviewAnimationType.UNDISCARD
                            || type == OverviewAnimationType.DISCARD_ALL)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Cancel scrolling animation which is a part of discarding animation.
     * @return true if the animation is canceled, false, if there is nothing to cancel.
     */
    private boolean cancelDiscardScrollingAnimation() {
        if (mOverviewAnimationType == OverviewAnimationType.DISCARD
                || mOverviewAnimationType == OverviewAnimationType.UNDISCARD
                || mOverviewAnimationType == OverviewAnimationType.DISCARD_ALL) {
            if (mAnimatorSetTabAnimations != null) {
                Iterator<FloatProperty> propertyIterator =
                        mAnimatorSetTabAnimations.second.iterator();
                Iterator<Animator> animatorIterator =
                        mAnimatorSetTabAnimations.first.getChildAnimations().iterator();

                while (animatorIterator.hasNext()) {
                    CompositorAnimator a = (CompositorAnimator) animatorIterator.next();
                    if (propertyIterator.next() == StackTab.SCROLL_OFFSET) a.cancel();
                }
            }
            return true;
        }
        return false;
    }

    /**
     * Checks any Android view animations to see if they have finished yet.
     * @param time      The current time of the app in ms.
     * @param jumpToEnd Whether to finish the animation.
     * @return          Whether the animation was finished.
     */
    public boolean onUpdateViewAnimation(long time, boolean jumpToEnd) {
        boolean finished = true;
        if (mViewAnimations != null) {
            finished = !mViewAnimations.isRunning();
            finishAnimationsIfDone(time, jumpToEnd);
        }
        return finished;
    }

    /**
     * Steps the animation forward and updates all the animated values.
     * @param time      The current time of the app in ms.
     * @param jumpToEnd Whether to finish the animation.
     * @return          Whether the animation was finished.
     */
    public boolean onUpdateCompositorAnimations(long time, boolean jumpToEnd) {
        if (!jumpToEnd) updateScrollOffset(time);

        boolean animatorSetFinished = true;
        if (mAnimatorSetTabAnimations != null) {
            animatorSetFinished = jumpToEnd ? true : !mAnimatorSetTabAnimations.first.isRunning();
        }

        if (mAnimatorSetTabAnimations != null) finishAnimationsIfDone(time, jumpToEnd);
        if (jumpToEnd) forceScrollStop();

        return animatorSetFinished;
    }

    private void finishAnimationsIfDone(long time, boolean jumpToEnd) {
        boolean hasViewAnimations = mViewAnimations != null;
        boolean isViewFinished = hasViewAnimations ? !mViewAnimations.isRunning() : true;

        boolean hasAnimatorSetTabAnimations = mAnimatorSetTabAnimations != null;
        boolean isAnimatorSetTabFinished =
                hasAnimatorSetTabAnimations ? !mAnimatorSetTabAnimations.first.isRunning() : true;

        boolean hasAnimations = hasViewAnimations || hasAnimatorSetTabAnimations;

        boolean shouldFinish = jumpToEnd && hasAnimations;
        shouldFinish |= hasAnimations && (!hasViewAnimations || isViewFinished)
                && (!hasAnimatorSetTabAnimations || isAnimatorSetTabFinished);

        if (shouldFinish) finishAnimation(time);
    }

    /**
     * Determines which action was specified by the user's drag.
     *
     * @param scrollDrag  The number of pixels moved in the scroll direction.
     * @param discardDrag The number of pixels moved in the discard direction.
     * @return            The current lock mode or a hint if the motion was not strong enough
     *                    to fully lock the mode.
     */
    private @DragLock int computeDragLock(float scrollDrag, float discardDrag) {
        scrollDrag = Math.abs(scrollDrag);
        discardDrag = Math.abs(discardDrag);
        @DragLock
        int hintLock = (discardDrag * DRAG_ANGLE_THRESHOLD) > scrollDrag ? DragLock.DISCARD
                                                                         : DragLock.SCROLL;
        // If the user paused the drag for too long, re-determine what the new action is.
        long timeMillisecond = System.currentTimeMillis();
        if ((timeMillisecond - mLastScrollUpdate) > DRAG_TIME_THRESHOLD) {
            mDragLock = DragLock.NONE;
        }
        // Select the scroll lock if enough conviction is put into scrolling.
        if ((mDragLock == DragLock.NONE && Math.abs(scrollDrag - discardDrag) > mMinScrollMotion)
                || (mDragLock == DragLock.DISCARD && discardDrag > mMinScrollMotion)
                || (mDragLock == DragLock.SCROLL && scrollDrag > mMinScrollMotion)) {
            mLastScrollUpdate = timeMillisecond;
            if (mDragLock == DragLock.NONE) {
                mDragLock = hintLock;
            }
        }
        // Returns a hint of the lock so we can show feedback even if the lock is not committed
        // yet.
        return mDragLock == DragLock.NONE ? hintLock : mDragLock;
    }

    /*
     * User Input Routines:
     *
     * The input routines that process gestures and click touches.  These
     * are the main way to interact with the view directly.  Other input
     * paths happen when model changes impact the view.  This can happen
     * as a result of some of these actions or from other user input (ie:
     * from the Toolbar).  These are ignored if an animation is currently
     * in progress.
     */

    /**
     * Called on drag event (from scroll events in the gesture detector).
     *
     * @param time    The current time of the app in ms.
     * @param x       The x coordinate of the end of the drag event.
     * @param y       The y coordinate of the end of the drag event.
     * @param amountX The number of pixels dragged in the x direction since the last event.
     * @param amountY The number of pixels dragged in the y direction since the last event.
     */
    public void drag(long time, float x, float y, float amountX, float amountY) {
        float scrollDrag, discardDrag;
        if (mCurrentMode == Orientation.PORTRAIT) {
            discardDrag = amountX;
            scrollDrag = amountY;
        } else {
            discardDrag = amountY;
            scrollDrag = LocalizationUtils.isLayoutRtl() ? -amountX : amountX;
        }
        @DragLock
        int hintLock = computeDragLock(scrollDrag, discardDrag);
        if (hintLock == DragLock.DISCARD) {
            discard(x, y, amountX, amountY);
        } else {
            // Only cancel the current discard attempt if the scroll lock is committed:
            // by using mDragLock instead of hintLock.
            if (mDragLock == DragLock.SCROLL && mDiscardingTab != null) {
                commitDiscard(time, false);
            }
            scroll(x, y, LocalizationUtils.isLayoutRtl() ? -amountX : amountX, amountY, false);
        }
        mLayout.requestUpdate();
    }

    /**
     * Discards and updates the position based on the input event values.
     *
     * @param x       The x coordinate of the end of the drag event.
     * @param y       The y coordinate of the end of the drag event.
     * @param amountX The number of pixels dragged in the x direction since the last event.
     * @param amountY The number of pixels dragged in the y direction since the last event.
     */
    private void discard(float x, float y, float amountX, float amountY) {
        if (mStackTabs == null
                || (mOverviewAnimationType != OverviewAnimationType.NONE
                        && mOverviewAnimationType != OverviewAnimationType.DISCARD
                        && mOverviewAnimationType != OverviewAnimationType.DISCARD_ALL
                        && mOverviewAnimationType != OverviewAnimationType.UNDISCARD)) {
            return;
        }

        if (mDiscardingTab == null) {
            if (!mInSwipe) {
                mDiscardingTab = getTabAtPositon(x, y);
            } else {
                if (mTabList.index() < 0) return;
                mDiscardingTab = mStackTabs[mTabList.index()];
            }

            if (mDiscardingTab != null) {
                cancelDiscardScrollingAnimation();

                // Make sure we are well within the tab in the discard direction.
                RectF target = mDiscardingTab.getLayoutTab().getClickTargetBounds();
                float distanceToEdge;
                float edgeToEdge;
                if (mCurrentMode == Orientation.PORTRAIT) {
                    mDiscardDirection = 1.0f;
                    distanceToEdge = Math.max(target.left - x, x - target.right);
                    edgeToEdge = target.width();
                } else {
                    mDiscardDirection = 2.0f - 4.0f * (x / mLayout.getWidth());
                    mDiscardDirection = MathUtils.clamp(mDiscardDirection, -1.0f, 1.0f);
                    distanceToEdge = Math.max(target.top - y, y - target.bottom);
                    edgeToEdge = target.height();
                }

                float scaledDiscardX = x - mDiscardingTab.getLayoutTab().getX();
                float scaledDiscardY = y - mDiscardingTab.getLayoutTab().getY();
                mDiscardingTab.setDiscardOriginX(scaledDiscardX / mDiscardingTab.getScale());
                mDiscardingTab.setDiscardOriginY(scaledDiscardY / mDiscardingTab.getScale());
                mDiscardingTab.setDiscardFromClick(false);

                if (Math.abs(distanceToEdge) < DISCARD_SAFE_SELECTION_PCTG * edgeToEdge) {
                    mDiscardingTab = null;
                }
            }
        }
        if (mDiscardingTab != null) {
            float deltaAmount = mCurrentMode == Orientation.PORTRAIT ? amountX : amountY;
            mDiscardingTab.addToDiscardAmount(deltaAmount);
        }
    }

    /**
     * Called on touch/tilt scroll event.
     *
     * @param x       The x coordinate of the end of the scroll event.
     * @param y       The y coordinate of the end of the scroll event.
     * @param amountX The number of pixels scrolled in the x direction.
     * @param amountY The number of pixels scrolled in the y direction.
     * @param isTilt  True if the call comes from a tilt event.
     */
    private void scroll(float x, float y, float amountX, float amountY, boolean isTilt) {
        if ((!mScroller.isFinished() && isTilt) || mStackTabs == null
                || (mOverviewAnimationType != OverviewAnimationType.NONE
                           && mOverviewAnimationType != OverviewAnimationType.DISCARD
                           && mOverviewAnimationType != OverviewAnimationType.UNDISCARD
                           && mOverviewAnimationType != OverviewAnimationType.DISCARD_ALL
                           && mOverviewAnimationType != OverviewAnimationType.ENTER_STACK)) {
            return;
        }

        float amountScreen = mCurrentMode == Orientation.PORTRAIT ? amountY : amountX;
        float amountScroll = amountScreen;
        float amountEvenOut = amountScreen;

        // Computes the right amount for the scrolling so the finger matches the tab under it.
        float tabScrollSpaceFinal = 0;
        if (mScrollingTab == null || isTilt) {
            mScrollingTab = getTabAtPositon(x, y);
        }

        if (mScrollingTab == null && mInSwipe && mStackTabs != null) {
            int index = mTabList.index();
            if (index >= 0 && index <= mStackTabs.length) mScrollingTab = mStackTabs[index];
        }

        if (mScrollingTab == null) {
            if (!isTilt) {
                amountScroll = 0;
                amountEvenOut = 0;
            }
        } else if (mScrollingTab.getIndex() == 0) {
            amountEvenOut = 0;
        } else {
            // Find the scroll that make the selected tab move the right
            // amount on the screen.
            float tabScrollSpace = mScrollingTab.getScrollOffset() + mScrollOffset;
            float tabScreen = scrollToScreen(tabScrollSpace);
            tabScrollSpaceFinal = screenToScroll(tabScreen + amountScreen);
            amountScroll = tabScrollSpaceFinal - tabScrollSpace;
            // Matching the finger is too strong of a constraints on the edges. So we make
            // sure the end value is not too far from the linear case.
            amountScroll = Math.signum(amountScreen)
                    * MathUtils.clamp(Math.abs(amountScroll), Math.abs(amountScreen) * 0.5f,
                              Math.abs(amountScreen) * 2.0f);
        }

        // Evens out the tabs and correct the scroll amount if needed.
        if (evenOutTabs(amountEvenOut, false) && mScrollingTab.getIndex() > 0) {
            // Adjust the amount after the even phase
            float tabScrollSpace = mScrollingTab.getScrollOffset() + mScrollOffset;
            amountScroll = tabScrollSpaceFinal - tabScrollSpace;
        }

        // Actually do the scrolling.
        setScrollTarget(mScrollTarget + amountScroll, false);
    }

    /**
     * OverlappingStack implements this to auto-magically the cards as the stack get scrolled.
     * NonOverlappingStack just ignores this call.
     *
     * @param amount                The amount of scroll performed in pixel. The sign indicates
     *                              the direction.
     * @param allowReverseDirection Whether or not to allow corrections in the reverse direction
     *                              of the amount scrolled.
     * @return                      True if any tab had been 'visibly' moved.
     */
    protected abstract boolean evenOutTabs(float amount, boolean allowReverseDirection);

    /**
     * Called on touch fling event. Scroll the stack or help to discard a tab.
     *
     * @param time      The current time of the app in ms.
     * @param x         The y coordinate of the start of the fling event.
     * @param y         The y coordinate of the start of the fling event.
     * @param velocityX The amount of velocity in the x direction.
     * @param velocityY The amount of velocity in the y direction.
     */
    public void fling(long time, float x, float y, float velocityX, float velocityY) {
        if (mDragLock != DragLock.SCROLL && mDiscardingTab != null) {
            float velocity = mCurrentMode == Orientation.PORTRAIT ? velocityX : velocityY;
            float maxDelta = getDiscardRange() * DISCARD_FLING_MAX_CONTRIBUTION;
            float deltaAmount = MathUtils.clamp(velocity * DISCARD_FLING_DT, -maxDelta, maxDelta);
            mDiscardingTab.addToDiscardAmount(deltaAmount);
        } else if (mOverviewAnimationType == OverviewAnimationType.NONE && mScroller.isFinished()
                && mOverScrollOffset == 0 && getTabIndexAtPositon(x, y) >= 0) {
            float velocity = mCurrentMode == Orientation.PORTRAIT
                    ? velocityY
                    : (LocalizationUtils.isLayoutRtl() ? -velocityX : velocityX);
            // Fling only overscrolls when the stack is fully unfolded.
            mScroller.fling(0, (int) mScrollTarget, 0, (int) velocity, 0, 0,
                    (int) getMinScroll(false), (int) getMaxScroll(false), 0,
                    (int) ((velocity > 0 ? mMaxOverScroll : mMaxUnderScroll)
                            * MAX_OVER_FLING_SCALE),
                    time);

            // Set the target to the final scroll position to make sure
            // the offset finally gets there regardless of what happens.
            // We override this when the user interrupts the fling though.
            setScrollTarget(mScroller.getFinalY(), false);
        }
    }

    /**
     * Get called on down touch event.
     *
     * @param time The current time of the app in ms.
     */
    public void onDown(long time) {
        mDragLock = DragLock.NONE;
        if (mOverviewAnimationType == OverviewAnimationType.NONE) {
            stopScrollingMovement(time);
        }
        // Resets the scrolling state.
        mScrollingTab = null;
        commitDiscard(time, false);
    }

    /**
     * Get called on long press touch event.
     *
     * @param time The current time of the app in ms.
     * @param x The x coordinate in pixel inside the stack view.
     * @param y The y coordinate in pixel inside the stack view.
     */
    public abstract void onLongPress(long time, float x, float y);

    /**
     * Called when at least 2 touch events are detected.
     *
     * @param time       The current time of the app in ms.
     * @param x0         The x coordinate of the first touch event.
     * @param y0         The y coordinate of the first touch event.
     * @param x1         The x coordinate of the second touch event.
     * @param y1         The y coordinate of the second touch event.
     * @param firstEvent The pinch is the first of a sequence of pinch events.
     */
    public abstract void onPinch(
            long time, float x0, float y0, float x1, float y1, boolean firstEvent);

    /**
     * Commits or release the that currently being considered for discard. This function
     * also triggers the associated animations.
     *
     * @param time         The current time of the app in ms.
     * @param allowDiscard Whether to allow to discard the tab currently being considered
     *                     for discard.
     */
    protected void commitDiscard(long time, boolean allowDiscard) {
        if (mDiscardingTab == null) return;

        assert mStackTabs != null;
        StackTab discarded = mDiscardingTab;
        if (Math.abs(discarded.getDiscardAmount()) / getDiscardRange() > DISCARD_COMMIT_THRESHOLD
                && allowDiscard) {
            mLayout.uiRequestingCloseTab(time, discarded.getId());
            RecordUserAction.record("MobileStackViewSwipeCloseTab");
            RecordUserAction.record("MobileTabClosed");
        } else {
            startAnimation(time, OverviewAnimationType.UNDISCARD);
        }
        mDiscardingTab = null;
        mLayout.requestUpdate();
    }

    /**
     * Called on touch up or cancel event.
     */
    public void onUpOrCancel(long time) {
        // Commit or uncommit discard tab
        commitDiscard(time, true);

        resetInputActionIndices();

        springBack(time);
    }

    /**
     * Bounces the scroll position back to a valid value (e.g. to correct an overscroll or
     * implement snapping).
     */
    protected abstract void springBack(long time);

    /**
     * Called on touch click event.
     *
     * @param time The current time of the app in ms.
     * @param x    The x coordinate in pixel inside the stack view.
     * @param y    The y coordinate in pixel inside the stack view.
     */
    public void click(long time, float x, float y) {
        if (mOverviewAnimationType != OverviewAnimationType.NONE
                && mOverviewAnimationType != OverviewAnimationType.DISCARD
                && mOverviewAnimationType != OverviewAnimationType.UNDISCARD
                && mOverviewAnimationType != OverviewAnimationType.DISCARD_ALL) {
            return;
        }
        int clicked = getTabIndexAtPositon(x, y, LayoutTab.getTouchSlop());
        if (clicked >= 0) {
            // Check if the click was within the boundaries of the close button defined by its
            // visible coordinates.
            if (mStackTabs[clicked].getLayoutTab().checkCloseHitTest(x, y)) {
                // Tell the model to close the tab because the close button was pressed.  The
                // model will then trigger a notification which will start the actual close
                // process here if necessary.
                StackTab tab = mStackTabs[clicked];
                final float halfCloseBtnWidth = LayoutTab.CLOSE_BUTTON_WIDTH_DP / 2.f;
                final float halfCloseBtnHeight = mBorderTopPadding / 2.f;
                final float contentWidth = tab.getLayoutTab().getOriginalContentWidth();

                tab.setDiscardOriginY(halfCloseBtnHeight);
                tab.setDiscardOriginX(isCloseButtonOnRight() ? contentWidth - halfCloseBtnWidth
                                                             : halfCloseBtnWidth);
                tab.setDiscardFromClick(true);
                mLayout.uiRequestingCloseTab(time, tab.getId());
                RecordUserAction.record("MobileStackViewCloseTab");
                RecordUserAction.record("MobileTabClosed");
            } else {
                // Let the model know that a new {@link LayoutTab} was selected. The model will
                // notify us if we need to do anything visual. setIndex() will possibly switch
                // the models and broadcast the event.
                mLayout.uiSelectingTab(time, mStackTabs[clicked].getId());
            }
        }
    }

    /*
     * Initialization and Utility Methods
     */

    /**
     * @param context The current Android's context.
     */
    public void contextChanged(Context context) {
        Resources res = context.getResources();
        final float pxToDp = 1.0f / res.getDisplayMetrics().density;

        mMinScrollMotion = DRAG_MOTION_THRESHOLD_DP;
        final float maxOverScrollPx = res.getDimensionPixelOffset(R.dimen.over_scroll);
        final float maxUnderScrollPx = Math.round(maxOverScrollPx * MAX_UNDER_SCROLL_SCALE);
        mMaxOverScroll = maxOverScrollPx * pxToDp;
        mMaxUnderScroll = maxUnderScrollPx * pxToDp;
        mMaxOverScrollAngle = res.getInteger(R.integer.over_scroll_angle);
        mMaxOverScrollSlide = res.getDimensionPixelOffset(R.dimen.over_scroll_slide) * pxToDp;
        mBorderTransparentTop =
                res.getDimension(R.dimen.tabswitcher_border_frame_transparent_top) * pxToDp;
        mBorderTransparentSide =
                res.getDimension(R.dimen.tabswitcher_border_frame_transparent_side) * pxToDp;
        mBorderTopPadding = res.getDimension(R.dimen.tabswitcher_border_frame_padding_top) * pxToDp;
        mBorderLeftPadding =
                res.getDimension(R.dimen.tabswitcher_border_frame_padding_left) * pxToDp;

        // Just in case the density has changed, rebuild the OverScroller.
        mScroller = new StackScroller(context);
    }

    /**
     * @param width       The new width of the layout.
     * @param height      The new height of the layout.
     * @param orientation The new orientation of the layout.
     */
    public void notifySizeChanged(float width, float height, @Orientation int orientation) {
        updateCurrentMode(orientation);

        // Changing the orientation can change which side of the tab we want to show the close
        // button on (if the horizontal tab switcher experiment is not enabled).
        if (mStackTabs == null) return;
        boolean closeButtonIsOnRight = isCloseButtonOnRight();
        for (int i = 0; i < mStackTabs.length; i++) {
            mStackTabs[i].getLayoutTab().setCloseButtonIsOnRight(closeButtonIsOnRight);
        }
    }

    protected float getScrollDimensionSize() {
        return mCurrentMode == Orientation.PORTRAIT ? mLayout.getHeightMinusBrowserControls()
                                                    : mLayout.getWidth();
    }

    /**
     * Gets the tab instance at the requested position.
     *
     * @param x The x coordinate where to perform the hit test.
     * @param y The y coordinate where to perform the hit test.
     * @return  The instance of the tab selected. null if none.
     */
    private StackTab getTabAtPositon(float x, float y) {
        int tabIndexAtPosition = getTabIndexAtPositon(x, y, 0);
        return tabIndexAtPosition < 0 ? null : mStackTabs[tabIndexAtPosition];
    }

    /**
     * Gets the tab index at the requested position.
     *
     * @param x The x coordinate where to perform the hit test.
     * @param y The y coordinate where to perform the hit test.
     * @return  The index of the tab selected. -1 if none.
     */
    protected int getTabIndexAtPositon(float x, float y) {
        return getTabIndexAtPositon(x, y, 0);
    }

    /**
     * Gets the tab index at the requested position.
     *
     * @param x    The x coordinate where to perform the hit test.
     * @param y    The y coordinate where to perform the hit test.
     * @param slop The acceptable distance to a tab for it to be considered.
     * @return     The index of the tab selected. -1 if none.
     */
    private int getTabIndexAtPositon(float x, float y, float slop) {
        int closestIndex = -1;
        float closestDistance = mLayout.getHeight() + mLayout.getWidth();
        if (mStackTabs != null) {
            for (int i = mStackTabs.length - 1; i >= 0; --i) {
                // This is a fail safe.  We should never have a situation where a dying
                // {@link LayoutTab} can get accessed (the animation check should catch it).
                if (!mStackTabs[i].isDying() && mStackTabs[i].getLayoutTab().isVisible()) {
                    float d = mStackTabs[i].getLayoutTab().computeDistanceTo(x, y);
                    // Strict '<' is very important here because we might have several tab at
                    // the same place and we want the one above.
                    if (d < closestDistance) {
                        closestIndex = i;
                        closestDistance = d;
                        if (d == 0) break;
                    }
                }
            }
        }
        return closestDistance <= slop ? closestIndex : -1;
    }

    /**
     * ComputeTabPosition pass 1:
     * Combine the overall stack scale with the animated tab scale.
     *
     * @param stackRect The frame of the stack.
     */
    private void computeTabScaleAlphaDepthHelper(RectF stackRect) {
        final float stackScale = getStackScale(stackRect);
        final float discardRange = getDiscardRange();

        for (int i = 0; i < mStackTabs.length; ++i) {
            assert mStackTabs[i] != null;
            StackTab stackTab = mStackTabs[i];
            LayoutTab layoutTab = stackTab.getLayoutTab();
            final float discard = stackTab.getDiscardAmount();

            // Scale
            float discardScale =
                    computeDiscardScale(discard, discardRange, stackTab.getDiscardFromClick());
            layoutTab.setScale(stackTab.getScale() * discardScale * stackScale);
            layoutTab.setBorderScale(discardScale);

            // Alpha
            float discardAlpha = computeDiscardAlpha(discard, discardRange);
            layoutTab.setAlpha(stackTab.getAlpha() * discardAlpha);
        }
    }

    /**
     * ComputeTabPosition pass 2:
     * Adjust the scroll offsets of each tab so no there is no void in between tabs.
     */
    private void computeTabScrollOffsetHelper() {
        float maxScrollOffset = Float.MAX_VALUE;
        for (int i = 0; i < mStackTabs.length; ++i) {
            if (mStackTabs[i].isDying()) continue;

            float tabScrollOffset = Math.min(maxScrollOffset, mStackTabs[i].getScrollOffset());
            mStackTabs[i].setScrollOffset(tabScrollOffset);

            float maxScreenScrollOffset = scrollToScreen(mScrollOffset + tabScrollOffset);
            maxScrollOffset = -mScrollOffset
                    + screenToScroll(maxScreenScrollOffset
                            + mStackTabs[i].getSizeInScrollDirection(mCurrentMode));
        }
    }

    /**
     * @return Whether or not to enable logic that gives the tabs a "stacked" appearance at the
     *         top (in portrait mode) or left (in landscape mode).
     */
    protected abstract boolean shouldStackTabsAtTop();

    /**
     * @return Whether or not to enable logic that gives the tabs a "stacked" appearance at the
     *         bottom (in portrait mode) or right (in landscape mode).
     */
    protected abstract boolean shouldStackTabsAtBottom();

    /**
     * @return How much the stack should adjust the y position of each LayoutTab in portrait
     *         mode (as a fraction of the amount space that would be above and below the tab if
     *         it were centered).
     */
    protected abstract float getStackPortraitYOffsetProportion();

    /**
     * @return How much the stack should adjust the x position of each LayoutTab in landscape
     *         mode (as a fraction of the amount space that would be to the left and right of
     *         the tab if it were centered).
     */
    protected abstract float getStackLandscapeStartOffsetProportion();

    /**
     * @return How much the stack should adjust the x position of each LayoutTab in portrait
     *         mode (as a fraction of the amount space that would be above and below the tab if
     *         it were centered).
     */
    protected abstract float getStackLandscapeYOffsetProportion();

    /**
     * ComputeTabPosition pass 3:
     * Compute the position of the tabs. Adjust for top and bottom stacking.
     *
     * @param stackRect The frame of the stack.
     */
    private void computeTabOffsetHelper(RectF stackRect) {
        final boolean portrait = mCurrentMode == Orientation.PORTRAIT;

        // Precompute the position using scroll offset and top stacking.
        final float parentWidth = stackRect.width();
        final float parentHeight = stackRect.height();
        final float overscrollPercent = computeOverscrollPercent();
        final float scrollOffset =
                MathUtils.clamp(mScrollOffset, getMinScroll(false), getMaxScroll(false));
        final float stackScale = getStackScale(stackRect);

        int stackedCount = 0;
        float minStackedPosition = 0.0f;
        for (int i = 0; i < mStackTabs.length; ++i) {
            assert mStackTabs[i] != null;
            StackTab stackTab = mStackTabs[i];
            LayoutTab layoutTab = stackTab.getLayoutTab();

            // Position
            final float stackScrollOffset =
                    stackTab.isDying() ? mScrollOffsetForDyingTabs : scrollOffset;
            float screenScrollOffset = approxScreen(stackTab, stackScrollOffset);

            if (shouldStackTabsAtTop()) {
                // Resolve top stacking
                screenScrollOffset = Math.max(minStackedPosition, screenScrollOffset);
                if (stackedCount < MAX_NUMBER_OF_STACKED_TABS_TOP) {
                    // This make sure all the tab get stacked up as one when all the tabs do a
                    // full roll animation.
                    final float tiltXcos = (float) Math.cos(Math.toRadians(layoutTab.getTiltX()));
                    final float tiltYcos = (float) Math.cos(Math.toRadians(layoutTab.getTiltY()));
                    float collapse = Math.min(Math.abs(tiltXcos), Math.abs(tiltYcos));
                    collapse *= layoutTab.getAlpha();
                    minStackedPosition += StackTab.sStackedTabVisibleSize * collapse;
                }
                stackedCount += stackTab.isDying() ? 0 : 1;
                if (overscrollPercent < 0) {
                    // Oversroll at the top of the screen. For the first
                    // OVERSCROLL_TOP_SLIDE_PCTG of the overscroll, slide the tabs
                    // together so they completely overlap.  After that, stop scrolling the
                    // tabs.
                    screenScrollOffset +=
                            (overscrollPercent / OVERSCROLL_TOP_SLIDE_PCTG) * screenScrollOffset;
                    screenScrollOffset = Math.max(0, screenScrollOffset);
                }
            }

            // Note: All the Offsets except for centering shouldn't depend on the tab's scaling
            //       because it interferes the scaling center.

            // Centers the tab in its parent.
            float xIn = (parentWidth - layoutTab.getScaledContentWidth()) / 2.0f;
            float yIn = (parentHeight - layoutTab.getScaledContentHeight()) / 2.0f;

            // We want slight offset from the center so that multiple tab browsing
            // have more space to its expanding direction. e.g., On portrait mode,
            // there will be more space on the bottom than top.
            final float horizontalPadding =
                    (parentWidth
                            - layoutTab.getOriginalContentWidth() * getScaleAmount() * stackScale)
                    / 2.0f;
            final float verticalPadding =
                    (parentHeight
                            - layoutTab.getOriginalContentHeight() * getScaleAmount() * stackScale)
                    / 2.0f;

            if (portrait) {
                yIn += getStackPortraitYOffsetProportion() * verticalPadding;
                yIn += screenScrollOffset;
            } else {
                if (LocalizationUtils.isLayoutRtl()) {
                    xIn -= getStackLandscapeStartOffsetProportion() * horizontalPadding;
                    xIn -= screenScrollOffset;
                } else {
                    xIn += getStackLandscapeStartOffsetProportion() * horizontalPadding;
                    xIn += screenScrollOffset;
                }
                yIn += getStackLandscapeYOffsetProportion() * verticalPadding;
            }

            layoutTab.setX(xIn);
            layoutTab.setY(yIn);
        }

        if (shouldStackTabsAtBottom()) {
            // Resolve bottom stacking
            stackedCount = 0;
            float maxStackedPosition =
                    portrait ? mLayout.getHeightMinusBrowserControls() : mLayout.getWidth();
            for (int i = mStackTabs.length - 1; i >= 0; i--) {
                assert mStackTabs[i] != null;
                StackTab stackTab = mStackTabs[i];
                LayoutTab layoutTab = stackTab.getLayoutTab();
                if (stackTab.isDying()) continue;

                float pos;
                if (portrait) {
                    pos = layoutTab.getY();
                    layoutTab.setY(Math.min(pos, maxStackedPosition));
                } else if (LocalizationUtils.isLayoutRtl()) {
                    // On RTL landscape, pos is a distance between tab's right and mLayout's
                    // right.
                    float posOffset = mLayout.getWidth()
                            - layoutTab.getOriginalContentWidth() * getScaleAmount() * stackScale;
                    pos = -layoutTab.getX() + posOffset;
                    layoutTab.setX(-Math.min(pos, maxStackedPosition) + posOffset);
                } else {
                    pos = layoutTab.getX();
                    layoutTab.setX(Math.min(pos, maxStackedPosition));
                }
                if (pos >= maxStackedPosition && stackedCount < MAX_NUMBER_OF_STACKED_TABS_BOTTOM) {
                    maxStackedPosition -= StackTab.sStackedTabVisibleSize;
                    stackedCount++;
                }
            }
        }

        // final position blend
        final float discardRange = getDiscardRange();
        for (int i = 0; i < mStackTabs.length; ++i) {
            assert mStackTabs[i] != null;
            StackTab stackTab = mStackTabs[i];
            LayoutTab layoutTab = stackTab.getLayoutTab();

            final float xIn = layoutTab.getX() + stackTab.getXInStackOffset();
            final float yIn = layoutTab.getY() + stackTab.getYInStackOffset();
            final float xOut = stackTab.getXOutOfStack();
            final float yOut = stackTab.getYOutOfStack();
            float x = MathUtils.interpolate(xOut, xIn, stackTab.getXInStackInfluence());
            float y = MathUtils.interpolate(yOut, yIn, stackTab.getYInStackInfluence());

            // Discard offsets
            if (stackTab.getDiscardAmount() != 0) {
                float discard = stackTab.getDiscardAmount();
                boolean fromClick = stackTab.getDiscardFromClick();
                float scale = computeDiscardScale(discard, discardRange, fromClick);
                float deltaX = stackTab.getDiscardOriginX()
                        - stackTab.getLayoutTab().getOriginalContentWidth() / 2.f;
                float deltaY = stackTab.getDiscardOriginY()
                        - stackTab.getLayoutTab().getOriginalContentHeight() / 2.f;
                float discardOffset = fromClick ? 0.f : discard;
                if (portrait) {
                    x += discardOffset + deltaX * (1.f - scale);
                    y += deltaY * (1.f - scale);
                } else {
                    x += deltaX * (1.f - scale);
                    y += discardOffset + deltaY * (1.f - scale);
                }
            }

            // Finally apply the stack translation
            layoutTab.setX(stackRect.left + x);
            layoutTab.setY(stackRect.top + y);
        }
    }

    /**
     * ComputeTabPosition pass 5:
     * Computes the clipping, visibility and adjust overall alpha if needed.
     */
    protected abstract void computeTabClippingVisibilityHelper();

    /**
     * Computes the index that should be assumed to be the currently centered tab, for purposes
     * of prioritizing which thumbnails to render.
     */
    protected abstract int computeReferenceIndex();

    /**
     * ComputeTabPosition pass 6:
     * Updates the visibility sorting value to use to figure out which thumbnails to load.
     *
     * @param stackRect The frame of the stack.
     */
    private void computeTabVisibilitySortingHelper(RectF stackRect) {
        int referenceIndex = mReferenceOrderIndex;
        if (referenceIndex == -1) referenceIndex = computeReferenceIndex();

        final float width = mLayout.getWidth();
        final float height = mLayout.getHeight();
        final float left = MathUtils.clamp(stackRect.left, 0, width);
        final float right = MathUtils.clamp(stackRect.right, 0, width);
        final float top = MathUtils.clamp(stackRect.top, 0, height);
        final float bottom = MathUtils.clamp(stackRect.bottom, 0, height);
        final float stackArea = (right - left) * (bottom - top);
        final float layoutArea = Math.max(width * height, 1.0f);
        final float stackVisibilityMultiplier = stackArea / layoutArea;

        for (int i = 0; i < mStackTabs.length; i++) {
            mStackTabs[i].updateStackVisiblityValue(stackVisibilityMultiplier);
            mStackTabs[i].updateVisiblityValue(referenceIndex);
        }
    }

    /**
     * Determine the current amount of overscroll. If the value is 0, there is
     * no overscroll. If the value is < 0, tabs are overscrolling towards the
     * top or or left. If the value is > 0, tabs are overscrolling towards the
     * bottom or right.
     */
    private float computeOverscrollPercent() {
        if (mOverScrollOffset >= 0) {
            return mOverScrollOffset / mMaxOverScroll;
        } else {
            return mOverScrollOffset / mMaxUnderScroll;
        }
    }

    /**
     * Update the tilt of each tab for full roll if necessary.
     *
     * @param time      The current time of the app in ms.
     * @param stackRect The frame of the stack.
     */
    private void fullRollHelper(long time, RectF stackRect) {
        if (mOverviewAnimationType != OverviewAnimationType.FULL_ROLL
                && computeOverscrollPercent() < 0
                && mOverScrollCounter >= OVERSCROLL_FULL_ROLL_TRIGGER) {
            startAnimation(time, OverviewAnimationType.FULL_ROLL);
            mOverScrollCounter = 0;
            // Remove overscroll so when the animation finishes the overscroll won't
            // be bothering.
            setScrollTarget(
                    MathUtils.clamp(mScrollOffset, getMinScroll(false), getMaxScroll(false)),
                    false);
        }
    }

    /** Whether or not to apply logic to enforce that there are no gaps between tabs. */
    protected abstract boolean shouldCloseGapsBetweenTabs();

    /**
     * Computes the {@link LayoutTab} position from the stack and the stackTab data.
     *
     * @param time      The current time of the app in ms.
     * @param stackRect The rectangle the stack should be drawn into. It may change over frames.
     */
    public void computeTabPosition(long time, RectF stackRect) {
        if (mStackTabs == null || mStackTabs.length == 0) return;

        // Step 1: Updates the {@link LayoutTab} scale, alpha and depth values.
        computeTabScaleAlphaDepthHelper(stackRect);

        if (shouldCloseGapsBetweenTabs()) {
            // Step 2: Fix tab scroll offsets to avoid gaps.
            computeTabScrollOffsetHelper();
        }

        // Step 3: Compute the actual position.
        computeTabOffsetHelper(stackRect);

        // Step 4: Test if the full-roll animation needs to be run.
        fullRollHelper(time, stackRect);

        // Step 5: Clipping, visibility and adjust overall alpha.
        computeTabClippingVisibilityHelper();

        // Step 6: Update visibility sorting for prioritizing thumbnail texture request.
        computeTabVisibilitySortingHelper(stackRect);
    }

    /**
     * @param stackFocus The current amount of focus of the stack [0 .. 1]
     * @param orderIndex The index in the stack of the focused tab. -1 to ask the
     *                   stack to compute it.
     */
    public void setStackFocusInfo(float stackFocus, int orderIndex) {
        if (mStackTabs == null) return;
        mReferenceOrderIndex = orderIndex;
        for (int i = 0; i < mStackTabs.length; i++) {
            mStackTabs[i].getLayoutTab().setBorderCloseButtonAlpha(stackFocus);
        }
    }

    /**
     * Reverts the closure of the tab specified by {@code tabId}.  This will run an undiscard
     * animation on that tab.
     * @param time  The current time of the app in ms.
     * @param tabId The id of the tab to animate.
     */
    public void undoClosure(long time, int tabId) {
        createStackTabs(true);
        if (mStackTabs == null) return;

        for (int i = 0; i < mStackTabs.length; i++) {
            StackTab tab = mStackTabs[i];

            if (tab.getId() == tabId) {
                tab.setDiscardAmount(getDiscardRange());
                tab.setDying(false);
                tab.getLayoutTab().setMaxContentHeight(getMaxTabHeight());
            }
        }

        mSpacing = computeSpacing(mStackTabs.length);
        startAnimation(time, OverviewAnimationType.UNDISCARD);
    }

    /**
     * Creates the {@link StackTab}s needed for display and populates {@link #mStackTabs}.
     * It is called from show() at the beginning of every new draw phase. It tries to reuse old
     * {@link StackTab} instead of creating new ones every time.
     * @param restoreState Whether or not to restore the {@link LayoutTab} state when we rebuild
     *                     the {@link StackTab}s.  There are some properties like maximum content
     *                     size or whether or not to show the toolbar that might have to be
     *                     restored if we're calling this while the switcher is already visible.
     */
    private void createStackTabs(boolean restoreState) {
        final int count = mTabList.getCount();
        if (count == 0) {
            cleanupTabs();
        } else {
            StackTab[] oldTabs = mStackTabs;
            mStackTabs = new StackTab[count];

            final boolean isIncognito = mTabList.isIncognito();
            final boolean needTitle = !mLayout.isHiding();
            for (int i = 0; i < count; ++i) {
                Tab tab = mTabList.getTabAt(i);
                int tabId = tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
                mStackTabs[i] = findTabById(oldTabs, tabId);

                float maxContentWidth = -1.f;
                float maxContentHeight = -1.f;

                if (mStackTabs[i] != null && mStackTabs[i].getLayoutTab() != null && restoreState) {
                    maxContentWidth = mStackTabs[i].getLayoutTab().getMaxContentWidth();
                    maxContentHeight = mStackTabs[i].getLayoutTab().getMaxContentHeight();
                }

                LayoutTab layoutTab = mLayout.createLayoutTab(tabId, isIncognito,
                        Layout.SHOW_CLOSE_BUTTON, needTitle, maxContentWidth, maxContentHeight);
                layoutTab.setInsetBorderVertical(true);
                layoutTab.setShowToolbar(true);
                layoutTab.setToolbarAlpha(0.f);
                layoutTab.setAnonymizeToolbar(!mIsStackForCurrentTabList || mTabList.index() != i);
                layoutTab.setCloseButtonIsOnRight(isCloseButtonOnRight());

                if (mStackTabs[i] == null) {
                    mStackTabs[i] = new StackTab(layoutTab);
                } else {
                    mStackTabs[i].setLayoutTab(layoutTab);
                }

                mStackTabs[i].setNewIndex(i);
                // The initial enterStack animation will take care of
                // positioning, scaling, etc.
            }
        }
    }

    private StackTab findTabById(StackTab[] layoutTabs, int id) {
        if (layoutTabs == null) return null;
        final int count = layoutTabs.length;
        for (int i = 0; i < count; i++) {
            if (layoutTabs[i].getId() == id) return layoutTabs[i];
        }
        return null;
    }

    /**
     * Creates a {@link StackTab}.
     * This function should ONLY be called from {@link #tabCreated(long, int)} and nowhere else.
     *
     * @param id The id of the tab.
     * @return   Whether the tab has successfully been created and added.
     */
    private boolean createTabHelper(int id) {
        if (TabModelUtils.getTabById(mTabList, id) == null) return false;

        // Check to see if the tab already exists in our model.  This is
        // just to cover the case where stackEntered and then tabCreated()
        // called in a row.
        if (mStackTabs != null) {
            final int count = mStackTabs.length;
            for (int i = 0; i < count; ++i) {
                if (mStackTabs[i].getId() == id) {
                    return false;
                }
            }
        }

        createStackTabs(true);

        return true;
    }

    /**
     * @return The percentage of the screen that defines the spacing between tabs by default (no
     *         pinch).
     */
    protected abstract float getSpacingScreen();

    /**
     * This redetermines the proper spacing for the {@link StackTab}. It takes in a parameter
     * for the size instead of using the mStackTabs.length property because we could be setting
     * the spacing for a delete before the tab has been removed (will help with animations).
     * @param layoutTabCount The number of layout tabs currently in the Stack.
     * @return               How far apart the tabs should be spaced (modulo certain
     *                       adjustments, such as non-linear warping).
     */
    protected abstract int computeSpacing(int layoutTabCount);

    private float getStackScale(RectF stackRect) {
        return mCurrentMode == Orientation.PORTRAIT
                ? stackRect.width() / mLayout.getWidth()
                : stackRect.height() / mLayout.getHeightMinusBrowserControls();
    }

    protected void setScrollTarget(float offset, boolean immediate) {
        // Ensure that the stack cannot be scrolled too far in either direction.
        // mScrollOffset is clamped between [-min, 0], where offset 0 has the
        // farthest back tab (the first tab) at the top, with everything else
        // pulled down, and -min has the tab at the top of the stack (the last
        // tab) is pulled up and fully visible.
        final boolean overscroll = allowOverscroll();
        mScrollTarget = MathUtils.clamp(offset, getMinScroll(overscroll), getMaxScroll(overscroll));
        if (immediate) mScrollOffset = mScrollTarget;
        mCurrentScrollDirection = Math.signum(mScrollTarget - mScrollOffset);
    }

    /**
     * Gets the min scroll value.
     *
     * @param allowUnderScroll True if underscroll is allowed.
     */
    protected abstract float getMinScroll(boolean allowUnderScroll);

    /**
     * Gets the max scroll value.
     *
     * @param allowOverscroll True if overscroll is allowed.
     */
    protected float getMaxScroll(boolean allowOverscroll) {
        if (mStackTabs == null || !allowOverscroll) {
            return 0;
        } else {
            return mMaxOverScroll;
        }
    }

    private void stopScrollingMovement(long time) {
        // We have to cancel the fling if it is in progress.
        if (mScroller.computeScrollOffset(time)) {
            // Set the current offset and target to the current scroll
            // position so the {@link StackTab}s won't scroll anymore.
            setScrollTarget(mScroller.getCurrY(), true /* immediate */);

            // Tell the scroller to finish scrolling.
            mScroller.forceFinished(true);
        } else {
            // If we aren't scrolling just set the target to the current
            // offset so we don't move anymore.
            setScrollTarget(mScrollOffset, false);
        }
    }

    protected boolean allowOverscroll() {
        // All the animations that want to leave the tilt value to be set by the overscroll must
        // be added here.
        return mOverviewAnimationType == OverviewAnimationType.NONE
                || mOverviewAnimationType == OverviewAnimationType.VIEW_MORE
                || mOverviewAnimationType == OverviewAnimationType.ENTER_STACK;
    }

    /**
     * Smoothes input signal. The definition of the input is lower than the
     * pixel density of the screen so we need to smooth the input to give the illusion of smooth
     * animation on screen from chunky inputs.
     * The combination of 20 pixels and 0.9f ensures that the output is not more than 2 pixels
     * away from the target.
     * TODO: This has nothing to do with time, just draw rate.
     *       Is this okay or do we want to have the interpolation based on the time elapsed?
     * @param current   The current value of the signal.
     * @param input     The raw input value.
     * @return          The smoothed signal.
     */
    private float smoothInput(float current, float input) {
        current = MathUtils.clamp(current, input - 20, input + 20);
        return MathUtils.interpolate(current, input, 0.9f);
    }

    protected void forceScrollStop() {
        mScroller.forceFinished(true);
        updateOverscrollOffset();
        mScrollTarget = mScrollOffset;
    }

    private void updateScrollOffset(long time) {
        // If we are still scrolling, which is determined by a disparity
        // between our scroll offset and our scroll target, we need
        // to try to move closer to that position.
        if (mScrollOffset != mScrollTarget) {
            if (mScroller.computeScrollOffset(time)) {
                final float newScrollOffset = mScroller.getCurrY();
                evenOutTabs(newScrollOffset - mScrollOffset, true);
                // We are currently in the process of being flinged.  Just
                // ask the scroller for the new position.
                mScrollOffset = newScrollOffset;
            } else {
                // We are just being dragged or scrolled, not flinged.  This
                // means we should move closer to our target quickly but not
                // quickly enough to show the stuttering that could be
                // exposed by the touch event rate.
                mScrollOffset = smoothInput(mScrollOffset, mScrollTarget);
            }
            mLayout.requestUpdate();
        } else {
            // Make sure that the scroller is marked as finished when the destination is
            // reached.
            mScroller.forceFinished(true);
        }
        updateOverscrollOffset();
    }

    private void updateOverscrollOffset() {
        float clamped = MathUtils.clamp(mScrollOffset, getMinScroll(false), getMaxScroll(false));
        if (!allowOverscroll()) {
            mScrollOffset = clamped;
        }
        float overscroll = mScrollOffset - clamped;

        // Counts the number of overscroll push in the same direction in a row.
        int derivativeState = (int) Math.signum(Math.abs(mOverScrollOffset) - Math.abs(overscroll));
        if (derivativeState != mOverScrollDerivative && derivativeState == 1 && overscroll < 0) {
            mOverScrollCounter++;
        } else if (overscroll > 0 || mCurrentMode == Orientation.LANDSCAPE) {
            mOverScrollCounter = 0;
        }
        mOverScrollDerivative = derivativeState;

        mOverScrollOffset = overscroll;
    }

    /**
     * Called when the stack is opened to reset all the tab and scroll positions.
     */
    protected abstract void resetAllScrollOffset();

    protected float approxScreen(StackTab tab, float globalScrollOffset) {
        return scrollToScreen(tab.getScrollOffset() + globalScrollOffset);
    }

    /**
     * Maps from scroll coordinates to screen coordinates.
     * @param scrollSpace The offset in scroll space.
     * @return            The offset on screen corresponding to the scroll space offset.
     */
    public abstract float scrollToScreen(float scrollSpace);

    /**
     * Maps from screen coordinates to scroll coordinates. This allows Stack subclasses (e.g.
     * OverlappingStack) to use non-linear scrolling.
     * @param scrollSpace The offset in screen space.
     * @return            The offset in scroll space corresponding to the offset on screen.
     */
    public abstract float screenToScroll(float screenSpace);

    /**
     * @return The range of the discard action. At the end of the +/- range the discarded tab
     *         will be fully transparent.
     */
    private float getDiscardRange() {
        return getRange(DISCARD_RANGE_SCREEN);
    }

    private float getRange(float range) {
        return range
                * (mCurrentMode == Orientation.PORTRAIT ? mLayout.getWidth()
                                                        : mLayout.getHeightMinusBrowserControls());
    }

    /**
     * @return The maximum height of a layout tab in the tab switcher.
     */
    public abstract float getMaxTabHeight();

    /**
     * @return The current spacing between tabs.
     */
    public float getSpacing() {
        return mSpacing;
    }

    /**
     * @return The current overall scroll offset for the Stack.
     */
    public float getScrollOffset() {
        return mScrollOffset;
    }

    /**
     * Computes the scale of the tab based on its discard status.
     *
     * @param amount    The discard amount.
     * @param range     The range of the absolute value of discard amount.
     * @param fromClick Whether or not the discard was from a click or a swipe.
     * @return          The scale of the tab to use to draw the tab.
     */
    public static float computeDiscardScale(float amount, float range, boolean fromClick) {
        if (Math.abs(amount) < 1.0f) return 1.0f;
        float t = amount / range;
        float endScale = fromClick ? DISCARD_END_SCALE_CLICK : DISCARD_END_SCALE_SWIPE;
        return MathUtils.interpolate(1.0f, endScale, Math.abs(t));
    }

    /**
     * Computes the alpha value of the tab based on its discard status.
     *
     * @param amount The discard amount.
     * @param range  The range of the absolute value of discard amount.
     * @return       The alpha value that need to be applied on the tab.
     */
    public static float computeDiscardAlpha(float amount, float range) {
        if (Math.abs(amount) < 1.0f) return 1.0f;
        float t = amount / range;
        t = MathUtils.clamp(t, -1.0f, 1.0f);
        return 1.f - Math.abs(t);
    }

    protected void updateCurrentMode(@Orientation int orientation) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)) {
            mCurrentMode = Orientation.LANDSCAPE;
        } else {
            mCurrentMode = orientation;
        }

        mDiscardDirection = getDefaultDiscardDirection();
        final float opaqueTopPadding = mBorderTopPadding - mBorderTransparentTop;
        mAnimationFactory = new StackAnimation(this, mLayout.getWidth(), mLayout.getHeight(),
                mLayout.getTopBrowserControlsHeight(), mBorderTopPadding, opaqueTopPadding,
                mBorderLeftPadding, mCurrentMode);
        mViewAnimationFactory = new StackViewAnimation(mLayout.getContext().getResources());
        if (mStackTabs == null) return;
        float width = mLayout.getWidth();
        for (int i = 0; i < mStackTabs.length; i++) {
            LayoutTab tab = mStackTabs[i].getLayoutTab();
            if (tab == null) continue;
            tab.setMaxContentWidth(width);
            tab.setMaxContentHeight(getMaxTabHeight());
        }
    }

    /**
     * Called to release everything. Called well after the view has been really hidden.
     */
    public void cleanupTabs() {
        mStackTabs = null;
        resetInputActionIndices();
    }

    /**
     * Resets all the indices that are pointing to tabs for various features.
     */
    protected void resetInputActionIndices() {
        mScrollingTab = null;
        mDiscardingTab = null;
    }

    /**
     * Reset session based parameters.
     * Called before the a session starts. Before the show, regardless if the stack is
     * displayable.
     */
    public void reset() {
        mIsDying = false;
    }

    /**
     * Called when the swipe animation get initiated. It gives a chance to initialize
     * everything.
     * @param time      The current time of the app in ms.
     * @param direction The direction the swipe is in.
     * @param x         The horizontal coordinate the swipe started at in dp.
     * @param y         The vertical coordinate the swipe started at in dp.
     */
    public void swipeStarted(long time, @ScrollDirection int direction, float x, float y) {
        if (direction != ScrollDirection.DOWN) return;

        // Restart the enter stack animation with the new warp values.
        startAnimation(time, OverviewAnimationType.ENTER_STACK);

        // Update the scroll offset to put the focused tab at the top.
        final int index = mTabList.index();

        if (mCurrentMode == Orientation.PORTRAIT
                || ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)) {
            mScrollOffset = -index * mSpacing;
        } else {
            mScrollOffset = -index * mSpacing + x - LANDSCAPE_SWIPE_DRAG_TAB_OFFSET_DP;
            mScrollOffset =
                    MathUtils.clamp(mScrollOffset, getMinScroll(false), getMaxScroll(false));
        }
        setScrollTarget(mScrollOffset, true);

        // Set up the tracking scroll parameters.
        mSwipeUnboundScrollOffset = mScrollOffset;
        mSwipeBoundedScrollOffset = mScrollOffset;

        // Reset other state.
        mSwipeIsCancelable = false;
        mSwipeCanScroll = false;
        mInSwipe = true;
    }

    /**
     * Updates a swipe gesture.
     * @param time The current time of the app in ms.
     * @param x    The horizontal coordinate the swipe is currently at in dp.
     * @param y    The vertical coordinate the swipe is currently at in dp.
     * @param dx   The horizontal delta since the last update in dp.
     * @param dy   The vertical delta since the last update in dp.
     * @param tx   The horizontal difference between the start and the current position in dp.
     * @param ty   The vertical difference between the start and the current position in dp.
     */
    public void swipeUpdated(long time, float x, float y, float dx, float dy, float tx, float ty) {
        if (!mInSwipe) return;

        final float toolbarSize = mLayout.getTopBrowserControlsHeight();
        if (ty > toolbarSize) mSwipeCanScroll = true;
        if (!mSwipeCanScroll) return;

        final int index = mTabList.index();

        // Check to make sure the index is still valid.
        if (index < 0 || index >= mStackTabs.length) {
            assert false : "Tab index out of bounds in Stack#swipeUpdated()";
            return;
        }

        final float delta = mCurrentMode == Orientation.PORTRAIT ? dy : dx;

        // Update the unbound scroll offset, tracking delta regardless of constraints.
        mSwipeUnboundScrollOffset += delta;

        // Figure out the new constrained position.
        final float minScroll = getMinScroll(true);
        final float maxScroll = getMaxScroll(true);
        float offset = MathUtils.clamp(mSwipeUnboundScrollOffset, minScroll, maxScroll);

        final float constrainedDelta = offset - mSwipeBoundedScrollOffset;
        mSwipeBoundedScrollOffset = offset;

        if (constrainedDelta == 0.f) return;

        if (mCurrentMode == Orientation.PORTRAIT) {
            dy = constrainedDelta;
        } else {
            dx = constrainedDelta;
        }

        // Propagate the new drag event.
        drag(time, x, y, dx, dy);

        // Figure out if the user has scrolled down enough that they can scroll back up and
        // exit.
        if (mCurrentMode == Orientation.PORTRAIT) {
            // The cancelable threshold is determined by the top position of the tab in the
            // stack.
            final float discardOffset = mStackTabs[index].getScrollOffset();
            final boolean beyondThreshold = -mScrollOffset < discardOffset;

            // Allow the user to cancel in the future if they're beyond the threshold.
            mSwipeIsCancelable |= beyondThreshold;

            // If the user can cancel the swipe and they're back behind the threshold, cancel.
            if (mSwipeIsCancelable && !beyondThreshold) swipeCancelled(time);
        } else {
            // The cancelable threshold is determined by the top position of the tab.
            final float discardOffset = mStackTabs[index].getLayoutTab().getY();

            boolean aboveThreshold = discardOffset < getRange(SWIPE_LANDSCAPE_THRESHOLD);

            mSwipeIsCancelable |= !aboveThreshold;

            if (mSwipeIsCancelable && aboveThreshold) swipeCancelled(time);
        }
    }

    /**
     * Called when the swipe ends; most likely on finger up event. It gives a chance to start
     * an ending animation to exit the mode gracefully.
     * @param time The current time of the app in ms.
     */
    public void swipeFinished(long time) {
        if (!mInSwipe) return;

        mInSwipe = false;

        onUpOrCancel(time);
    }

    /**
     * Called when the user has cancelled a swipe; most likely if they have dragged their finger
     * back to the starting position.  Some handlers will throw swipeFinished() instead.
     * @param time The current time of the app in ms.
     */
    public void swipeCancelled(long time) {
        if (!mInSwipe) return;

        mDiscardingTab = null;

        mInSwipe = false;

        // Select the current tab so we exit the switcher.
        Tab tab = TabModelUtils.getCurrentTab(mTabList);
        mLayout.uiSelectingTab(time, tab != null ? tab.getId() : Tab.INVALID_TAB_ID);
    }

    /**
     * Fling from a swipe gesture.
     * @param time The current time of the app in ms.
     * @param x    The horizontal coordinate the swipe is currently at in dp.
     * @param y    The vertical coordinate the swipe is currently at in dp.
     * @param tx   The horizontal difference between the start and the current position in dp.
     * @param ty   The vertical difference between the start and the current position in dp.
     * @param vx   The horizontal velocity of the fling.
     * @param vy   The vertical velocity of the fling.
     */
    public void swipeFlingOccurred(
            long time, float x, float y, float tx, float ty, float vx, float vy) {
        if (!mInSwipe) return;

        // Propagate the fling data.
        fling(time, x, y, vx, vy);

        onUpOrCancel(time);
    }

    public static final FloatProperty<Stack> SCROLL_OFFSET =
            new FloatProperty<Stack>("SCROLL_OFFSET") {
                @Override
                public void setValue(Stack stack, float v) {
                    stack.setScrollTarget(v, true);
                }

                @Override
                public Float get(Stack stack) {
                    return stack.getScrollOffset();
                }
            };
}
