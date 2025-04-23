// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.HapticFeedbackConstants;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

/** Tests for {@link TabReorderStrategy}. */
@Config(qualifiers = "sw600dp")
@RunWith(BaseRobolectricTestRunner.class)
public class TabReorderStrategyTest extends ReorderStrategyTestBase {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Constants
    private static final float DELTA = 0.f;

    // Data = [Group1]([Tab1])  [Tab2]  [Tab3]  [Group2]([Tab4])
    private StripLayoutGroupTitle mGroupTitle1;
    private StripLayoutGroupTitle mGroupTitle2;
    private StripLayoutTab mStripTab1;
    private StripLayoutTab mStripTab2;
    private StripLayoutTab mStripTab3;
    private StripLayoutTab mStripTab4;
    private StripLayoutTab mStripTab5;

    private StripLayoutView[] mExpandedGroup;
    private StripLayoutView[] mCollapsedGroup;

    // Dependencies
    private final ObservableSupplierImpl<Boolean> mInReorderModeSupplier =
            new ObservableSupplierImpl<>();

    // Target
    private TabReorderStrategy mStrategy;

    @Before
    @Override
    public void setup() {
        super.setup();
        mockTabGroup(GROUP_ID1, TAB_ID1, mModel.getTabById(TAB_ID1));
        mockTabGroup(GROUP_ID2, TAB_ID4, mModel.getTabById(TAB_ID4), mModel.getTabById(TAB_ID5));

        mInReorderModeSupplier.set(false);
        mStrategy =
                new TabReorderStrategy(
                        mReorderDelegate,
                        mStripUpdateDelegate,
                        mAnimationHost,
                        mScrollDelegate,
                        mModel,
                        mTabGroupModelFilter,
                        mContainerView,
                        mGroupIdToHideSupplier,
                        mTabWidthSupplier,
                        mLastReorderScrollTimeSupplier,
                        mInReorderModeSupplier);
    }

    /**
     * Mocks the {@link StripLayoutView}s. One ungrouped tab, one tab in an expanded group, and one
     * tab in a collapsed group.
     */
    @Override
    protected void setupStripViews() {
        // [Group1]([Tab1])  [Tab2]  [Tab3]  [Group2]([Tab4])
        mGroupTitle1 = buildGroupTitle(TAB_ID1, GROUP_ID1, /* x= */ 0);
        mStripTab1 = buildStripTab(TAB_ID1, TAB_WIDTH);
        mStripTab2 = buildStripTab(TAB_ID2, 2 * TAB_WIDTH);
        mStripTab3 = buildStripTab(TAB_ID3, 3 * TAB_WIDTH);
        mGroupTitle2 = buildGroupTitle(TAB_ID4, GROUP_ID2, 4 * TAB_WIDTH);
        mStripTab4 = buildStripTab(TAB_ID4, 5 * TAB_WIDTH);
        mStripTab5 = buildStripTab(TAB_ID5, 6 * TAB_WIDTH);

        // Construct expanded group.
        mExpandedGroup = new StripLayoutView[] {mGroupTitle1, mStripTab1};
        mGroupTitle1.setBottomIndicatorWidth(2 * TAB_WIDTH);

        // Construct collapsed group.
        mCollapsedGroup = new StripLayoutView[] {mGroupTitle2, mStripTab4};
        for (StripLayoutView view : mCollapsedGroup) {
            view.setCollapsed(/* collapsed= */ true);
        }

        // Populate state.
        mStripTabs =
                new StripLayoutTab[] {mStripTab1, mStripTab2, mStripTab3, mStripTab4, mStripTab5};
        mGroupTitles = new StripLayoutGroupTitle[] {mGroupTitle1, mGroupTitle2};
        mStripViews =
                new StripLayoutView[] {
                    mGroupTitle1,
                    mStripTab1,
                    mStripTab2,
                    mStripTab3,
                    mGroupTitle2,
                    mStripTab4,
                    mStripTab5
                };
    }

    @Test
    public void testStartReorder() {
        startReorder(mStripTab3);

        // Verify
        assertEquals(
                "Unexpected interacting view.", mInteractingTab, mStrategy.getInteractingView());
        assertTrue("Interacting tab should be foregrounded.", mInteractingTab.isForegrounded());

        String message = "Unexpected group margin.";
        float expectedMargin =
                StripLayoutUtils.getHalfTabWidth(mTabWidthSupplier)
                        * StripLayoutUtils.REORDER_OVERLAP_SWITCH_PERCENTAGE;
        verify(mScrollDelegate).setReorderStartMargin(expectedMargin);
        assertEquals(message, 0, mStripTab1.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mStripTab2.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mStripTab3.getTrailingMargin(), DELTA);
        assertEquals(message, 0, mStripTab4.getTrailingMargin(), DELTA);
        assertEquals(message, expectedMargin, mStripTab5.getTrailingMargin(), DELTA);

        verify(mAnimationHost).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), isNull());
        verify(mContainerView).performHapticFeedback(eq(HapticFeedbackConstants.LONG_PRESS));
    }

    @Test
    public void testStopReorder() {
        // Start reorder and verify.
        startReorder(mStripTab3);
        verify(mAnimationHost).startAnimations(anyList(), isNull());

        // Stop reorder and verify.
        mStrategy.stopReorderMode(mStripViews, mGroupTitles);
        verify(mScrollDelegate).setReorderStartMargin(0);
        assertEquals(
                "Should no longer have trailing margin.", 0, mStripTab5.getTrailingMargin(), DELTA);
        verify(mAnimationHost, times(2)).finishAnimationsAndPushTabUpdates();
        verify(mAnimationHost).startAnimations(anyList(), isNotNull());
    }

    // ============================================================================================
    // Event helpers
    // ============================================================================================

    private void startReorder(StripLayoutTab interactingTab) {
        mInteractingTab = interactingTab;
        mStrategy.startReorderMode(
                mStripViews, mStripTabs, mGroupTitles, mInteractingTab, DRAG_START_POINT);
    }
}
