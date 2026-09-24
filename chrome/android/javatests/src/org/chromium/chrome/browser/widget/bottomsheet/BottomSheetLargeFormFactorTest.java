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

import org.chromium.base.DeviceInfo;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.EnableFeatures;
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

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Instrumentation tests for {@link BottomSheet} large form factor (desktop popup and fallback)
 * layouts on a real Android view hierarchy and layout engine.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.BOTTOM_SHEET_ON_DESKTOP_WINDOWING})
@Batch(Batch.PER_CLASS)
@DisableIf.Build(sdk_is_less_than = 32, message = "crbug.com/565680022")
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
            // Isolate desktop bottom margin assertions from asynchronous browser-controls offset
            // updates on phone AVDs (e.g. android-x64-rel).
            return true;
        }

        @Override
        public BottomSheetType getSheetType() {
            return new BottomSheetType.Builder().setModal(!mIsNonModal).build();
        }
    }

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(true);
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
                    assertTrue(mSheetController.isLargeFormFactorUiEnabled(content));
                    ViewGroup container = mTestSupport.getSheetContainer();
                    View sheet = container.findViewById(R.id.bottom_sheet);
                    assertNotNull(sheet);
                    assertNotNull(sheet.findViewById(R.id.bottom_sheet_close_button));
                    assertNotNull(sheet.findViewById(R.id.desktop_fallback_shadow));

                    int expectedBottomMargin =
                            sheet.getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.bottom_sheet_desktop_bottom_margin);
                    MarginLayoutParams containerLp =
                            (MarginLayoutParams) container.getLayoutParams();
                    assertEquals(expectedBottomMargin, containerLp.bottomMargin);

                    TypedValue tv = new TypedValue();
                    sheet.getContext()
                            .getTheme()
                            .resolveAttribute(R.attr.popupBgCornerRadius, tv, true);
                    float expectedPopupRadius =
                            tv.getDimension(sheet.getResources().getDisplayMetrics());
                    GradientDrawable bg =
                            (GradientDrawable)
                                    sheet.findViewById(R.id.background).getBackground().mutate();
                    assertEquals(expectedPopupRadius, bg.getCornerRadius(), 0.5f);
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
                    assertEquals(View.VISIBLE, closeButton.getVisibility());
                    assertTrue(closeButton.performClick());
                });

        closedHelper.waitForOnly();
        pollUiThread(() -> mSheetController.getSheetState() == SheetState.HIDDEN);
        assertEquals(StateChangeReason.CLOSE_BUTTON, closedReason.get());
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
                    assertFalse(mSheetController.isLargeFormFactorUiEnabled(content));
                    ViewGroup container = mTestSupport.getSheetContainer();
                    View sheet = container.findViewById(R.id.bottom_sheet);
                    View fallbackShadow = sheet.findViewById(R.id.desktop_fallback_shadow);
                    View closeButton = sheet.findViewById(R.id.bottom_sheet_close_button);
                    View contentContainer = sheet.findViewById(R.id.bottom_sheet_content);

                    assertEquals(View.VISIBLE, fallbackShadow.getVisibility());
                    assertEquals(View.GONE, closeButton.getVisibility());

                    MarginLayoutParams containerLp =
                            (MarginLayoutParams) container.getLayoutParams();
                    assertEquals(0, containerLp.bottomMargin);

                    float expectedTopRadius =
                            sheet.getResources()
                                    .getDimensionPixelSize(R.dimen.bottom_sheet_corner_radius);
                    GradientDrawable bg =
                            (GradientDrawable)
                                    sheet.findViewById(R.id.background).getBackground().mutate();
                    float[] radii = bg.getCornerRadii();
                    assertNotNull(radii);
                    assertEquals(expectedTopRadius, radii[0], 0.5f);
                    assertEquals(expectedTopRadius, radii[1], 0.5f);
                    assertEquals(expectedTopRadius, radii[2], 0.5f);
                    assertEquals(expectedTopRadius, radii[3], 0.5f);
                    assertEquals(0f, radii[4], 0.5f);
                    assertEquals(0f, radii[5], 0.5f);
                    assertEquals(0f, radii[6], 0.5f);
                    assertEquals(0f, radii[7], 0.5f);

                    assertEquals(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            contentContainer.getLayoutParams().height);
                });
    }

    private LffTestBottomSheetContent createContent() {
        return runOnUiThreadBlocking(() -> new LffTestBottomSheetContent(mTestRule.getActivity()));
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
