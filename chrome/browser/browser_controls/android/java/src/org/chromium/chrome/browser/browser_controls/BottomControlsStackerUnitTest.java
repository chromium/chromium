// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLog.LogItem;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Log;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.OffsetTagConstraints;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;

import java.util.List;

/** Unit tests for the BrowserStateBrowserControlsVisibilityDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class, ShadowLog.class})
public class BottomControlsStackerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final @LayerType int ZERO_HEIGHT_TOP_LAYER = LayerType.PROGRESS_BAR;
    private static final @LayerType int TOP_LAYER = LayerType.READ_ALOUD_PLAYER;
    private static final @LayerType int MID_LAYER = LayerType.TABSTRIP_TOOLBAR;
    private static final @LayerType int BOTTOM_LAYER = LayerType.TEST_BOTTOM_LAYER;

    @Mock BrowserControlsSizer mBrowserControlsSizer;
    @Mock private Context mContext;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Resources mResources;
    @Mock private DisplayAndroid mDisplayAndroid;

    private BottomControlsStacker mBottomControlsStacker;
    private final Configuration mConfig = new Configuration();

    @Before
    public void setup() {
        doReturn(mResources).when(mContext).getResources();
        doReturn(mConfig).when(mResources).getConfiguration();
        doReturn(mDisplayAndroid).when(mWindowAndroid).getDisplay();
        doReturn(1.0f).when(mDisplayAndroid).getDipScale();
        mConfig.screenHeightDp = 800;
        mBottomControlsStacker =
                new BottomControlsStacker(mBrowserControlsSizer, mContext, mWindowAndroid);
    }

    @Test
    public void testHasVisibleLayersOtherThan() {
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);

        assertFalse(
                "Only the bottom layer is currently showing.",
                mBottomControlsStacker.hasVisibleLayersOtherThan(bottom.mType));

        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);

        assertTrue(
                "Not just the bottom layer is showing, the top layer is also showing.",
                mBottomControlsStacker.hasVisibleLayersOtherThan(bottom.mType));
    }

    // Visibility

    @Test
    public void layerVisibilities_visibleIfOthersVisible_switchSingleLayerVisibility() {
        // Add a layer that is VISIBLE_IF_OTHERS_VISIBLE. As no layers are unconditionally VISIBLE,
        // the height should be 0.
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);

        bottom.setVisibility(LayerVisibility.VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(10, 0);
    }

    @Test
    public void layerVisibilities_visibleIfOthersVisible_toggleSecondLayerVisibility() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 110);

        top.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);

        top.setVisibility(LayerVisibility.VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer, times(2)).setBottomControlsHeight(110, 110);
    }

    @Test
    public void layerVisibilities_changeHiddenToVisibleIfOthersVisible() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.HIDDEN);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);

        bottom.setVisibility(LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);
    }

    @Test
    public void layerVisibilities_visibleLayer_addVisibleIfOthersVisibleLayer() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 100);

        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 110);
    }

    @Test
    public void layerVisibilities_hiddenLayer_addVisibleIfOthersVisibleLayer() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.HIDDEN);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);

        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer, times(2)).setBottomControlsHeight(0, 0);
    }

    @Test
    public void layerVisibilities_visibleIfOthersVisibleLayer_addHiddenLayer() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);

        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);

        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.HIDDEN);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);

        verify(mBrowserControlsSizer, times(2)).setBottomControlsHeight(0, 0);
    }

    @Test
    public void layerVisibilities_visibleIfOthersVisible_showsIfVisibleLayerAdded() {
        // Add a layer that is VISIBLE_IF_OTHERS_VISIBLE. As no layers are unconditionally VISIBLE,
        // the height should be 0.
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);

        // Add a second layer that is VISIBLE_IF_OTHERS_VISIBLE. As no layers are unconditionally
        // VISIBLE, the height should be 0.
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer, times(2)).setBottomControlsHeight(0, 0);

        // Add a VISIBLE layer. The VISIBLE_IF_OTHERS_VISIBLE layers should now contribute to the
        // height.
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(160, 0);

        // Hide the VISIBLE layer. The height should return to 0.
        top.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer, times(3)).setBottomControlsHeight(0, 0);
    }

    @Test
    public void singleLayerScrollOff() {
        TestLayer layer =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);
        assertLayerNonScrollable(TOP_LAYER, false);
        assertHasMultipleNonScrollableLayer(false);
    }

    @Test
    public void singleLayerNoScrollOff() {
        TestLayer layer =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 100);
        assertLayerNonScrollable(TOP_LAYER, true);
        assertHasMultipleNonScrollableLayer(false);
    }

    @Test
    public void singleLayerNotVisible() {
        TestLayer layer =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        layer.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);
        assertLayerNonScrollable(TOP_LAYER, false);
        assertHasMultipleNonScrollableLayer(false);
    }

    @Test
    public void testLayerMinHeightClearedWhenHidden() {
        TestLayer layer =
                new TestLayer(
                        MID_LAYER,
                        50,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);

        assertTrue(mBottomControlsStacker.isLayerNonScrollable(MID_LAYER));
        assertTrue(mBottomControlsStacker.hasNonScrollableLayersOtherThan(LayerType.BOTTOM_CHIN));

        // Hide the layer. It should no longer be considered non-scrollable.
        layer.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.requestLayerUpdate(false);

        assertFalse(mBottomControlsStacker.isLayerNonScrollable(MID_LAYER));
        assertFalse(mBottomControlsStacker.hasNonScrollableLayersOtherThan(LayerType.BOTTOM_CHIN));
    }

    @Test
    public void stackLayerBothScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        assertLayerNonScrollable(TOP_LAYER, false);
        assertLayerNonScrollable(BOTTOM_LAYER, false);
        assertHasMultipleNonScrollableLayer(false);
    }

    @Test
    public void stackLayerBothNoScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 110);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        assertLayerNonScrollable(TOP_LAYER, true);
        assertLayerNonScrollable(BOTTOM_LAYER, true);
        assertHasMultipleNonScrollableLayer(true);
    }

    @Test
    public void stackLayerOneScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 10);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        assertLayerNonScrollable(TOP_LAYER, false);
        assertLayerNonScrollable(BOTTOM_LAYER, true);
        assertHasMultipleNonScrollableLayer(false);
    }

    @Test
    public void stackLayerDefaultNoScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        MID_LAYER,
                        60,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer3 =
                new TestLayer(
                        BOTTOM_LAYER,
                        20,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.addLayer(layer3);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(180, 80);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        assertLayerNonScrollable(TOP_LAYER, false);
        assertLayerNonScrollable(MID_LAYER, true);
        assertLayerNonScrollable(BOTTOM_LAYER, true);
        assertHasMultipleNonScrollableLayer(true);
    }

    @Test
    public void stackLayerDefaultNoScrollOff_ZeroHeightNeverScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        0,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        20,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(20, 20);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        assertLayerNonScrollable(TOP_LAYER, true);
        assertLayerNonScrollable(BOTTOM_LAYER, true);
        assertHasMultipleNonScrollableLayer(true);
    }

    @Test
    public void stackLayerDefaultScrollsOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        20,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(120, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        assertLayerNonScrollable(TOP_LAYER, false);
        assertLayerNonScrollable(BOTTOM_LAYER, false);
        assertHasMultipleNonScrollableLayer(false);
    }

    @Test
    public void stackLayerChangeHeight() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);

        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 10);

        layer1.setHeight(1000);
        layer2.setHeight(9);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1009, 9);
    }

    @Test
    public void onBottomControlsHeightChanged_ThreeLayers() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        mBottomControlsStacker.onBottomControlsHeightChanged(1110, 0);

        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    // Reposition layer test

    @Test
    public void reposition_ScrollOff_OneLayer_AppliedByBrowser() {
        TestLayer layer =
                new TestLayer(
                        BOTTOM_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);

        // Controls fully visible
        onBottomControlsOffsetChanged(0, 0, false, true);
        assertLayerYOffset(layer, 0);

        // Scroll down.
        onBottomControlsOffsetChanged(60, 0, false, true);
        assertLayerYOffset(layer, 60);

        // Controls Full scroll off.
        onBottomControlsOffsetChanged(100, 0, false, true);
        assertLayerYOffset(layer, 100);

        // Scroll up.
        onBottomControlsOffsetChanged(30, 0, false, true);
        assertLayerYOffset(layer, 30);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 0, false, true);
        assertLayerYOffset(layer, 0);
    }

    @Test
    public void reposition_ScrollOff_TwoLayers_AppliedByBrowser() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Browser controls fully shown.
        onBottomControlsOffsetChanged(0, 0, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Bottom layer partially scrolled off.
        onBottomControlsOffsetChanged(5, 0, false, true);
        assertLayerYOffset(top, -5);
        assertLayerYOffset(bottom, 5);

        // Bottom layer scrolled off, top layer partially scrolled off
        onBottomControlsOffsetChanged(50, 0, false, true);
        assertLayerYOffset(top, 40);
        assertLayerYOffset(bottom, 10);

        // Fully scroll off.
        onBottomControlsOffsetChanged(110, 0, false, true);
        assertLayerYOffset(top, 100);
        assertLayerYOffset(bottom, 10);

        // Scroll back up. Top layer moves first.
        onBottomControlsOffsetChanged(40, 0, false, true);
        assertLayerYOffset(top, 30);
        assertLayerYOffset(bottom, 10);

        // Scroll back to browser controls fully shown.
        onBottomControlsOffsetChanged(0, 0, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_ScrollOff_ThreeLayers_AppliedByBrowser() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);

        // Browser controls fully shown.
        onBottomControlsOffsetChanged(0, 0, false, true);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Bottom layer partially scrolled off.
        onBottomControlsOffsetChanged(5, 0, false, true);
        assertLayerYOffset(top, -105);
        assertLayerYOffset(mid, -5);
        assertLayerYOffset(bottom, 5);

        // Bottom layer scrolled off, mid layer partially scrolled off.
        onBottomControlsOffsetChanged(50, 0, false, true);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, 40);
        assertLayerYOffset(bottom, 10);

        // Bottom and min layer both scrolled off, top layer partially scrolled off.
        onBottomControlsOffsetChanged(500, 0, false, true);
        assertLayerYOffset(top, 390);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 10);

        // All layers fully scroll off.
        onBottomControlsOffsetChanged(1110, 0, false, true);
        assertLayerYOffset(top, 1000);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 10);

        // Scroll back up. Top layer moves first.
        onBottomControlsOffsetChanged(900, 0, false, true);
        assertLayerYOffset(top, 790);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 10);

        // Scroll back the mid layer start showing.
        onBottomControlsOffsetChanged(90, 0, false, true);
        assertLayerYOffset(top, -20);
        assertLayerYOffset(mid, 80);
        assertLayerYOffset(bottom, 10);

        // Scroll back the bottom layer start showing.
        onBottomControlsOffsetChanged(9, 0, false, true);
        assertLayerYOffset(top, -101);
        assertLayerYOffset(mid, -1);
        assertLayerYOffset(bottom, 9);

        // Full visible.
        onBottomControlsOffsetChanged(0, 0, false, true);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_NoScrollOff_OneLayer() {
        TestLayer layer =
                new TestLayer(
                        BOTTOM_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 100);

        // Controls fully visible as min height.
        onBottomControlsOffsetChanged(0, 100, false);
        assertLayerYOffset(layer, 0);
    }

    @Test
    public void reposition_Mixed_TwoLayers_BottomLayerNoScroll_AppliedByBrowser() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 10);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 10, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling down.
        onBottomControlsOffsetChanged(5, 10, false, true);
        assertLayerYOffset(top, -5);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling down.
        onBottomControlsOffsetChanged(50, 10, false, true);
        assertLayerYOffset(top, 40);
        assertLayerYOffset(bottom, 0);

        // Top controls fully scroll off.
        onBottomControlsOffsetChanged(100, 10, false, true);
        assertLayerYOffset(top, 90);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling back up.
        onBottomControlsOffsetChanged(80, 10, false, true);
        assertLayerYOffset(top, 70);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling up.
        onBottomControlsOffsetChanged(5, 10, false, true);
        assertLayerYOffset(top, -5);
        assertLayerYOffset(bottom, 0);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 10, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_HidingLayer_Counteraction() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.HIDING);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        // Total height should exclude HIDING layer! So 100 + 10 = 110.
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Simulate animation: bottomOffset goes from -50 to 0.
        // Frame 1: bottomOffset = -50.
        onBottomControlsOffsetChanged(-50, 0, false, false);

        // Since isVisibilityForced is false, it uses resting offsets.
        // - bottom (VISIBLE) gets 0.
        // - mid (HIDING) is skipped in resting offset calculation and falls back to height = 50.
        // - top (VISIBLE) gets height - totalHeight = 100 - 110 = -10.
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, 50);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_ShowingLayer_NoCounteraction() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.SHOWING);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(160, 0);

        // Simulate animation: bottomOffset goes from 50 to 0.
        // Frame 1: bottomOffset = 50.
        onBottomControlsOffsetChanged(50, 0, false, false);

        // Since isVisibilityForced is false, it uses resting offsets.
        // All layers are considered visible (including SHOWING).
        // - bottom gets 0.
        // - mid gets -bottom.height = -10.
        // - top gets -bottom.height - mid.height = -10 - 50 = -60.
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_Mixed_ThreeLayers_DefaultScrollUnderNeverScroll_AppliedByBrowser() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        50,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(160, 60);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 60, false, true);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling down.
        onBottomControlsOffsetChanged(5, 60, false, true);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling down.
        onBottomControlsOffsetChanged(50, 60, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Top controls fully scroll off.
        onBottomControlsOffsetChanged(100, 60, false, true);
        assertLayerYOffset(top, 40);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling back up.
        onBottomControlsOffsetChanged(80, 60, false, true);
        assertLayerYOffset(top, 20);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling up.
        onBottomControlsOffsetChanged(5, 60, false, true);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 60, false, true);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_Mixed_ThreeLayers_DefaultScrollAboveNeverScroll_AppliedByBrowser() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(160, 50);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 50, false, true);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -50);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling down.
        onBottomControlsOffsetChanged(5, 50, false, true);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -45);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling down.
        onBottomControlsOffsetChanged(50, 50, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, -40);
        assertLayerYOffset(bottom, 0);

        // Top controls fully scroll off.
        onBottomControlsOffsetChanged(110, 50, false, true);
        assertLayerYOffset(top, 50);
        assertLayerYOffset(mid, -40);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling back up.
        onBottomControlsOffsetChanged(80, 50, false, true);
        assertLayerYOffset(top, 20);
        assertLayerYOffset(mid, -40);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling up.
        onBottomControlsOffsetChanged(5, 50, false, true);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -45);
        assertLayerYOffset(bottom, 0);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 50, false, true);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -50);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_RemoveLayer_RemoveTop() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Do a offset update so each layers are positioned.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Simulate a browser controls height change because top layer is removed.
        mBottomControlsStacker.removeLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(10, 0);

        // Simulate browser controls update. As browser controls is not animated, dispatch the
        // offset directly.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_RemoveLayer_RemoveTop_Animated() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Do a offset update so each layers are positioned.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Simulate a browser controls height change because top layer is removed.
        top.setVisibility(LayerVisibility.HIDING);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(10, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        // Simulate browser controls update. As top control is removed, the top layer should still
        // receive updates.
        onBottomControlsOffsetChanged(-100, 0, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(-60, 0, true);
        assertLayerYOffset(top, 30);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(-20, 0, true);
        assertLayerYOffset(top, 70);
        assertLayerYOffset(bottom, 0);

        // When animation finished, the hidden layer has its yOffset is set to its height.
        onBottomControlsOffsetChanged(0, 0, true);
        assertLayerYOffset(top, 100);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_RemoveLayer_RemovedBottom() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Do a offset update so each layers are positioned.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Simulate a browser controls height change because top layer is removed.
        mBottomControlsStacker.removeLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);

        // Simulate browser controls update. As browser controls is not animated, dispatch the
        // offset directly.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, 0);
    }

    @Test
    public void reposition_RemoveLayer_RemovedBottom_Animated() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Do a offset update so each layers are positioned.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Simulate a browser controls height change because bottom layer is removed.
        bottom.setVisibility(LayerVisibility.HIDING);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        // Simulate browser controls update. As the bottom layer is removed, both layer will
        // receive updates.
        onBottomControlsOffsetChanged(-10, 0, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(-6, 0, true);
        assertLayerYOffset(top, -6);
        assertLayerYOffset(bottom, 4);

        onBottomControlsOffsetChanged(-2, 0, true);
        assertLayerYOffset(top, -2);
        assertLayerYOffset(bottom, 8);

        // When animation settled, the yOffset for the bottom layer is set to its height.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, 0);
        assertLayerYOffset(bottom, 10);
    }

    @Test
    public void reposition_RemoveLayer_RemovedMid() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);

        // Do a offset update so each layers are positioned.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Simulate a browser controls height change because mid layer is removed.
        mBottomControlsStacker.removeLayer(mid);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1010, 0);

        // Simulate browser controls update. As browser controls is not animated, dispatch the
        // offset directly.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_RemoveLayer_RemovedMid_Animated() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);

        // Do a offset update so each layers are positioned.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Simulate a browser controls height change because mid layer is hidden.
        mid.setVisibility(LayerVisibility.HIDING);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1010, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        // Animation started - bottom controls will hold a negative offset.
        // As mid layer is hidden, the top layer will change its offset.
        // The mid layer will receive offsets too, but its height is not part of the browser
        // controls anymore.
        onBottomControlsOffsetChanged(-100, 0, true);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(-60, 0, true);
        assertLayerYOffset(top, -70);
        assertLayerYOffset(mid, 30);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(-20, 0, true);
        assertLayerYOffset(top, -30);
        assertLayerYOffset(mid, 70);
        assertLayerYOffset(bottom, 0);

        // After animation settled, the yOffset for mid layer is set to its heght.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_AddLayers_AddBottom() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, 0);

        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_AddLayers_AddBottom_Animated() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, 0);

        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        // Animation started, both top layer and bottom layer gradually moves up.
        onBottomControlsOffsetChanged(10, 0, true);
        assertLayerYOffset(top, 0);
        assertLayerYOffset(bottom, 10);

        onBottomControlsOffsetChanged(6, 0, true);
        assertLayerYOffset(top, -4);
        assertLayerYOffset(bottom, 6);

        onBottomControlsOffsetChanged(2, 0, true);
        assertLayerYOffset(top, -8);
        assertLayerYOffset(bottom, 2);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_AddLayers_AddTop() {
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(10, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(bottom, 0);

        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_AddLayers_AddTop_Animated() {
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(10, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(bottom, 0);

        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        // Animation started - only the top layer moves, the bottom layer stay in-place.
        onBottomControlsOffsetChanged(100, 0, true);
        assertLayerYOffset(top, 90);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(60, 0, true);
        assertLayerYOffset(top, 50);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(20, 0, true);
        assertLayerYOffset(top, 10);
        assertLayerYOffset(bottom, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_AddLayers_AddMid() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1010, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_AddLayers_AddMid_Animated() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1010, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);

        // Animation started, the offset will be set to a positive value as browser control grows.
        // Since layer mid was added, the bottom layer's offset should not change.
        onBottomControlsOffsetChanged(100, 0, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, 90);
        assertLayerYOffset(bottom, 0);

        // As offset reduces, the top and mid layer will shift up.
        onBottomControlsOffsetChanged(60, 0, true);
        assertLayerYOffset(top, -50);
        assertLayerYOffset(mid, 50);
        assertLayerYOffset(bottom, 0);

        // As offset reduces, the top and mid layer will shift up.
        onBottomControlsOffsetChanged(20, 0, true);
        assertLayerYOffset(top, -90);
        assertLayerYOffset(mid, 10);
        assertLayerYOffset(bottom, 0);

        // Reached the final state. No more animations.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void testLayerWithZeroHeight() {
        TestLayer topWithZeroHeight =
                new TestLayer(
                        ZERO_HEIGHT_TOP_LAYER,
                        0,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        93,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(topWithZeroHeight);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);

        verify(mBrowserControlsSizer).setBottomControlsHeight(1093, 0);

        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -93);
        assertLayerYOffset(topWithZeroHeight, -1 * (top.getHeight() + bottom.getHeight()));
        assertLayerYOffset(bottom, 0);

        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1193, 0);
        onBottomControlsOffsetChanged(0, 0, false);

        assertLayerYOffset(top, -193);
        assertLayerYOffset(
                topWithZeroHeight, -1 * (top.getHeight() + mid.getHeight() + bottom.getHeight()));
        assertLayerYOffset(mid, -93);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void testLayerMetrics() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE_IF_OTHERS_VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BottomControlsStacker.NumberOfVisibleLayers", 2)
                        .expectIntRecord(
                                "Android.BottomControlsStacker.PercentageOfWindowUsedByBottomControlsAtMaxHeight2",
                                19)
                        .expectIntRecord(
                                "Android.BottomControlsStacker.PercentageOfWindowUsedByBottomControlsAtMinHeight2",
                                0)
                        .build();

        mBottomControlsStacker.notifyDidFinishNavigationInPrimaryMainFrame();
        histogramWatcher.assertExpected();

        TestLayer middle =
                new TestLayer(
                        MID_LAYER,
                        10,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(middle);
        mBottomControlsStacker.requestLayerUpdate(false);

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.BottomControlsStacker.NumberOfVisibleLayers", 3)
                        .expectIntRecord(
                                "Android.BottomControlsStacker.PercentageOfWindowUsedByBottomControlsAtMaxHeight2",
                                20)
                        .expectIntRecord(
                                "Android.BottomControlsStacker.PercentageOfWindowUsedByBottomControlsAtMinHeight2",
                                8)
                        .build();

        mBottomControlsStacker.notifyDidFinishNavigationInPrimaryMainFrame();
        histogramWatcher.assertExpected();
    }

    @Test
    public void testCalculateHeightFromLayer() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.updateLayerVisibilitiesAndSizes();

        assertEquals(
                "top, mid and bottom layers should be counted",
                1110,
                mBottomControlsStacker.getHeightFromLayerToBottom(TOP_LAYER));
        assertEquals(
                "Only mid and bottom layers should be counted",
                110,
                mBottomControlsStacker.getHeightFromLayerToBottom(MID_LAYER));
        assertEquals(
                "Only bottom layer should be counted",
                10,
                mBottomControlsStacker.getHeightFromLayerToBottom(BOTTOM_LAYER));
        assertEquals(
                "invalid layer shoud return INVALID_HEIGHT",
                BottomControlsStacker.INVALID_HEIGHT,
                mBottomControlsStacker.getHeightFromLayerToBottom(-1));
    }

    @Test
    public void testCalculateHeightFromLayer_oneLayerIsHiding() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.HIDING);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.updateLayerVisibilitiesAndSizes();

        assertEquals(
                "Only top and mid layers should be counted since bottom layer is hided.",
                1100,
                mBottomControlsStacker.getHeightFromLayerToBottom(TOP_LAYER));
        assertEquals(
                "Only mid layer should be counted since bottom layer is hided.",
                100,
                mBottomControlsStacker.getHeightFromLayerToBottom(MID_LAYER));
        assertEquals(
                "No layers should be counted",
                0,
                mBottomControlsStacker.getHeightFromLayerToBottom(BOTTOM_LAYER));
        assertEquals(
                "invalid layer shoud return INVALID_HEIGHT",
                BottomControlsStacker.INVALID_HEIGHT,
                mBottomControlsStacker.getHeightFromLayerToBottom(-1));
    }

    @Test
    public void testCalculateHeightFromLayer_bottomChinWithBottomToolbarPresent() {
        TestLayer toolbar =
                new TestLayer(
                        LayerType.BOTTOM_TOOLBAR,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer chin =
                new TestLayer(
                        LayerType.BOTTOM_CHIN,
                        30,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(toolbar);
        mBottomControlsStacker.addLayer(chin);
        mBottomControlsStacker.updateLayerVisibilitiesAndSizes();

        assertEquals(
                "Only bottom chin layer should be counted when starting from BOTTOM_CHIN",
                30,
                mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.BOTTOM_CHIN));
        assertEquals(
                "Both toolbar and chin should be counted when starting from BOTTOM_TOOLBAR",
                130,
                mBottomControlsStacker.getHeightFromLayerToBottom(LayerType.BOTTOM_TOOLBAR));
    }

    @Test
    public void reposition_AppliedByViz() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        150,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        20,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(170, 0);

        // When visibility isn't forced, BCIV takes over and controls should always be positioned
        // at their fully visible positions.
        onBottomControlsOffsetChanged(10, 0, false, false);
        assertLayerYOffset(top, -20);
        assertLayerYOffset(bottom, 0);

        // When visibility is forced, behavior is identical to when BCIV is disabled.
        onBottomControlsOffsetChanged(10, 0, false, true);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 10);
    }

    @Test
    public void testOnControlsConstraintsChanged() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        150,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        20,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(170, 20);

        BrowserControlsOffsetTagsInfo tagsInfo = new BrowserControlsOffsetTagsInfo();
        mBottomControlsStacker.onOffsetTagsInfoChanged(null, tagsInfo, 0, false);
        mBottomControlsStacker.updateLayerVisibilitiesAndSizes();

        OffsetTag offsetTag = tagsInfo.getBottomControlsOffsetTag();
        assertEquals(offsetTag, top.getOffsetTag());
        assertEquals(null, bottom.getOffsetTag());

        // Nothing has triggered an update of the layer's offset yet.
        assertLayerYOffset(top, 0);
        assertLayerYOffset(bottom, 0);

        // Only consider the additional height from the scrollable layer.
        int additionalHeight = TestLayer.ADDITIONAL_HEIGHT;
        verify(mBrowserControlsSizer).setBottomControlsAdditionalHeight(additionalHeight);

        // The OffsetTagsInfo should get updated with the new OffsetTagConstraints.
        int totalHeight = top.getHeight() + bottom.getHeight();
        int maxScrollOffset = totalHeight + additionalHeight;
        OffsetTagConstraints newConstraints =
                tagsInfo.getConstraints().getBottomControlsConstraints();
        OffsetTagConstraints expectedConstraints =
                new OffsetTagConstraints(0, 0, 0, maxScrollOffset);
        assertTrue(newConstraints.equals(expectedConstraints));

        mBottomControlsStacker.onOffsetTagsInfoChanged(null, tagsInfo, 0, true);
        assertLayerYOffset(top, -20);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void testOnControlsConstraintsChanged_clearOffsetTag() {
        TestLayer topLayer =
                new TestLayer(
                        TOP_LAYER,
                        150,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer middleLayer =
                new TestLayer(
                        MID_LAYER,
                        30,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottomLayer =
                new TestLayer(
                        BOTTOM_LAYER,
                        20,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(topLayer);
        mBottomControlsStacker.addLayer(middleLayer);
        mBottomControlsStacker.addLayer(bottomLayer);
        mBottomControlsStacker.requestLayerUpdate(false);

        BrowserControlsOffsetTagsInfo tagsInfo = new BrowserControlsOffsetTagsInfo();
        mBottomControlsStacker.onOffsetTagsInfoChanged(null, tagsInfo, 0, false);

        assertNotNull(
                "Top layer is scrollable and should have offset tag.", topLayer.getOffsetTag());
        assertNull("Mid layer should not have offset tag.", middleLayer.getOffsetTag());
        assertNull("Bottom layer should not have offset tag.", bottomLayer.getOffsetTag());
    }

    // Test helpers

    private void onBottomControlsOffsetChanged(
            int bottomControlsOffset, int bottomControlsMinHeightOffset, boolean requestNewFrame) {
        onBottomControlsOffsetChanged(
                bottomControlsOffset, bottomControlsMinHeightOffset, false, requestNewFrame, false);
    }

    private void onBottomControlsOffsetChanged(
            int bottomControlsOffset,
            int bottomControlsMinHeightOffset,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        onBottomControlsOffsetChanged(
                bottomControlsOffset,
                bottomControlsMinHeightOffset,
                false,
                requestNewFrame,
                isVisibilityForced);
    }

    private void onBottomControlsOffsetChanged(
            int bottomControlsOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        mBottomControlsStacker.onControlsOffsetChanged(
                0,
                0,
                false,
                bottomControlsOffset,
                bottomControlsMinHeightOffset,
                bottomControlsMinHeightChanged,
                requestNewFrame,
                isVisibilityForced);
    }

    private void assertLayerYOffset(TestLayer layer, int expectedOffset) {
        assertEquals("Different yOffset observed.", expectedOffset, layer.mYOffset);
    }

    /**
     * Mirrors BrowserControlsManager#setBottomControlsHeight: early-return when the heights are
     * unchanged, otherwise store them (reflected by the getters) and notify observers via
     * onBottomControlsHeightChanged, so the stacker's re-show path is exercised the way the manager
     * would drive it.
     */
    private void simulateBrowserControlsManagerHeightDispatch() {
        final int[] lastHeights = new int[] {0, 0};
        doAnswer(invocation -> lastHeights[0])
                .when(mBrowserControlsSizer)
                .getBottomControlsHeight();
        doAnswer(invocation -> lastHeights[1])
                .when(mBrowserControlsSizer)
                .getBottomControlsMinHeight();
        doAnswer(
                        invocation -> {
                            int height = invocation.getArgument(0);
                            int minHeight = invocation.getArgument(1);
                            if (lastHeights[0] == height && lastHeights[1] == minHeight) {
                                return null;
                            }
                            lastHeights[0] = height;
                            lastHeights[1] = minHeight;
                            mBottomControlsStacker.onBottomControlsHeightChanged(height, minHeight);
                            return null;
                        })
                .when(mBrowserControlsSizer)
                .setBottomControlsHeight(anyInt(), anyInt());
    }

    private void assertLayerNonScrollable(@LayerType int type, boolean nonScrollable) {
        assertEquals(
                "isLayerNonScrollable(" + type + ") is unexpected.",
                nonScrollable,
                mBottomControlsStacker.isLayerNonScrollable(type));
    }

    private void assertHasMultipleNonScrollableLayer(boolean hasOtherLayers) {
        assertEquals(
                "hasMultipleNonScrollableLayer() is unexpected.",
                hasOtherLayers,
                mBottomControlsStacker.hasMultipleNonScrollableLayer());
    }

    private static class TestLayer implements BottomControlsLayer {
        public static final int ADDITIONAL_HEIGHT = 10;
        private final @LayerType int mType;
        private final @LayerScrollBehavior int mScrollBehavior;
        private int mHeight;
        private @LayerVisibility int mVisibility;
        private int mYOffset;
        private OffsetTag mOffsetTag;

        TestLayer(
                @LayerType int type,
                int height,
                @LayerScrollBehavior int scrollBehavior,
                @LayerVisibility int layerVisibility) {
            mType = type;
            mHeight = height;
            mScrollBehavior = scrollBehavior;
            mVisibility = layerVisibility;
        }

        public void setVisibility(@LayerVisibility int visibility) {
            mVisibility = visibility;
        }

        public void setHeight(int height) {
            mHeight = height;
        }

        @Override
        public int getHeight() {
            return mHeight;
        }

        public OffsetTag getOffsetTag() {
            return mOffsetTag;
        }

        @Override
        public @LayerScrollBehavior int getScrollBehavior() {
            return mScrollBehavior;
        }

        @Override
        public @LayerVisibility int getLayerVisibility() {
            return mVisibility;
        }

        @Override
        public @LayerType int getType() {
            return mType;
        }

        @Override
        public void onBrowserControlsOffsetUpdate(int layerYOffset) {
            mYOffset = layerYOffset;
        }

        @Override
        public int updateOffsetTag(BrowserControlsOffsetTagsInfo offsetTagsInfo) {
            mOffsetTag = offsetTagsInfo.getBottomControlsOffsetTag();
            return ADDITIONAL_HEIGHT;
        }

        @Override
        public void clearOffsetTag() {
            mOffsetTag = null;
        }
    }

    private static class TestLayerWithColor extends TestLayer {
        private final @ColorInt int mBackgroundColor;

        TestLayerWithColor(
                @LayerType int type,
                int height,
                @LayerScrollBehavior int scrollBehavior,
                @LayerVisibility int layerVisibility,
                @ColorInt int backgroundColor) {
            super(type, height, scrollBehavior, layerVisibility);
            mBackgroundColor = backgroundColor;
        }

        @Override
        public @Nullable @ColorInt Integer getBackgroundColor() {
            return mBackgroundColor;
        }
    }

    @Test
    public void testUpdateBackgroundColorFromLayers_noLayersWithBackgroundColor() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(false);

        // Verify that notifyBackgroundColor is never called since no layers provide
        // background colors.
        verify(mBrowserControlsSizer, never()).notifyBackgroundColor(anyInt());
    }

    @Test
    public void testUpdateBackgroundColorFromLayers_bottomMostVisibleLayerColorSelected() {
        TestLayerWithColor topLayer =
                new TestLayerWithColor(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE,
                        Color.BLUE);
        TestLayerWithColor bottomLayer =
                new TestLayerWithColor(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE,
                        Color.GREEN);

        mBottomControlsStacker.addLayer(topLayer);
        mBottomControlsStacker.addLayer(bottomLayer);
        mBottomControlsStacker.requestLayerUpdate(false);

        // Verify that the bottom-most visible layer's color is used.
        verify(mBrowserControlsSizer).notifyBackgroundColor(Color.GREEN);
    }

    @Test
    public void testUpdateBackgroundColorFromLayers_bottomLayerNoColor_topLayerHasColor() {
        TestLayerWithColor topLayer =
                new TestLayerWithColor(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE,
                        Color.RED);
        TestLayer bottomLayer =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(topLayer);
        mBottomControlsStacker.addLayer(bottomLayer);
        mBottomControlsStacker.requestLayerUpdate(false);

        // Verify that the top layer's color is used since bottom layer doesn't provide a color.
        verify(mBrowserControlsSizer).notifyBackgroundColor(Color.RED);
    }

    @Test
    public void testIsTopmostVisibleLayer() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);

        assertTrue(mBottomControlsStacker.isTopmostVisibleLayer(TOP_LAYER));
        assertFalse(mBottomControlsStacker.isTopmostVisibleLayer(BOTTOM_LAYER));
    }

    @Test
    public void testHasNonScrollableLayersOtherThan() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);

        assertTrue(mBottomControlsStacker.hasNonScrollableLayersOtherThan(TOP_LAYER));
        assertFalse(mBottomControlsStacker.hasNonScrollableLayersOtherThan(BOTTOM_LAYER));
    }

    @Test
    public void testAnimationEnded_restoresOffsetTagWhenNonScrollableHidden() {
        // Register a scrollable layer (BOTTOM_LAYER) and a non-scrollable layer (TOP_LAYER).
        TestLayer scrollable =
                new TestLayer(
                        BOTTOM_LAYER,
                        50,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer nonScrollable =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(scrollable);
        mBottomControlsStacker.addLayer(nonScrollable);

        // Initial update.
        mBottomControlsStacker.requestLayerUpdate(false);

        // Setup offset tags info.
        BrowserControlsOffsetTagsInfo offsetTagsInfo = new BrowserControlsOffsetTagsInfo();
        mBottomControlsStacker.onOffsetTagsInfoChanged(null, offsetTagsInfo, 0, false);

        // Trigger animation ended while both are visible.
        mBottomControlsStacker.onBottomControlsHeightAnimationEnded();
        assertNull(scrollable.mOffsetTag);

        // Hide the non-scrollable layer.
        nonScrollable.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.requestLayerUpdate(false);

        // Trigger animation ended again. Now the scrollable layer should have its OffsetTag
        // restored.
        mBottomControlsStacker.onBottomControlsHeightAnimationEnded();
        assertEquals(offsetTagsInfo.getBottomControlsOffsetTag(), scrollable.mOffsetTag);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT)
    public void repositionLayers_suppressesRedundantUpdatesWhileHidden() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        TestLayer layer =
                spy(
                        new TestLayer(
                                BOTTOM_LAYER,
                                50,
                                LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                                LayerVisibility.VISIBLE));
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(layer, times(1)).onBrowserControlsOffsetUpdate(anyInt());

        // Hide the layer.
        layer.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.requestLayerUpdate(false);
        // The transition to HIDDEN should dispatch onBrowserControlsOffsetUpdate once.
        verify(layer, times(2)).onBrowserControlsOffsetUpdate(anyInt());
        verify(layer, times(1)).onBrowserControlsOffsetUpdate(50);

        // Subsequent repositionings while still HIDDEN should NOT invoke
        // onBrowserControlsOffsetUpdate again.
        onBottomControlsOffsetChanged(0, 0, false);
        verify(layer, times(2)).onBrowserControlsOffsetUpdate(anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT
    })
    public void testRequestLayerUpdate_hiddenSheetReshown_scrollableBar_redispatchesOffset() {
        simulateBrowserControlsManagerHeightDispatch();
        TestLayer bar =
                spy(
                        new TestLayer(
                                LayerType.BOTTOM_APP_BAR,
                                100,
                                LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                                LayerVisibility.VISIBLE));
        // BottomSheetLayer contributes 0 height when the content is not acting as browser
        // controls (BottomSheetManager#calculateContributedHeight).
        TestLayer sheet =
                spy(
                        new TestLayer(
                                LayerType.BOTTOM_SHEET,
                                0,
                                LayerScrollBehavior.NEVER_SCROLL_OFF,
                                LayerVisibility.HIDDEN));
        mBottomControlsStacker.addLayer(sheet);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);
        // Hidden sheet dispatched exactly once.
        verify(sheet, times(1)).onBrowserControlsOffsetUpdate(anyInt());

        // Renderer scrolls the bar off. The hidden sheet layer is deduped.
        doReturn(50).when(mBrowserControlsSizer).getBottomControlOffset();
        onBottomControlsOffsetChanged(50, 0, false);
        doReturn(100).when(mBrowserControlsSizer).getBottomControlOffset();
        onBottomControlsOffsetChanged(100, 0, false);
        verify(sheet, times(1)).onBrowserControlsOffsetUpdate(anyInt());

        // Sheet opens: the layer flips VISIBLE and pins the bar, so min height changes 0 -> 100
        // and the sizer's height change repositions the layers.
        sheet.setVisibility(LayerVisibility.VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 100);
        verify(sheet, times(2)).onBrowserControlsOffsetUpdate(anyInt());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_BOTTOM_BAR,
        ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT
    })
    public void testRequestLayerUpdate_hiddenSheetReshown_barAlreadyPinned_redispatchesOffset() {
        simulateBrowserControlsManagerHeightDispatch();
        // READ_ALOUD_PLAYER is NEVER_SCROLL_OFF and sits above the bar, so the bar is already
        // pinned before the sheet opens.
        TestLayer miniPlayer =
                spy(
                        new TestLayer(
                                LayerType.READ_ALOUD_PLAYER,
                                50,
                                LayerScrollBehavior.NEVER_SCROLL_OFF,
                                LayerVisibility.VISIBLE));
        TestLayer bar =
                spy(
                        new TestLayer(
                                LayerType.BOTTOM_APP_BAR,
                                100,
                                LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                                LayerVisibility.VISIBLE));
        TestLayer sheet =
                spy(
                        new TestLayer(
                                LayerType.BOTTOM_SHEET,
                                0,
                                LayerScrollBehavior.NEVER_SCROLL_OFF,
                                LayerVisibility.HIDDEN));
        mBottomControlsStacker.addLayer(sheet);
        mBottomControlsStacker.addLayer(miniPlayer);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(150, 150);
        verify(sheet, times(1)).onBrowserControlsOffsetUpdate(anyInt());
        verify(bar, times(1)).onBrowserControlsOffsetUpdate(anyInt());

        // Sheet opens: the layer flips VISIBLE but neither height nor min height changes, so the
        // sizer early-returns. The stacker must reposition on its own so the sheet layer does not
        // keep the value from its last hidden dispatch.
        sheet.setVisibility(LayerVisibility.VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer, times(2)).setBottomControlsHeight(150, 150);
        assertTrue(mBottomControlsStacker.isLayerVisible(LayerType.BOTTOM_SHEET));
        verify(sheet, times(2)).onBrowserControlsOffsetUpdate(anyInt());
        verify(bar, times(2)).onBrowserControlsOffsetUpdate(anyInt());

        // A further update with no visibility change must not reposition again.
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(sheet, times(2)).onBrowserControlsOffsetUpdate(anyInt());
        verify(bar, times(2)).onBrowserControlsOffsetUpdate(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    @DisableFeatures(ChromeFeatureList.BOTTOM_CONTROLS_JANK_IMPROVEMENT)
    public void testRequestLayerUpdate_hiddenSheetReshown_barAlreadyPinned_jankDisabled() {
        simulateBrowserControlsManagerHeightDispatch();
        TestLayer miniPlayer =
                spy(
                        new TestLayer(
                                LayerType.READ_ALOUD_PLAYER,
                                50,
                                LayerScrollBehavior.NEVER_SCROLL_OFF,
                                LayerVisibility.VISIBLE));
        TestLayer bar =
                spy(
                        new TestLayer(
                                LayerType.BOTTOM_APP_BAR,
                                100,
                                LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                                LayerVisibility.VISIBLE));
        TestLayer sheet =
                spy(
                        new TestLayer(
                                LayerType.BOTTOM_SHEET,
                                0,
                                LayerScrollBehavior.NEVER_SCROLL_OFF,
                                LayerVisibility.HIDDEN));
        mBottomControlsStacker.addLayer(sheet);
        mBottomControlsStacker.addLayer(miniPlayer);
        mBottomControlsStacker.addLayer(bar);
        // All layers are pinned and at rest.
        doReturn(150).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(150, 150);
        // Hidden layers are dispatched their height.
        verify(sheet, times(1)).onBrowserControlsOffsetUpdate(0);
        verify(miniPlayer, times(1)).onBrowserControlsOffsetUpdate(-100);
        verify(bar, times(1)).onBrowserControlsOffsetUpdate(0);

        // The re-show reposition gives the sheet its visible position right away; the other layers
        // receive the same offsets they already had.
        sheet.setVisibility(LayerVisibility.VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer, times(2)).setBottomControlsHeight(150, 150);
        verify(sheet, times(1)).onBrowserControlsOffsetUpdate(-150);
        verify(miniPlayer, times(2)).onBrowserControlsOffsetUpdate(-100);
        verify(bar, times(2)).onBrowserControlsOffsetUpdate(0);
    }

    @Test
    public void testRepositionLayers_doesNotLogMismatchWhenLayersConsistent() {
        TestLayer bottomBar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        180,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer chin =
                new TestLayer(
                        LayerType.BOTTOM_CHIN,
                        72,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(bottomBar);
        mBottomControlsStacker.addLayer(chin);
        mBottomControlsStacker.requestLayerUpdate(false);

        ShadowLog.reset();
        // Reposition with offsets applied by browser.
        onBottomControlsOffsetChanged(0, 252, false, true);

        List<LogItem> logs = ShadowLog.getLogsForTag("cr_BotControlsStacker");
        assertTrue(
                "No height mismatch warning should be logged when layers are consistent.",
                logs.stream().noneMatch(item -> item.msg.contains("Height mismatch observed")));
    }

    @Test
    public void testRepositionLayers_logsMismatchWhenLayerHeightChangesWithoutExplicitRequest() {
        TestLayer bottomBar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        180,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer chin =
                new TestLayer(
                        LayerType.BOTTOM_CHIN,
                        72,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);

        mBottomControlsStacker.addLayer(bottomBar);
        mBottomControlsStacker.addLayer(chin);
        mBottomControlsStacker.requestLayerUpdate(false);

        // Chin height changes without requestLayerUpdate (e.g. 72 -> 0).
        chin.setHeight(0);

        ShadowLog.reset();
        onBottomControlsOffsetChanged(0, 252, false, true);

        List<LogItem> mismatchLogs =
                ShadowLog.getLogsForTag("cr_BotControlsStacker").stream()
                        .filter(item -> item.msg.contains("Height mismatch observed"))
                        .toList();
        assertEquals(
                "Exactly one height mismatch warning should be logged with final totals.",
                1,
                mismatchLogs.size());
        LogItem log = mismatchLogs.get(0);
        assertEquals(Log.WARN, log.type);
        assertTrue(log.msg.contains("expectedHeight= 252"));
        assertTrue(log.msg.contains("expectedMinHeight= 252"));
        assertTrue(log.msg.contains("actualHeight = 180"));
        assertTrue(log.msg.contains("actualMinHeight= 180"));
    }

    @Test
    public void testRepositionLayers_beforeFirstRequestLayerUpdate_doesNotLogMismatch() {
        TestLayer bottomBar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        180,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bottomBar);

        ShadowLog.reset();
        // Reposition before any requestLayerUpdate has initialized mTotalHeight.
        onBottomControlsOffsetChanged(0, 0, false, true);

        List<LogItem> logs = ShadowLog.getLogsForTag("cr_BotControlsStacker");
        assertTrue(
                "No height mismatch warning should be logged before first requestLayerUpdate.",
                logs.stream().noneMatch(item -> item.msg.contains("Height mismatch observed")));
    }

    @Test
    public void testRequestLayerUpdate_offsetOverriddenMidScroll_doesNotBakeOffsetIntoYOffset() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                null, new BrowserControlsOffsetTagsInfo(), BrowserControlsState.SHOWN, false);

        TestLayer bar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        60,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.requestLayerUpdate(false);

        for (int bottomOffset : new int[] {0, 15, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }

        // Trigger requestLayerUpdate(false) mid-scroll at offset 30.
        doReturn(30).when(mBrowserControlsSizer).getBottomControlOffset();
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        mBottomControlsStacker.requestLayerUpdate(false);
        assertLayerYOffset(bar, 0);

        for (int bottomOffset : new int[] {45, 60, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }
    }

    @Test
    public void
            testRequestLayerUpdate_offsetOverriddenMidScroll_withChin_doesNotBakeOffsetIntoYOffset() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                null, new BrowserControlsOffsetTagsInfo(), BrowserControlsState.SHOWN, false);

        TestLayer bar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        60,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer chin =
                new TestLayer(
                        LayerType.BOTTOM_CHIN,
                        48,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.addLayer(chin);
        mBottomControlsStacker.requestLayerUpdate(false);

        for (int bottomOffset : new int[] {0, 15, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, -48);
            assertLayerYOffset(chin, 0);
        }

        // Trigger requestLayerUpdate(false) mid-scroll at offset 30.
        doReturn(30).when(mBrowserControlsSizer).getBottomControlOffset();
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        mBottomControlsStacker.requestLayerUpdate(false);
        assertLayerYOffset(bar, -48);
        assertLayerYOffset(chin, 0);

        for (int bottomOffset : new int[] {45, 60, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, -48);
            assertLayerYOffset(chin, 0);
        }
    }

    @Test
    public void
            testRequestLayerUpdate_offsetOverriddenMidScroll_chinHides_updatesToRestingOffset() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                null, new BrowserControlsOffsetTagsInfo(), BrowserControlsState.SHOWN, false);

        TestLayer bar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        60,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer chin =
                new TestLayer(
                        LayerType.BOTTOM_CHIN,
                        48,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.addLayer(chin);
        mBottomControlsStacker.requestLayerUpdate(false);

        for (int bottomOffset : new int[] {0, 15, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, -48);
            assertLayerYOffset(chin, 0);
        }

        // Trigger requestLayerUpdate(false) mid-scroll at offset 30 with chin hidden.
        doReturn(30).when(mBrowserControlsSizer).getBottomControlOffset();
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        chin.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.requestLayerUpdate(false);
        assertLayerYOffset(bar, 0);

        for (int bottomOffset : new int[] {45, 60, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }
    }

    @Test
    public void
            testRequestLayerUpdate_offsetOverriddenMidScroll_chinShows_updatesToRestingOffset() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                null, new BrowserControlsOffsetTagsInfo(), BrowserControlsState.SHOWN, false);

        TestLayer bar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        60,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer chin =
                new TestLayer(
                        LayerType.BOTTOM_CHIN,
                        48,
                        LayerScrollBehavior.NEVER_SCROLL_OFF,
                        LayerVisibility.HIDDEN);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.addLayer(chin);
        mBottomControlsStacker.requestLayerUpdate(false);

        for (int bottomOffset : new int[] {0, 15, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }

        // Trigger requestLayerUpdate(false) mid-scroll at offset 30 with chin shown.
        doReturn(30).when(mBrowserControlsSizer).getBottomControlOffset();
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        chin.setVisibility(LayerVisibility.VISIBLE);
        mBottomControlsStacker.requestLayerUpdate(false);
        assertLayerYOffset(bar, -48);
        assertLayerYOffset(chin, 0);

        for (int bottomOffset : new int[] {45, 60, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, -48);
            assertLayerYOffset(chin, 0);
        }
    }

    @Test
    public void
            testOnBottomControlsHeightChanged_offsetOverriddenMidScroll_doesNotBakeOffsetIntoYOffset() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        doReturn(false).when(mBrowserControlsSizer).shouldAnimateBrowserControlsHeightChanges();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                null, new BrowserControlsOffsetTagsInfo(), BrowserControlsState.SHOWN, false);

        TestLayer bar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        60,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.requestLayerUpdate(false);

        for (int bottomOffset : new int[] {0, 15, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }

        // Trigger onBottomControlsHeightChanged mid-scroll at offset 30.
        doReturn(30).when(mBrowserControlsSizer).getBottomControlOffset();
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        mBottomControlsStacker.onBottomControlsHeightChanged(60, 0);
        assertLayerYOffset(bar, 0);

        for (int bottomOffset : new int[] {45, 60, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }
    }

    @Test
    public void
            testOnOffsetTagsInfoChanged_offsetOverriddenMidScroll_doesNotBakeOffsetIntoYOffset() {
        doReturn(true).when(mBrowserControlsSizer).offsetOverridden();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                null, new BrowserControlsOffsetTagsInfo(), BrowserControlsState.SHOWN, false);

        TestLayer bar =
                new TestLayer(
                        LayerType.BOTTOM_APP_BAR,
                        60,
                        LayerScrollBehavior.DEFAULT_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(bar);
        mBottomControlsStacker.requestLayerUpdate(false);

        for (int bottomOffset : new int[] {0, 15, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }

        // Trigger onOffsetTagsInfoChanged with shouldUpdateOffsets=true mid-scroll at offset 30.
        doReturn(30).when(mBrowserControlsSizer).getBottomControlOffset();
        doReturn(0).when(mBrowserControlsSizer).getBottomControlsMinHeightOffset();
        mBottomControlsStacker.onOffsetTagsInfoChanged(
                new BrowserControlsOffsetTagsInfo(),
                new BrowserControlsOffsetTagsInfo(),
                BrowserControlsState.SHOWN,
                /* shouldUpdateOffsets= */ true);
        assertLayerYOffset(bar, 0);

        for (int bottomOffset : new int[] {45, 60, 30}) {
            onBottomControlsOffsetChanged(
                    bottomOffset,
                    /* bottomControlsMinHeightOffset= */ 0,
                    /* requestNewFrame= */ true,
                    /* isVisibilityForced= */ true);
            assertLayerYOffset(bar, 0);
        }
    }
}
