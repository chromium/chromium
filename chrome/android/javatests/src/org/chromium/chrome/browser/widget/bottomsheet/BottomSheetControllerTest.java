// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

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
public class BottomSheetControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mSheetController;
    private TestBottomSheetContent mLowPriorityContent;
    private TestBottomSheetContent mHighPriorityContent;
    private TestBottomSheetContent mPeekableContent;
    private TestBottomSheetContent mNonPeekableContent;
    private ScrimView mScrimView;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup coordinator = activity.findViewById(org.chromium.chrome.R.id.coordinator);
            BottomSheet.setSmallScreenForTesting(false);

            mScrimView = activity.getScrim();

            mSheetController = activity.getBottomSheetController();

            mLowPriorityContent = new TestBottomSheetContent(
                    mActivityTestRule.getActivity(), BottomSheetContent.ContentPriority.LOW, false);
            mHighPriorityContent = new TestBottomSheetContent(mActivityTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.HIGH, false);

            mPeekableContent = new TestBottomSheetContent(mActivityTestRule.getActivity());
            mNonPeekableContent = new TestBottomSheetContent(mActivityTestRule.getActivity());
            mNonPeekableContent.setPeekHeight(BottomSheetContent.HeightMode.DISABLED);
        });
    }

    /** @return The activity's BottomSheet. */
    private BottomSheet getBottomSheet() {
        return (BottomSheet) mSheetController.getBottomSheetViewForTesting();
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
        requestContentInSheet(mHighPriorityContent, true);
        hideCallbackHelper.waitForCallback(callCount);
        assertEquals("The bottom sheet is showing incorrect content.", mHighPriorityContent,
                mSheetController.getCurrentSheetContent());
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
                getBottomSheet().getSheetState());
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.handleBackPress();
            getBottomSheet().endAnimations();
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
                getBottomSheet().getSheetState());
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetController.handleBackPress();
            getBottomSheet().endAnimations();
        });
        assertEquals("The bottom sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"BottomSheetController"})
    public void testSheetPeekAfterTabSwitcher() throws TimeoutException {
        requestContentInSheet(mLowPriorityContent, true);
        enterAndExitTabSwitcher();
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
        Tab tab1 = mActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab tab2 = mActivityTestRule.getActivity().getActivityTab();

        requestContentInSheet(mLowPriorityContent, true);

        // Enter the tab switcher and select a different tab.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManager().showOverview(false);
            getBottomSheet().endAnimations();
            assertEquals("The bottom sheet should be hidden.",
                    BottomSheetController.SheetState.HIDDEN, mSheetController.getSheetState());
            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().setIndex(
                    0, TabSelectionType.FROM_USER);
            mActivityTestRule.getActivity().getLayoutManager().hideOverview(false);
            getBottomSheet().endAnimations();
        });

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
            getBottomSheet().setSheetState(BottomSheetController.SheetState.HIDDEN, false);
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
                getBottomSheet().getSheetState());
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
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        final int originalTabIndex =
                activity.getTabModelSelector().getCurrentModel().indexOf(activity.getActivityTab());
        requestContentInSheet(mLowPriorityContent, true);

        assertEquals("The bottom sheet should be peeking.", BottomSheetController.SheetState.PEEK,
                mSheetController.getSheetState());

        openNewTabInForeground();

        assertEquals("The bottom sheet should be hidden.", BottomSheetController.SheetState.HIDDEN,
                mSheetController.getSheetState());
        assertEquals("The bottom sheet is showing incorrect content.", null,
                mSheetController.getCurrentSheetContent());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getTabModelSelector().getCurrentModel().setIndex(
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

        TestBottomSheetContent customLifecycleContent = new TestBottomSheetContent(
                mActivityTestRule.getActivity(), BottomSheetContent.ContentPriority.LOW, true);
        requestContentInSheet(customLifecycleContent, false);
        assertEquals(mHighPriorityContent, mSheetController.getCurrentSheetContent());

        // Change URL and wait for PageLoadStarted event.
        CallbackHelper pageLoadStartedHelper = new CallbackHelper();
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                pageLoadStartedHelper.notifyCalled();
            }
        });
        int currentCallCount = pageLoadStartedHelper.getCallCount();
        ChromeTabUtils.loadUrlOnUiThread(tab, "about:blank");
        pageLoadStartedHelper.waitForCallback(currentCallCount, 1);

        ThreadUtils.runOnUiThreadBlocking(getBottomSheet()::endAnimations);
        assertEquals(customLifecycleContent, mSheetController.getCurrentSheetContent());
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
                        -> getBottomSheet().setSheetState(
                                BottomSheetController.SheetState.HIDDEN, false));

        contentChangedHelper.waitForCallback(0, 1);

        assertEquals("The sheet's content should be null!", null,
                mSheetController.getCurrentSheetContent());
    }

    @Test
    @MediumTest
    public void testScrimTapClosesSheet() throws TimeoutException, ExecutionException {
        requestContentInSheet(mHighPriorityContent, true);
        BottomSheetTestRule.Observer observer = new BottomSheetTestRule.Observer();
        mSheetController.addObserver(observer);

        expandSheet();

        TestThreadUtils.runOnUiThreadBlocking(() -> mScrimView.performClick());

        observer.mClosedCallbackHelper.waitForCallback(0);
    }

    @Test
    @MediumTest
    public void testCustomHalfRatio() throws TimeoutException {
        final float customHalfHeight = 0.3f;
        int containerHeight =
                mActivityTestRule.getActivity().getActivityTabProvider().get().getHeight();
        mLowPriorityContent.setHalfHeightRatio(customHalfHeight);
        requestContentInSheet(mLowPriorityContent, true);

        expandSheet();

        assertEquals("Half height is incorrect for custom ratio.",
                customHalfHeight * containerHeight, getBottomSheet().getCurrentOffsetPx(),
                MathUtils.EPSILON);
    }

    @Test
    @MediumTest
    public void testCustomFullRatio() throws TimeoutException {
        final float customFullHeight = 0.5f;
        int containerHeight =
                mActivityTestRule.getActivity().getActivityTabProvider().get().getHeight();
        mLowPriorityContent.setFullHeightRatio(customFullHeight);
        requestContentInSheet(mLowPriorityContent, true);

        maximizeSheet();

        assertEquals("Full height is incorrect for custom ratio.",
                customFullHeight * containerHeight, getBottomSheet().getCurrentOffsetPx(),
                MathUtils.EPSILON);
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
                    getBottomSheet().endAnimations();
                });

        if (expectContentChange) contentChangedHelper.waitForCallback(currentCallCount, 1);
    }

    /**
     * Expand the bottom sheet to a non-peek height. If the sheet has no content, an assert is
     * thrown.
     */
    private void expandSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getBottomSheet().setSheetState(BottomSheetController.SheetState.HALF, false));
    }

    /** Expand the bottom sheet to it's maximum height. */
    private void maximizeSheet() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getBottomSheet().setSheetState(BottomSheetController.SheetState.FULL, false));
    }

    /**
     * Enter and immediately exit the tab switcher. This function will assert that the sheet is not
     * showing in the tab switcher.
     */
    private void enterAndExitTabSwitcher() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getLayoutManager().showOverview(false);
            getBottomSheet().endAnimations();
            assertEquals("The bottom sheet should be hidden.",
                    BottomSheetController.SheetState.HIDDEN, getBottomSheet().getSheetState());
            mActivityTestRule.getActivity().getLayoutManager().hideOverview(false);
            getBottomSheet().endAnimations();
        });
    }

    /**
     * Open a new tab behind the active tab and wait for the tab selection event.
     */
    private void openNewTabInBackground() throws TimeoutException {
        CallbackHelper tabSelectedHelper = new CallbackHelper();
        mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().addObserver(
                new EmptyTabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        tabSelectedHelper.notifyCalled();
                    }
                });

        int previousCallCount = tabSelectedHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabCreator(false).createNewTab(
                    new LoadUrlParams("about:blank"), TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                    null);
        });

        tabSelectedHelper.waitForCallback(previousCallCount, 1);
        ThreadUtils.runOnUiThreadBlocking(() -> getBottomSheet().endAnimations());
    }

    /**
     * Open a new tab in front of the active tab and wait for it to be completely loaded.
     */
    private void openNewTabInForeground() {
        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), "about:blank", false);
        ThreadUtils.runOnUiThreadBlocking(() -> getBottomSheet().endAnimations());
    }
}
