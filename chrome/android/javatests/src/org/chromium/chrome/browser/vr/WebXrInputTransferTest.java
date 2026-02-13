// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.WebXrArTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.WebXrArTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.ArTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.TransferInputToVizResult;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * Tests that input transfer to Viz is blocked during a WebXR AR session. Modeled after detecting
 * scrolls in //chrome/android/javatests/src/org/chromium/chrome/browser/input/InputOnVizTest.java.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=WebXR,WebXRARModule,LogJsConsoleMessages,InputOnViz"
})
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class WebXrInputTransferTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            ArTestRuleUtils.generateDefaultTestRuleParameters();

    @Rule public RuleChain mRuleChain;

    private final ChromeActivityTestRule mTestRule;
    private WebXrArTestFramework mWebXrArTestFramework;

    private int mLastSeenEventAction = -1;
    private int mScrollOffsetY;

    public WebXrInputTransferTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = ArTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrArTestFramework = new WebXrArTestFramework(mTestRule);
    }

    private void setupInputListeners() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GestureListenerManager.fromWebContents(mTestRule.getWebContents())
                            .addListener(
                                    new GestureStateListener() {
                                        @Override
                                        public void onScrollOffsetOrExtentChanged(
                                                int scrollOffsetY, int scrollExtentY) {
                                            mScrollOffsetY = scrollOffsetY;
                                        }
                                    });
                    mTestRule
                            .getActivity()
                            .getActivityTab()
                            .getContentView()
                            .setOnTouchListener(
                                    new View.OnTouchListener() {
                                        @Override
                                        public boolean onTouch(View view, MotionEvent event) {
                                            mLastSeenEventAction = event.getActionMasked();
                                            return false;
                                        }
                                    });
                });
    }

    private void performScrollAndCheckTransfer(boolean expectTransfer) {
        mLastSeenEventAction = -1;
        int initialScrollOffset = mScrollOffsetY;

        if (!expectTransfer) {
            mWebXrArTestFramework.runJavaScriptOrFail(
                    "numFramesWithInput = 0; numSelectEvents = 0;", POLL_TIMEOUT_SHORT_MS);
        }

        HistogramWatcher histograms;
        if (expectTransfer) {
            histograms =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    "Android.InputOnViz.Browser.TransferInputToVizResult2",
                                    TransferInputToVizResult.SUCCESSFULLY_TRANSFERRED)
                            .build();
        } else {
            histograms =
                    HistogramWatcher.newBuilder()
                            .expectIntRecord(
                                    "Android.InputOnViz.Browser.TransferInputToVizResult2",
                                    TransferInputToVizResult.XR_IS_ACTIVE)
                            .build();
        }

        UiAutomatorUtils.getInstance().swipeUpVertically(0.3f);

        if (expectTransfer) {
            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        return mLastSeenEventAction == MotionEvent.ACTION_CANCEL;
                    });
            Assert.assertTrue(mScrollOffsetY >= initialScrollOffset);
        } else {
            // Verify that WebXR AR did get the input by checking that the JS side saw at least one
            // frame with an input source and only one click.
            mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                    "numFramesWithInput >= 1", POLL_TIMEOUT_SHORT_MS);
            mWebXrArTestFramework.pollJavaScriptBooleanOrFail(
                    "numSelectEvents == 1", POLL_TIMEOUT_SHORT_MS);

            // If not transferred, we shouldn't see ACTION_CANCEL from transfer.
            Assert.assertNotEquals(mLastSeenEventAction, MotionEvent.ACTION_CANCEL);
            // We should NOT see a scroll because the input was consumed by the XR overlay.
            Assert.assertEquals(mScrollOffsetY, initialScrollOffset);
        }
        histograms.assertExpected();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.CTA})
    public void testInputTransferBlockedDuringAr() {
        mWebXrArTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_input_transfer", PAGE_LOAD_TIMEOUT_S);
        setupInputListeners();

        // 1. Before session: input should be transferred to Viz.
        performScrollAndCheckTransfer(/* expectTransfer= */ true);

        // 2. During session: input should NOT be transferred to Viz.
        mWebXrArTestFramework.enterSessionWithUserGestureOrFail();
        performScrollAndCheckTransfer(/* expectTransfer= */ false);

        // 3. After session: input should be transferred to Viz.
        mWebXrArTestFramework.endSession();
        performScrollAndCheckTransfer(/* expectTransfer= */ true);
    }
}
