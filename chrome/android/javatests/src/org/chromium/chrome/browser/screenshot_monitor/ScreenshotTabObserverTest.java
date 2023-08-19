// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for ScreenshotTabObserver class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ScreenshotTabObserverTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private ScreenshotTabObserver mObserver;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                (Runnable) () -> mObserver = ScreenshotTabObserver.from(mTab));
    }

    @Test
    @SmallTest
    @DisabledTest
    public void testScreenshotUserCounts() {
        UserActionTester userActionTester = new UserActionTester();
        mObserver.onScreenshotTaken();
        // Must wait for the user action to arrive on the UI thread before checking it.
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> {
            List<String> actions = userActionTester.getActions();
            Assert.assertEquals("Tab.Screenshot", actions.get(0));
        });
    }

    @Test
    @SmallTest
    public void testScreenshotNumberReportingOne() throws TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Screenshot.ScreenshotsPerPage", 1);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();

        mObserver.onScreenshotTaken();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getTabModelSelector().closeTab(mTab); });
        callbackHelper.waitForCallback(count);

        histogramWatcher.assertExpected("Should be one page with one snapshot reported.");
    }

    @Test
    @SmallTest
    public void testScreenshotNumberReportingTwo() throws TimeoutException {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Tab.Screenshot.ScreenshotsPerPage", 2);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();

        mObserver.onScreenshotTaken();
        mObserver.onScreenshotTaken();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getTabModelSelector().closeTab(mTab); });
        callbackHelper.waitForCallback(count);

        histogramWatcher.assertExpected("Should be one page with two snapshots reported.");
    }

    @Test
    @SmallTest
    public void testScreenshotActionReporting() throws TimeoutException {
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher("Tab.Screenshot.Action", 1);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();

        mObserver.onScreenshotTaken();
        mObserver.onActionPerformedAfterScreenshot(ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getTabModelSelector().closeTab(mTab); });
        callbackHelper.waitForCallback(count);

        histogramWatcher.assertExpected(
                "Should be one share action reported, but none of the other types.");
    }

    private void setupOnReportCompleteCallbackHelper(CallbackHelper callbackHelper) {
        mObserver.setOnReportCompleteForTesting(new Runnable() {
            @Override
            public void run() {
                callbackHelper.notifyCalled();
            }
        });
    }
}
