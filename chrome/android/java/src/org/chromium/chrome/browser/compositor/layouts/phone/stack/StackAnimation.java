// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.animation.FloatProperty;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.BakedBezierInterpolator;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * A factory that builds animations for the tab stack.
 */
public class StackAnimation {
    @IntDef({OverviewAnimationType.ENTER_STACK, OverviewAnimationType.NEW_TAB_OPENED,
            OverviewAnimationType.TAB_FOCUSED, OverviewAnimationType.VIEW_MORE,
            OverviewAnimationType.REACH_TOP, OverviewAnimationType.DISCARD,
            OverviewAnimationType.DISCARD_ALL, OverviewAnimationType.UNDISCARD,
            OverviewAnimationType.START_PINCH, OverviewAnimationType.FULL_ROLL,
            OverviewAnimationType.NONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OverviewAnimationType {
        int ENTER_STACK = 0;
        int NEW_TAB_OPENED = 1;
        int TAB_FOCUSED = 2;
        int VIEW_MORE = 3;
        int REACH_TOP = 4;
        // Commit/uncommit tab discard animations
        int DISCARD = 5;
        int DISCARD_ALL = 6;
        int UNDISCARD = 7;
        // Start pinch animation un-tilt all the tabs.
        int START_PINCH = 8;
        // Special animation
        int FULL_ROLL = 9;
        // Used for when the current state of the system is not animating
        int NONE = 10;
    }

    private static final int ENTER_STACK_ANIMATION_DURATION_MS = 300;
    private static final int ENTER_STACK_BORDER_ALPHA_DURATION_MS = 200;
    private static final int ENTER_STACK_RESIZE_DELAY_MS = 10;
    private static final int ENTER_STACK_TOOLBAR_ALPHA_DURATION_MS = 100;
    private static final int ENTER_STACK_TOOLBAR_ALPHA_DELAY_MS = 100;
    private static final float ENTER_STACK_SIZE_RATIO = 0.35f;

    private static final int TAB_FOCUSED_ANIMATION_DURATION_MS = 400;
    private static final int TAB_FOCUSED_BORDER_ALPHA_DURATION_MS = 200;
    private static final int TAB_FOCUSED_TOOLBAR_ALPHA_DURATION_MS = 250;
    private static final int TAB_FOCUSED_Y_STACK_DURATION_MS = 200;
    private static final int TAB_FOCUSED_MAX_DELAY_MS = 100;

    private static final int VIEW_MORE_ANIMATION_DURATION_MS = 400;
    private static final int VIEW_MORE_MIN_SIZE = 200;
    private static final float VIEW_MORE_SIZE_RATIO = 0.75f;

    private static final int REACH_TOP_ANIMATION_DURATION_MS = 400;

    private static final int UNDISCARD_ANIMATION_DURATION_MS = 150;

    private static final int TAB_OPENED_ANIMATION_DURATION_MS = 300;

    private static final int DISCARD_ANIMATION_DURATION_MS = 150;

    private static final int TAB_REORDER_DURATION_MS = 500;
    private static final int TAB_REORDER_START_SPAN = 400;

    private static final int START_PINCH_ANIMATION_DURATION_MS = 75;

    private static final int FULL_ROLL_ANIMATION_DURATION_MS = 1000;

    private final float mWidth;
    private final float mHeight;
    private final float mTopBrowserControlsHeight;
    private final float mBorderTopHeight;
    private final float mBorderTopOpaqueHeight;
    private final float mBorderLeftWidth;
    private final Stack mStack;
    private final @Orientation int mOrientation;

    /**
     * Protected constructor.
     *
     * @param stack                       The stack using the animations provided by this class.
     * @param width                       The width of the layout in dp.
     * @param height                      The height of the layout in dp.
     * @param heightMinusBrowserControls  The height of the layout minus the browser controls in dp.
     * @param borderFramePaddingTop       The top padding of the border frame in dp.
     * @param borderFramePaddingTopOpaque The opaque top padding of the border frame in dp.
     * @param borderFramePaddingLeft      The left padding of the border frame in dp.
     */
    protected StackAnimation(Stack stack, float width, float height, float topBrowserControlsHeight,
            float borderFramePaddingTop, float borderFramePaddingTopOpaque,
            float borderFramePaddingLeft, @Orientation int orientation) {
        mStack = stack;
        mWidth = width;
        mHeight = height;
        mTopBrowserControlsHeight = topBrowserControlsHeight;
        mOrientation = orientation;

        mBorderTopHeight = borderFramePaddingTop;
        mBorderTopOpaqueHeight = borderFramePaddingTopOpaque;
        mBorderLeftWidth = borderFramePaddingLeft;
    }

    /**
     * The wrapper method responsible for delegating the animations request to the appropriate
     * helper method.  Not all parameters are used for each request.
     *
     * @param type          The type of animation to be created.  This is what
     *                      determines which helper method is called.
     * @param stack         The current stack.
     * @param tabs          The tabs that make up the current stack that will
     *                      be animated.
     * @param focusIndex    The index of the tab that is the focus of this animation.
     * @param sourceIndex   The index of the tab that triggered this animation.
     * @param spacing       The default spacing between the tabs.
     * @param discardRange  The range of the discard amount value.
     * @return              The resulting AnimatorSet that will animate the tabs
     *                      (with related FloatProperty)
     */
    public Pair<AnimatorSet, ArrayList<FloatProperty>> createAnimatorSetForType(
            @OverviewAnimationType int type, Stack stack, StackTab[] tabs, int focusIndex,
            int sourceIndex, int spacing, float discardRange) {
        if (tabs == null) return null;

        ArrayList<Animator> animationList = new ArrayList<>();
        ArrayList<FloatProperty> propertyList = new ArrayList<>();
        CompositorAnimationHandler handler = stack.getAnimationHandler();

        switch (type) {
            case OverviewAnimationType.DISCARD: // Purposeful fall through
            case OverviewAnimationType.DISCARD_ALL: // Purposeful fall through
            case OverviewAnimationType.UNDISCARD:
                createLandscapePortraitUpdateDiscardAnimatorSet(
                        animationList, propertyList, handler, stack, tabs, spacing, discardRange);
                break;
            case OverviewAnimationType.ENTER_STACK:
                // Responsible for generating the animations that shows the stack being entered.
                if (mOrientation == Orientation.LANDSCAPE) {
                    createLandscapeEnterStackAnimatorSet(
                            animationList, propertyList, handler, tabs, focusIndex, spacing);
                } else {
                    createPortraitEnterStackAnimatorSet(
                            animationList, propertyList, handler, tabs, focusIndex, spacing);
                }
                break;
            case OverviewAnimationType.FULL_ROLL:
                // Responsible for generating the animations that make all the tabs do a full roll.
                for (int i = 0; i < tabs.length; ++i) {
                    LayoutTab layoutTab = tabs[i].getLayoutTab();
                    // Set the pivot
                    layoutTab.setTiltX(
                            layoutTab.getTiltX(), layoutTab.getScaledContentHeight() / 2.0f);
                    layoutTab.setTiltY(
                            layoutTab.getTiltY(), layoutTab.getScaledContentWidth() / 2.0f);
                    // Create the angle animation
                    addLandscapePortraitTiltScrollAnimation(animationList, propertyList, handler,
                            layoutTab, -360.0f, FULL_ROLL_ANIMATION_DURATION_MS);
                }
                break;
            case OverviewAnimationType.NEW_TAB_OPENED:
                // Responsible for generating the animations that shows a new tab being opened.
                if (mOrientation == Orientation.LANDSCAPE) return null;

                for (int i = 0; i < tabs.length; i++) {
                    addToAnimation(animationList, propertyList, handler, tabs[i],
                            StackTab.SCROLL_OFFSET, tabs[i].getScrollOffset(), 0.0f,
                            TAB_OPENED_ANIMATION_DURATION_MS);
                }
                break;
            case OverviewAnimationType.REACH_TOP:
                // Responsible for generating the TabSwitcherAnimation that moves the tabs up so
                // they reach the to top the screen.
                float screenTarget = 0.0f;
                for (int i = 0; i < tabs.length; ++i) {
                    if (screenTarget
                            >= getLandscapePortraitScreenPositionInScrollDirection(tabs[i])) {
                        break;
                    }
                    addToAnimation(animationList, propertyList, handler, tabs[i],
                            StackTab.SCROLL_OFFSET, tabs[i].getScrollOffset(),
                            mStack.screenToScroll(screenTarget), REACH_TOP_ANIMATION_DURATION_MS);
                    screenTarget += mOrientation == Orientation.LANDSCAPE
                            ? tabs[i].getLayoutTab().getScaledContentWidth()
                            : tabs[i].getLayoutTab().getScaledContentHeight();
                }
                break;
            case OverviewAnimationType.START_PINCH:
                // Responsible for generating the animations that flattens tabs when a pinch begins.
                for (int i = 0; i < tabs.length; ++i) {
                    addLandscapePortraitTiltScrollAnimation(animationList, propertyList, handler,
                            tabs[i].getLayoutTab(), 0, START_PINCH_ANIMATION_DURATION_MS);
                }
                break;
            case OverviewAnimationType.TAB_FOCUSED:
                createLandscapePortraitTabFocusedAnimatorSet(
                        animationList, propertyList, handler, tabs, focusIndex, spacing);
                break;
            case OverviewAnimationType.VIEW_MORE:
                // Responsible for generating the animations that Shows more of the selected tab.
                if (sourceIndex + 1 >= tabs.length) return null;

                float offset = mOrientation == Orientation.LANDSCAPE
                        ? tabs[sourceIndex].getLayoutTab().getScaledContentWidth()
                        : tabs[sourceIndex].getLayoutTab().getScaledContentHeight();
                offset = offset * VIEW_MORE_SIZE_RATIO + tabs[sourceIndex].getScrollOffset()
                        - tabs[sourceIndex + 1].getScrollOffset();
                offset = Math.max(VIEW_MORE_MIN_SIZE, offset);

                for (int i = sourceIndex + 1; i < tabs.length; ++i) {
                    addToAnimation(animationList, propertyList, handler, tabs[i],
                            StackTab.SCROLL_OFFSET, tabs[i].getScrollOffset(),
                            tabs[i].getScrollOffset() + offset, VIEW_MORE_ANIMATION_DURATION_MS);
                }
                break;
            default:
                return null;
        }

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animationList);
        return Pair.create(set, propertyList);
    }

    private float getLandscapePortraitScreenPositionInScrollDirection(StackTab tab) {
        return mOrientation == Orientation.LANDSCAPE ? tab.getLayoutTab().getX()
                                                     : tab.getLayoutTab().getY();
    }

    private void addLandscapePortraitTiltScrollAnimation(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler,
            LayoutTab tab, float end, int durationMs) {
        if (mOrientation == Orientation.LANDSCAPE) {
            addToAnimation(animationList, propertyList, handler, tab, LayoutTab.TILTY,
                    tab.getTiltY(), end, durationMs);
        } else {
            addToAnimation(animationList, propertyList, handler, tab, LayoutTab.TILTX,
                    tab.getTiltX(), end, durationMs);
        }
    }

    // If this flag is enabled, we're using the non-overlapping tab switcher.
    private boolean isHorizontalTabSwitcherFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);
    }

    private void createPortraitEnterStackAnimatorSet(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler,
            StackTab[] tabs, int focusIndex, int spacing) {
        final float initialScrollOffset = mStack.screenToScroll(0);

        float trailingScrollOffset = 0.f;
        if (focusIndex >= 0 && focusIndex < tabs.length - 1) {
            final float focusOffset = tabs[focusIndex].getScrollOffset();
            final float nextOffset = tabs[focusIndex + 1].getScrollOffset();
            final float topSpacing = focusIndex == 0 ? spacing : 0.f;
            final float extraSpace = tabs[focusIndex].getLayoutTab().getScaledContentHeight()
                    * ENTER_STACK_SIZE_RATIO;
            trailingScrollOffset = Math.max(focusOffset - nextOffset + topSpacing + extraSpace, 0);
        }

        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];

            tab.resetOffset();
            tab.setScale(mStack.getScaleAmount());
            tab.setAlpha(1.f);
            tab.getLayoutTab().setToolbarAlpha(i == focusIndex ? 1.f : 0.f);
            tab.getLayoutTab().setBorderScale(1.f);

            float scrollOffset = mStack.screenToScroll(i * spacing);

            if (i < focusIndex) {
                tab.getLayoutTab().setMaxContentHeight(mStack.getMaxTabHeight());
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCROLL_OFFSET,
                        initialScrollOffset, scrollOffset, ENTER_STACK_ANIMATION_DURATION_MS);
            } else if (i > focusIndex) {
                tab.getLayoutTab().setMaxContentHeight(mStack.getMaxTabHeight());
                tab.setScrollOffset(scrollOffset + trailingScrollOffset);
                addToAnimation(animationList, propertyList, handler, tab,
                        StackTab.Y_IN_STACK_OFFSET, mHeight, 0, ENTER_STACK_ANIMATION_DURATION_MS);
            } else { // i == focusIndex
                tab.setScrollOffset(scrollOffset);

                addToAnimationWithDelay(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.MAX_CONTENT_HEIGHT,
                        tab.getLayoutTab().getUnclampedOriginalContentHeight(),
                        mStack.getMaxTabHeight(), ENTER_STACK_ANIMATION_DURATION_MS,
                        ENTER_STACK_RESIZE_DELAY_MS);
                addToAnimation(animationList, propertyList, handler, tab,
                        StackTab.Y_IN_STACK_INFLUENCE, 0.0f, 1.0f,
                        ENTER_STACK_BORDER_ALPHA_DURATION_MS);
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCALE, 1.0f,
                        mStack.getScaleAmount(), ENTER_STACK_BORDER_ALPHA_DURATION_MS);
                addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.TOOLBAR_Y_OFFSET, 0.f, getToolbarOffsetToLineUpWithBorder(),
                        ENTER_STACK_BORDER_ALPHA_DURATION_MS);
                addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.SIDE_BORDER_SCALE, 0.f, 1.f,
                        ENTER_STACK_BORDER_ALPHA_DURATION_MS);

                addToAnimationWithDelay(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.TOOLBAR_ALPHA, 1.f, 0.f, ENTER_STACK_BORDER_ALPHA_DURATION_MS,
                        ENTER_STACK_TOOLBAR_ALPHA_DELAY_MS);

                tab.setYOutOfStack(getStaticTabPosition());
            }
        }
    }

    private void createLandscapeEnterStackAnimatorSet(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler,
            StackTab[] tabs, int focusIndex, int spacing) {
        final float initialScrollOffset = mStack.screenToScroll(0);

        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];

            tab.resetOffset();
            tab.setScale(mStack.getScaleAmount());
            tab.setAlpha(1.f);
            tab.getLayoutTab().setToolbarAlpha(i == focusIndex ? 1.f : 0.f);
            tab.getLayoutTab().setBorderScale(1.f);

            final float scrollOffset = mStack.screenToScroll(i * spacing);

            addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                    LayoutTab.MAX_CONTENT_HEIGHT,
                    tab.getLayoutTab().getUnclampedOriginalContentHeight(),
                    mStack.getMaxTabHeight(), ENTER_STACK_ANIMATION_DURATION_MS);
            if (i < focusIndex) {
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCROLL_OFFSET,
                        initialScrollOffset, scrollOffset, ENTER_STACK_ANIMATION_DURATION_MS);
            } else if (i > focusIndex) {
                tab.setScrollOffset(scrollOffset);
                addToAnimation(animationList, propertyList, handler, tab,
                        StackTab.X_IN_STACK_OFFSET,
                        (mWidth > mHeight && LocalizationUtils.isLayoutRtl()) ? -mWidth : mWidth,
                        0.0f, ENTER_STACK_ANIMATION_DURATION_MS);
            } else { // i == focusIndex
                tab.setScrollOffset(scrollOffset);

                addToAnimation(animationList, propertyList, handler, tab,
                        StackTab.X_IN_STACK_INFLUENCE, 0.0f, 1.0f,
                        ENTER_STACK_BORDER_ALPHA_DURATION_MS);
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCALE, 1.0f,
                        mStack.getScaleAmount(), ENTER_STACK_BORDER_ALPHA_DURATION_MS);
                addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.TOOLBAR_Y_OFFSET, 0.f, getToolbarOffsetToLineUpWithBorder(),
                        ENTER_STACK_BORDER_ALPHA_DURATION_MS);
                addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.SIDE_BORDER_SCALE, 0.f, 1.f,
                        ENTER_STACK_BORDER_ALPHA_DURATION_MS);

                addToAnimationWithDelay(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.TOOLBAR_ALPHA, 1.f, 0.f, ENTER_STACK_TOOLBAR_ALPHA_DURATION_MS,
                        ENTER_STACK_TOOLBAR_ALPHA_DELAY_MS);
            }
        }
    }

    /**
     * Responsible for generating the animations that shows a tab being
     * focused (the stack is being left).
     *
     * @param animationList List for created animations.
     * @param propertyList  List of FloatProperty used for creating animations.
     * @param handler       Handler for animations.
     * @param tabs          The tabs that make up the stack.  These are the
     *                      tabs that will be affected by the TabSwitcherAnimation.
     * @param focusIndex    The focused index.  In this case, this is the index of
     *                      the tab clicked and is being brought up to view.
     * @param spacing       The default spacing between tabs.
     */
    private void createLandscapePortraitTabFocusedAnimatorSet(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler,
            StackTab[] tabs, int focusIndex, int spacing) {
        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];
            LayoutTab layoutTab = tab.getLayoutTab();

            addLandscapePortraitTiltScrollAnimation(animationList, propertyList, handler, layoutTab,
                    0.0f, TAB_FOCUSED_ANIMATION_DURATION_MS);
            addToAnimation(animationList, propertyList, handler, tab, StackTab.DISCARD_AMOUNT,
                    tab.getDiscardAmount(), 0.0f, TAB_FOCUSED_ANIMATION_DURATION_MS);

            if (i < focusIndex) {
                // Landscape: for tabs left of the focused tab move them left to 0.
                // Portrait: for tabs above the focused tab move them up to 0.
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCROLL_OFFSET,
                        tab.getScrollOffset(),
                        mOrientation == Orientation.LANDSCAPE
                                ? Math.max(0.0f, tab.getScrollOffset() - mWidth - spacing)
                                : tab.getScrollOffset() - mHeight - spacing,
                        TAB_FOCUSED_ANIMATION_DURATION_MS);
                continue;
            } else if (i > focusIndex) {
                if (mOrientation == Orientation.LANDSCAPE) {
                    // We also need to animate the X Translation to move them right
                    // off the screen.
                    float coveringTabPosition = layoutTab.getX();
                    float distanceToBorder = LocalizationUtils.isLayoutRtl()
                            ? coveringTabPosition + layoutTab.getScaledContentWidth()
                            : mWidth - coveringTabPosition;
                    float clampedDistanceToBorder = MathUtils.clamp(distanceToBorder, 0, mWidth);
                    float delay = TAB_FOCUSED_MAX_DELAY_MS * clampedDistanceToBorder / mWidth;
                    addToAnimationWithDelay(animationList, propertyList, handler, tab,
                            StackTab.X_IN_STACK_OFFSET, tab.getXInStackOffset(),
                            tab.getXInStackOffset()
                                    + (LocalizationUtils.isLayoutRtl() ? -mWidth : mWidth),
                            (TAB_FOCUSED_ANIMATION_DURATION_MS - (long) delay), (long) delay);
                } else { // mOrientation == Orientation.PORTRAIT
                    // We also need to animate the Y Translation to move them down
                    // off the screen.
                    float coveringTabPosition = layoutTab.getY();
                    float distanceToBorder =
                            MathUtils.clamp(mHeight - coveringTabPosition, 0, mHeight);
                    float delay = TAB_FOCUSED_MAX_DELAY_MS * distanceToBorder / mHeight;
                    addToAnimationWithDelay(animationList, propertyList, handler, tab,
                            StackTab.Y_IN_STACK_OFFSET, tab.getYInStackOffset(),
                            tab.getYInStackOffset() + mHeight,
                            (TAB_FOCUSED_ANIMATION_DURATION_MS - (long) delay), (long) delay);
                }
                continue;
            }

            // This is the focused tab.  We need to scale it back to
            // 1.0f, move it to the top of the screen, and animate the
            // X Translation (for Landscape) / Y Translation (for Portrait) so that it looks like it
            // is zooming into the full screen view.
            //
            // In Landscape we additionally move the card to the top left and extend it out so it
            // becomes a full card.
            tab.setXOutOfStack(0);
            tab.setYOutOfStack(0.0f);
            layoutTab.setBorderScale(1.f);

            if (mOrientation == Orientation.LANDSCAPE) {
                addToAnimation(animationList, propertyList, handler, tab,
                        StackTab.X_IN_STACK_INFLUENCE, tab.getXInStackInfluence(), 0.0f,
                        TAB_FOCUSED_ANIMATION_DURATION_MS);
                if (!isHorizontalTabSwitcherFlagEnabled()) {
                    addToAnimation(animationList, propertyList, handler, tab,
                            StackTab.SCROLL_OFFSET, tab.getScrollOffset(), mStack.screenToScroll(0),
                            TAB_FOCUSED_ANIMATION_DURATION_MS);
                }
            } else { // mOrientation == Orientation.PORTRAIT
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCROLL_OFFSET,
                        tab.getScrollOffset(),
                        Math.max(0.0f, tab.getScrollOffset() - mWidth - spacing),
                        TAB_FOCUSED_ANIMATION_DURATION_MS);
            }

            addToAnimation(animationList, propertyList, handler, tab, StackTab.SCALE,
                    tab.getScale(), 1.0f, TAB_FOCUSED_ANIMATION_DURATION_MS);
            addToAnimation(animationList, propertyList, handler, tab, StackTab.Y_IN_STACK_INFLUENCE,
                    tab.getYInStackInfluence(), 0.0f, TAB_FOCUSED_Y_STACK_DURATION_MS);
            addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                    LayoutTab.MAX_CONTENT_HEIGHT, tab.getLayoutTab().getMaxContentHeight(),
                    tab.getLayoutTab().getUnclampedOriginalContentHeight(),
                    TAB_FOCUSED_ANIMATION_DURATION_MS);

            tab.setYOutOfStack(getStaticTabPosition());

            if (layoutTab.shouldStall()) {
                addToAnimation(animationList, propertyList, handler, layoutTab,
                        LayoutTab.SATURATION, 1.0f, 0.0f, TAB_FOCUSED_BORDER_ALPHA_DURATION_MS);
            }
            addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                    LayoutTab.TOOLBAR_ALPHA, layoutTab.getToolbarAlpha(), 1.f,
                    TAB_FOCUSED_TOOLBAR_ALPHA_DURATION_MS);
            addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                    LayoutTab.TOOLBAR_Y_OFFSET, getToolbarOffsetToLineUpWithBorder(), 0.f,
                    TAB_FOCUSED_TOOLBAR_ALPHA_DURATION_MS);
            addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                    LayoutTab.SIDE_BORDER_SCALE, 1.f, 0.f, TAB_FOCUSED_TOOLBAR_ALPHA_DURATION_MS);
        }
    }

    /**
     * Responsible for generating the animations that moves the tabs back in from
     * discard attempt or commit the current discard (if any). It also re-even the tabs
     * if one of then is removed.
     *
     * @param animationList List for created animations.
     * @param propertyList  List of FloatProperty used for creating animations.
     * @param handler       Handler for animations.
     * @param stack         Stack.
     * @param tabs          The tabs that make up the stack. These are the
     *                      tabs that will be affected by the TabSwitcherAnimation.
     * @param spacing       The default spacing between tabs.
     * @param discardRange  The maximum value the discard amount.
     */
    private void createLandscapePortraitUpdateDiscardAnimatorSet(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler, Stack stack,
            StackTab[] tabs, int spacing, float discardRange) {
        int dyingTabsCount = 0;
        int firstDyingTabIndex = -1;
        float firstDyingTabOffset = 0;
        for (int i = 0; i < tabs.length; ++i) {
            addLandscapePortraitTiltScrollAnimation(animationList, propertyList, handler,
                    tabs[i].getLayoutTab(), 0.0f, UNDISCARD_ANIMATION_DURATION_MS);

            if (tabs[i].isDying()) {
                dyingTabsCount++;
                if (dyingTabsCount == 1) {
                    firstDyingTabIndex = i;
                    firstDyingTabOffset =
                            getLandscapePortraitScreenPositionInScrollDirection(tabs[i]);
                }
            }
        }

        float screenSizeInScrollDirection =
                mOrientation == Orientation.LANDSCAPE ? mWidth : mHeight;

        // This is used to determine the discard direction when user just clicks X to close a
        // tab. On portrait, positive direction (x) is right hand side (on clicking the close
        // button, discard the tab to the right on LTR, to the left on RTL). On landscape,
        // positive direction (y) is towards bottom.
        boolean defaultDiscardDirectionPositive =
                mOrientation == Orientation.LANDSCAPE ? true : !LocalizationUtils.isLayoutRtl();

        int newIndex = 0;
        for (int i = 0; i < tabs.length; ++i) {
            StackTab tab = tabs[i];
            // If the non-overlapping horizontal tab switcher is enabled, we shift all the
            // tabs over simultaneously. Otherwise we stagger the animation start times to
            // create a ripple effect.
            long startTime = isHorizontalTabSwitcherFlagEnabled()
                    ? 0
                    : (long) Math.max(0,
                            TAB_REORDER_START_SPAN / screenSizeInScrollDirection
                                    * (getLandscapePortraitScreenPositionInScrollDirection(tab)
                                            - firstDyingTabOffset));
            if (tab.isDying()) {
                float discard = tab.getDiscardAmount();
                if (discard == 0.0f) discard = defaultDiscardDirectionPositive ? 0.0f : -0.0f;
                float s = Math.copySign(1.0f, discard);
                long duration = (long) (DISCARD_ANIMATION_DURATION_MS
                        * (1.0f - Math.abs(discard / discardRange)));

                animationList.add(CompositorAnimator.ofFloatProperty(handler, tab,
                        StackTab.DISCARD_AMOUNT, discard, discardRange * s, duration,
                        BakedBezierInterpolator.FADE_OUT_CURVE));
                propertyList.add(StackTab.DISCARD_AMOUNT);
            } else {
                if (tab.getDiscardAmount() != 0.f) {
                    addToAnimation(animationList, propertyList, handler, tab,
                            StackTab.DISCARD_AMOUNT, tab.getDiscardAmount(), 0.0f,
                            UNDISCARD_ANIMATION_DURATION_MS);
                }
                addToAnimation(animationList, propertyList, handler, tab, StackTab.SCALE,
                        tab.getScale(), mStack.getScaleAmount(), DISCARD_ANIMATION_DURATION_MS);

                addToAnimation(animationList, propertyList, handler, tab.getLayoutTab(),
                        LayoutTab.MAX_CONTENT_HEIGHT, tab.getLayoutTab().getMaxContentHeight(),
                        mStack.getMaxTabHeight(), DISCARD_ANIMATION_DURATION_MS);

                float newScrollOffset = mStack.screenToScroll(spacing * newIndex);

                // If the tab is not dying we want to readjust it's position
                // based on the new spacing requirements.  For a fully discarded tab, just
                // put it in the right place.
                if (tab.getDiscardAmount() >= discardRange) {
                    tab.setScrollOffset(newScrollOffset);
                    tab.setScale(mStack.getScaleAmount());
                } else {
                    float start = tab.getScrollOffset();
                    if (start != newScrollOffset) {
                        addToAnimation(animationList, propertyList, handler, tab,
                                StackTab.SCROLL_OFFSET, start, newScrollOffset,
                                TAB_REORDER_DURATION_MS);
                    }
                }
                newIndex++;
            }
        }

        // Scroll offset animation for non-overlapping horizontal tab switcher (if enabled)
        if (isHorizontalTabSwitcherFlagEnabled()) {
            NonOverlappingStack nonOverlappingStack = (NonOverlappingStack) stack;
            int centeredTabIndex = nonOverlappingStack.getCenteredTabIndex();

            // For all tab closures (except for the last one), we slide the remaining tabs
            // in to fill the gap.
            //
            // There are two cases where we also need to animate the NonOverlappingStack's
            // overall scroll position over by one tab:
            //
            // - Closing the last tab while centered on it (since we don't have a tab we can
            //   slide over to replace it)
            //
            // - Closing any tab prior to the currently centered one (so we can keep the
            //   same tab centered). Together with animating the individual scroll offsets for
            //   each tab, this has the visual appearance of sliding in the prior tabs from the
            //   left (in LTR mode) to fill the gap.
            boolean closingLastTabWhileCentered =
                    firstDyingTabIndex == tabs.length - 1 && firstDyingTabIndex == centeredTabIndex;
            boolean closingPriorTab =
                    firstDyingTabIndex != -1 && firstDyingTabIndex < centeredTabIndex;

            boolean shouldAnimateStackScrollOffset = closingLastTabWhileCentered || closingPriorTab;

            if (shouldAnimateStackScrollOffset) {
                nonOverlappingStack.suppressScrollClampingForAnimation();
                addToAnimation(animationList, propertyList, handler, nonOverlappingStack,
                        Stack.SCROLL_OFFSET, stack.getScrollOffset(),
                        -(centeredTabIndex - 1) * stack.getSpacing(), TAB_REORDER_DURATION_MS);
            }
        }
    }

    /**
     * @return The offset for the toolbar to line the top up with the opaque component of
     *         the border.
     */
    private float getToolbarOffsetToLineUpWithBorder() {
        return mTopBrowserControlsHeight - mBorderTopOpaqueHeight;
    }

    /**
     * @return The position of the static tab when entering or exiting the tab switcher.
     */
    private float getStaticTabPosition() {
        return mTopBrowserControlsHeight - mBorderTopHeight;
    }

    /**
     * Helper method to create and add new {@link CompositorAnimator}
     * to a list of {@link Animator} and add associated {@link FloatProperty} info
     * to a list of {@link FloatProperty}
     *
     * @param animationList The list of {@link Animator} to add animation to.
     * @param propertyList  The list of {@link FloatProperty} to add FloatProperty info to.
     * @param handler       The associated handler for animations.
     * @param target        Target associated with animated property.
     * @param property      The property being animated.
     * @param startValue    The starting value of the animation.
     * @param endValue      The ending value of the animation.
     * @param durationMs    The duration of the animation.
     * @param startTimeMs   The start time.
     */
    private static <T> void addToAnimationWithDelay(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler,
            final T target, final FloatProperty<T> property, float startValue, float endValue,
            long durationMs, long startTimeMs) {
        CompositorAnimator compositorAnimator = CompositorAnimator.ofFloatProperty(
                handler, target, property, startValue, endValue, durationMs);
        compositorAnimator.setStartDelay(startTimeMs);

        animationList.add(compositorAnimator);
        propertyList.add(property);
    }

    private static <T> void addToAnimation(ArrayList<Animator> animationList,
            ArrayList<FloatProperty> propertyList, CompositorAnimationHandler handler,
            final T target, final FloatProperty<T> property, float startValue, float endValue,
            long durationMs) {
        addToAnimationWithDelay(animationList, propertyList, handler, target, property, startValue,
                endValue, durationMs, 0);
    }
}
