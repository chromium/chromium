// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for ScreenshotTabObserver class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/344675714): Failing when batched, batch this again.
public class ScreenshotTabObserverTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private Tab mTab;
    private ScreenshotTabObserver mObserver;

    @Before
    public void setUp() throws Exception {
        mTab = sActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> mObserver = ScreenshotTabObserver.from(mTab));
    }

    @Test
    @MediumTest
    public void testScreenshotUserCounts() {
        UserActionTester userActionTester = new UserActionTester();
        mObserver.onScreenshotTaken();
        // Must wait for the user action to arrive on the UI thread before checking it.
        CriteriaHelper.pollUiThread(
                () -> {
                    List<String> actions = userActionTester.getActions();
                    Criteria.checkThat(actions, Matchers.hasItem("Tab.Screenshot"));
                });
    }

    @Test
    @MediumTest
    public void testScreenshotNumberReportingOne() throws TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Screenshot.ScreenshotsPerPage", 1);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();

        mObserver.onScreenshotTaken();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule.getActivity().getTabModelSelector().closeTab(mTab);
                });
        callbackHelper.waitForCallback(count);

        histogramWatcher.assertExpected("Should be one page with one snapshot reported.");
    }

    @Test
    @MediumTest
    public void testScreenshotNumberReportingTwo() throws TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Screenshot.ScreenshotsPerPage", 2);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();

        mObserver.onScreenshotTaken();
        mObserver.onScreenshotTaken();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule.getActivity().getTabModelSelector().closeTab(mTab);
                });
        callbackHelper.waitForCallback(count);

        histogramWatcher.assertExpected("Should be one page with two snapshots reported.");
    }

    @Test
    @MediumTest
    public void testScreenshotActionReporting() throws TimeoutException {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher("Tab.Screenshot.Action", 1);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();

        mObserver.onScreenshotTaken();
        mObserver.onActionPerformedAfterScreenshot(ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule.getActivity().getTabModelSelector().closeTab(mTab);
                });
        callbackHelper.waitForCallback(count);

        histogramWatcher.assertExpected(
                "Should be one share action reported, but none of the other types.");
    }

    private void setupOnReportCompleteCallbackHelper(CallbackHelper callbackHelper) {
        mObserver.setOnReportCompleteForTesting(
                new Runnable() {
                    @Override
                    public void run() {
                        callbackHelper.notifyCalled();
                    }
                });
    }
}
