// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * This class contains tests for the logic that shows and hides the bottom sheet as a result of
 * different browser events. These tests use a bottom sheet and controller different from the ones
 * created by the activity that are used by different experiments.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Restriction(DeviceFormFactor.PHONE) // TODO(mdjones): Remove this (crbug.com/837838).
@Batch(Batch.PER_CLASS)
public class BottomSheetControllerTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mIninialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;

    private BottomSheetController mSheetController;
    private BottomSheetTestSupport mTestSupport;
    private TestBottomSheetContent mLowPriorityContent;
    private TestBottomSheetContent mHighPriorityContent;
    private TestBottomSheetContent mPeekableContent;
    private TestBottomSheetContent mNonPeekableContent;
    private TestBottomSheetContent mBackInterceptingContent;
    private ScrimCoordinator mScrimCoordinator;
    private int mSuppressionToken;
    private TestEdgeToEdgeController mEdgeToEdgeController;

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BottomSheetTestSupport.setSmallScreen(false);

                    mScrimCoordinator =
                            mActivity
                                    .getRootUiCoordinatorForTesting()
                                    .getScrimCoordinatorForTesting();
                    mScrimCoordinator.disableAnimationForTesting(true);

                    mSheetController =
                            mActivity.getRootUiCoordinatorForTesting().getBottomSheetController();
                    mTestSupport = new BottomSheetTestSupport(mSheetController);

                    mLowPriorityContent =
                            new TestBottomSheetContent(
                                    mActivity, BottomSheetContent.ContentPriority.LOW, false);
                    mHighPriorityContent =
                            new TestBottomSheetContent(
                                    mActivity, BottomSheetContent.ContentPriority.HIGH, false);

                    mBackInterceptingContent =
                            new TestBottomSheetContent(
                                    mActivity, BottomSheetContent.ContentPriority.LOW, false);
                    mBackInterceptingContent.setHandleBackPress(true);

                    mPeekableContent = new TestBottomSheetContent(mActivity);
                    mNonPeekableContent = new TestBottomSheetContent(mActivity);
                    mNonPeekableContent.setPeekHeight(BottomSheetContent.HeightMode.DISABLED);
                    mEdgeToEdgeController = new TestEdgeToEdgeController();
                    mActivity
                            .getEdgeToEdgeControllerSupplierForTesting()
                            .set(mEdgeToEdgeController);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.forceDismissAllContent();
                    mTestSupport.endAllAnimations();
                });
    }

    /**
     * @return The height of the container view.
     */
    private int getContainerHeight() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getActivityTabProvider().get().getView().getHeight());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPeek() {
        requestContentInSheet(mLowPriorityContent, true);
        assertEquals(
                "The bottom sheet should be peeking.",
                SheetState.PEEK,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPeek_hideKeyboard() {
        KeyboardVisibilityDelegate keyboardDelegate = KeyboardVisibilityDelegate.getInstance();
        ThreadUtils.runOnUiThreadBlocking(
                () -> keyboardDelegate.showKeyboard(mActivity.getTabsView()));
        requestContentInSheet(mLowPriorityContent, true);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertFalse(
                                keyboardDelegate.isKeyboardShowing(
                                        mActivity, mActivity.getTabsView())));
        BottomSheetTestSupport.waitForContentChange(mSheetController, mLowPriorityContent);
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.PEEK);
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testShowWithBottomInset() {
        requestContentInSheet(mLowPriorityContent, true);
        View bottomSheet = mActivity.findViewById(R.id.bottom_sheet);
        float transYWithoutBottomInset = bottomSheet.getTranslationY();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.hideContent(mLowPriorityContent, false);
                });

        mEdgeToEdgeController.bottomInset = 100;

        requestContentInSheet(mLowPriorityContent, true);
        float transYWithBottomInset = bottomSheet.getTranslationY();

        Assert.assertEquals(
                "The translate is not adjusted for the extra content when it is expanded to edge.",
                transYWithoutBottomInset,
                transYWithBottomInset + ViewUtils.dpToPx(mActivity, 100),
                MathUtils.EPSILON);
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPriorityInPeekState() {
        requestContentInSheet(mLowPriorityContent, true);
        int lowPriorityDestroyCalls = mLowPriorityContent.destroyCallbackHelper.getCallCount();
        requestContentInSheet(mHighPriorityContent, true);
        BottomSheetTestSupport.waitForContentChange(mSheetController, mHighPriorityContent);
        assertEquals(
                "The low priority content should not have been destroyed!",
                lowPriorityDestroyCalls,
                mLowPriorityContent.destroyCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPriorityInExpandedState() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();
        requestContentInSheet(mHighPriorityContent, false);
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                mLowPriorityContent,
                mSheetController.getCurrentSheetContent());

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        BottomSheetTestSupport.waitForContentChange(mSheetController, mHighPriorityContent);
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPressPeekable() {
        requestContentInSheet(mPeekableContent, true);
        expandSheet();
        assertEquals(
                "The bottom sheet should be expanded.",
                SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.handleBackPress();
                    mTestSupport.endAllAnimations();
                });
        assertEquals(
                "The bottom sheet should be peeking.",
                SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPressNonPeekable() {
        requestContentInSheet(mNonPeekableContent, true);
        expandSheet();
        assertEquals(
                "The bottom sheet should be expanded.",
                SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.handleBackPress();
                    mTestSupport.endAllAnimations();
                });
        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    /**
     * Test that BottomSheet hide animation when user navigates page back cannot be reversed via a
     * gesture.
     */
    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    @DisabledTest(message = "https://crbug.com/1523222")
    public void testGestureCannotMoveSheetDuringHideAnimation() {
        Rect visibleViewportRect = new Rect();
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleViewportRect);

        MotionEvent initialEvent =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_MOVE,
                        visibleViewportRect.left,
                        visibleViewportRect.bottom,
                        0);
        MotionEvent currentEvent =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_MOVE,
                        visibleViewportRect.left,
                        visibleViewportRect.bottom - 1,
                        0);

        requestContentInSheet(mNonPeekableContent, true);
        expandSheet();
        assertEquals(
                "The bottom sheet should be expanded.",
                SheetState.HALF,
                mSheetController.getSheetState());

        // Check that gesture can be processed when sheet is expanded.
        assertTrue(
                "Gesture should move sheet",
                mTestSupport.shouldGestureMoveSheet(initialEvent, currentEvent));

        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.handleBackPress();
                });

        // Check that gesture is not processed during a hide animation.
        assertFalse(
                "Gesture should not move sheet",
                mTestSupport.shouldGestureMoveSheet(initialEvent, currentEvent));

        // Check that the animation is still in progress.
        assertTrue(mSheetController.isSheetOpen());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.endAllAnimations();
                });
        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSheetGoneAfterTabSwitcher() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        enterAndExitTabSwitcher();
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.HIDDEN);
        assertNull(
                "The bottom sheet is unexpectedly showing content.",
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSheetGoneAfterTransitioningToAndFromSwitcher() throws TimeoutException {
        // Open a second tab.
        Tab tab1 = mActivity.getActivityTab();
        openNewTabInForeground();
        Tab tab2 = mActivity.getActivityTab();

        requestContentInSheet(mLowPriorityContent, true);
        assertEquals(
                "The tab bottom sheet should be visible.",
                SheetState.PEEK,
                mSheetController.getSheetState());
        assertEquals(
                "The tab bottom sheet contains the incorrect content.",
                mLowPriorityContent,
                mSheetController.getCurrentSheetContent());

        // Enter the tab switcher.
        setTabSwitcherState(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The tab bottom sheet should be hidden.",
                            SheetState.HIDDEN,
                            mSheetController.getSheetState());
                    assertNull(
                            "The bottom sheet is unexpectedly showing content.",
                            mSheetController.getCurrentSheetContent());
                });

        // Show a sheet in the tab switcher.
        requestContentInSheet(mHighPriorityContent, true);
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.PEEK);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The GTS bottom sheet should be visible.",
                            SheetState.PEEK,
                            mSheetController.getSheetState());
                    assertEquals(
                            "The GTS bottom sheet contains the incorrect content.",
                            mHighPriorityContent,
                            mSheetController.getCurrentSheetContent());
                    mActivity
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(0, TabSelectionType.FROM_USER);
                });

        // Exit tab switcher.
        setTabSwitcherState(false);

        BottomSheetTestSupport.waitForContentChange(mSheetController, null);
        assertEquals(
                "The GTS bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertNull(
                "The bottom sheet is unexpectedly showing content.",
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testContentDestroyedOnHidden() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        int destroyCallCount = mLowPriorityContent.destroyCallbackHelper.getCallCount();

        // Enter the tab switcher and select a different tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.setSheetState(SheetState.HIDDEN, false);
                });

        mLowPriorityContent.destroyCallbackHelper.waitForCallback(destroyCallCount);
        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testOpenTabInBackground() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();
        openNewTabInBackground();

        assertEquals(
                "The bottom sheet should be expanded.",
                SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSwitchTabs() {
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals(
                "The bottom sheet should be peeking.",
                SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @DisabledTest(message = "https://crbug.com/837809")
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSwitchTabsMultipleTimes() throws TimeoutException {
        final int originalTabIndex =
                mActivity
                        .getTabModelSelector()
                        .getCurrentModel()
                        .indexOf(mActivity.getActivityTab());
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals(
                "The bottom sheet should be peeking.",
                SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                null,
                mSheetController.getCurrentSheetContent());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(originalTabIndex, TabSelectionType.FROM_USER);
                });

        // Request content be shown again.
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();

        openNewTabInBackground();

        assertEquals(
                "The bottom sheet should be expanded.",
                SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testCustomLifecycleContent() throws TimeoutException {
        requestContentInSheet(mHighPriorityContent, true);
        requestContentInSheet(mLowPriorityContent, false);

        TestBottomSheetContent customLifecycleContent =
                new TestBottomSheetContent(mActivity, BottomSheetContent.ContentPriority.LOW, true);
        requestContentInSheet(customLifecycleContent, false);
        assertEquals(mHighPriorityContent, mSheetController.getCurrentSheetContent());

        // Change URL and wait for PageLoadStarted event.
        CallbackHelper pageLoadStartedHelper = new CallbackHelper();
        Tab tab = mActivity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.addObserver(
                            new EmptyTabObserver() {
                                @Override
                                public void onPageLoadStarted(Tab tab, GURL url) {
                                    pageLoadStartedHelper.notifyCalled();
                                }
                            });
                });
        int currentCallCount = pageLoadStartedHelper.getCallCount();
        ChromeTabUtils.loadUrlOnUiThread(tab, "about:blank");
        pageLoadStartedHelper.waitForCallback(currentCallCount, 1);

        ThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
        assertEquals(customLifecycleContent, mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testScrim() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        assertNull("There should currently be no scrim.", mScrimCoordinator.getViewForTesting());

        expandSheet();

        assertEquals(
                "The scrim should be visible.",
                View.VISIBLE,
                ((View) mScrimCoordinator.getViewForTesting()).getVisibility());

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertNull(
                "There should be no scrim when the sheet is closed.",
                mScrimCoordinator.getViewForTesting());
    }

    @Test
    @MediumTest
    public void testCustomScrimLifecycle() {
        TestBottomSheetContent customScrimContent =
                new TestBottomSheetContent(mActivity, BottomSheetContent.ContentPriority.LOW, true);
        customScrimContent.setHasCustomScrimLifecycle(true);
        requestContentInSheet(customScrimContent, true);

        expandSheet();

        assertEquals(
                "The scrim should not be visible with a custom scrim lifecycle.",
                null,
                mScrimCoordinator.getViewForTesting());
    }

    @Test
    @MediumTest
    public void testCloseEvent() throws TimeoutException {
        requestContentInSheet(mHighPriorityContent, true);
        expandSheet();

        CallbackHelper closedHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.addObserver(
                            new EmptyBottomSheetObserver() {
                                @Override
                                public void onSheetClosed(@StateChangeReason int reason) {
                                    closedHelper.notifyCalled();
                                    mSheetController.removeObserver(this);
                                }
                            });
                    mTestSupport.setSheetState(SheetState.HIDDEN, false);
                });

        closedHelper.waitForOnly();

        BottomSheetTestSupport.waitForContentChange(mSheetController, null);
    }

    @Test
    @MediumTest
    public void testScrimTapClosesSheet() throws TimeoutException, ExecutionException {
        requestContentInSheet(mHighPriorityContent, true);
        CallbackHelper closedCallbackHelper = new CallbackHelper();
        BottomSheetObserver observer =
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(@StateChangeReason int reason) {
                        closedCallbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.addObserver(observer));

        expandSheet();

        ThreadUtils.runOnUiThreadBlocking(
                () -> ((View) mScrimCoordinator.getViewForTesting()).callOnClick());

        closedCallbackHelper.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testCustomHalfRatio() {
        final float customHalfHeight = 0.3f;
        mLowPriorityContent.setHalfHeightRatio(customHalfHeight);
        requestContentInSheet(mLowPriorityContent, true);

        expandSheet();

        int computedOffset = (int) (customHalfHeight * mSheetController.getContainerHeight());
        assertEquals(
                "Half height is incorrect for custom ratio.",
                computedOffset,
                mSheetController.getCurrentOffset());
    }

    @Test
    @MediumTest
    public void testCustomFullRatio() {
        final float customFullHeight = 0.5f;
        mLowPriorityContent.setFullHeightRatio(customFullHeight);
        requestContentInSheet(mLowPriorityContent, true);

        maximizeSheet();

        int computedOffset = (int) (customFullHeight * mSheetController.getContainerHeight());
        assertEquals(
                "Full height is incorrect for custom ratio.",
                computedOffset,
                mSheetController.getCurrentOffset());
    }

    @Test
    @MediumTest
    public void testExpandWithDisabledHalfState() {
        mLowPriorityContent.setHalfHeightRatio(BottomSheetContent.HeightMode.DISABLED);
        requestContentInSheet(mLowPriorityContent, true);

        expandSheet();

        assertEquals(
                "The bottom sheet should be at the full state when half is disabled.",
                SheetState.FULL,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testCollapseSheet() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertEquals(
                "The bottom sheet should be at the peeking state.",
                SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testCollapseSheet_peekDisabled() throws ExecutionException {
        mLowPriorityContent.setPeekHeight(BottomSheetContent.HeightMode.DISABLED);
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertEquals(
                "The bottom sheet should be at the half state when peek is disabled.",
                SheetState.HALF,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress() {
        requestContentInSheet(mBackInterceptingContent, true);

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the peeking state.",
                            SheetState.PEEK,
                            mSheetController.getSheetState());
                    assertTrue(
                            "The back event should have been handled by the content.",
                            mTestSupport.handleBackPress());
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be in the peeking state.",
                SheetState.PEEK,
                mSheetController.getSheetState());
    }

    /**
     * "Refactored" suffix means compared with non-suffix version, this test is executed with
     * BACK_GESTURE_REFACTORED enabled. This feature involves a new way of handling back press. The
     * test flow is basically same with non-suffix version, but suffixed version includes more
     * statements to verify the values of refactor-related variables.
     */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_Refactored() {
        requestContentInSheet(mBackInterceptingContent, true);
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the peeking state.",
                            SheetState.PEEK,
                            mSheetController.getSheetState());
                    assertTrue(
                            "Bottom sheet controller should be ready for handling back press.",
                            getBackPressState());
                });
        Espresso.pressBack();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Bottom sheet controller should have handled back press.",
                            getBackPressState());
                    assertEquals(
                            BackPressHandler.Type.BOTTOM_SHEET,
                            mActivity
                                    .getBackPressManagerForTesting()
                                    .getLastCalledHandlerForTesting());
                    mTestSupport.endAllAnimations();
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_sheetOpen() {
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the half state.",
                            SheetState.HALF,
                            mSheetController.getSheetState());
                    assertTrue(
                            "The back event should not have been handled by the content.",
                            mTestSupport.handleBackPress());
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be at the half state if the content handled the back event.",
                SheetState.HALF,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_sheetOpen_Refactored() {
        requestContentInSheet(mBackInterceptingContent, true);
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();
        expandSheet();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the half state.",
                            SheetState.HALF,
                            mSheetController.getSheetState());
                    assertTrue(
                            "Bottom sheet controller should be ready for handling back press.",
                            getBackPressState());
                });

        Espresso.pressBack();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "The sheet should still be ready for back press if content handled it",
                            getBackPressState());
                    assertEquals(
                            BackPressHandler.Type.BOTTOM_SHEET,
                            mActivity
                                    .getBackPressManagerForTesting()
                                    .getLastCalledHandlerForTesting());
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be at the half state if the content handled the back event.",
                SheetState.HALF,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_noIntercept() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBackInterceptingContent.setHandleBackPress(false));
        requestContentInSheet(mBackInterceptingContent, true);

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the peeking state.",
                            SheetState.PEEK,
                            mSheetController.getSheetState());
                    assertFalse(
                            "The back event should not have been handled by the content.",
                            mTestSupport.handleBackPress());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_noIntercept_Refactored() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBackInterceptingContent.setHandleBackPress(false));
        requestContentInSheet(mBackInterceptingContent, true);
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the peeking state.",
                            SheetState.PEEK,
                            mSheetController.getSheetState());
                    assertFalse(
                            "Bottom sheet controller should not be ready for handling back press.",
                            getBackPressState());
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_noIntercept_sheetOpen() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBackInterceptingContent.setHandleBackPress(false));
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the half state.",
                            SheetState.HALF,
                            mSheetController.getSheetState());
                    assertFalse(
                            "The back event should not be handled by the content.",
                            mBackInterceptingContent.handleBackPress());
                    assertTrue(
                            "The back event should still be handled by the controller.",
                            mTestSupport.handleBackPress());
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be peeking if the content didn't handle the back event.",
                SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.BACK_GESTURE_REFACTOR)
    public void testHandleBackPress_noIntercept_sheetOpen_Refactored() {
        ThreadUtils.runOnUiThreadBlocking(() -> mBackInterceptingContent.setHandleBackPress(false));
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();
        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "The sheet should be in the half state.",
                            SheetState.HALF,
                            mSheetController.getSheetState());
                    assertFalse(
                            "The back event should not be handled by the content.",
                            mBackInterceptingContent.getBackPressStateChangedSupplier().get());
                    assertTrue(
                            "Bottom sheet controller should be ready for handling back press.",
                            getBackPressState());
                    mSheetController.getBottomSheetBackPressHandler().handleBackPress();
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be peeking if the content didn't handle the back event.",
                SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPress_withCustomScrimLifecycle() {
        mPeekableContent.setHasCustomScrimLifecycle(true);
        requestContentInSheet(mPeekableContent, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPeekableContent.setHandleBackPress(true);
                });
        expandSheet();
        assertEquals(
                "The bottom sheet should be expanded.",
                SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTestSupport.handleBackPress();
                    mTestSupport.endAllAnimations();
                });
    }

    @Test
    @MediumTest
    public void testSheetPriorityDuringSuppression() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.expandSheet();
                    mTestSupport.endAllAnimations();
                });

        assertTrue("The sheet should be open.", mSheetController.isSheetOpen());

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSuppressionToken = mTestSupport.suppressSheet(StateChangeReason.NONE));

        assertEquals(
                "The sheet should be hidden.", SheetState.HIDDEN, mSheetController.getSheetState());

        requestContentInSheet(mHighPriorityContent, true);

        assertEquals(
                "The sheet should still be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());

        ThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.unsuppressSheet(mSuppressionToken));

        assertEquals(
                "The high priority content should be shown.",
                mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testReplaceLowPriorityContentWhileOpen() throws ExecutionException {
        // Allow the content to be replaced without first closing the sheet.
        mLowPriorityContent.setCanSuppressInAnyState(true);
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.expandSheet();
                    mTestSupport.endAllAnimations();
                });

        assertTrue("The sheet should be open.", mSheetController.isSheetOpen());

        requestContentInSheet(mHighPriorityContent, true);

        assertEquals(
                "The high priority content should be shown.",
                mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testOpenTwiceWhileInQueue() {
        requestContentInSheet(mHighPriorityContent, true);
        expandSheet();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // While high priority content is visible, request new content to be shown
                    // twice.
                    mSheetController.requestShowContent(mLowPriorityContent, false);
                    mSheetController.requestShowContent(mLowPriorityContent, false);

                    // Now hide high priority content, this should cause low priority content to be
                    // shown.
                    mSheetController.hideContent(mHighPriorityContent, false);
                });
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.PEEK);
        assertEquals(
                "The low priority content should be shown.",
                mLowPriorityContent,
                mSheetController.getCurrentSheetContent());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.hideContent(mLowPriorityContent, false);
                    mTestSupport.endAllAnimations();
                });
        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals(
                "The bottom sheet is showing incorrect content.",
                null,
                mSheetController.getCurrentSheetContent());
    }

    /**
     * Request content be shown in the bottom sheet and end animations.
     *
     * @param content The content to show.
     * @param expectContentChange If the content is expected to change, setting this to true will
     *     cause the method to wait for BottomSheetObserver#onSheetContentChanged.
     */
    private void requestContentInSheet(BottomSheetContent content, boolean expectContentChange) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.requestShowContent(content, false);
                    mTestSupport.endAllAnimations();
                });

        if (expectContentChange) {
            BottomSheetTestSupport.waitForContentChange(mSheetController, content);
        }
    }

    /**
     * Expand the bottom sheet to a non-peek height. If the sheet has no content, an assert is
     * thrown.
     */
    private void expandSheet() {
        ThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.setSheetState(SheetState.HALF, false));
    }

    /** Expand the bottom sheet to it's maximum height. */
    private void maximizeSheet() {
        ThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.setSheetState(SheetState.FULL, false));
    }

    /**
     * Enter and immediately exit the tab switcher. This function will assert that the sheet is not
     * showing in the tab switcher.
     */
    private void enterAndExitTabSwitcher() throws TimeoutException {
        setTabSwitcherState(true);

        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());

        setTabSwitcherState(false);
    }

    /**
     * Set the tab switcher state and wait for that state to be settled.
     *
     * @param shown Whether the tab switcher should be shown.
     */
    private void setTabSwitcherState(boolean shown) {
        @LayoutType int targetLayout = shown ? LayoutType.TAB_SWITCHER : LayoutType.BROWSING;
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivity.getLayoutManager(), targetLayout, false);
        ThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
    }

    /** Open a new tab behind the active tab and wait for the tab selection event. */
    private void openNewTabInBackground() throws TimeoutException {
        CallbackHelper tabSelectedHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel = mActivity.getTabModelSelector().getCurrentModel();
                    tabModel.addObserver(
                            new TabModelObserver() {
                                @Override
                                public void didSelectTab(
                                        Tab tab, @TabSelectionType int type, int lastId) {
                                    tabSelectedHelper.notifyCalled();
                                    tabModel.removeObserver(this);
                                }
                            });
                    mActivity
                            .getTabCreator(false)
                            .createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    null);
                });

        tabSelectedHelper.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
    }

    /** Open a new tab in front of the active tab and wait for it to be completely loaded. */
    private void openNewTabInForeground() {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity, "about:blank", false);
        ThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
    }

    /** Whether back press will be consumed by bottom sheet. */
    private Boolean getBackPressState() {
        return mSheetController
                .getBottomSheetBackPressHandler()
                .getHandleBackPressChangedSupplier()
                .get();
    }

    private static class TestEdgeToEdgeController implements EdgeToEdgeController {
        public int bottomInset;

        @Override
        public void destroy() {}

        @Override
        public int getBottomInset() {
            return bottomInset;
        }

        @Override
        public int getBottomInsetPx() {
            return bottomInset;
        }

        @Override
        public int getSystemBottomInsetPx() {
            return bottomInset;
        }

        @Override
        public void registerAdjuster(EdgeToEdgePadAdjuster adjuster) {}

        @Override
        public void unregisterAdjuster(EdgeToEdgePadAdjuster adjuster) {}

        @Override
        public void registerObserver(ChangeObserver changeObserver) {}

        @Override
        public void unregisterObserver(ChangeObserver changeObserver) {}

        @Override
        public boolean isPageOptedIntoEdgeToEdge() {
            return bottomInset != 0;
        }

        @Override
        public boolean isDrawingToEdge() {
            return bottomInset != 0;
        }
    }
}
