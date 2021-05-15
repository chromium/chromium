// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Tests for RevampedContextMenuHeader view and {@link RevampedContextMenuHeaderViewBinder}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class RevampedContextMenuChipControllerTest extends DummyUiActivityTestCase {
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

    private final Runnable mEmptyChipClickCallbackForTesting = () -> {
        return;
    };

    private float mMeasuredDeviceDensity;
    private View mAnchorView;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        DummyUiActivity.setTestLayout(R.layout.context_menu_fullscreen_container);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        mMeasuredDeviceDensity = getActivity().getResources().getDisplayMetrics().density;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAnchorView = getActivity().findViewById(R.id.context_menu_chip_anchor_point);
        });
    }

    @Test
    @SmallTest
    public void testChipShownWhenCallbackReturnsChipRenderParams() {
        RevampedContextMenuChipController chipController =
                new RevampedContextMenuChipController(getActivity(), mAnchorView, () -> {});
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChipRenderParams chipRenderParams = new ChipRenderParams();
            chipRenderParams.titleResourceId = R.string.contextmenu_shop_image_with_google_lens;
            chipRenderParams.iconResourceId = R.drawable.lens_icon;
            chipRenderParams.onClickCallback = mEmptyChipClickCallbackForTesting;
            chipController.showChip(chipRenderParams);
        });

        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNotNull("Popup window was not initialized.",
                chipController.getCurrentPopupWindowForTesting());
        assertTrue("Popup window not showing.",
                chipController.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    public void testDismissChipWhenNotShownBeforeClassificationReturned() {
        RevampedContextMenuChipController chipController =
                new RevampedContextMenuChipController(getActivity(), mAnchorView, () -> {});
        TestThreadUtils.runOnUiThreadBlocking(() -> { chipController.dismissChipIfShowing(); });

        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNull("Popup window was initialized unexpectedly.",
                chipController.getCurrentPopupWindowForTesting());
    }

    @Test
    @SmallTest
    public void testDismissChipWhenShown() {
        RevampedContextMenuChipController chipController =
                new RevampedContextMenuChipController(getActivity(), mAnchorView, () -> {});
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChipRenderParams chipRenderParams = new ChipRenderParams();
            chipRenderParams.titleResourceId = R.string.contextmenu_shop_image_with_google_lens;
            chipRenderParams.iconResourceId = R.drawable.lens_icon;
            chipRenderParams.onClickCallback = mEmptyChipClickCallbackForTesting;
            chipController.showChip(chipRenderParams);
            chipController.dismissChipIfShowing();
        });

        assertNotNull("Anchor view was not initialized.", mAnchorView);
        assertNotNull("Popup window was initialized unexpectedly.",
                chipController.getCurrentPopupWindowForTesting());
        assertFalse("Popup window showing unexpectedly.",
                chipController.getCurrentPopupWindowForTesting().isShowing());
    }

    @Test
    @SmallTest
    public void testExpectedVerticalPxNeededForChip() {
        RevampedContextMenuChipController chipController =
                new RevampedContextMenuChipController(getActivity(), mAnchorView, () -> {});
        assertEquals("Vertical px is not matching the expectation",
                (int) (EXPECTED_VERTICAL_DP * mMeasuredDeviceDensity),
                chipController.getVerticalPxNeededForChip());
    }

    @Test
    @SmallTest
    public void testExpectedChipTextMaxWidthPx() {
        RevampedContextMenuChipController chipController =
                new RevampedContextMenuChipController(getActivity(), mAnchorView, () -> {});
        assertEquals("Chip width px is not matching the expectation",
                (int) (EXPECTED_CHIP_WIDTH_DP * mMeasuredDeviceDensity),
                chipController.getChipTextMaxWidthPx(false));
    }

    @Test
    @SmallTest
    public void testExpectedChipTextMaxWidthPx_EndButtonHidden() {
        RevampedContextMenuChipController chipController =
                new RevampedContextMenuChipController(getActivity(), mAnchorView, () -> {});
        assertEquals("Chip width px is not matching the expectation",
                (int) (EXPECTED_CHIP_NO_END_BUTTON_WIDTH_DP * mMeasuredDeviceDensity),
                chipController.getChipTextMaxWidthPx(true));
    }
}
