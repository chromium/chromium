// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
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

        ThreadUtils.runOnUiThreadBlocking(() -> {
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
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.forceDismissAllContent();
            mTestSupport.endAllAnimations();
        });
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPeek() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        assertEquals("The bottom sheet should be peeking.", BottomSheetController.SheetState.PEEK,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPriorityInPeekState() throws TimeoutException {
        CallbackHelper hideCallbackHelper = new CallbackHelper();
        mSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent content) {
                hideCallbackHelper.notifyCalled();
            }
        });
        requestContentInSheet(mLowPriorityContent, true);
        int callCount = hideCallbackHelper.getCallCount();
        int lowPriorityDestroyCalls = mLowPriorityContent.destroyCallbackHelper.getCallCount();
        requestContentInSheet(mHighPriorityContent, true);
        hideCallbackHelper.waitForCallback(callCount);
        assertEquals("The bottom sheet is showing incorrect content.", mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
        assertEquals("The low priority content should not have been destroyed!",
                lowPriorityDestroyCalls, mLowPriorityContent.destroyCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"BottomSheetController"})
    public void testSheetPriorityInExpandedState() throws TimeoutException, ExecutionException {
        CallbackHelper hideCallbackHelper = new CallbackHelper();
        mSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent content) {
                hideCallbackHelper.notifyCalled();
            }
        });
        int callCount = hideCallbackHelper.getCallCount();
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();
        requestContentInSheet(mHighPriorityContent, false);
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        hideCallbackHelper.waitForCallback(callCount);

        assertEquals("The bottom sheet is showing incorrect content.", mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPressPeekable() throws TimeoutException {
        requestContentInSheet(mPeekableContent, true);
        expandSheet();
        assertEquals("The bottom sheet should be expanded.", BottomSheetController.SheetState.HALF,
                mSheetController.getSheetState());
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.handleBackPress();
            mTestSupport.endAllAnimations();
        });
        assertEquals("The bottom sheet should be peeking.", BottomSheetController.SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testHandleBackPressNonPeekable() throws TimeoutException {
        requestContentInSheet(mNonPeekableContent, true);
        expandSheet();
        assertEquals("The bottom sheet should be expanded.", BottomSheetController.SheetState.HALF,
                mSheetController.getSheetState());
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.handleBackPress();
            mTestSupport.endAllAnimations();
        });
        assertEquals("The bottom sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSheetPeekAfterTabSwitcher() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        CallbackHelper peekCallbackHelper = new CallbackHelper();
        mSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(int newState) {
                if (newState == BottomSheetController.SheetState.PEEK) {
                    peekCallbackHelper.notifyCalled();
                }
            }
        });
        enterAndExitTabSwitcher();
        peekCallbackHelper.waitForCallback(0);
        assertEquals("The bottom sheet should be peeking.", BottomSheetController.SheetState.PEEK,
                mSheetController.getSheetState());
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

        CallbackHelper contentChangeHelper = new CallbackHelper();
        mSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent newContent) {
                contentChangeHelper.notifyCalled();
            }
        });

        // Enter the tab switcher and select a different tab.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getLayoutManager().showOverview(false);
            mTestSupport.endAllAnimations();
            assertEquals("The bottom sheet should be hidden.",
                    BottomSheetController.SheetState.HIDDEN, mSheetController.getSheetState());
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    0, TabSelectionType.FROM_USER);
            mActivity.getLayoutManager().hideOverview(false);
            mTestSupport.endAllAnimations();
        });

        contentChangeHelper.waitForCallback(0);
        assertEquals("The bottom sheet still should be hidden.",
                BottomSheetController.SheetState.HIDDEN, mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testContentDestroyedOnHidden() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        int destroyCallCount = mLowPriorityContent.destroyCallbackHelper.getCallCount();

        // Enter the tab switcher and select a different tab.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.setSheetState(BottomSheetController.SheetState.HIDDEN, false);
        });

        mLowPriorityContent.destroyCallbackHelper.waitForCallback(destroyCallCount);
        assertEquals("The bottom sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
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

        assertEquals("The bottom sheet should be expanded.", BottomSheetController.SheetState.HALF,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", mLowPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSwitchTabs() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals("The bottom sheet should be peeking.", BottomSheetController.SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals("The bottom sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @FlakyTest(message = "https://crbug.com/837809")
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSwitchTabsMultipleTimes() throws TimeoutException {
        final int originalTabIndex = mActivity.getTabModelSelector().getCurrentModel().indexOf(
                mActivity.getActivityTab());
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals("The bottom sheet should be peeking.", BottomSheetController.SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals("The bottom sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTabModelSelector().getCurrentModel().setIndex(
                    originalTabIndex, TabSelectionType.FROM_USER);
        });

        // Request content be shown again.
        requestContentInSheet(mLowPriorityContent, true);
        expandSheet();

        openNewTabInBackground();

        assertEquals("The bottom sheet should be expanded.", BottomSheetController.SheetState.HALF,
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
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                pageLoadStartedHelper.notifyCalled();
            }
        });
        int currentCallCount = pageLoadStartedHelper.getCallCount();
        ChromeTabUtils.loadUrlOnUiThread(tab, "about:blank");
        pageLoadStartedHelper.waitForCallback(currentCallCount, 1);

        ThreadUtils.runOnUiThreadBlocking(mTestSupport::endAllAnimations);
        assertEquals(customLifecycleContent, mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testScrim() throws ExecutionException, TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);

        assertNull("There should currently be no scrim.", mScrimCoordinator.getViewForTesting());

        expandSheet();

        assertEquals("The scrim should be visible.", View.VISIBLE,
                ((View) mScrimCoordinator.getViewForTesting()).getVisibility());

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertNull("There should be no scrim when the sheet is closed.",
                mScrimCoordinator.getViewForTesting());
    }

    @Test
    @MediumTest
    public void testCustomScrimLifecycle() throws TimeoutException {
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

        CallbackHelper contentChangedHelper = new CallbackHelper();
        mSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent content) {
                contentChangedHelper.notifyCalled();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mTestSupport.setSheetState(
                                BottomSheetController.SheetState.HIDDEN, false));

        contentChangedHelper.waitForCallback(0, 1);

        assertEquals("The sheet's content should be null!", null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testScrimTapClosesSheet() throws TimeoutException, ExecutionException {
        requestContentInSheet(mHighPriorityContent, true);
        CallbackHelper closedCallbackHelper = new CallbackHelper();
        BottomSheetObserver observer = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                closedCallbackHelper.notifyCalled();
            }
        };
        mSheetController.addObserver(observer);

        expandSheet();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> ((View) mScrimCoordinator.getViewForTesting()).callOnClick());

        closedCallbackHelper.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testCustomHalfRatio() throws TimeoutException {
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
    public void testCustomFullRatio() throws TimeoutException {
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
    public void testExpandWithDisabledHalfState() throws TimeoutException {
        mLowPriorityContent.setHalfHeightRatio(BottomSheetContent.HeightMode.DISABLED);
        requestContentInSheet(mLowPriorityContent, true);

        expandSheet();

        assertEquals("The bottom sheet should be at the full state when half is disabled.",
                BottomSheetController.SheetState.FULL, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testCollapseSheet() throws TimeoutException, ExecutionException {
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertEquals("The bottom sheet should be at the peeking state.",
                BottomSheetController.SheetState.PEEK, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testCollapseSheet_peekDisabled() throws TimeoutException, ExecutionException {
        mLowPriorityContent.setPeekHeight(BottomSheetContent.HeightMode.DISABLED);
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(() -> mSheetController.collapseSheet(false));

        assertEquals("The bottom sheet should be at the half state when peek is disabled.",
                BottomSheetController.SheetState.HALF, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testHandleBackpress() throws TimeoutException {
        requestContentInSheet(mBackInterceptingContent, true);

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the peeking state.",
                    BottomSheetController.SheetState.PEEK, mSheetController.getSheetState());
            assertTrue("The back event should have been handled by the content.",
                    mTestSupport.handleBackPress());
            mTestSupport.endAllAnimations();
        });
    }

    @Test
    @MediumTest
    public void testHandleBackpress_sheetOpen() throws TimeoutException {
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the half state.",
                    BottomSheetController.SheetState.HALF, mSheetController.getSheetState());
            assertTrue("The back event should not have been handled by the content.",
                    mTestSupport.handleBackPress());
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be at the half state if the content handled the back event.",
                BottomSheetController.SheetState.HALF, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testHandleBackpress_noIntercept() throws TimeoutException {
        mBackInterceptingContent.setHandleBackPress(false);
        requestContentInSheet(mBackInterceptingContent, true);

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the peeking state.",
                    BottomSheetController.SheetState.PEEK, mSheetController.getSheetState());
            assertFalse("The back event should not have been handled by the content.",
                    mTestSupport.handleBackPress());
        });
    }

    @Test
    @MediumTest
    public void testHandleBackpress_noIntercept_sheetOpen() throws TimeoutException {
        mBackInterceptingContent.setHandleBackPress(false);
        requestContentInSheet(mBackInterceptingContent, true);
        expandSheet();

        // Fake a back button press on the controller.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals("The sheet should be in the half state.",
                    BottomSheetController.SheetState.HALF, mSheetController.getSheetState());
            assertFalse("The back event should not be handled by the content.",
                    mBackInterceptingContent.handleBackPress());
            assertTrue("The back event should still be handled by the controller.",
                    mTestSupport.handleBackPress());
            mTestSupport.endAllAnimations();
        });

        assertEquals("The sheet should be peeking if the content didn't handle the back event.",
                BottomSheetController.SheetState.PEEK, mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testSheetPriorityDuringSuppression() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.expandSheet();
            mTestSupport.endAllAnimations();
        });

        assertTrue("The sheet should be open.", mSheetController.isSheetOpen());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSuppressionToken =
                    mTestSupport.suppressSheet(BottomSheetController.StateChangeReason.NONE);
        });

        assertEquals("The sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());

        requestContentInSheet(mHighPriorityContent, true);

        assertEquals("The sheet should still be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());

        ThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.unsuppressSheet(mSuppressionToken));

        assertEquals("The high priority content should be shown.", mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
    }

    /**
     * Request content be shown in the bottom sheet and end animations.
     * @param content The content to show.
     * @param expectContentChange If the content is expected to change, setting this to true will
     *                            cause the method to wait for
     *                            BottomSheetObserver#onSheetContentChanged.
     */
    private void requestContentInSheet(BottomSheetContent content, boolean expectContentChange)
            throws TimeoutException {
        CallbackHelper contentChangedHelper = new CallbackHelper();
        mSheetController.addObserver(new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent content) {
                contentChangedHelper.notifyCalled();
            }
        });
        int currentCallCount = contentChangedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSheetController.requestShowContent(content, false);
                    mTestSupport.endAllAnimations();
                });

        if (expectContentChange) contentChangedHelper.waitForCallback(currentCallCount, 1);
    }

    /**
     * Expand the bottom sheet to a non-peek height. If the sheet has no content, an assert is
     * thrown.
     */
    private void expandSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.HALF, false));
    }

    /** Expand the bottom sheet to it's maximum height. */
    private void maximizeSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.FULL, false));
    }

    /**
     * Enter and immediately exit the tab switcher. This function will assert that the sheet is not
     * showing in the tab switcher.
     */
    private void enterAndExitTabSwitcher() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getLayoutManager().showOverview(false);
            mTestSupport.endAllAnimations();
            assertEquals("The bottom sheet should be hidden.",
                    BottomSheetController.SheetState.HIDDEN, mSheetController.getSheetState());
            mActivity.getLayoutManager().hideOverview(false);
            mTestSupport.endAllAnimations();
        });
    }

    /**
     * Open a new tab behind the active tab and wait for the tab selection event.
     */
    private void openNewTabInBackground() throws TimeoutException {
        CallbackHelper tabSelectedHelper = new CallbackHelper();
        mActivity.getTabModelSelector().getCurrentModel().addObserver(new TabModelObserver() {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                tabSelectedHelper.notifyCalled();
            }
        });

        int previousCallCount = tabSelectedHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTabCreator(false).createNewTab(new LoadUrlParams("about:blank"),
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
        });

        tabSelectedHelper.waitForCallback(previousCallCount, 1);
        ThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }

    /**
     * Open a new tab in front of the active tab and wait for it to be completely loaded.
     */
    private void openNewTabInForeground() {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(), mActivity, "about:blank", false);
        ThreadUtils.runOnUiThreadBlocking(() -> mTestSupport.endAllAnimations());
    }
}
