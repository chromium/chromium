// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
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
import org.chromium.build.annotations.Nullable;
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
        private static final int LAYER_HEIGHT_STATUS_INDICATOR = 20;
        private static final int LAYER_HEIGHT_TAB_STRIP = 50;
        private static final int LAYER_HEIGHT_TOOLBAR = 100;
        private static final int LAYER_HEIGHT_BOOKMARK_BAR = 120;
        private static final int LAYER_HEIGHT_PROGRESS_BAR = 5;

        private final String mName;
        private final @TopControlType int mType;
        private final @ScrollBehavior int mScrollBehavior;
        private final boolean mContributesToTotalHeight;
        private final int mHeight;

        private @TopControlVisibility int mVisibility;
        private @Nullable BrowserControlsOffsetTagsInfo mOffsetTagsInfo;

        TestLayer(
                String name,
                @TopControlType int type,
                @TopControlVisibility int visibility,
                @ScrollBehavior int scrollBehavior,
                boolean contributesToTotalHeight,
                int height) {
            mName = name;
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

        @Override
        public void updateOffsetTag(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {
            mOffsetTagsInfo = offsetTagsInfo;
        }

        // Assert methods

        void assertHasOffsetTags(@Nullable BrowserControlsOffsetTagsInfo offsetTagsInfo) {
            assertEquals(
                    mName + " should holds offset tags info.", offsetTagsInfo, mOffsetTagsInfo);
        }

        void assertHasNoOffsetTags() {
            Assert.assertNull(
                    mName + " Unscrollable layer should not have offset tags info.",
                    mOffsetTagsInfo);
        }

        // Factory methods

        static TestLayer statusIndicatorLayer() {
            return new TestLayer(
                    "STATUS_INDICATOR",
                    TopControlType.STATUS_INDICATOR,
                    TopControlVisibility.VISIBLE,
                    ScrollBehavior.NEVER_SCROLLABLE,
                    /* contributesToTotalHeight= */ true,
                    LAYER_HEIGHT_STATUS_INDICATOR);
        }

        static TestLayer tabStripLayer() {
            return new TestLayer(
                    "TABSTRIP",
                    TopControlType.TABSTRIP,
                    TopControlVisibility.VISIBLE,
                    ScrollBehavior.DEFAULT_SCROLLABLE,
                    /* contributesToTotalHeight= */ true,
                    LAYER_HEIGHT_TAB_STRIP);
        }

        static TestLayer toolbarLayer() {
            return new TestLayer(
                    "TOOLBAR",
                    TopControlType.TOOLBAR,
                    TopControlVisibility.VISIBLE,
                    ScrollBehavior.DEFAULT_SCROLLABLE,
                    /* contributesToTotalHeight= */ true,
                    LAYER_HEIGHT_TOOLBAR);
        }

        static TestLayer bookmarkLayer() {
            return new TestLayer(
                    "BOOKMARK_BAR",
                    TopControlType.BOOKMARK_BAR,
                    TopControlVisibility.VISIBLE,
                    ScrollBehavior.DEFAULT_SCROLLABLE,
                    /* contributesToTotalHeight= */ true,
                    LAYER_HEIGHT_BOOKMARK_BAR);
        }

        static TestLayer progressBarLayer() {
            return new TestLayer(
                    "PROGRESS_BAR",
                    TopControlType.PROGRESS_BAR,
                    TopControlVisibility.VISIBLE,
                    ScrollBehavior.DEFAULT_SCROLLABLE,
                    /* contributesToTotalHeight= */ false,
                    LAYER_HEIGHT_PROGRESS_BAR);
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
        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);
        assertControlsHeight(100, 0);

        mTopControlsStacker.removeControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);
        assertControlsHeight(0, 0);
    }

    @Test
    public void testHeightCalculation() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer tabStrip = TestLayer.tabStripLayer();

        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(150, 0);
    }

    @Test
    public void testHeightCalculation_HiddenControl() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer tabStrip = TestLayer.tabStripLayer();

        tabStrip.mVisibility = TopControlVisibility.HIDDEN;

        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(100, 0);
    }

    @Test
    public void testHeightCalculation_DoesNotContributeToTotalHeight() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer progressBar = TestLayer.progressBarLayer();

        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(100, 0);
    }

    @Test
    public void testMinHeightCalculation() {
        TestLayer statusIndicator = TestLayer.statusIndicatorLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();

        mTopControlsStacker.addControl(statusIndicator);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(120, 20);
    }

    @Test(expected = AssertionError.class)
    public void testAddSameControlTwice() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(toolbar);
    }

    @Test
    public void testRemoveControlNotAdded() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.removeControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);
        assertControlsHeight(0, 0);
    }

    @Test
    public void testZeroHeightControl() {
        TestLayer progressBar = TestLayer.progressBarLayer();
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);
        assertControlsHeight(0, 0);
    }

    @Test
    public void testScrollingDisabled() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.onOffsetTagsInfoChanged(
                new BrowserControlsOffsetTagsInfo(),
                new BrowserControlsOffsetTagsInfo(),
                BrowserControlsState.SHOWN,
                false);

        mTopControlsStacker.setScrollingDisabled(true);

        assertControlsHeight(100, 100);
    }

    @Test
    public void testScrollingDisabled_HiddenToShown() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.setScrollingDisabled(true);
        BrowserControlsOffsetTagsInfo offsetTagsInfo = new BrowserControlsOffsetTagsInfo();
        mTopControlsStacker.onOffsetTagsInfoChanged(
                new BrowserControlsOffsetTagsInfo(),
                offsetTagsInfo,
                BrowserControlsState.HIDDEN,
                false);
        assertControlsHeight(100, 0);
        toolbar.assertHasOffsetTags(offsetTagsInfo);

        // Simulate a browser controls state change without offset tag update.
        reset(mBrowserControlsSizer);

        verify(mVisibilityDelegate).addObserver(mVisibilityCallbackCaptor.capture());
        mVisibilityCallbackCaptor.getValue().onResult(BrowserControlsState.SHOWN);
        assertControlsHeight(100, 100);
        toolbar.assertHasNoOffsetTags();
    }

    @Test
    public void testScrollingDisabled_OffsetTagsInfoChanged() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.setScrollingDisabled(true);
        assertControlsHeight(100, 100);
        reset(mBrowserControlsSizer);

        mTopControlsStacker.onOffsetTagsInfoChanged(
                new BrowserControlsOffsetTagsInfo(),
                new BrowserControlsOffsetTagsInfo(),
                BrowserControlsState.BOTH,
                false);
        assertControlsHeight(100, 100);
        toolbar.assertHasNoOffsetTags();
    }

    @Test
    public void testOffsetTagsInfo_MultipleLayers() {
        BrowserControlsOffsetTagsInfo offsetTagsInfo = new BrowserControlsOffsetTagsInfo();
        mTopControlsStacker.onOffsetTagsInfoChanged(
                new BrowserControlsOffsetTagsInfo(),
                offsetTagsInfo,
                BrowserControlsState.BOTH,
                false);

        TestLayer statusIndicator = TestLayer.statusIndicatorLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();

        mTopControlsStacker.addControl(statusIndicator);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(120, 20);
        statusIndicator.assertHasNoOffsetTags();
        toolbar.assertHasOffsetTags(offsetTagsInfo);
    }

    @Test
    public void testOffsetTagsInfo_ChangeConstraints() {
        BrowserControlsOffsetTagsInfo offsetTagsInfo = new BrowserControlsOffsetTagsInfo();
        mTopControlsStacker.onOffsetTagsInfoChanged(
                new BrowserControlsOffsetTagsInfo(),
                offsetTagsInfo,
                BrowserControlsState.BOTH,
                false);

        TestLayer toolbar = TestLayer.toolbarLayer();
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(100, 0);
        toolbar.assertHasOffsetTags(offsetTagsInfo);

        BrowserControlsOffsetTagsInfo newOffsetTagsInfo = new BrowserControlsOffsetTagsInfo();
        mTopControlsStacker.onOffsetTagsInfoChanged(
                offsetTagsInfo, newOffsetTagsInfo, BrowserControlsState.SHOWN, false);
        // Assert new offset tags are populated.
        toolbar.assertHasOffsetTags(newOffsetTagsInfo);
    }

    @Test
    public void testIsLayerAtBottom() {
        // Create test layers. The bookmark bar is hidden, and the hairline is visible but does not
        // contribute to total height, so neither will prevent the above from being bottom layer.
        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer bookmarkBar = TestLayer.bookmarkLayer();

        bookmarkBar.mVisibility = TopControlVisibility.HIDDEN;

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

    private void assertControlsHeight(int totalHeight, int minHeight) {
        assertEquals(
                "Total height does not match.",
                totalHeight,
                mTopControlsStacker.getVisibleTopControlsTotalHeight());
        assertEquals(
                "Total minHeight does not match.",
                minHeight,
                mTopControlsStacker.getVisibleTopControlsMinHeight());
        verify(mBrowserControlsSizer).setTopControlsHeight(totalHeight, minHeight);

        reset(mBrowserControlsSizer);
    }
}
