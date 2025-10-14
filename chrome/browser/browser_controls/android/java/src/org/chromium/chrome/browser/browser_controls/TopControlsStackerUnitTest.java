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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.ScrollBehavior;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link TopControlsStacker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.TOP_CONTROLS_REFACTOR,
    ChromeFeatureList.TOP_CONTROLS_REFACTOR_V2
})
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
        private int mHeight;
        private @TopControlVisibility int mVisibility;
        private @Nullable BrowserControlsOffsetTagsInfo mOffsetTagsInfo;
        private int mLatestYOffset;

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

        @Override
        public void onBrowserControlsOffsetUpdate(int layerYOffset) {
            mLatestYOffset = layerYOffset;
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

        void assertOffset(int expectedOffset) {
            assertEquals(mName + " has wrong offset.", expectedOffset, mLatestYOffset);
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

    @Test
    public void repositionLayer_Scroll_TwoScrollable() {
        var simulator = new TestBrowserControlsOffsetHelper();

        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);

        // Scroll until fully hidden.
        simulator.scrollBy(-20);
        tabStrip.assertOffset(-20);
        toolbar.assertOffset(30);

        simulator.scrollBy(-30);
        tabStrip.assertOffset(-50);
        toolbar.assertOffset(0);

        simulator.scrollBy(-100);
        tabStrip.assertOffset(-50);
        toolbar.assertOffset(-100);

        // Scroll to fully shown.
        simulator.scrollBy(80);
        tabStrip.assertOffset(-50);
        toolbar.assertOffset(-20);

        simulator.scrollBy(40);
        tabStrip.assertOffset(-30);
        toolbar.assertOffset(20);

        simulator.scrollBy(30);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
    }

    @Test
    public void repositionLayer_Scroll_OneScrollableOneNonScrollable() {
        TestLayer statusIndicator = TestLayer.statusIndicatorLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();

        mTopControlsStacker.addControl(statusIndicator);
        mTopControlsStacker.addControl(toolbar);

        // Initiate the simulator and sets the correct offset for mocks.
        var simulator = new TestBrowserControlsOffsetHelper(0, 50);

        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(20);

        // Scroll to hide layer.
        simulator.scrollBy(-20);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(0);

        simulator.scrollBy(-30);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(-30);

        simulator.scrollBy(-50);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(-80);

        // Scroll to re-show the layer.
        simulator.scrollBy(30);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(-50);

        simulator.scrollBy(30);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(-20);

        simulator.scrollBy(40);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(20);
    }

    @Test
    public void repositionLayer_ChangeHeight_HideTopLayer() {
        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer progressBar = TestLayer.progressBarLayer();

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        assertControlsHeight(150, 0);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        progressBar.assertOffset(150);

        // Hide tab strip - other layers should move up.
        tabStrip.mVisibility = TopControlVisibility.HIDDEN;
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(100, 0);
        tabStrip.assertOffset(-50);
        toolbar.assertOffset(0);
        progressBar.assertOffset(100);
    }

    @Test
    public void repositionLayer_ChangeHeight_HideTopLayerWithMinHeight() {
        TestLayer statusIndicator = TestLayer.statusIndicatorLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer progressBar = TestLayer.progressBarLayer();

        mTopControlsStacker.addControl(statusIndicator);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(progressBar);

        var simulator = new TestBrowserControlsOffsetHelper(0, 20);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        assertControlsHeight(120, 20);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(20);
        progressBar.assertOffset(120);

        // Hide tab strip - other layers should move up.
        statusIndicator.mVisibility = TopControlVisibility.HIDDEN;
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(100, 0);
        statusIndicator.assertOffset(-20);
        toolbar.assertOffset(0);
        progressBar.assertOffset(100);
    }

    @Test
    public void repositionLayer_ChangeHeight_HideMidLayer() {
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer bookmark = TestLayer.bookmarkLayer();
        TestLayer progressBar = TestLayer.progressBarLayer();

        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(bookmark);
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        assertControlsHeight(220, 0);
        toolbar.assertOffset(0);
        bookmark.assertOffset(100);
        progressBar.assertOffset(220);

        // Hide bookmark - other layers should move up, top layer remains
        bookmark.mVisibility = TopControlVisibility.HIDDEN;
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(100, 0);
        toolbar.assertOffset(0);
        bookmark.assertOffset(-120);
        progressBar.assertOffset(100);
    }

    @Test
    public void repositionLayer_ChangeHeight_HideBottomLayer() {
        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer bookmark = TestLayer.bookmarkLayer();

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(bookmark);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        assertControlsHeight(270, 0);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        bookmark.assertOffset(150);

        // Hide bookmark - other layers should move up, top layer remains
        bookmark.mVisibility = TopControlVisibility.HIDDEN;
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(150, 0);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        bookmark.assertOffset(-120);
    }

    @Test
    public void repositionLayer_ChangeHeight_LayerChangeHeight() {
        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer bookmark = TestLayer.bookmarkLayer();

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(bookmark);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        assertControlsHeight(270, 0);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        bookmark.assertOffset(150); // Initially hidden

        // Tab strip height grow.
        tabStrip.mHeight += 10;
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(280, 0);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(60);
        bookmark.assertOffset(160);
    }

    @Test
    public void repositionLayer_ChangeHeight_AddTopLayer() {
        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer progressBar = TestLayer.progressBarLayer();

        tabStrip.mVisibility = TopControlVisibility.HIDDEN;

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.

        assertControlsHeight(100, 0);
        tabStrip.assertOffset(-50); // Initially hidden
        toolbar.assertOffset(0);
        progressBar.assertOffset(100);

        // Hide tab strip - other layers should move up.
        tabStrip.mVisibility = TopControlVisibility.VISIBLE;
        mTopControlsStacker.requestLayerUpdate(false);

        assertControlsHeight(150, 0);
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        progressBar.assertOffset(150);
    }

    @Test
    public void repositionLayer_ChangeHeight_AddTopLayerMinHeight() {
        var simulator = new TestBrowserControlsOffsetHelper();
        TestLayer statusIndicator = TestLayer.statusIndicatorLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer progressBar = TestLayer.progressBarLayer();

        statusIndicator.mVisibility = TopControlVisibility.HIDDEN;

        mTopControlsStacker.addControl(statusIndicator);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(progressBar);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        assertControlsHeight(100, 0);
        statusIndicator.assertOffset(-20); // Initially hidden
        toolbar.assertOffset(0);
        progressBar.assertOffset(100);

        // Hide tab strip - other layers should move up.
        statusIndicator.mVisibility = TopControlVisibility.VISIBLE;
        mTopControlsStacker.requestLayerUpdate(false);

        // Simulate the controls reaches the resting state.
        simulator.driveMinHeightOffsetBy(20);

        assertControlsHeight(120, 20);
        statusIndicator.assertOffset(0);
        toolbar.assertOffset(20);
        progressBar.assertOffset(120);
    }

    @Test
    public void repositionLayer_ChangeHeight_AddBottomLayer() {
        TestLayer tabStrip = TestLayer.tabStripLayer();
        TestLayer toolbar = TestLayer.toolbarLayer();
        TestLayer bookmark = TestLayer.bookmarkLayer();

        bookmark.mVisibility = TopControlVisibility.HIDDEN;

        mTopControlsStacker.addControl(tabStrip);
        mTopControlsStacker.addControl(toolbar);
        mTopControlsStacker.addControl(bookmark);
        mTopControlsStacker.requestLayerUpdate(false);

        // Resting position.
        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        bookmark.assertOffset(-120); // Initially hidden

        // Hide tab strip - other layers should move up.
        bookmark.mVisibility = TopControlVisibility.VISIBLE;
        mTopControlsStacker.requestLayerUpdate(false);

        tabStrip.assertOffset(0);
        toolbar.assertOffset(50);
        bookmark.assertOffset(150);
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

    // Helper class to store the current offset during test caes.
    private class TestBrowserControlsOffsetHelper {

        private int mCurrentTopOffset;
        private int mCurrentTopControlsMinHeightOffset;

        TestBrowserControlsOffsetHelper() {
            this(0, 0);
        }

        TestBrowserControlsOffsetHelper(int startTopOffset, int startTopMinHeightOffset) {
            mCurrentTopOffset = startTopOffset;
            mCurrentTopControlsMinHeightOffset = startTopMinHeightOffset;
            doReturn(startTopOffset).when(mBrowserControlsSizer).getTopControlOffset();
            doReturn(startTopMinHeightOffset)
                    .when(mBrowserControlsSizer)
                    .getTopControlsMinHeightOffset();
        }

        /** Simulate scroll the offset by delta. */
        public void scrollBy(int delta) {
            mCurrentTopOffset += delta;
            doReturn(mCurrentTopOffset).when(mBrowserControlsSizer).getTopControlOffset();
            mTopControlsStacker.onControlsOffsetChanged(
                    mCurrentTopOffset,
                    mCurrentTopControlsMinHeightOffset,
                    /* topControlsMinHeightChanged= */ false,
                    0,
                    0,
                    /* bottomControlsMinHeightChanged= */ false,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ false);
        }

        public void driveMinHeightOffsetBy(int delta) {
            mCurrentTopOffset += delta;
            mCurrentTopControlsMinHeightOffset += delta;

            doReturn(mCurrentTopOffset).when(mBrowserControlsSizer).getTopControlOffset();
            doReturn(mCurrentTopControlsMinHeightOffset)
                    .when(mBrowserControlsSizer)
                    .getTopControlsMinHeightOffset();
            mTopControlsStacker.onControlsOffsetChanged(
                    mCurrentTopOffset,
                    mCurrentTopControlsMinHeightOffset,
                    /* topControlsMinHeightChanged= */ true,
                    0,
                    0,
                    /* bottomControlsMinHeightChanged= */ false,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ false);
        }
    }
}
