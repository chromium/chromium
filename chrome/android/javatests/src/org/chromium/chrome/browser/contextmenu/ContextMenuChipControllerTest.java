// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.TimeoutException;

/** Tests for {@link ContextMenuChipController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContextMenuChipControllerTest {
    // Epsilon value for assertions. Comparing raw pixel values can be error-prone due to
    // float-to-int conversions and rounding. Using an epsilon accounts for these minor
    // discrepancies. For example, calculating a total size by summing rounded parts can differ from
    // rounding the sum of the parts:
    //
    // round(3 dp * 1.2) + round(4 dp * 1.2) = round(3.6) + round(4.8) = 4 + 5 = 9 px
    // vs
    // round((3 dp + 4 dp) * 1.2) = round(7dp + 1.2) = round(8.4) = 8 px
    private static final float EPSILON_PX = 2.0f;

    // This is the combination of the expected vertical margins and the chip height.
    private static final int EXPECTED_VERTICAL_DP = 80;
    // Computed by taking the 338dp max width and subtracting:
    // 16 (chip start padding)
    // 24 (main icon width)
    // 8 (text start padding)
    // 16 (close button start padding)
    // 24 (close button icon width)
    // 16 (close button end padding)
    private static final int EXPECTED_CHIP_WIDTH_DP = 234;
    // Computed by taking the 338dp max width and subtracting:
    // 16 (chip start padding)
    // 24 (main icon width)
    // 8 (text start padding)
    private static final int EXPECTED_CHIP_NO_END_BUTTON_WIDTH_DP = 290;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private final CallbackHelper mChipClickCallbackHelper = new CallbackHelper();
    private final CallbackHelper mDismissCallbackHelper = new CallbackHelper();

    private static Activity sActivity;

    private float mMeasuredDeviceDensity;
    private View mAnchorView;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        mMeasuredDeviceDensity = sActivity.getResources().getDisplayMetrics().density;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.context_menu_fullscreen_container);
                    mAnchorView = sActivity.findViewById(R.id.context_menu_chip_anchor_point);
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @SmallTest
    public void testDismissChipWhenNotShownBeforeClassificationReturned() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(
                        sActivity, mAnchorView, mDismissCallbackHelper::notifyCalled);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    chipController.dismissChipIfShowing();
                });

        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNull(
                "Popup window was initialized unexpectedly.",
                chipController.getCurrentPopupWindowForTesting());
    }

    @Test
    @SmallTest
    public void testDismissChipWhenShown() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(
                        sActivity, mAnchorView, mDismissCallbackHelper::notifyCalled);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChipRenderParams chipRenderParams = new ChipRenderParams();
                    chipRenderParams.titleResourceId =
                            R.string.contextmenu_translate_image_with_google_lens;
                    chipRenderParams.iconResourceId = R.drawable.lens_icon;
                    chipRenderParams.onClickCallback = mChipClickCallbackHelper::notifyCalled;
                    chipController.showChip(chipRenderParams);
                    chipController.dismissChipIfShowing();
                });

        assertEquals(0, mDismissCallbackHelper.getCallCount());
        assertEquals(0, mChipClickCallbackHelper.getCallCount());
        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNotNull(
                "Popup window was not initialized.",
                chipController.getCurrentPopupWindowForTesting());
        assertFalse(
                "Popup window was showing unexpectedly.",
                chipController.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    public void testClickChipWhenShown() throws TimeoutException {
        ContextMenuChipController chipController =
                new ContextMenuChipController(
                        sActivity, mAnchorView, mDismissCallbackHelper::notifyCalled);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChipRenderParams chipRenderParams = new ChipRenderParams();
                    chipRenderParams.titleResourceId =
                            R.string.contextmenu_translate_image_with_google_lens;
                    chipRenderParams.iconResourceId = R.drawable.lens_icon;
                    chipRenderParams.onClickCallback = mChipClickCallbackHelper::notifyCalled;
                    chipController.showChip(chipRenderParams);
                    chipController.clickChipForTesting();
                });

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mDismissCallbackHelper.waitForOnly();
        mChipClickCallbackHelper.waitForOnly();
        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNotNull(
                "Popup window was not initialized.",
                chipController.getCurrentPopupWindowForTesting());
        assertTrue(
                "Dismiss callback does not dismiss the popup window, so it should still be"
                        + " showing.",
                chipController.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    public void testExpectedVerticalPxNeededForChip() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(
                        sActivity, mAnchorView, mDismissCallbackHelper::notifyCalled);
        assertEquals(
                "Vertical px is not matching the expectation",
                EXPECTED_VERTICAL_DP * mMeasuredDeviceDensity,
                chipController.getVerticalPxNeededForChip(),
                EPSILON_PX);
    }

    @Test
    @SmallTest
    public void testExpectedChipTextMaxWidthPx() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(
                        sActivity, mAnchorView, mDismissCallbackHelper::notifyCalled);
        assertEquals(
                "Chip width px is not matching the expectation",
                EXPECTED_CHIP_WIDTH_DP * mMeasuredDeviceDensity,
                chipController.getChipTextMaxWidthPx(false),
                EPSILON_PX);
    }

    @Test
    @SmallTest
    public void testExpectedChipTextMaxWidthPx_EndButtonHidden() {
        ContextMenuChipController chipController =
                new ContextMenuChipController(
                        sActivity, mAnchorView, mDismissCallbackHelper::notifyCalled);
        assertEquals(
                "Chip width px is not matching the expectation",
                EXPECTED_CHIP_NO_END_BUTTON_WIDTH_DP * mMeasuredDeviceDensity,
                chipController.getChipTextMaxWidthPx(true),
                EPSILON_PX);
    }
}
