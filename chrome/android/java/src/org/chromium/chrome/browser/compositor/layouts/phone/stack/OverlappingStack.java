// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.content.Context;
import android.content.res.Resources;

import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.Layout.Orientation;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayoutBase;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.StackAnimation.OverviewAnimationType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.ui.base.LocalizationUtils;

/**
 * The overlapping tab stack we use when the HorizontalTabSwitcherAndroid flag is not enabled.
 */
public class OverlappingStack extends Stack {
    private static final float SCALE_AMOUNT = 0.90f;

    /**
     * The percentage of the screen that defines the spacing between tabs by default (no pinch).
     */
    private static final float SPACING_SCREEN = 0.26f;

    /**
     * Percentage of the screen to wrap the scroll space.
     */
    private static final float SCROLL_WARP_PCTG = 0.4f;

    /**
     * How much the stack should adjust the y position of each LayoutTab in portrait mode (as a
     * fraction of the amount space that would be above and below the tab if it were centered).
     */
    private static final float STACK_PORTRAIT_Y_OFFSET_PROPORTION = -0.8f;

    /**
     * How much the stack should adjust the x position of each LayoutTab in landscape mode (as a
     * fraction of the amount space that would be to the left and right of the tab if it were
     * centered).
     */
    private static final float STACK_LANDSCAPE_START_OFFSET_PROPORTION = -0.7f;

    /**
     * How much the stack should adjust the x position of each LayoutTab in portrait mode (as a
     * fraction of the amount space that would be above and below the tab if it were centered).
     */
    private static final float STACK_LANDSCAPE_Y_OFFSET_PROPORTION = -0.5f;

    private float mWarpSize;

    // During pinch, the finger the closest to the bottom of the stack changes the scrolling
    // and the other finger locally stretches the spacing between the tabs.
    private int mPinch0TabIndex = -1;
    private int mPinch1TabIndex = -1;
    private float mLastPinch0Offset;
    private float mLastPinch1Offset;

    // Current progress of the 'even out' phase. This progress as the screen get scrolled.
    private float mEvenOutProgress = 1.0f;
    // Rate to even out all the tabs.
    private float mEvenOutRate = 1.0f; // This will be updated from dimens.xlm

    private float mMinSpacing; // This will be updated from dimens.xml

    /**
     * @param layout The parent layout.
     */
    public OverlappingStack(Context context, StackLayoutBase layout) {
        super(context, layout);
    }

    @Override
    public float getScaleAmount() {
        return SCALE_AMOUNT;
    }

    @Override
    protected boolean evenOutTabs(float amount, boolean allowReverseDirection) {
        if (mStackTabs == null || mOverviewAnimationType != OverviewAnimationType.NONE
                || mEvenOutProgress >= 1.0f || amount == 0) {
            return false;
        }
        boolean changed = false;
        boolean reverseScrolling = false;

        // The evening out process last until mEvenOutRate reaches 1.0. Tabs blend linearly
        // between the current position to a nice evenly scaled pattern. Because we do not store
        // the starting position for each tab we need more complicated math to do the blend.
        // The absoluteProgress is how much we need progress this step on the [0, 1] scale.
        float absoluteProgress = Math.min(Math.abs(amount) * mEvenOutRate, 1.0f - mEvenOutProgress);
        // The relativeProgress is how much we need to blend the target to the current to get there.
        float relativeProgress = absoluteProgress / (1.0f - mEvenOutProgress);

        float screenMax = getScrollDimensionSize();
        for (int i = 0; i < mStackTabs.length; ++i) {
            float source = mStackTabs[i].getScrollOffset();
            float target = screenToScroll(i * mSpacing);
            float sourceScreen = Math.min(screenMax, scrollToScreen(source + mScrollTarget));
            float targetScreen = Math.min(screenMax, scrollToScreen(target + mScrollTarget));
            // If the target and the current position matches on the screen then we snap to the
            // target.
            if (sourceScreen == targetScreen) {
                mStackTabs[i].setScrollOffset(target);
                continue;
            }
            float step = source + (target - source) * relativeProgress;
            float stepScreen = Math.min(screenMax, scrollToScreen(step + mScrollTarget));
            // If the step can be performed without noticing then we do it.
            if (sourceScreen == stepScreen) {
                mStackTabs[i].setScrollOffset(step);
                continue;
            }
            // If the scrolling goes in the same direction as the step then the motion is applied.
            if ((targetScreen - sourceScreen) * amount > 0 || allowReverseDirection) {
                mStackTabs[i].setScrollOffset(step);
                changed = true;
            } else {
                reverseScrolling = true;
            }
        }
        // Only account for progress if the scrolling was in the right direction. It assumes here
        // That if any of the tabs was going in the wrong direction then the progress is not
        // recorded at all. This is very conservative to avoid poping in the scrolling. It works
        // for now but might need to be revisited if we see artifacts.
        if (!reverseScrolling) {
            mEvenOutProgress += absoluteProgress;
        }
        return changed;
    }

    @Override
    public void onLongPress(long time, float x, float y) {
        if (mOverviewAnimationType == OverviewAnimationType.NONE) {
            int longPressSelected = getTabIndexAtPositon(x, y);
            if (longPressSelected >= 0) {
                startAnimation(time, OverviewAnimationType.VIEW_MORE, longPressSelected, false);
                mEvenOutProgress = 0.0f;
            }
        }
    }

    @Override
    public void onPinch(long time, float x0, float y0, float x1, float y1, boolean firstEvent) {
        if ((mOverviewAnimationType != OverviewAnimationType.START_PINCH
                    && mOverviewAnimationType != OverviewAnimationType.NONE)
                || mStackTabs == null) {
            return;
        }
        if (mPinch0TabIndex < 0) startAnimation(time, OverviewAnimationType.START_PINCH);

        // Reordering the fingers so pinch0 is always the closest to the top of the stack.
        // This allows simpler math down the line where we assume that
        // pinch0TabIndex <= pinch0TabIndex
        // It also means that crossing the finger will separate the tabs again.
        boolean inverse = (mCurrentMode == Orientation.PORTRAIT)
                ? y0 > y1
                : LocalizationUtils.isLayoutRtl() ? (x0 <= x1) : (x0 > x1);
        float pinch0X = inverse ? x1 : x0;
        float pinch0Y = inverse ? y1 : y0;
        float pinch1X = inverse ? x0 : x1;
        float pinch1Y = inverse ? y0 : y1;
        float pinch0Offset = (mCurrentMode == Orientation.PORTRAIT)
                ? pinch0Y
                : LocalizationUtils.isLayoutRtl() ? -pinch0X : pinch0X;
        float pinch1Offset = (mCurrentMode == Orientation.PORTRAIT)
                ? pinch1Y
                : LocalizationUtils.isLayoutRtl() ? -pinch1X : pinch1X;

        if (firstEvent) {
            // Resets pinch and scrolling state.
            mPinch0TabIndex = -1;
            mPinch1TabIndex = -1;
            mScrollingTab = null;
            commitDiscard(time, false);
        }
        int pinch0TabIndex = mPinch0TabIndex;
        int pinch1TabIndex = mPinch1TabIndex;
        if (mPinch0TabIndex < 0) {
            pinch0TabIndex = getTabIndexAtPositon(pinch0X, pinch0Y);
            pinch1TabIndex = getTabIndexAtPositon(pinch1X, pinch1Y);
            // If any of them is invalid we invalidate both.
            if (pinch0TabIndex < 0 || pinch1TabIndex < 0) {
                pinch0TabIndex = -1;
                pinch1TabIndex = -1;
            }
        }

        if (pinch0TabIndex >= 0 && mPinch0TabIndex == pinch0TabIndex
                && mPinch1TabIndex == pinch1TabIndex) {
            final float minScrollTarget = getMinScroll(false);
            final float maxScrollTarget = getMaxScroll(false);
            final float oldScrollTarget =
                    MathUtils.clamp(mScrollTarget, minScrollTarget, maxScrollTarget);
            // pinch0TabIndex > pinch1TabIndex is unexpected but we do not want to exit
            // ungracefully so process it as if the tabs were the same.
            if (pinch0TabIndex >= pinch1TabIndex) {
                // If one tab is pinched then we only scroll.
                float screenDelta0 = pinch0Offset - mLastPinch0Offset;
                if (pinch0TabIndex == 0) {
                    // Linear scroll on the top tab for the overscroll to kick-in linearly.
                    setScrollTarget(oldScrollTarget + screenDelta0, false);
                } else {
                    float tab0ScrollSpace =
                            mStackTabs[pinch0TabIndex].getScrollOffset() + oldScrollTarget;
                    float tab0Screen = scrollToScreen(tab0ScrollSpace);
                    float tab0ScrollFinal = screenToScroll(tab0Screen + screenDelta0);
                    setScrollTarget(
                            tab0ScrollFinal - mStackTabs[pinch0TabIndex].getScrollOffset(), false);
                }
                // This is the common case of the pinch, 2 fingers on 2 different tabs.
            } else {
                // Find the screen space position before and after the scroll so the tab 0 matches
                // the finger 0 motion.
                float screenDelta0 = pinch0Offset - mLastPinch0Offset;
                float tab0ScreenBefore = approxScreen(mStackTabs[pinch0TabIndex], oldScrollTarget);
                float tab0ScreenAfter = tab0ScreenBefore + screenDelta0;

                // Find the screen space position before and after the scroll so the tab 1 matches
                // the finger 1 motion.
                float screenDelta1 = pinch1Offset - mLastPinch1Offset;
                float tab1ScreenBefore = approxScreen(mStackTabs[pinch1TabIndex], oldScrollTarget);
                float tab1ScreenAfter = tab1ScreenBefore + screenDelta1;

                // Heuristic: the scroll is defined by half the change of the first pinched tab.
                // The rational is that it looks nice this way :)... Scrolling creates a sliding
                // effect. When a finger does not move then it is expected that none of the tabs
                // past that steady finger should move. This does the job.
                float globalScrollBefore = screenToScroll(tab0ScreenBefore);
                float globalScrollAfter = screenToScroll((tab0ScreenAfter + tab0ScreenBefore) / 2);
                setScrollTarget(oldScrollTarget + globalScrollAfter - globalScrollBefore, true);

                // Evens out the tabs in between
                float minScreen = tab0ScreenAfter;
                float maxScreen = tab0ScreenAfter;
                for (int i = pinch0TabIndex; i <= pinch1TabIndex; i++) {
                    float screenBefore = approxScreen(mStackTabs[i], oldScrollTarget);
                    float t = (tab1ScreenBefore == tab0ScreenBefore)
                            ? 1
                            : ((screenBefore - tab0ScreenBefore)
                                      / (tab1ScreenBefore - tab0ScreenBefore));
                    float screenAfter = (1 - t) * tab0ScreenAfter + t * tab1ScreenAfter;
                    screenAfter = Math.max(minScreen, screenAfter);
                    screenAfter = Math.min(maxScreen, screenAfter);
                    minScreen = screenAfter + StackTab.sStackedTabVisibleSize;
                    maxScreen = screenAfter + mStackTabs[i].getSizeInScrollDirection(mCurrentMode);
                    float newScrollOffset = screenToScroll(screenAfter) - mScrollTarget;
                    mStackTabs[i].setScrollOffset(newScrollOffset);
                }

                // Push a bit the tabs bellow pinch1.
                float delta1 = tab1ScreenAfter - tab1ScreenBefore;
                for (int i = pinch1TabIndex + 1; i < mStackTabs.length; i++) {
                    delta1 /= 2;
                    float screenAfter = approxScreen(mStackTabs[i], oldScrollTarget) + delta1;
                    screenAfter = Math.max(minScreen, screenAfter);
                    screenAfter = Math.min(maxScreen, screenAfter);
                    minScreen = screenAfter + StackTab.sStackedTabVisibleSize;
                    maxScreen = screenAfter + mStackTabs[i].getSizeInScrollDirection(mCurrentMode);
                    mStackTabs[i].setScrollOffset(screenToScroll(screenAfter) - mScrollTarget);
                }

                // Pull a bit the tabs above pinch0.
                minScreen = tab0ScreenAfter;
                maxScreen = tab0ScreenAfter;
                float posScreen = tab0ScreenAfter;
                float delta0 = tab0ScreenAfter - tab0ScreenBefore;
                for (int i = pinch0TabIndex - 1; i > 0; i--) {
                    delta0 /= 2;
                    minScreen = posScreen - mStackTabs[i].getSizeInScrollDirection(mCurrentMode);
                    maxScreen = posScreen - StackTab.sStackedTabVisibleSize;
                    float screenAfter = approxScreen(mStackTabs[i], oldScrollTarget) + delta0;
                    screenAfter = Math.max(minScreen, screenAfter);
                    screenAfter = Math.min(maxScreen, screenAfter);
                    mStackTabs[i].setScrollOffset(screenToScroll(screenAfter) - mScrollTarget);
                }
            }
        }
        mPinch0TabIndex = pinch0TabIndex;
        mPinch1TabIndex = pinch1TabIndex;
        mLastPinch0Offset = pinch0Offset;
        mLastPinch1Offset = pinch1Offset;
        mEvenOutProgress = 0.0f;
        mLayout.requestUpdate();
    }

    @Override
    public void onUpOrCancel(long time) {
        // Make sure the bottom tab always goes back to the top of the screen.
        if (mPinch0TabIndex >= 0) {
            startAnimation(time, OverviewAnimationType.REACH_TOP);
            mLayout.requestUpdate();
        }

        super.onUpOrCancel(time);
    }

    @Override
    protected void springBack(long time) {
        if (mScroller.isFinished()) {
            int minScroll = (int) getMinScroll(false);
            int maxScroll = (int) getMaxScroll(false);
            if (mScrollTarget < minScroll || mScrollTarget > maxScroll) {
                mScroller.springBack(0, (int) mScrollTarget, 0, 0, minScroll, maxScroll, time);
                setScrollTarget(MathUtils.clamp(mScrollTarget, minScroll, maxScroll), false);
                mLayout.requestUpdate();
            }
        }
    }

    /**
     * @param context The current Android's context.
     */
    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);

        Resources res = context.getResources();
        final float pxToDp = 1.0f / res.getDisplayMetrics().density;

        mEvenOutRate = 1.0f / (res.getDimension(R.dimen.even_out_scrolling) * pxToDp);
        mMinSpacing = res.getDimensionPixelOffset(R.dimen.min_spacing) * pxToDp;
    }

    @Override
    protected boolean shouldStackTabsAtTop() {
        return true;
    }

    @Override
    protected boolean shouldStackTabsAtBottom() {
        return true;
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
    protected float getSpacingScreen() {
        return SPACING_SCREEN;
    }

    @Override
    protected boolean shouldCloseGapsBetweenTabs() {
        return true;
    }

    @Override
    protected void computeTabClippingVisibilityHelper() {
        // alpha override, clipping and culling.
        final boolean portrait = mCurrentMode == Orientation.PORTRAIT;

        // Iterate through each tab starting at the top of the stack and working
        // backwards. Set the clip on each tab such that it does not extend past
        // the beginning of the tab above it. clipOffset is used to keep track
        // of where the previous tab started.
        float clipOffset;
        if (portrait) {
            // portrait LTR & RTL
            clipOffset = mLayout.getHeight() + StackTab.sStackedTabVisibleSize;
        } else if (!LocalizationUtils.isLayoutRtl()) {
            // landscape LTR
            clipOffset = mLayout.getWidth() + StackTab.sStackedTabVisibleSize;
        } else {
            // landscape RTL
            clipOffset = -StackTab.sStackedTabVisibleSize;
        }

        for (int i = mStackTabs.length - 1; i >= 0; i--) {
            LayoutTab layoutTab = mStackTabs[i].getLayoutTab();
            layoutTab.setVisible(true);

            // Don't bother with clipping tabs that are dying, rotating, with an X offset, or
            // non-opaque.
            if (mStackTabs[i].isDying() || mStackTabs[i].getXInStackOffset() != 0.0f
                    || layoutTab.getAlpha() < 1.0f) {
                layoutTab.setClipOffset(0.0f, 0.0f);
                layoutTab.setClipSize(Float.MAX_VALUE, Float.MAX_VALUE);
                continue;
            }

            // The beginning, size, and clipped size of the current tab.
            float tabOffset;
            float tabSize;
            float tabClippedSize;
            float borderAdjustmentSize;
            float insetBorderPadding;
            if (portrait) {
                // portrait LTR & RTL
                tabOffset = layoutTab.getY();
                tabSize = layoutTab.getScaledContentHeight();
                tabClippedSize = Math.min(tabSize, clipOffset - tabOffset);
                borderAdjustmentSize = mBorderTransparentTop;
                insetBorderPadding = mBorderTopPadding;
            } else if (!LocalizationUtils.isLayoutRtl()) {
                // landscape LTR
                tabOffset = layoutTab.getX();
                tabSize = layoutTab.getScaledContentWidth();
                tabClippedSize = Math.min(tabSize, clipOffset - tabOffset);
                borderAdjustmentSize = mBorderTransparentSide;
                insetBorderPadding = 0;
            } else {
                // landscape RTL
                tabOffset = layoutTab.getX() + layoutTab.getScaledContentWidth();
                tabSize = layoutTab.getScaledContentWidth();
                tabClippedSize = Math.min(tabSize, tabOffset - clipOffset);
                borderAdjustmentSize = -mBorderTransparentSide;
                insetBorderPadding = 0;
            }

            float absBorderAdjustmentSize = Math.abs(borderAdjustmentSize);

            if (tabClippedSize <= absBorderAdjustmentSize) {
                // If the tab is completed covered, don't bother drawing it at all.
                layoutTab.setVisible(false);
                layoutTab.setDrawDecoration(true);
                mLayout.releaseResourcesForTab(layoutTab);
            } else {
                // Fade the tab as it gets too close to the next one. This helps
                // prevent overlapping shadows from becoming too dark.
                float fade = MathUtils.clamp(((tabClippedSize - absBorderAdjustmentSize)
                                                     / StackTab.sStackedTabVisibleSize),
                        0, 1);
                layoutTab.setDecorationAlpha(fade);

                // When tabs tilt forward, it will expose more of the tab
                // underneath. To compensate, make the clipping size larger.
                // Note, this calculation is only an estimate that seems to
                // work.
                float clipScale = 1.0f;
                if (layoutTab.getTiltX() > 0
                        || ((!portrait && LocalizationUtils.isLayoutRtl())
                                           ? layoutTab.getTiltY() < 0
                                           : layoutTab.getTiltY() > 0)) {
                    final float tilt =
                            Math.max(layoutTab.getTiltX(), Math.abs(layoutTab.getTiltY()));
                    clipScale += (tilt / mMaxOverScrollAngle) * 0.60f;
                }

                float scaledTabClippedSize = Math.min(tabClippedSize * clipScale, tabSize);
                // Set the clip
                layoutTab.setClipOffset((!portrait && LocalizationUtils.isLayoutRtl())
                                ? (tabSize - scaledTabClippedSize)
                                : 0,
                        0);
                layoutTab.setClipSize(portrait ? Float.MAX_VALUE : scaledTabClippedSize,
                        portrait ? scaledTabClippedSize : Float.MAX_VALUE);
            }

            // Clip the next tab where this tab begins.
            if (i > 0) {
                LayoutTab nextLayoutTab = mStackTabs[i - 1].getLayoutTab();
                if (nextLayoutTab.getScale() <= layoutTab.getScale()) {
                    clipOffset = tabOffset;
                } else {
                    clipOffset = tabOffset + tabClippedSize * layoutTab.getScale();
                }

                // Extend the border just a little bit. Otherwise, the
                // rounded borders will intersect and make it look like the
                // content is actually smaller.
                clipOffset += borderAdjustmentSize;

                if (layoutTab.getBorderAlpha() < 1.f && layoutTab.getToolbarAlpha() < 1.f) {
                    clipOffset += insetBorderPadding;
                }
            }
        }
    }

    @Override
    protected int computeReferenceIndex() {
        int centerIndex =
                getTabIndexAtPositon(mLayout.getWidth() / 2.0f, mLayout.getHeight() / 2.0f);
        // Alter the center to take into account the scrolling direction.
        if (mCurrentScrollDirection > 0) centerIndex++;
        if (mCurrentScrollDirection < 0) centerIndex--;
        return MathUtils.clamp(centerIndex, 0, mStackTabs.length - 1);
    }

    @Override
    protected float getMinScroll(boolean allowUnderScroll) {
        float maxOffset = 0;
        if (mStackTabs != null) {
            // The tabs are not always ordered so we need to browse them all.
            for (int i = 0; i < mStackTabs.length; i++) {
                if (!mStackTabs[i].isDying() && mStackTabs[i].getLayoutTab().isVisible()) {
                    maxOffset = Math.max(mStackTabs[i].getScrollOffset(), maxOffset);
                }
            }
        }
        return (allowUnderScroll ? -mMaxUnderScroll : 0) - maxOffset;
    }

    @Override
    protected int computeSpacing(int layoutTabCount) {
        int spacing = 0;
        if (layoutTabCount > 1) {
            final float dimension = getScrollDimensionSize();
            int minSpacing = (int) Math.max(dimension * SPACING_SCREEN, mMinSpacing);
            if (mStackTabs != null) {
                for (int i = 0; i < mStackTabs.length; i++) {
                    assert mStackTabs[i] != null;
                    if (!mStackTabs[i].isDying()) {
                        minSpacing = (int) Math.min(
                                minSpacing, mStackTabs[i].getSizeInScrollDirection(mCurrentMode));
                    }
                }
            }
            spacing = (int) ((dimension - 20) / (layoutTabCount * .8f));
            spacing = Math.max(spacing, minSpacing);
        }
        return spacing;
    }

    @Override
    protected boolean allowOverscroll() {
        return super.allowOverscroll() && mPinch0TabIndex < 0;
    }

    @Override
    protected void resetAllScrollOffset() {
        if (mTabList == null) return;
        // Reset the scroll position to put the important {@link StackTab} into focus.
        // This does not scroll the {@link StackTab}s there but rather moves everything
        // there immediately.
        // The selected tab is supposed to show at the center of the screen.
        float maxTabsPerPage = getScrollDimensionSize() / mSpacing;
        float centerOffsetIndex = maxTabsPerPage / 2.0f - 0.5f;
        final int count = mTabList.getCount();
        final int index = mTabList.index();
        if (index < centerOffsetIndex || count <= maxTabsPerPage) {
            mScrollOffset = 0;
        } else if (index == count - 1 && Math.ceil(maxTabsPerPage) < count) {
            mScrollOffset = (maxTabsPerPage - count - 1) * mSpacing;
        } else if ((count - index - 1) < centerOffsetIndex) {
            mScrollOffset = (maxTabsPerPage - count) * mSpacing;
        } else {
            mScrollOffset = (centerOffsetIndex - index) * mSpacing;
        }
        // Reset the scroll offset of the tabs too.
        if (mStackTabs != null) {
            for (int i = 0; i < mStackTabs.length; i++) {
                mStackTabs[i].setScrollOffset(screenToScroll(i * mSpacing));
            }
        }
        setScrollTarget(mScrollOffset, false);
    }

    /**
     * Unwarps x so it matches the above warp function.
     * @see #scrollToScreen(float)
     *
     * [-oo, 0] -> -warpSize
     * [0, warpSize] -> 2 * warpSize * sqrt(x / warpSize).
     * [warpSize, +oo] -> x + warpSize
     * @param x        The screen space offset.
     * @param warpSize The size in scroll space of the slow down effect.
     * @return         The offset in scroll space corresponding to the offset on screen.
     */
    private float screenToScroll(float x, float warpSize) {
        if (x <= 0) return 0;
        if (x >= warpSize) return x + warpSize;
        return (float) Math.sqrt(x * warpSize) * 2;
    }

    /**
     * Public version of screenToScroll(float, float) that uses the current warp size.
     * @param scrollSpace The offset in screen space.
     * @return            The offset in scroll space corresponding to the offset on screen.
     */
    @Override
    public float screenToScroll(float screenSpace) {
        return screenToScroll(screenSpace, mWarpSize);
    }

    /**
     * The scroll space does not map linearly to the screen so it creates a nice slow down
     * effect at the top of the screen while scrolling.
     * Warps x so it matches y(x) = x - warpSize on the positive side and 0 on the negative side
     * with a smooth transition between [0, 2 * warpSize].
     * @see #screenToScroll(float)
     *
     * [-oo, 0] -> 0
     * [0, 2 * warpSize] -> warpSize * ((x-warpSize) / 2 * warpSize + 0.5) ^ 2.
     * [2 * warpSize, +oo] -> x
     * @param x        The offset in scroll space.
     * @param warpSize The size in scroll space of the slow down effect.
     * @return         The offset on screen corresponding to the scroll space offset.
     */
    private float scrollToScreen(float x, float warpSize) {
        if (x <= 0) return 0;
        if (x >= 2 * warpSize) return x - warpSize;
        x = (x - warpSize) / (2.0f * warpSize) + 0.5f;
        return x * x * warpSize;
    }

    /**
     * Public version of scrollToScreen(float, float) that uses the current warp size.
     * Maps from scroll coordinates to screen coordinates.
     * @param scrollSpace The offset in scroll space.
     * @return            The offset on screen corresponding to the scroll space offset.
     */
    @Override
    public float scrollToScreen(float scrollSpace) {
        return scrollToScreen(scrollSpace, mWarpSize);
    }

    @Override
    public float getMaxTabHeight() {
        // TODO(crbug.com/1095698): Rework when the stack enter animation is created so that we can
        // remove this feature specific fix.
        // When conditional tab strip is enabled, the bottom browser control height should be 0
        // eventually when overview mode is visible. Hence, we pre-acknowledge the fact and assume
        // the bottom control height to be 0 here so that the animation is correctly set up.
        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            return mLayout.getHeight() - mLayout.getTopContentOffsetDp();
        }
        return mLayout.getHeightMinusContentOffsetsDp();
    }

    @Override
    protected void updateCurrentMode(@Orientation int orientation) {
        setWarpState(true, false);
        super.updateCurrentMode(orientation);
    }

    @Override
    protected void resetInputActionIndices() {
        super.resetInputActionIndices();

        mPinch0TabIndex = -1;
        mPinch1TabIndex = -1;
    }

    /**
     * Whether or not the tab positions warp from linear to nonlinear as the tabs approach the edge
     * of the screen.  This allows us to move the tabs to linear space to track finger movements,
     * but also move them back to non-linear space without any visible change to the user.
     * @param canWarp           Whether or not the tabs are allowed to warp.
     * @param adjustCurrentTabs Whether or not to change the tab positions so there's no visible
     *                          difference after the change.
     */
    private void setWarpState(boolean canWarp, boolean adjustCurrentTabs) {
        float warp = canWarp ? getScrollDimensionSize() * SCROLL_WARP_PCTG : 0.f;

        if (mStackTabs != null && adjustCurrentTabs && Float.compare(warp, mWarpSize) != 0) {
            float scrollOffset =
                    MathUtils.clamp(mScrollOffset, getMinScroll(false), getMaxScroll(false));
            for (int i = 0; i < mStackTabs.length; i++) {
                StackTab tab = mStackTabs[i];
                float tabScrollOffset = tab.getScrollOffset();
                float tabScrollSpace = tabScrollOffset + scrollOffset;
                float tabScreen = scrollToScreen(tabScrollSpace, mWarpSize);
                float tabScrollSpaceFinal = screenToScroll(tabScreen, warp);
                float scrollDelta = tabScrollSpaceFinal - tabScrollSpace;
                tab.setScrollOffset(tabScrollOffset + scrollDelta);
            }
        }

        mWarpSize = warp;
    }
}
