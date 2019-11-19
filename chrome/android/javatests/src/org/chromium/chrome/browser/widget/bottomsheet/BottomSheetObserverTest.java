// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // Bottom sheet is only used on phones.
public class BottomSheetObserverTest {
    @Rule
    public BottomSheetTestRule mBottomSheetTestRule = new BottomSheetTestRule();
    private BottomSheetTestRule.Observer mObserver;
    private TestBottomSheetContent mSheetContent;

    @Before
    public void setUp() throws Exception {
        BottomSheet.setSmallScreenForTesting(false);
        mBottomSheetTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent = new TestBottomSheetContent(mBottomSheetTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.HIGH, false);
            mBottomSheetTestRule.getBottomSheetController().requestShowContent(
                    mSheetContent, false);
        });
        mObserver = mBottomSheetTestRule.getObserver();
    }

    /** Test that the onSheetClosed event is triggered if the sheet is closed without animation. */
    @Test
    @MediumTest
    public void testCloseEventCalled_noAnimation() throws TimeoutException {
        runCloseEventTest(false, true);
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed without animation and
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testCloseEventCalled_noAnimationNoPeekState() throws TimeoutException {
        runCloseEventTest(false, false);
    }

    /** Test that the onSheetClosed event is triggered if the sheet is closed with animation. */
    @Test
    @MediumTest
    public void testCloseEventCalled_withAnimation() throws TimeoutException {
        runCloseEventTest(true, true);
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed with animation but
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testCloseEventCalled_withAnimationNoPeekState() throws TimeoutException {
        runCloseEventTest(true, false);
    }

    /**
     * Run different versions of the onSheetClosed event test.
     * @param animationEnabled Whether to run the test with animation.
     * @param peekStateEnabled Whether the sheet's content has a peek state.
     */
    private void runCloseEventTest(boolean animationEnabled, boolean peekStateEnabled)
            throws TimeoutException {
        CallbackHelper hiddenHelper = mObserver.mHiddenCallbackHelper;
        int initialHideEvents = hiddenHelper.getCallCount();

        mBottomSheetTestRule.setSheetState(BottomSheetController.SheetState.FULL, false);

        mSheetContent.setPeekHeight(peekStateEnabled ? BottomSheetContent.HeightMode.DEFAULT
                                                     : BottomSheetContent.HeightMode.DISABLED);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();

        int targetState = peekStateEnabled ? BottomSheetController.SheetState.PEEK
                                           : BottomSheetController.SheetState.HIDDEN;
        mBottomSheetTestRule.setSheetState(targetState, animationEnabled);

        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        if (targetState == BottomSheetController.SheetState.HIDDEN) {
            hiddenHelper.waitForCallback(initialHideEvents, 1);
        }

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals("Close event should have only been called once.",
                closedCallbackCount + 1, closedCallbackHelper.getCallCount());
    }

    /** Test that the onSheetOpened event is triggered if the sheet is opened without animation. */
    @Test
    @MediumTest
    public void testOpenedEventCalled_noAnimation() throws TimeoutException {
        runOpenEventTest(false, true);
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened without animation and
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalled_noAnimationNoPeekState() throws TimeoutException {
        runOpenEventTest(false, false);
    }

    /** Test that the onSheetOpened event is triggered if the sheet is opened with animation. */
    @Test
    @MediumTest
    public void testOpenedEventCalled_withAnimation() throws TimeoutException {
        runOpenEventTest(true, true);
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened with animation and
     * without a peek state.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalled_withAnimationNoPeekState() throws TimeoutException {
        runOpenEventTest(true, false);
    }

    /**
     * Run different versions of the onSheetOpened event test.
     * @param animationEnabled Whether to run the test with animation.
     * @param peekStateEnabled Whether the sheet's content has a peek state.
     */
    private void runOpenEventTest(boolean animationEnabled, boolean peekStateEnabled)
            throws TimeoutException {
        mSheetContent.setPeekHeight(peekStateEnabled ? BottomSheetContent.HeightMode.DEFAULT
                                                     : BottomSheetContent.HeightMode.DISABLED);

        CallbackHelper fullCallbackHelper = mObserver.mFullCallbackHelper;
        int initialFullCount = fullCallbackHelper.getCallCount();
        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;
        int openedCallbackCount = openedCallbackHelper.getCallCount();
        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;
        int initialClosedCount = closedCallbackHelper.getCallCount();

        mBottomSheetTestRule.setSheetState(
                mBottomSheetTestRule.getBottomSheet().getOpeningState(), false);

        assertNotEquals("Sheet should not be hidden.",
                mBottomSheetTestRule.getBottomSheet().getSheetState(),
                BottomSheetController.SheetState.HIDDEN);
        if (!peekStateEnabled) {
            assertNotEquals("Sheet should be above the peeking state when peek is disabled.",
                    mBottomSheetTestRule.getBottomSheet().getSheetState(),
                    BottomSheetController.SheetState.PEEK);
        }

        mBottomSheetTestRule.setSheetState(BottomSheetController.SheetState.FULL, animationEnabled);

        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);
        fullCallbackHelper.waitForCallback(initialFullCount, 1);

        assertEquals("Open event should have only been called once.",
                openedCallbackCount + 1, openedCallbackHelper.getCallCount());

        assertEquals(initialClosedCount, closedCallbackHelper.getCallCount());
    }

    /**
     * Test the onOffsetChanged event.
     */
    @Test
    @MediumTest
    public void testOffsetChangedEvent() throws TimeoutException {
        mBottomSheetTestRule.setSheetState(BottomSheetController.SheetState.FULL, false);
        CallbackHelper callbackHelper = mObserver.mOffsetChangedCallbackHelper;

        BottomSheet bottomSheet = mBottomSheetTestRule.getBottomSheet();
        float hiddenHeight = bottomSheet.getHiddenRatio() * bottomSheet.getSheetContainerHeight();
        float fullHeight = bottomSheet.getFullRatio() * bottomSheet.getSheetContainerHeight();

        // The sheet's half state is not necessarily 50% of the way to the top.
        float midPeekFull = (hiddenHeight + fullHeight) / 2f;

        // When in the hidden state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(hiddenHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // When in the full state, the transition value should be 1.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(fullHeight);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // Halfway between peek and full should send 0.5.
        callbackCount = callbackHelper.getCallCount();
        mBottomSheetTestRule.setSheetOffsetFromBottom(midPeekFull);
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);
    }

    @Test
    @MediumTest
    public void testLoadUrlEvent() throws TimeoutException, ExecutionException {
        int initialCount = mObserver.mLoadUrlCallbackHelper.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mBottomSheetTestRule.getBottomSheet().loadUrl(
                                new LoadUrlParams(UrlConstants.ABOUT_URL), false));

        mObserver.mLoadUrlCallbackHelper.waitForCallback(initialCount);

        assertEquals("onLoadUrl event should have been called a single time", initialCount + 1,
                mObserver.mLoadUrlCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testWrapContentBehavior() throws TimeoutException {
        // We make sure the height of the wrapped content is smaller than sheetContainerHeight.
        BottomSheet bottomSheet = mBottomSheetTestRule.getBottomSheet();
        int wrappedContentHeight = (int) bottomSheet.getSheetContainerHeight() / 2;
        assertTrue(wrappedContentHeight > 0);

        // Show content that should be wrapped.
        CallbackHelper callbackHelper = mObserver.mContentChangedCallbackHelper;
        int callCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            bottomSheet.showContent(new TestBottomSheetContent(mBottomSheetTestRule.getActivity(),
                    BottomSheetContent.ContentPriority.HIGH, false) {
                private final ViewGroup mContentView;

                {
                    // We wrap the View in a FrameLayout as we need something to read the hard coded
                    // height in the layout params. There is no way to create a View with a specific
                    // height on its own as View::onMeasure will by default set its height/width to
                    // be the minimum height/width of its background (if any) or expand as much as
                    // it can.
                    mContentView = new FrameLayout(mBottomSheetTestRule.getActivity());
                    View child = new View(mBottomSheetTestRule.getActivity());
                    child.setLayoutParams(new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT, wrappedContentHeight));
                    mContentView.addView(child);
                }

                @Override
                public View getContentView() {
                    return mContentView;
                }

                @Override
                public float getFullHeightRatio() {
                    return HeightMode.WRAP_CONTENT;
                }
            });
        });
        callbackHelper.waitForCallback(callCount);

        // HALF state is forbidden when wrapping the content.
        mBottomSheetTestRule.setSheetState(BottomSheetController.SheetState.HALF, false);
        assertEquals(BottomSheetController.SheetState.FULL, bottomSheet.getSheetState());

        // Check the offset.
        assertEquals(wrappedContentHeight + bottomSheet.getToolbarShadowHeight(),
                bottomSheet.getCurrentOffsetPx(), MathUtils.EPSILON);
    }
}
