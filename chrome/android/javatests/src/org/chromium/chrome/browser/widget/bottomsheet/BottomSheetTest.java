// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.flags.ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheet}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(DeviceFormFactor.PHONE)
@CommandLineFlags.Add({DISABLE_FIRST_RUN_EXPERIENCE})
public class BottomSheetTest {
    private static final float VELOCITY_WHEN_MOVING_UP = 1.0f;
    private static final float VELOCITY_WHEN_MOVING_DOWN = -1.0f;

    @Rule public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();
    private TestBottomSheetContent mLowPriorityContent;
    private TestBottomSheetContent mHighPriorityContent;
    private BottomSheetController mSheetController;
    private BottomSheetTestSupport mTestSupport;
    private int mSuppressToken;

    @Before
    public void setUp() throws Exception {
        BottomSheetTestSupport.setSmallScreen(false);
        mTestRule.startMainActivityOnBlankPage();
        mSheetController =
                mTestRule.getActivity().getRootUiCoordinatorForTesting().getBottomSheetController();
        mTestSupport = new BottomSheetTestSupport(mSheetController);

        runOnUiThreadBlocking(
                () -> {
                    mLowPriorityContent =
                            new TestBottomSheetContent(
                                    mTestRule.getActivity(),
                                    BottomSheetContent.ContentPriority.LOW,
                                    false);
                    mHighPriorityContent =
                            new TestBottomSheetContent(
                                    mTestRule.getActivity(),
                                    BottomSheetContent.ContentPriority.HIGH,
                                    false);
                });
        mHighPriorityContent.setPeekHeight(HeightMode.DISABLED);
        mHighPriorityContent.setHalfHeightRatio(0.5f);
        mHighPriorityContent.setSkipHalfStateScrollingDown(false);
    }

    @Test
    @MediumTest
    public void testCustomPeekRatio() {
        int customToolbarHeight = TestBottomSheetContent.TOOLBAR_HEIGHT + 50;
        mHighPriorityContent.setPeekHeight(customToolbarHeight);

        showContent(mHighPriorityContent, SheetState.PEEK);

        assertEquals(
                "Sheet should be peeking at the custom height.",
                customToolbarHeight,
                mSheetController.getCurrentOffset());
    }

    @Test
    @MediumTest
    public void testMovingDownFromFullClearsThresholdToReachHalfState() {
        showContent(mHighPriorityContent, SheetState.FULL);

        assertEquals(
                "Sheet should reach half state.",
                SheetState.HALF,
                simulateScrollTo(0.6f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingDownFromFullDoesntClearThresholdToReachHalfState() {
        showContent(mHighPriorityContent, SheetState.FULL);

        assertEquals(
                "Sheet should remain in full state.",
                SheetState.FULL,
                simulateScrollTo(0.9f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingUpFromHalfClearsThresholdToReachFullState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals(
                "Sheet should reach full state.",
                SheetState.FULL,
                simulateScrollTo(0.8f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_UP));
    }

    @Test
    @MediumTest
    public void testMovingUpFromHalfDoesntClearThresholdToReachHalfState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals(
                "Sheet should remain in half state.",
                SheetState.HALF,
                simulateScrollTo(0.6f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_UP));
    }

    @Test
    @MediumTest
    public void testMovingDownFromHalfClearsThresholdToReachHiddenState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals(
                "Sheet should reach hidden state.",
                SheetState.HIDDEN,
                simulateScrollTo(0.2f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testMovingDownFromHalfDoesntClearThresholdToReachHiddenState() {
        showContent(mHighPriorityContent, SheetState.HALF);

        assertEquals(
                "Sheet should remain in half state.",
                SheetState.HALF,
                simulateScrollTo(0.4f * getMaxSheetHeightInPx(), VELOCITY_WHEN_MOVING_DOWN));
    }

    @Test
    @MediumTest
    public void testTabObscuringState() throws TimeoutException {
        CallbackHelper obscuringStateChangedHelper = new CallbackHelper();
        TabObscuringHandler handler = mTestRule.getActivity().getTabObscuringHandler();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    handler.addObserver(
                            (isTabObscured, isToolbarObscured) ->
                                    obscuringStateChangedHelper.notifyCalled());
                });
        mHighPriorityContent.setHasCustomScrimLifecycle(false);

        assertFalse("The tab should not yet be obscured.", handler.isTabContentObscured());
        assertFalse("The toolbar should not yet be obscured.", handler.isToolbarObscured());

        int callCount = obscuringStateChangedHelper.getCallCount();
        showContent(mHighPriorityContent, SheetState.HALF);
        obscuringStateChangedHelper.waitForCallback("The tab should be obscured.", callCount);
        assertTrue("The tab should be obscured.", handler.isTabContentObscured());
        assertTrue("The toolbar should be obscured.", handler.isToolbarObscured());

        callCount = obscuringStateChangedHelper.getCallCount();
        hideSheet();
        obscuringStateChangedHelper.waitForCallback("The tab should not be obscured.", callCount);

        assertFalse("The tab should not be obscured.", handler.isTabContentObscured());
        assertFalse("The toolbar should not yet be obscured.", handler.isToolbarObscured());
    }

    @Test
    @MediumTest
    public void testTabObscuringState_customScrim() throws ExecutionException {
        CallbackHelper obscuringStateChangedHelper = new CallbackHelper();
        TabObscuringHandler handler = mTestRule.getActivity().getTabObscuringHandler();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    handler.addObserver(
                            (isTabObscured, isToolbarObscured) ->
                                    obscuringStateChangedHelper.notifyCalled());
                });
        mHighPriorityContent.setHasCustomScrimLifecycle(true);

        assertFalse("The tab should not be obscured.", handler.isTabContentObscured());

        showContent(mHighPriorityContent, SheetState.HALF);
        assertFalse("The tab should still not be obscured.", handler.isTabContentObscured());

        hideSheet();

        assertEquals(
                "The obscuring handler should not have been called.",
                0,
                obscuringStateChangedHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testOmniboxFocusSuppressesSheet() {
        ToolbarManager toolbarManager =
                mTestRule.getActivity().getRootUiCoordinatorForTesting().getToolbarManager();
        showContent(mHighPriorityContent, SheetState.HALF);

        runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(true, 0));

        assertEquals(
                "The bottom sheet should be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());

        runOnUiThreadBlocking(() -> toolbarManager.setUrlBarFocus(false, 0));

        assertNotEquals(
                "The bottom sheet should not be hidden.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testSuppressionState_unsuppress() {
        showContent(mHighPriorityContent, SheetState.HALF);

        runOnUiThreadBlocking(
                () -> {
                    mSuppressToken = mTestSupport.suppressSheet(StateChangeReason.NONE);
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be in the hidden state.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());

        runOnUiThreadBlocking(
                () -> {
                    mTestSupport.unsuppressSheet(mSuppressToken);
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be restored to the half state.",
                SheetState.HALF,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testSuppressionState_unsuppressDifferentContent() {
        showContent(mHighPriorityContent, SheetState.HALF);

        runOnUiThreadBlocking(
                () -> {
                    mSuppressToken = mTestSupport.suppressSheet(StateChangeReason.NONE);
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be in the hidden state.",
                SheetState.HIDDEN,
                mSheetController.getSheetState());

        runOnUiThreadBlocking(() -> mSheetController.hideContent(mHighPriorityContent, false));

        showContent(mLowPriorityContent, SheetState.PEEK);

        runOnUiThreadBlocking(
                () -> {
                    mTestSupport.unsuppressSheet(mSuppressToken);
                    mTestSupport.endAllAnimations();
                });

        assertEquals(
                "The sheet should be restored to the peek state.",
                SheetState.PEEK,
                mSheetController.getSheetState());
    }

    @Test
    @MediumTest
    public void testWrapContentHeightChange() throws ExecutionException, TimeoutException {
        final int startingHeight = 300;
        final int endingHeight = 400;

        runOnUiThreadBlocking(
                () -> {
                    // Set up content view.
                    final ViewGroup contentView = new FrameLayout(mTestRule.getActivity());
                    View child = new View(mTestRule.getActivity());
                    child.setLayoutParams(
                            new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT, startingHeight));
                    contentView.addView(child);

                    // Set up bottom sheet.
                    TestBottomSheetContent sizeChangingContent =
                            new TestBottomSheetContent(
                                    mTestRule.getActivity(),
                                    ContentPriority.HIGH,
                                    false,
                                    contentView);
                    sizeChangingContent.setFullHeightRatio(HeightMode.WRAP_CONTENT);
                    sizeChangingContent.setHalfHeightRatio(HeightMode.DISABLED);
                    sizeChangingContent.setPeekHeight(HeightMode.DISABLED);

                    // Show content view in bottom sheet.
                    mSheetController.requestShowContent(sizeChangingContent, false);
                });

        BottomSheetTestSupport.waitForState(mSheetController, SheetState.FULL);
        assertEquals(startingHeight, mSheetController.getCurrentOffset());

        // Change the size of the content to make sure the sheet's height reflects the change.
        runOnUiThreadBlocking(
                () -> {
                    ViewGroup contentView =
                            (ViewGroup) mSheetController.getCurrentSheetContent().getContentView();
                    View child = contentView.getChildAt(0);
                    ViewGroup.LayoutParams params =
                            (ViewGroup.LayoutParams) child.getLayoutParams();
                    params.height = endingHeight;
                    child.setLayoutParams(params);
                });
        // Expect SCROLLING state to be reached to the animation caused by the height change.
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.SCROLLING);

        // The bottom sheet should reach FULL state again after the animation is finished.
        BottomSheetTestSupport.waitForState(mSheetController, SheetState.FULL);

        assertEquals(endingHeight, mSheetController.getCurrentOffset());
    }

    private void hideSheet() {
        runOnUiThreadBlocking(() -> mTestSupport.setSheetState(SheetState.HIDDEN, false));
    }

    private float getMaxSheetHeightInPx() {
        return mSheetController.getContainerHeight();
    }

    private @SheetState int simulateScrollTo(float targetHeightInPx, float yUpwardsVelocity) {
        return mTestSupport.forceScrolling(targetHeightInPx, yUpwardsVelocity);
    }

    /**
     * @param content The content to show in the bottom sheet.
     */
    private void showContent(BottomSheetContent content, @SheetState int targetState) {
        runOnUiThreadBlocking(
                () -> {
                    boolean shown = mSheetController.requestShowContent(content, false);
                    if (shown) {
                        mTestSupport.setSheetState(targetState, false);
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
}
