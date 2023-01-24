// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
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
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;
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
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // TODO(mdjones): Remove this (crbug.com/837838).
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

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BottomSheetTestSupport.setSmallScreen(false);

            mScrimCoordinator =
                    mActivity.getRootUiCoordinatorForTesting().getScrimCoordinatorForTesting();
            mScrimCoordinator.disableAnimationForTesting(true);

            mSheetController =
                    mActivity.getRootUiCoordinatorForTesting().getBottomSheetController();
            mTestSupport = new BottomSheetTestSupport(mSheetController);

            mLowPriorityContent = new TestBottomSheetContent(
                    mActivity, BottomSheetContent.ContentPriority.LOW, false);
            mHighPriorityContent = new TestBottomSheetContent(
                    mActivity, BottomSheetContent.ContentPriority.HIGH, false);

            mBackInterceptingContent = new TestBottomSheetContent(
                    mActivity, BottomSheetContent.ContentPriority.LOW, false);
            mBackInterceptingContent.setHandleBackPress(true);

            mPeekableContent = new TestBottomSheetContent(mActivity);
            mNonPeekableContent = new TestBottomSheetContent(mActivity);
            mNonPeekableContent.setPeekHeight(BottomSheetContent.HeightMode.DISABLED);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.forceDismissAllContent();
            mTestSupport.endAllAnimations();
        });
    }

    /** @return The height of the container view. */
    private int getContainerHeight() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivity.getActivityTabProvider().get().getView().getHeight());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPeek() {
        requestContentInSheet(mLowPriorityContent, true);
        assertEquals("The bottom sheet should be peeking.", SheetState.PEEK,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPriorityInPeekState() {
        requestContentInSheet(mLowPriorityContent, true);
        int lowPriorityDestroyCalls = mLowPriorityContent.destroyCallbackHelper.getCallCount();
        requestContentInSheet(mHighPriorityContent, true);
        BottomSheetTestSupport.waitForContentChange(mSheetController, mHighPriorityContent);
        assertEquals("The low priority content should not have been destroyed!",
                lowPriorityDestroyCalls, mLowPriorityContent.destroyCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPriorityInExpandedState() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();
        requestContentInSheet(mHighPriorityContent, false);
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());

        TestThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        BottomSheetTestSupport.waitForContentChange(mSheetController, mHighPriorityContent);
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPressPeekable() {
        requestContentInSheet(mPeekableContent, true);
        expandSheet();
        assertEquals("The bottom sheet should be expanded.", SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.handleBackPress();
            mTestSupport.endAllAnimations();
        });
        assertEquals("The bottom sheet should be peeking.", SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPressNonPeekable() {
        requestContentInSheet(mNonPeekableContent, true);
        expandSheet();
        assertEquals("The bottom sheet should be expanded.", SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.handleBackPress();
            mTestSupport.endAllAnimations();
        });
        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSheetPeekAfterTabSwitcher() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        enterAndExitTabSwitcher();
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.PEEK);
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSheetHiddenAfterTabSwitcher() throws TimeoutException {
        // Open a second tab and then reselect the original activity tab.
        Tab tab1 = mActivity.getActivityTab();
        openNewTabInForeground();
        Tab tab2 = mActivity.getActivityTab();

        requestContentInSheet(mLowPriorityContent, true);

        // Enter the tab switcher and select a different tab.
        setTabSwitcherState(true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                    mSheetController.getSheetState());
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    0, TabSelectionType.FROM_USER, false);
        });

        setTabSwitcherState(false);

        BottomSheetTestSupport.waitForContentChange(mSheetController, null);
        assertEquals("The bottom sheet still should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testContentDestroyedOnHidden() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        int destroyCallCount = mLowPriorityContent.destroyCallbackHelper.getCallCount();

        // Enter the tab switcher and select a different tab.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestSupport.setSheetState(SheetState.HIDDEN, false); });

        mLowPriorityContent.destroyCallbackHelper.waitForCallback(destroyCallCount);
        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testOpenTabInBackground() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();
        openNewTabInBackground();

        assertEquals("The bottom sheet should be expanded.", SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSwitchTabs() {
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals("The bottom sheet should be peeking.", SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @DisabledTest(message = "https://crbug.com/837809")
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSwitchTabsMultipleTimes() throws TimeoutException {
        final int originalTabIndex = mActivity.getTabModelSelector().getCurrentModel().indexOf(
                mActivity.getActivityTab());
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals("The bottom sheet should be peeking.", SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    originalTabIndex, TabSelectionType.FROM_USER, false);
        });

        // Request content be shown again.
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();

        openNewTabInBackground();

        assertEquals("The bottom sheet should be expanded.", SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    pageLoadStartedHelper.notifyCalled();
                }
            });
        });
        int currentCallCount = pageLoadStartedHelper.getCallCount();
        ChromeTabUtils.loadUrlOnUiThread(tab, "about:blank");
        pageLoadStartedHelper.waitForCallback(currentCallCount, 1);

        TestThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
        assertEquals(customLifecycleContent, mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testScrim() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        assertNull("There should currently be no scrim.", mScrimCoordinator.getViewForTesting());

        expandSheet();

        assertEquals("The scrim should be visible.", View.VISIBLE,
                ((View) mScrimCoordinator.getViewForTesting()).getVisibility());

        TestThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertNull("There should be no scrim when the sheet is closed.",
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

        assertEquals("The scrim should not be visible with a custom scrim lifecycle.", null,
                mScrimCoordinator.getViewForTesting());
    }

    @Test
    @MediumTest
    public void testCloseEvent() throws TimeoutException {
        requestContentInSheet(mHighPriorityContent, true);
        expandSheet();

        CallbackHelper closedHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.addObserver(new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    closedHelper.notifyCalled();
                    mSheetController.removeObserver(this);
                }
            });
            mTestSupport.setSheetState(SheetState.HIDDEN, false);
        });

        closedHelper.waitForFirst();

        BottomSheetTestSupport.waitForContentChange(mSheetController, null);
    }

    @Test
    @MediumTest
    public void testScrimTapClosesSheet() throws TimeoutException, ExecutionException {
        requestContentInSheet(mHighPriorityContent, true);
        CallbackHelper closedCallbackHelper = new CallbackHelper();
        BottomSheetObserver observer = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                closedCallbackHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> mSheetController.addObserver(observer));

        expandSheet();

        TestThreadUtils.runOnUiThreadBlocking(
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
        assertEquals("Half height is incorrect for custom ratio.", computedOffset,
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
        assertEquals("Full height is incorrect for custom ratio.", computedOffset,
                mSheetController.getCurrentOffset());
    }

    @Test
    @MediumTest
    public void testExpandWithDisabledHalfState() {
        mLowPriorityContent.setHalfHeightRatio(BottomSheetContent.HeightMode.DISABLED);
        requestContentInSheet(mLowPriorityContent, true);

        expandSheet();

        assertEquals("The bottom sheet should be at the full state when half is disabled.",
                SheetState.FULL, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testCollapseSheet() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertEquals("The bottom sheet should be at the peeking state.", SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testCollapseSheet_peekDisabled() throws ExecutionException {
        mLowPriorityContent.setPeekHeight(BottomSheetContent.HeightMode.DISABLED);
        requestContentInSheet(mLowPriorityContent, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertEquals("The bottom sheet should be at the half state when peek is disabled.",
                SheetState.HALF, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress() {
        requestContentInSheet(mBackInterceptingContent, true);

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the peeking state.", SheetState.PEEK,
                    mSheetController.getSheetState());
            assertTrue("The back event should have been handled by the content.",
                    mTestSupport.handleBackPress());
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be in the peeking state.", SheetState.PEEK,
                mSheetController.getSheetState());
    }

    /**
     * "Refactored" suffix means compared with non-suffix version, this test is executed with
     * BACK_GESTURE_REFACTORED enabled. This feature involves a new way of handling back press.
     * The test flow is basically same with non-suffix version, but suffixed version includes more
     * statements to verify the values of refactor-related variables.
     */
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_Refactored() {
        requestContentInSheet(mBackInterceptingContent, true);
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the peeking state.", SheetState.PEEK,
                    mSheetController.getSheetState());
            assertTrue("Bottom sheet controller should be ready for handling back press.",
                    getBackPressState());
        });
        Espresso.pressBack();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertFalse(
                    "Bottom sheet controller should have handled back press.", getBackPressState());
            assertEquals(BackPressHandler.Type.BOTTOM_SHEET,
                    mActivity.getBackPressManagerForTesting().getLastCalledHandlerForTesting());
            mTestSupport.endAllAnimations();
        });
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_sheetOpen() {
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the half state.", SheetState.HALF,
                    mSheetController.getSheetState());
            assertTrue("The back event should not have been handled by the content.",
                    mTestSupport.handleBackPress());
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be at the half state if the content handled the back event.",
                SheetState.HALF, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_sheetOpen_Refactored() {
        requestContentInSheet(mBackInterceptingContent, true);
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();
        expandSheet();

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the half state.", SheetState.HALF,
                    mSheetController.getSheetState());
            assertTrue("Bottom sheet controller should be ready for handling back press.",
                    getBackPressState());
        });

        Espresso.pressBack();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue("The sheet should still be ready for back press if content handled it",
                    getBackPressState());
            assertEquals(BackPressHandler.Type.BOTTOM_SHEET,
                    mActivity.getBackPressManagerForTesting().getLastCalledHandlerForTesting());
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be at the half state if the content handled the back event.",
                SheetState.HALF, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_noIntercept() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBackInterceptingContent.setHandleBackPress(false));
        requestContentInSheet(mBackInterceptingContent, true);

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the peeking state.", SheetState.PEEK,
                    mSheetController.getSheetState());
            assertFalse("The back event should not have been handled by the content.",
                    mTestSupport.handleBackPress());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_noIntercept_Refactored() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBackInterceptingContent.setHandleBackPress(false));
        requestContentInSheet(mBackInterceptingContent, true);
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the peeking state.", SheetState.PEEK,
                    mSheetController.getSheetState());
            assertFalse("Bottom sheet controller should not be ready for handling back press.",
                    getBackPressState());
        });
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_noIntercept_sheetOpen() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBackInterceptingContent.setHandleBackPress(false));
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();

        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the half state.", SheetState.HALF,
                    mSheetController.getSheetState());
            assertFalse("The back event should not be handled by the content.",
                    mBackInterceptingContent.handleBackPress());
            assertTrue("The back event should still be handled by the controller.",
                    mTestSupport.handleBackPress());
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be peeking if the content didn't handle the back event.",
                SheetState.PEEK, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.BACK_GESTURE_REFACTOR})
    public void testHandleBackpress_noIntercept_sheetOpen_Refactored() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBackInterceptingContent.setHandleBackPress(false));
        mActivity.getBackPressManagerForTesting().resetLastCalledHandlerForTesting();
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();
        // Fake a back button press on the controller.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the half state.", SheetState.HALF,
                    mSheetController.getSheetState());
            assertFalse("The back event should not be handled by the content.",
                    mBackInterceptingContent.getBackPressStateChangedSupplier().get());
            assertTrue("Bottom sheet controller should be ready for handling back press.",
                    getBackPressState());
            mSheetController.getBottomSheetBackPressHandler().handleBackPress();
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be peeking if the content didn't handle the back event.",
                SheetState.PEEK, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPress_withCustomScrimLifecycle() {
        mPeekableContent.setHasCustomScrimLifecycle(true);
        requestContentInSheet(mPeekableContent, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mPeekableContent.setHandleBackPress(true); });
        expandSheet();
        assertEquals("The bottom sheet should be expanded.", SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("Back press event should be consumed", Boolean.TRUE, getBackPressState());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.handleBackPress();
            mTestSupport.endAllAnimations();
        });
    }

    @Test
    @MediumTest
    public void testSheetPriorityDuringSuppression() throws ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.expandSheet();
            mTestSupport.endAllAnimations();
        });

        assertTrue("The sheet should be open.", mSheetController.isSheetOpen());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mSuppressionToken = mTestSupport.suppressSheet(StateChangeReason.NONE));

        assertEquals(
                "The sheet should be hidden.", SheetState.HIDDEN, mSheetController.getSheetState());

        requestContentInSheet(mHighPriorityContent, true);

        assertEquals("The sheet should still be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.unsuppressSheet(mSuppressionToken));

        assertEquals("The high priority content should be shown.", mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testOpenTwiceWhileInQueue() {
        requestContentInSheet(mHighPriorityContent, true);
        expandSheet();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // While high priority content is visible, request new content to be shown twice.
            mSheetController.requestShowContent(mLowPriorityContent, false);
            mSheetController.requestShowContent(mLowPriorityContent, false);

            // Now hide high priority content, this should cause low priority content to be shown.
            mSheetController.hideContent(mHighPriorityContent, false);
        });
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.PEEK);
        assertEquals("The low priority content should be shown.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.hideContent(mLowPriorityContent, false);
            mTestSupport.endAllAnimations();
        });
        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());
    }

    /**
     * Request content be shown in the bottom sheet and end animations.
     * @param content The content to show.
     * @param expectContentChange If the content is expected to change, setting this to true will
     *                            cause the method to wait for
     *                            BottomSheetObserver#onSheetContentChanged.
     */
    private void requestContentInSheet(BottomSheetContent content, boolean expectContentChange) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(SheetState.HALF, false));
    }

    /** Expand the bottom sheet to it's maximum height. */
    private void maximizeSheet() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(SheetState.FULL, false));
    }

    /**
     * Enter and immediately exit the tab switcher. This function will assert that the sheet is not
     * showing in the tab switcher.
     */
    private void enterAndExitTabSwitcher() throws TimeoutException {
        setTabSwitcherState(true);

        assertEquals("The bottom sheet should be hidden.", SheetState.HIDDEN,
                mSheetController.getSheetState());

        setTabSwitcherState(false);
    }

    /**
     * Set the tab switcher state and wait for that state to be settled.
     * @param shown Whether the tab switcher should be shown.
     */
    private void setTabSwitcherState(boolean shown) {
        @LayoutType
        int targetLayout = shown ? LayoutType.TAB_SWITCHER : LayoutType.BROWSING;
        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivity.getLayoutManager(), targetLayout, false);
        TestThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
    }

    /**
     * Open a new tab behind the active tab and wait for the tab selection event.
     */
    private void openNewTabInBackground() throws TimeoutException {
        CallbackHelper tabSelectedHelper = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModel tabModel = mActivity.getTabModelSelector().getCurrentModel();
            tabModel.addObserver(new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                    tabSelectedHelper.notifyCalled();
                    tabModel.removeObserver(this);
                }
            });
            mActivity.getTabCreator(false).createNewTab(new LoadUrlParams("about:blank"),
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
        });

        tabSelectedHelper.waitForFirst();
        TestThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
    }

    /**
     * Open a new tab in front of the active tab and wait for it to be completely loaded.
     */
    private void openNewTabInForeground() {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity, "about:blank", false);
        TestThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
    }

    /**
     * Whether back press will be consumed by bottom sheet.
     */
    private Boolean getBackPressState() {
        return mSheetController.getBottomSheetBackPressHandler()
                .getHandleBackPressChangedSupplier()
                .get();
    }
}
