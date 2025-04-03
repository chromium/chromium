// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.input;

import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;

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
    public void secondScrollIsHandledOnViz() {
        // Scroll 1 will be handled by the browser. On the browser side it says not to transfer
        // the sequence when the web contents is at 0 offset, which would be the case when the
        // page loads and we are scrolling for the first time.
        UiAutomatorUtils.getInstance().swipeUpVertically(0.3f);
        // Wait for the scroll offset to have changed as a result of scroll.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mScrollOffsetY, Matchers.greaterThan(0));
                });
        // Wait for scroll end.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mScrolling, Matchers.equalTo(false));
                });

        // Scroll 2 should be handled by Viz since the page would be at non-zero offset after
        // the first scroll.
        // This is not completely accurate since it's possible a late
        // running frame updates the scroll offset due to scroll updates from previous scroll.
        // TODO(crbug.com/393576167): Remove this and just check for offset>0, once the first
        // scroll goes to Viz as well.
        int scrollOffsetAfterScroll1 = mScrollOffsetY;
        UiAutomatorUtils.getInstance().swipeUpVertically(0.3f);
        // We should have received a cancel corresponding to a successful touch transfer.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mLastSeenEventAction, Matchers.equalTo(MotionEvent.ACTION_CANCEL));
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mScrollOffsetY, Matchers.greaterThan(scrollOffsetAfterScroll1));
                });
    }
}
