// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for the BrowserStateBrowserControlsVisibilityDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLooper.class})
@EnableFeatures(ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR)
public class BottomControlsStackerUnitTest {
    private static final @LayerType int TOP_LAYER = LayerType.TABSTRIP_TOOLBAR;
    private static final @LayerType int MID_LAYER = LayerType.READ_ALOUD_PLAYER;
    private static final @LayerType int BOTTOM_LAYER = LayerType.TEST_BOTTOM_LAYER;

    @Mock BrowserControlsSizer mBrowserControlsSizer;

    private BottomControlsStacker mBottomControlsStacker;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        mBottomControlsStacker = new BottomControlsStacker(mBrowserControlsSizer);

        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.sDisableBottomControlsStackerYOffsetDispatching, "false");
        testValues.addFeatureFlagOverride(ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR, true);
        FeatureList.setTestValues(testValues);
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);
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
    }

    @Test
    public void singleLayerNotVisible() {
        TestLayer layer =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        layer.setVisibility(LayerVisibility.HIDDEN);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);
    }

    @Test
    public void stackLayerBothScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer layer2 =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);
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
    }

    @Test
    public void stackLayerOneScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
    }

    @Test
    public void stackLayerDefaultNoScrollOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
    }

    @Test
    public void stackLayerDefaultScrollsOff() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
    }

    @Test(expected = AssertionError.class)
    public void stackLayerInvalidScrollBehavior() {
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);

        // Cannot have bottom layer scroll off while the top layer does not.
        mBottomControlsStacker.requestLayerUpdate(true);
    }

    @Test
    public void stackLayerChangeHeight() {
        TestLayer layer1 =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
    public void reposition_ScrollOff_OneLayer() {
        TestLayer layer =
                new TestLayer(
                        BOTTOM_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(layer);

        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);

        // Controls fully visible
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(layer, 0);

        // Scroll down.
        onBottomControlsOffsetChanged(60, 0, false);
        assertLayerYOffset(layer, 60);

        // Controls Full scroll off.
        onBottomControlsOffsetChanged(100, 0, false);
        assertLayerYOffset(layer, 100);

        // Scroll up.
        onBottomControlsOffsetChanged(30, 0, false);
        assertLayerYOffset(layer, 30);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(layer, 0);
    }

    @Test
    public void reposition_ScrollOff_TwoLayers() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);

        // Browser controls fully shown.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Bottom layer partially scrolled off.
        onBottomControlsOffsetChanged(5, 0, false);
        assertLayerYOffset(top, -5);
        assertLayerYOffset(bottom, 5);

        // Bottom layer scrolled off, top layer partially scrolled off
        onBottomControlsOffsetChanged(50, 0, false);
        assertLayerYOffset(top, 40);
        assertLayerYOffset(bottom, 10);

        // Fully scroll off.
        onBottomControlsOffsetChanged(110, 0, false);
        assertLayerYOffset(top, 100);
        assertLayerYOffset(bottom, 10);

        // Scroll back up. Top layer moves first.
        onBottomControlsOffsetChanged(40, 0, false);
        assertLayerYOffset(top, 30);
        assertLayerYOffset(bottom, 10);

        // Scroll back to browser controls fully shown.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_ScrollOff_ThreeLayers() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        1000,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        mBottomControlsStacker.addLayer(top);
        mBottomControlsStacker.addLayer(mid);
        mBottomControlsStacker.addLayer(bottom);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1110, 0);

        // Browser controls fully shown.
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, -110);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Bottom layer partially scrolled off.
        onBottomControlsOffsetChanged(5, 0, false);
        assertLayerYOffset(top, -105);
        assertLayerYOffset(mid, -5);
        assertLayerYOffset(bottom, 5);

        // Bottom layer scrolled off, mid layer partially scrolled off.
        onBottomControlsOffsetChanged(50, 0, false);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, 40);
        assertLayerYOffset(bottom, 10);

        // Bottom and min layer both scrolled off, top layer partially scrolled off.
        onBottomControlsOffsetChanged(500, 0, false);
        assertLayerYOffset(top, 390);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 10);

        // All layers fully scroll off.
        onBottomControlsOffsetChanged(1110, 0, false);
        assertLayerYOffset(top, 1000);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 10);

        // Scroll back up. Top layer moves first.
        onBottomControlsOffsetChanged(900, 0, false);
        assertLayerYOffset(top, 790);
        assertLayerYOffset(mid, 100);
        assertLayerYOffset(bottom, 10);

        // Scroll back the mid layer start showing.
        onBottomControlsOffsetChanged(90, 0, false);
        assertLayerYOffset(top, -20);
        assertLayerYOffset(mid, 80);
        assertLayerYOffset(bottom, 10);

        // Scroll back the bottom layer start showing.
        onBottomControlsOffsetChanged(9, 0, false);
        assertLayerYOffset(top, -101);
        assertLayerYOffset(mid, -1);
        assertLayerYOffset(bottom, 9);

        // Full visible.
        onBottomControlsOffsetChanged(0, 0, false);
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
    public void reposition_Mixed_TwoLayers_BottomLayerNoScroll() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
        onBottomControlsOffsetChanged(0, 10, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling down.
        onBottomControlsOffsetChanged(5, 10, false);
        assertLayerYOffset(top, -5);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling down.
        onBottomControlsOffsetChanged(50, 10, false);
        assertLayerYOffset(top, 40);
        assertLayerYOffset(bottom, 0);

        // Top controls fully scroll off.
        onBottomControlsOffsetChanged(100, 10, false);
        assertLayerYOffset(top, 90);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling back up.
        onBottomControlsOffsetChanged(80, 10, false);
        assertLayerYOffset(top, 70);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling up.
        onBottomControlsOffsetChanged(5, 10, false);
        assertLayerYOffset(top, -5);
        assertLayerYOffset(bottom, 0);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 10, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_Mixed_ThreeLayers_DefaultScrollUnderNeverScroll() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
        onBottomControlsOffsetChanged(0, 60, false);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling down.
        onBottomControlsOffsetChanged(5, 60, false);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling down.
        onBottomControlsOffsetChanged(50, 60, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Top controls fully scroll off.
        onBottomControlsOffsetChanged(100, 60, false);
        assertLayerYOffset(top, 40);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling back up.
        onBottomControlsOffsetChanged(80, 60, false);
        assertLayerYOffset(top, 20);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling up.
        onBottomControlsOffsetChanged(5, 60, false);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 60, false);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -10);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_Mixed_ThreeLayers_DefaultScrollAboveNeverScroll() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
        onBottomControlsOffsetChanged(0, 50, false);
        assertLayerYOffset(top, -60);
        assertLayerYOffset(mid, -50);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling down.
        onBottomControlsOffsetChanged(5, 50, false);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -45);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling down.
        onBottomControlsOffsetChanged(50, 50, false);
        assertLayerYOffset(top, -10);
        assertLayerYOffset(mid, -40);
        assertLayerYOffset(bottom, 0);

        // Top controls fully scroll off.
        onBottomControlsOffsetChanged(110, 50, false);
        assertLayerYOffset(top, 50);
        assertLayerYOffset(mid, -40);
        assertLayerYOffset(bottom, 0);

        // Starts scrolling back up.
        onBottomControlsOffsetChanged(80, 50, false);
        assertLayerYOffset(top, 20);
        assertLayerYOffset(mid, -40);
        assertLayerYOffset(bottom, 0);

        // Keep scrolling up.
        onBottomControlsOffsetChanged(5, 50, false);
        assertLayerYOffset(top, -55);
        assertLayerYOffset(mid, -45);
        assertLayerYOffset(bottom, 0);

        // Controls fully visible.
        onBottomControlsOffsetChanged(0, 50, false);
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
        onBottomControlsOffsetChanged(0, 0, false);
        assertLayerYOffset(top, 100);
        assertLayerYOffset(bottom, 0);
    }

    @Test
    public void reposition_RemoveLayer_RemovedBottom() {
        TestLayer top =
                new TestLayer(
                        TOP_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer mid =
                new TestLayer(
                        MID_LAYER,
                        100,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
                        LayerVisibility.VISIBLE);
        TestLayer bottom =
                new TestLayer(
                        BOTTOM_LAYER,
                        10,
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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
                        LayerScrollBehavior.ALWAYS_SCROLL_OFF,
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

    // Test helpers

    private void onBottomControlsOffsetChanged(
            int bottomControlsOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        mBottomControlsStacker.onControlsOffsetChanged(
                0, 0, bottomControlsOffset, bottomControlsMinHeightOffset, needsAnimate, false);
    }

    private void assertLayerYOffset(TestLayer layer, int expectedOffset) {
        assertEquals("Different yOffset observed.", expectedOffset, layer.mYOffset);
    }

    private static class TestLayer implements BottomControlsLayer {
        private final @LayerType int mType;
        private final @LayerScrollBehavior int mScrollBehavior;
        private int mHeight;
        private @LayerVisibility int mVisibility;
        private int mYOffset;

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
    }
}
