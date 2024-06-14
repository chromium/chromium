// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for the BrowserStateBrowserControlsVisibilityDelegate. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLooper.class})
@EnableFeatures(ChromeFeatureList.BOTTOM_BROWSER_CONTROLS_REFACTOR)
public class BottomControlsStackerUnitTest {
    private static final @LayerType int TOP_LAYER = LayerType.BOTTOM_TOOLBAR;
    private static final @LayerType int BOTTOM_LAYER = LayerType.READ_ALOUD_PLAYER;

    @Mock BrowserControlsSizer mBrowserControlsSizer;

    private BottomControlsStacker mBottomControlsStacker;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        mBottomControlsStacker = new BottomControlsStacker(mBrowserControlsSizer);
    }

    @Test
    public void singleLayerScrollOff() {
        TestLayer layer = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 0);
    }

    @Test
    public void singleLayerNoScrollOff() {
        TestLayer layer = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.NO_SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(100, 100);
    }

    @Test
    public void singleLayerNotVisible() {
        TestLayer layer = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.SCROLL_OFF);
        layer.setVisible(false);
        mBottomControlsStacker.addLayer(layer);
        mBottomControlsStacker.requestLayerUpdate(false);
        verify(mBrowserControlsSizer).setBottomControlsHeight(0, 0);
    }

    @Test
    public void stackLayerBothScrollOff() {
        TestLayer layer1 = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.SCROLL_OFF);
        TestLayer layer2 = new TestLayer(BOTTOM_LAYER, 10, LayerScrollBehavior.SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 0);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);
    }

    @Test
    public void stackLayerBothNoScrollOff() {
        TestLayer layer1 = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.NO_SCROLL_OFF);
        TestLayer layer2 = new TestLayer(BOTTOM_LAYER, 10, LayerScrollBehavior.NO_SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 110);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);
    }

    @Test
    public void stackLayerOneScrollOff() {
        TestLayer layer1 = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.SCROLL_OFF);
        TestLayer layer2 = new TestLayer(BOTTOM_LAYER, 10, LayerScrollBehavior.NO_SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);
        mBottomControlsStacker.requestLayerUpdate(true);

        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 10);
        verify(mBrowserControlsSizer).setAnimateBrowserControlsHeightChanges(true);
    }

    @Test(expected = AssertionError.class)
    public void stackLayerInvalidScrollBehavior() {
        TestLayer layer1 = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.NO_SCROLL_OFF);
        TestLayer layer2 = new TestLayer(BOTTOM_LAYER, 10, LayerScrollBehavior.SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);

        // Cannot have bottom layer scroll off while the top layer does not.
        mBottomControlsStacker.requestLayerUpdate(true);
    }

    @Test
    public void stackLayerChangeHeight() {
        TestLayer layer1 = new TestLayer(TOP_LAYER, 100, LayerScrollBehavior.SCROLL_OFF);
        TestLayer layer2 = new TestLayer(BOTTOM_LAYER, 10, LayerScrollBehavior.NO_SCROLL_OFF);
        mBottomControlsStacker.addLayer(layer1);
        mBottomControlsStacker.addLayer(layer2);

        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(110, 10);

        layer1.setHeight(1000);
        layer2.setHeight(9);
        mBottomControlsStacker.requestLayerUpdate(true);
        verify(mBrowserControlsSizer).setBottomControlsHeight(1009, 9);
    }

    // Test helpers

    private static class TestLayer implements BottomControlsLayer {
        private final @LayerType int mType;
        private final @LayerScrollBehavior int mScrollBehavior;
        private int mHeight;
        private boolean mIsVisible;

        TestLayer(@LayerType int type, int height, @LayerScrollBehavior int scrollBehavior) {
            mType = type;
            mHeight = height;
            mScrollBehavior = scrollBehavior;
            mIsVisible = true;
        }

        public void setVisible(boolean visible) {
            mIsVisible = visible;
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
        public boolean isVisible() {
            return mIsVisible;
        }

        @Override
        public @LayerType int getType() {
            return mType;
        }
    }
}
