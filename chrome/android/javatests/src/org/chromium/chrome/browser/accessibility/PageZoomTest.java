// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/** Integration tests for Page Zoom. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({
    ChromeFeatureList.ANDROID_BOTTOM_BAR,
    ChromeFeatureList.BOTTOM_SHEET_AS_BROWSER_CONTROLS
})
@Restriction(DeviceFormFactor.PHONE)
@Batch(Batch.PER_CLASS)
public class PageZoomTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mSheetController;
    private BottomSheetTestSupport mTestSupport;
    private TestBottomSheetContent mBottomSheetContent;
    private boolean mActsAsBrowserControls;

    private ChromeTabbedActivity launchActivity(boolean landscape) throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        if (landscape) {
            ActivityTestUtils.rotateActivityToOrientation(
                    activity, Configuration.ORIENTATION_LANDSCAPE);
        } else {
            ActivityTestUtils.rotateActivityToOrientation(
                    activity, Configuration.ORIENTATION_PORTRAIT);
        }

        return activity;
    }

    private void setupTestEnvironment(ChromeTabbedActivity activity) {
        mSheetController = activity.getRootUiCoordinatorForTesting().getBottomSheetController();
        mTestSupport = new BottomSheetTestSupport(mSheetController);
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetContent =
                            new TestBottomSheetContent(
                                    activity,
                                    BottomSheetContent.ContentPriority.LOW,
                                    /* hasCustomLifecycle= */ false) {
                                @Override
                                public boolean actsAsBrowserControls() {
                                    return mActsAsBrowserControls;
                                }

                                // Use a fixed peek height of 100 pixels to guarantee a stable,
                                // non-zero bottom
                                // sheet offset for testing translation calculations across
                                // different device screen sizes.
                                @Override
                                public int getPeekHeight() {
                                    return 100;
                                }
                            };
                });
    }

    @Test
    @MediumTest
    public void testPageZoomVisibleAboveBottomSheetInLandscape() throws Exception {
        runPageZoomBarOffsetTest(/* landscape= */ true, /* actsAsBrowserControls= */ false);
    }

    @Test
    @MediumTest
    public void testPageZoomVisibleAboveBottomSheetInLandscape_actsAsBrowserControls()
            throws Exception {
        runPageZoomBarOffsetTest(/* landscape= */ true, /* actsAsBrowserControls= */ true);
    }

    @Test
    @MediumTest
    public void testPageZoomVisibleAboveBottomSheetInPortrait() throws Exception {
        runPageZoomBarOffsetTest(/* landscape= */ false, /* actsAsBrowserControls= */ false);
    }

    @Test
    @MediumTest
    public void testPageZoomVisibleAboveBottomSheetInPortrait_actsAsBrowserControls()
            throws Exception {
        runPageZoomBarOffsetTest(/* landscape= */ false, /* actsAsBrowserControls= */ true);
    }

    private void runPageZoomBarOffsetTest(boolean landscape, boolean actsAsBrowserControls)
            throws Exception {
        mActsAsBrowserControls = actsAsBrowserControls;
        final ChromeTabbedActivity activity = launchActivity(landscape);
        setupTestEnvironment(activity);
        try {
            // 2. Trigger a peeked bottom sheet.
            showContent(mBottomSheetContent, SheetState.PEEK);

            // Verify sheet is in PEEK state and offset is > 0.
            assertEquals(SheetState.PEEK, mSheetController.getSheetState());
            pollUiThread(() -> mSheetController.getCurrentOffset() > 0);

            // 3. Open the zoom UI.
            runOnUiThreadBlocking(
                    () -> {
                        activity.getRootUiCoordinatorForTesting()
                                .handleMenuOrKeyboardAction(
                                        R.id.page_zoom_id, /* fromMenu= */ true);
                    });

            // Verify zoom UI is shown.
            final View zoomView = activity.findViewById(R.id.page_zoom_layout);
            assertTrue("Zoom view should not be null", zoomView != null);
            pollUiThread(() -> zoomView.getVisibility() == View.VISIBLE);

            // Wait for the show animation to complete and translation to stabilize.
            waitForTranslationToStabilize(zoomView);

            // 4. Ensure the zoom UI is fully visible (all parts).
            final View bottomSheetView = activity.findViewById(R.id.bottom_sheet);
            assertTrue("Bottom sheet view should not be null", bottomSheetView != null);

            final int[] zoomLocation = new int[2];
            final int[] sheetLocation = new int[2];
            View sheetContainerView = activity.findViewById(R.id.sheet_container);

            // Poll until the layout stabilizes and the Page Zoom bar is correctly positioned above
            // the bottom sheet.
            // This debounces layout passes which might be delayed on slower emulators.
            try {
                pollUiThread(
                        () -> {
                            zoomView.getLocationOnScreen(zoomLocation);
                            bottomSheetView.getLocationOnScreen(sheetLocation);
                            int zoomBottom = zoomLocation[1] + zoomView.getHeight();
                            int sheetTop = sheetLocation[1];
                            int gap = sheetTop - zoomBottom;
                            int gapDp = ViewUtils.pxToDp(activity, gap);
                            return zoomBottom <= sheetTop && gapDp >= 18 && gapDp <= 22;
                        });
            } catch (Exception e) {
                // If we timed out, run the assertions once manually to produce the detailed failure
                // message and debug info.
                runOnUiThreadBlocking(
                        () -> {
                            zoomView.getLocationOnScreen(zoomLocation);
                            bottomSheetView.getLocationOnScreen(sheetLocation);
                        });
                int zoomBottom = zoomLocation[1] + zoomView.getHeight();
                int sheetTop = sheetLocation[1];
                int gap = sheetTop - zoomBottom;
                int gapDp = ViewUtils.pxToDp(activity, gap);
                String orientationStr = landscape ? "landscape" : "portrait";

                String debugInfo =
                        String.format(
                                "\n"
                                    + "Debug Info:\n"
                                    + "  Screen Height (from resources): %d\n"
                                    + "  ZoomView: Y=%d, Height=%d, Bottom=%d, TranslationY=%.2f\n"
                                    + "  BottomSheetView: Y=%d, Height=%d, Top=%d,"
                                    + " TranslationY=%.2f\n"
                                    + "  SheetOffset (getCurrentOffset): %d\n"
                                    + "  IsFullWidth: %b\n"
                                    + "  BrowserControlsManager Bottom Height: %d\n"
                                    + "  SheetContainer Bottom Margin: %d\n"
                                    + "  Controller Peek Height (getCurrentPeekHeightPx): %d\n"
                                    + "  Content Peek Height (getPeekHeight): %d",
                                activity.getResources().getDisplayMetrics().heightPixels,
                                zoomLocation[1],
                                zoomView.getHeight(),
                                zoomBottom,
                                zoomView.getTranslationY(),
                                sheetLocation[1],
                                bottomSheetView.getHeight(),
                                sheetTop,
                                bottomSheetView.getTranslationY(),
                                mSheetController.getCurrentOffset(),
                                mSheetController.isFullWidth(),
                                activity.getBrowserControlsManager().getBottomControlsHeight(),
                                ((ViewGroup.MarginLayoutParams)
                                                sheetContainerView.getLayoutParams())
                                        .bottomMargin,
                                mSheetController.getCurrentPeekHeightPx(),
                                mBottomSheetContent.getPeekHeight());

                assertTrue(
                        "Zoom bar bottom ("
                                + zoomBottom
                                + ") should be above or at bottom sheet top ("
                                + sheetTop
                                + ")"
                                + debugInfo,
                        zoomBottom <= sheetTop);
                assertTrue(
                        "Gap in "
                                + orientationStr
                                + " ("
                                + gapDp
                                + " dp) should be the full margin (expected around 20dp)"
                                + debugInfo,
                        gapDp >= 18 && gapDp <= 22);
                throw e;
            }
        } finally {
            // Reset orientation.
            runOnUiThreadBlocking(
                    () -> {
                        ActivityTestUtils.clearActivityOrientation(activity);
                    });
        }
    }

    private void showContent(BottomSheetContent content, @SheetState int targetState) {
        runOnUiThreadBlocking(
                () -> {
                    boolean shown =
                            mSheetController.requestShowContent(content, /* animate= */ false);
                    if (shown) {
                        mTestSupport.setSheetState(targetState, /* animate= */ false);
                    } else {
                        assertEquals(
                                "The sheet should still be hidden.",
                                SheetState.HIDDEN,
                                mSheetController.getSheetState());
                    }
                });

        // If the content switched, wait for the desired state.
        if (mSheetController.getCurrentSheetContent() == content) {
            pollUiThread(() -> mSheetController.getSheetState() == targetState);
        }
    }

    private void waitForTranslationToStabilize(View view) {
        final float[] lastY = new float[1];
        lastY[0] = -1;
        pollUiThread(
                () -> {
                    float currentY = view.getY();
                    boolean stable = currentY == lastY[0];
                    lastY[0] = currentY;
                    return stable;
                });
    }
}
