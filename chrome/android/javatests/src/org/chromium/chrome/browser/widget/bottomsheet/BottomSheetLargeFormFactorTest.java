// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.flags.ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Instrumentation tests for {@link BottomSheet} large form factor (desktop popup and fallback)
 * layouts on a real Android view hierarchy and layout engine.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.BOTTOM_SHEET_ON_DESKTOP_WINDOWING})
@Batch(Batch.PER_CLASS)
@Restriction(DeviceFormFactor.DESKTOP)
public class BottomSheetLargeFormFactorTest {
    @Rule
    public FreshCtaTransitTestRule mTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;
    private BottomSheetController mSheetController;
    private BottomSheetTestSupport mTestSupport;

    private static class LffTestBottomSheetContent extends TestBottomSheetContent {
        private boolean mSupportsLargeFormFactor = true;
        private boolean mIsNonModal;

        LffTestBottomSheetContent(Context context) {
            super(context, ContentPriority.HIGH, /* hasCustomLifecycle= */ false);
            setPeekHeight(HeightMode.DISABLED);
            setHalfHeightRatio(0.5f);
            setSkipHalfStateScrollingDown(false);
        }

        void setSupportsLargeFormFactor(boolean supports) {
            mSupportsLargeFormFactor = supports;
        }

        void setNonModal(boolean nonModal) {
            mIsNonModal = nonModal;
            setHasCustomScrimLifecycle(nonModal);
        }

        @Override
        public @Nullable View getToolbarView() {
            return null;
        }

        @Override
        public boolean supportsLargeFormFactor() {
            return mSupportsLargeFormFactor;
        }

        @Override
        public boolean coversBottomControls() {
            return true;
        }

        @Override
        public BottomSheetType getSheetType() {
            return new BottomSheetType.Builder().setModal(!mIsNonModal).build();
        }
    }

    @Before
    public void setUp() {
        BottomSheetTestSupport.setSmallScreen(false);
        mPage = mTestRule.startOnBlankPage();
        mSheetController =
                mPage.getActivity().getRootUiCoordinatorForTesting().getBottomSheetController();
        mTestSupport = new BottomSheetTestSupport(mSheetController);
    }

    @After
    public void tearDown() {
        if (mTestSupport != null) {
            runOnUiThreadBlocking(
                    () -> {
                        mTestSupport.forceDismissAllContent();
                        mTestSupport.endAllAnimations();
                    });
        }
    }

    @Test
    @MediumTest
    public void testDesktopLayoutInflation() {
        LffTestBottomSheetContent content = createContent();
        showContent(content, SheetState.FULL);

        runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Large form factor UI should be enabled for desktop content",
                            mSheetController.isLargeFormFactorUiEnabled(content));
                    ViewGroup container = mTestSupport.getSheetContainer();
                    View sheet = container.findViewById(R.id.bottom_sheet);
                    assertNotNull("Bottom sheet view must be present in container", sheet);
                    View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
                    assertNotNull(
                            "Close button view should be present in desktop layout hierarchy",
                            closeButton);
                    assertEquals(
                            "Modal desktop popup content should not display a framework close"
                                    + " button",
                            View.GONE,
                            closeButton.getVisibility());
                    assertNotNull(
                            "Desktop fallback shadow view should be present in layout hierarchy",
                            sheet.findViewById(R.id.desktop_fallback_shadow));

                    // Expected values are read from the same resources that production uses.
                    int expectedBottomMargin =
                            sheet.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.bottom_sheet_desktop_bottom_margin);
                    MarginLayoutParams containerLp =
                            (MarginLayoutParams) container.getLayoutParams();
                    assertEquals(
                            "Container bottom margin should match desktop floating bottom margin",
                            expectedBottomMargin,
                            containerLp.bottomMargin);

                    TypedValue tv = new TypedValue();
                    sheet.getContext()
                            .getTheme()
                            .resolveAttribute(R.attr.popupBgCornerRadius, tv, true);
                    float expectedPopupRadius =
                            tv.getDimension(sheet.getResources().getDisplayMetrics());
                    GradientDrawable bg =
                            (GradientDrawable) sheet.findViewById(R.id.background).getBackground();
                    float[] radii = getPerCornerRadii(bg);
                    if (radii != null) {
                        for (int i = 0; i < radii.length; i++) {
                            assertEquals(
                                    "Every corner of the desktop popup background should use"
                                            + " popupBgCornerRadius (index "
                                            + i
                                            + ")",
                                    expectedPopupRadius,
                                    radii[i],
                                    0.5f);
                        }
                    } else {
                        assertEquals(
                                "Desktop popup background should use popupBgCornerRadius on all"
                                        + " corners",
                                expectedPopupRadius,
                                bg.getCornerRadius(),
                                0.5f);
                    }
                });
    }

    @Test
    @MediumTest
    public void testCloseButtonClick_DismissesWithCloseButtonReason() throws Exception {
        LffTestBottomSheetContent content = createContent();
        content.setNonModal(true);
        showContent(content, SheetState.FULL);

        CallbackHelper closedHelper = new CallbackHelper();
        AtomicInteger closedReason = new AtomicInteger(StateChangeReason.NONE);
        BottomSheetObserver observer =
                new BottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        closedReason.set(reason);
                        closedHelper.notifyCalled();
                    }
                };
        runOnUiThreadBlocking(() -> mSheetController.addObserver(observer));

        runOnUiThreadBlocking(
                () -> {
                    View sheet = mTestSupport.getSheetContainer().findViewById(R.id.bottom_sheet);
                    View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
                    assertEquals(
                            "Non-modal desktop popup must display the close button",
                            View.VISIBLE,
                            closeButton.getVisibility());
                    assertTrue("Close button click should succeed", closeButton.performClick());
                    mTestSupport.endAllAnimations();
                });

        closedHelper.waitForOnly();
        pollUiThread(() -> mSheetController.getSheetState() == SheetState.HIDDEN);
        assertEquals(
                "Sheet state change reason should be CLOSE_BUTTON",
                StateChangeReason.CLOSE_BUTTON,
                closedReason.get());
        runOnUiThreadBlocking(() -> mSheetController.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testBackPress_DismissesDesktopPopupToHidden() throws Exception {
        LffTestBottomSheetContent content = createContent();
        showContent(content, SheetState.FULL);

        CallbackHelper closedHelper = new CallbackHelper();
        AtomicInteger closedReason = new AtomicInteger(StateChangeReason.NONE);
        BottomSheetObserver observer =
                new BottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        closedReason.set(reason);
                        closedHelper.notifyCalled();
                    }
                };
        runOnUiThreadBlocking(() -> mSheetController.addObserver(observer));

        int backPressResult =
                runOnUiThreadBlocking(
                        () -> {
                            BackPressHandler handler =
                                    mSheetController.getBottomSheetBackPressHandler();
                            assertTrue(
                                    "Back press should be consumed when desktop popup sheet is"
                                            + " open",
                                    handler.getHandleBackPressChangedSupplier().get());
                            int result = handler.handleBackPress();
                            mTestSupport.endAllAnimations();
                            return result;
                        });
        assertEquals(
                "The sheet back press handler should report that it handled the back press",
                BackPressResult.SUCCESS,
                backPressResult);

        closedHelper.waitForOnly();
        pollUiThread(() -> mSheetController.getSheetState() == SheetState.HIDDEN);
        assertEquals(
                "Sheet state change reason should be BACK_PRESS",
                StateChangeReason.BACK_PRESS,
                closedReason.get());
        runOnUiThreadBlocking(() -> mSheetController.removeObserver(observer));
    }

    @Test
    @MediumTest
    public void testDesktopOptOutFallbackEndToEnd() {
        LffTestBottomSheetContent content = createContent();
        content.setSupportsLargeFormFactor(false);
        content.setNonModal(true);
        showContent(content, SheetState.FULL);

        runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Large form factor UI should be disabled for opted-out content",
                            mSheetController.isLargeFormFactorUiEnabled(content));
                    ViewGroup container = mTestSupport.getSheetContainer();
                    View sheet = container.findViewById(R.id.bottom_sheet);
                    View fallbackShadow = sheet.findViewById(R.id.desktop_fallback_shadow);
                    View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
                    View contentContainer = sheet.findViewById(R.id.bottom_sheet_content);

                    assertEquals(
                            "Fallback shadow should be visible in desktop fallback mode",
                            View.VISIBLE,
                            fallbackShadow.getVisibility());
                    assertEquals(
                            "Close button should be hidden in desktop fallback mode",
                            View.GONE,
                            closeButton.getVisibility());

                    MarginLayoutParams containerLp =
                            (MarginLayoutParams) container.getLayoutParams();
                    assertEquals(
                            "Fallback container bottom margin should not include the desktop"
                                    + " floating margin",
                            0,
                            containerLp.bottomMargin);

                    float expectedTopRadius =
                            sheet.getResources()
                                    .getDimensionPixelSize(R.dimen.bottom_sheet_corner_radius);
                    GradientDrawable bg =
                            (GradientDrawable) sheet.findViewById(R.id.background).getBackground();
                    float[] radii = bg.getCornerRadii();
                    assertNotNull("Background corner radii array must not be null", radii);

                    // Android GradientDrawable corner radii schema defines 8 float values
                    // corresponding to [x, y] radius pairs for each of the 4 corners:
                    // [TopLeft.x, TopLeft.y, TopRight.x, TopRight.y, BottomRight.x, BottomRight.y,
                    // BottomLeft.x, BottomLeft.y].
                    // In desktop fallback mode (phone-style sheet), only the top corners are
                    // rounded while bottom corners remain 0.
                    assertEquals(
                            "Top-left X radius should match bottom sheet top corner radius",
                            expectedTopRadius,
                            radii[0],
                            0.5f);
                    assertEquals(
                            "Top-left Y radius should match bottom sheet top corner radius",
                            expectedTopRadius,
                            radii[1],
                            0.5f);
                    assertEquals(
                            "Top-right X radius should match bottom sheet top corner radius",
                            expectedTopRadius,
                            radii[2],
                            0.5f);
                    assertEquals(
                            "Top-right Y radius should match bottom sheet top corner radius",
                            expectedTopRadius,
                            radii[3],
                            0.5f);
                    assertEquals(
                            "Bottom-right X radius should be 0 in fallback mode",
                            0f,
                            radii[4],
                            0.5f);
                    assertEquals(
                            "Bottom-right Y radius should be 0 in fallback mode",
                            0f,
                            radii[5],
                            0.5f);
                    assertEquals(
                            "Bottom-left X radius should be 0 in fallback mode",
                            0f,
                            radii[6],
                            0.5f);
                    assertEquals(
                            "Bottom-left Y radius should be 0 in fallback mode",
                            0f,
                            radii[7],
                            0.5f);

                    assertEquals(
                            "Content container height should be MATCH_PARENT in fallback mode",
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            contentContainer.getLayoutParams().height);
                });
    }

    private LffTestBottomSheetContent createContent() {
        return runOnUiThreadBlocking(() -> new LffTestBottomSheetContent(mTestRule.getActivity()));
    }

    /** Returns the per-corner radii of a shape, or null if the shape uses one radius. */
    private static @Nullable float[] getPerCornerRadii(GradientDrawable shape) {
        try {
            return shape.getCornerRadii();
        } catch (NullPointerException e) {
            // Some Android versions throw here instead of returning null when the shape uses one
            // radius for every corner.
            return null;
        }
    }

    private void showContent(BottomSheetContent content, @SheetState int targetState) {
        runOnUiThreadBlocking(
                () -> {
                    boolean shown = mSheetController.requestShowContent(content, false);
                    assertTrue("Sheet content should be shown", shown);
                    mTestSupport.setSheetState(targetState, false);
                });
        pollUiThread(() -> mSheetController.getSheetState() == targetState);
    }
}
