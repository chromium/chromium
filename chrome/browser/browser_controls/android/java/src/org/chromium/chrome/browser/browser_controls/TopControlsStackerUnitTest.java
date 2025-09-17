// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.ScrollBehavior;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;

/** Unit tests for {@link TopControlsStacker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TopControlsStackerUnitTest {
    /** Mock implementation of TestLayer for testing purposes. */
    private static class TestLayer implements TopControlLayer {
        private final @TopControlType int mType;
        private final @TopControlVisibility int mVisibility;
        private final @ScrollBehavior int mScrollBehavior;
        private final boolean mContributesToTotalHeight;
        private final int mHeight;

        TestLayer(
                @TopControlType int type,
                @TopControlVisibility int visibility,
                @ScrollBehavior int scrollBehavior,
                boolean contributesToTotalHeight,
                int height) {
            mType = type;
            mVisibility = visibility;
            mScrollBehavior = scrollBehavior;
            mContributesToTotalHeight = contributesToTotalHeight;
            mHeight = height;
        }

        @Override
        public @TopControlType int getTopControlType() {
            return mType;
        }

        @Override
        public @TopControlVisibility int getTopControlVisibility() {
            return mVisibility;
        }

        @Override
        public @ScrollBehavior int getScrollBehavior() {
            return mScrollBehavior;
        }

        @Override
        public boolean contributesToTotalHeight() {
            return mContributesToTotalHeight;
        }

        @Override
        public int getTopControlHeight() {
            return mHeight;
        }
    }

    @Mock private BrowserControlsSizer mBrowserControlsSizer;
    @Mock private BrowserStateBrowserControlsVisibilityDelegate mVisibilityDelegate;
    @Captor private ArgumentCaptor<Callback<Integer>> mVisibilityCallbackCaptor;

    private TopControlsStacker mTopControlsStacker;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        doReturn(mVisibilityDelegate).when(mBrowserControlsSizer).getBrowserVisibilityDelegate();
        doReturn(BrowserControlsState.BOTH).when(mVisibilityDelegate).get();
        mTopControlsStacker = new TopControlsStacker(mBrowserControlsSizer);
    }

    @Test
    public void testAddRemoveControl() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);
        Assert.assertEquals(
                "Total height should be 100.",
                100,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());

        mTopControlsStacker.removeControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);
        Assert.assertEquals(
                "Total height should be 0.",
                0,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
    }

    @Test
    public void testHeightCalculation() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        TestLayer tabstrip =
                new TestLayer(
                        TopControlType.TABSTRIP,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        50);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(tabstrip);
        mTopControlsStacker.requestLayerUpdate(false);

        Assert.assertEquals(
                "Total height should be 150.",
                150,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
        Assert.assertEquals(
                "Min height should be 0.", 0, mTopControlsStacker.getVisibleTopControlsMinHeight());
    }

    @Test
    public void testHeightCalculation_HiddenControl() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        TestLayer tabstrip =
                new TestLayer(
                        TopControlType.TABSTRIP,
                        TopControlVisibility.HIDDEN,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        50);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(tabstrip);
        mTopControlsStacker.requestLayerUpdate(false);

        Assert.assertEquals(
                "Total height should be 100.",
                100,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
    }

    @Test
    public void testHeightCalculation_DoesNotContributeToTotalHeight() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        TestLayer progressBar =
                new TestLayer(
                        TopControlType.PROGRESS_BAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ false,
                        5);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);

        Assert.assertEquals(
                "Total height should be 100.",
                100,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
    }

    @Test
    public void testMinHeightCalculation() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.NEVER_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        TestLayer tabstrip =
                new TestLayer(
                        TopControlType.TABSTRIP,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        50);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(tabstrip);
        mTopControlsStacker.requestLayerUpdate(false);

        Assert.assertEquals(
                "Total height should be 150.",
                150,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
        Assert.assertEquals(
                "Min height should be 100.",
                100,
                mTopControlsStacker.getVisibleTopControlsMinHeight());
    }

    @Test
    public void testScrollingDisabled() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.onControlsConstraintsChanged(
                new BrowserControlsOffsetTagsInfo(),
                new BrowserControlsOffsetTagsInfo(),
                BrowserControlsState.SHOWN,
                false);

        mTopControlsStacker.setScrollingDisabled(true);

        verify(mBrowserControlsSizer).setTopControlsHeight(100, 100);
    }

    @Test(expected = AssertionError.class)
    public void testAddSameControlTwice() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(toolbar);
    }

    @Test
    public void testRemoveControlNotAdded() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        mTopControlsStacker.removeControl(toolbar);

        Assert.assertEquals(
                "Total height should be 0.",
                0,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
    }

    @Test
    public void testZeroHeightControl() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        0);
        mTopControlsStacker.addControl(toolbar);

        Assert.assertEquals(
                "Total height should be 0.",
                0,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
    }

    @Test
    public void testBrowserControlsStateChange() {
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        100);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.setScrollingDisabled(true);
        mTopControlsStacker.onControlsConstraintsChanged(
                new BrowserControlsOffsetTagsInfo(),
                new BrowserControlsOffsetTagsInfo(),
                BrowserControlsState.HIDDEN,
                false);
        verify(mBrowserControlsSizer).setTopControlsHeight(100, 0);

        // Simulate a browser controls state change without offset tag update.
        reset(mBrowserControlsSizer);

        verify(mVisibilityDelegate).addObserver(mVisibilityCallbackCaptor.capture());
        mVisibilityCallbackCaptor.getValue().onResult(BrowserControlsState.SHOWN);
        verify(mBrowserControlsSizer).setTopControlsHeight(100, 100);
    }

    @Test
    public void testIsLayerAtBottom() {
        // Create test layers. The bookmark bar is hidden, and the hairline is visible but does not
        // contribute to total height, so neither will prevent the above from being bottom layer.
        TestLayer tabStrip =
                new TestLayer(
                        TopControlType.TABSTRIP,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        0);
        TestLayer toolbar =
                new TestLayer(
                        TopControlType.TOOLBAR,
                        TopControlVisibility.VISIBLE,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        0);
        TestLayer bookmarkBar =
                new TestLayer(
                        TopControlType.BOOKMARK_BAR,
                        TopControlVisibility.HIDDEN,
                        ScrollBehavior.DEFAULT_SCROLLABLE,
                        /* contributesToTotalHeight= */ true,
                        0);

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(bookmarkBar);

        Assert.assertFalse(
                "Layer is not at bottom if it has layers below it.",
                mTopControlsStacker.isLayerAtBottom(TopControlType.TABSTRIP));
        Assert.assertTrue(
                "Layer is at bottom if it has no visible height-contributing layers below it.",
                mTopControlsStacker.isLayerAtBottom(TopControlType.TOOLBAR));

        Assert.assertFalse(
                "Layers not in the current stack can never be at the bottom.",
                mTopControlsStacker.isLayerAtBottom(TopControlType.HAIRLINE));
    }
}
