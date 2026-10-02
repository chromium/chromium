// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.content.Context;
import android.graphics.Rect;
import android.view.Surface;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.appmenu.internal.R;

/** Tests AppMenu#getPopupPosition. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppMenuPopupPositionTest {

    private final int[] mTempLocation = new int[2];

    private static final int APP_WIDTH = 400;
    private static final int APP_HEIGHT = 1000;
    private static final int BG_PADDING = 10;
    private static final int POPUP_WIDTH = 300;
    private static final int POPUP_HEIGHT = 500;
    private static final int ANCHOR_X = 100;
    private static final int ANCHOR_Y = 300;
    private static final int ANCHOR_WIDTH = 40;
    private static final int NEGATIVE_SOFTWARE_VERTICAL_OFFSET = 25;
    private final Rect mAppRect = new Rect(0, 0, APP_WIDTH, APP_HEIGHT);
    private final Rect mBgPaddingRect = new Rect(BG_PADDING, BG_PADDING, BG_PADDING, BG_PADDING);
    private View mAnchorView;
    private int mBottomBarMargin;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mBottomBarMargin =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.bottom_bar_app_menu_lateral_margin);

        // Attach the anchor to a window so that getLocationInWindow() reflects its position.
        FrameLayout root = new FrameLayout(context);
        mAnchorView = new View(context);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(ANCHOR_WIDTH, 0);
        params.leftMargin = ANCHOR_X;
        params.topMargin = ANCHOR_Y;
        root.addView(mAnchorView, params);
        context.getSystemService(WindowManager.class)
                .addView(root, new WindowManager.LayoutParams());
        ShadowLooper.idleMainLooper();
    }

    @Test
    public void testPermanentButton_Portrait() {
        // Popup should should be in the middle of the screen, anchored near the anchor view.
        int expectedX = (APP_WIDTH - POPUP_WIDTH) / 2;
        int expectedY = ANCHOR_Y - BG_PADDING;

        int[] results =
                getPopupPosition(true, false, Surface.ROTATION_0, View.LAYOUT_DIRECTION_LTR, false);
        Assert.assertEquals("Incorrect popup x", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y", expectedY, results[1]);

        results =
                getPopupPosition(
                        true, false, Surface.ROTATION_180, View.LAYOUT_DIRECTION_LTR, false);
        Assert.assertEquals("Incorrect popup x for rotation 180", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y for rotation 180", expectedY, results[1]);
    }

    @Test
    public void testPermanentButton_Landscape() {
        int[] results =
                getPopupPosition(
                        true, false, Surface.ROTATION_90, View.LAYOUT_DIRECTION_LTR, false);

        // Popup should be positioned toward the right edge of the screen, anchored near the anchor
        // view.
        int expectedX = APP_WIDTH - POPUP_WIDTH;
        int expectedY = ANCHOR_Y - BG_PADDING;
        Assert.assertEquals("Incorrect popup x", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y", expectedY, results[1]);

        // Popup should be positioned toward the left edge of the screen, anchored near the anchor
        // view.
        expectedX = 0;
        results =
                getPopupPosition(
                        true, false, Surface.ROTATION_270, View.LAYOUT_DIRECTION_LTR, false);
        Assert.assertEquals("Incorrect popup x for rotation 180", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y for rotation 180", expectedY, results[1]);
    }

    @Test
    public void testTopButton_LTR() {
        int[] results =
                getPopupPosition(
                        false, false, Surface.ROTATION_0, View.LAYOUT_DIRECTION_LTR, false);

        // The top right edge of the popup should be aligned with the top right edge of the button.
        int expectedX = ANCHOR_X + ANCHOR_WIDTH - POPUP_WIDTH;
        int expectedY = ANCHOR_Y - NEGATIVE_SOFTWARE_VERTICAL_OFFSET;
        Assert.assertEquals("Incorrect popup x", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y", expectedY, results[1]);
    }

    @Test
    public void testTopButton_RTL() {
        int[] results =
                getPopupPosition(
                        false, false, Surface.ROTATION_0, View.LAYOUT_DIRECTION_RTL, false);

        // The top left edge of the popup should be aligned with the top left edge of the button.
        int expectedX = ANCHOR_X;
        int expectedY = ANCHOR_Y - NEGATIVE_SOFTWARE_VERTICAL_OFFSET;
        Assert.assertEquals("Incorrect popup x", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y", expectedY, results[1]);
    }

    @Test
    public void testBottomButton_LTR() {
        int[] results =
                getPopupPosition(false, true, Surface.ROTATION_0, View.LAYOUT_DIRECTION_LTR, false);

        int expectedX = APP_WIDTH - mBottomBarMargin - POPUP_WIDTH + BG_PADDING;
        int expectedY = ANCHOR_Y - POPUP_HEIGHT + BG_PADDING;
        Assert.assertEquals("Incorrect popup x", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y", expectedY, results[1]);
    }

    @Test
    public void testBottomButton_RTL() {
        int[] results =
                getPopupPosition(false, true, Surface.ROTATION_0, View.LAYOUT_DIRECTION_RTL, false);

        int expectedX = mBottomBarMargin - BG_PADDING;
        int expectedY = ANCHOR_Y - POPUP_HEIGHT + BG_PADDING;
        Assert.assertEquals("Incorrect popup x", expectedX, results[0]);
        Assert.assertEquals("Incorrect popup y", expectedY, results[1]);
    }

    private int[] getPopupPosition(
            boolean isByPermanentButton,
            boolean isFromBottomBar,
            int rotation,
            int layoutDirection,
            boolean mPositionBelowAnchor) {
        return AppMenu.getPopupPosition(
                mTempLocation,
                isByPermanentButton,
                isFromBottomBar,
                NEGATIVE_SOFTWARE_VERTICAL_OFFSET,
                rotation,
                mAppRect,
                mBgPaddingRect,
                mAnchorView,
                POPUP_WIDTH,
                POPUP_HEIGHT,
                layoutDirection,
                mPositionBelowAnchor);
    }
}
