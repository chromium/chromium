// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.input;

import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.TransferInputToVizResult;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

@RunWith(ChromeJUnit4ClassRunner.class)
@MediumTest
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({"InputOnViz"})
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class InputOnVizTest {
    private final String mLongHtmlTestPage =
            UrlUtils.encodeHtmlDataUri("<html><body style='height:100000px;'></body></html>");

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private GestureStateListener mGestureListener;
    private View.OnTouchListener mViewOnTouchListener;
    // Initialized to a value that is not a valid masked MotionEvent action.
    private int mLastSeenEventAction = -1;

    public int mScrollOffsetY;
    private boolean mScrolling;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(mLongHtmlTestPage);
        mGestureListener =
                new GestureStateListener() {
                    @Override
                    public void onScrollStarted(
                            int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                        mScrolling = true;
                    }

                    @Override
                    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                        mScrolling = false;
                    }

                    @Override
                    public void onScrollOffsetOrExtentChanged(
                            int scrollOffsetY, int scrollExtentY) {
                        mScrollOffsetY = scrollOffsetY;
                    }
                };
        mViewOnTouchListener =
                new View.OnTouchListener() {
                    @Override
                    public boolean onTouch(View view, MotionEvent event) {
                        mLastSeenEventAction = event.getActionMasked();
                        // Let the event go through normal view input handling.
                        return false;
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GestureListenerManager.fromWebContents(
                                    mActivityTestRule
                                            .getActivity()
                                            .getActivityTab()
                                            .getWebContents())
                            .addListener(mGestureListener);
                    mActivityTestRule
                            .getActivity()
                            .getActivityTab()
                            .getContentView()
                            .setOnTouchListener(mViewOnTouchListener);
                });
    }

    @Test
    @DisableFeatures({"UseAndroidBufferedInputDispatch", "DropInputEventsWhilePaintHolding"})
    // TODO(https://crbug.com/40057499): Resolve the input handling behavior conflict with
    // DropInputEventsWhilePaintHolding and enable the latter.
    public void scrollIsHandledOnViz_unbufferedInput() {
        checkScrollIsHandledOnViz();
    }

    @Test
    @DisabledTest(message = "https://crbug.com/409346743")
    @EnableFeatures({"UseAndroidBufferedInputDispatch"})
    public void scrollIsHandledOnViz_bufferedInput() {
        checkScrollIsHandledOnViz();
    }

    private void checkScrollIsHandledOnViz() {
        UiAutomatorUtils.getInstance().swipeUpVertically(0.3f);
        // Wait for the scroll offset to have changed as a result of scroll.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mScrollOffsetY, Matchers.greaterThan(0));
                });
        // We should have received a cancel corresponding to a successful touch transfer.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mLastSeenEventAction, Matchers.equalTo(MotionEvent.ACTION_CANCEL));
                });
    }

    @Test
    @DisableFeatures({"DropInputEventsWhilePaintHolding"})
    // TODO(https://crbug.com/40057499): Resolve the input handling behavior conflict with
    // DropInputEventsWhilePaintHolding and enable the latter.
    public void handlesOverscrollsWithInputVizard() throws Exception {
        TabLoadObserver observer =
                new TabLoadObserver(mActivityTestRule.getActivity().getActivityTab());
        observer.fullyLoadUrl(mLongHtmlTestPage);

        UserActionTester userActionTester = new UserActionTester();
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.InputOnViz.Browser.TransferInputToVizResult",
                                TransferInputToVizResult.SUCCESSFULLY_TRANSFERRED)
                        .build();

        WebContentsUtils.waitForCopyableViewInWebContents(
                mActivityTestRule.getActivity().getActivityTab().getWebContents());

        // Scrolling down should trigger refresh effect on the page.
        UiAutomatorUtils.getInstance().swipeDownVertically(0.6f);

        histograms.assertExpected();
        // Asserts that the input sequence ended up triggering OverscrollController which
        // triggered the page to reload.
        observer.assertLoaded();
        Assert.assertEquals(1, userActionTester.getActionCount("MobilePullGestureReload"));
    }
}
