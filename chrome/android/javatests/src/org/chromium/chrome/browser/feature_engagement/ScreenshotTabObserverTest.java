// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MetricsUtils;
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
@DisabledTest(message = "https://crbug.com/1305417")
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
        MetricsUtils.HistogramDelta histogramDeltaZeroScreenshots =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 0);
        MetricsUtils.HistogramDelta histogramDeltaOneScreenshot =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 1);
        MetricsUtils.HistogramDelta histogramDeltaTwoScreenshots =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 2);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();
        mObserver.onScreenshotTaken();
        TestThreadUtils.runOnUiThreadBlocking(mTab::destroy);
        callbackHelper.waitForCallback(count);
        // Check the first 3 buckets of the NumberOfScrenshots metric.
        Assert.assertEquals("Should be no pages with zero snapshots reported", 0,
                histogramDeltaZeroScreenshots.getDelta());
        Assert.assertEquals("Should be one page with one snapshot reported", 1,
                histogramDeltaOneScreenshot.getDelta());
        Assert.assertEquals("Should be no pages with two snapshots reported", 0,
                histogramDeltaTwoScreenshots.getDelta());
    }

    @Test
    @SmallTest
    public void testScreenshotNumberReportingTwo() throws TimeoutException {
        MetricsUtils.HistogramDelta histogramDeltaTwoScreenshots =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 2);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();
        mObserver.onScreenshotTaken();
        mObserver.onScreenshotTaken();
        TestThreadUtils.runOnUiThreadBlocking(mTab::destroy);
        callbackHelper.waitForCallback(count);
        Assert.assertEquals("Should be one page with two snapshots reported", 1,
                histogramDeltaTwoScreenshots.getDelta());
    }

    @Test
    @SmallTest
    public void testScreenshotActionReporting() throws TimeoutException {
        MetricsUtils.HistogramDelta histogramDeltaScreenshotNoAction =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.Action", 0);
        MetricsUtils.HistogramDelta histogramDeltaScreenshotShareAction =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.Action", 1);
        MetricsUtils.HistogramDelta histogramDeltaScreenshotDownloadIPHAction =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.Action", 2);
        CallbackHelper callbackHelper = new CallbackHelper();
        setupOnReportCompleteCallbackHelper(callbackHelper);
        int count = callbackHelper.getCallCount();
        mObserver.onScreenshotTaken();
        mObserver.onActionPerformedAfterScreenshot(ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        TestThreadUtils.runOnUiThreadBlocking(mTab::destroy);
        callbackHelper.waitForCallback(count);
        Assert.assertEquals("Should be no none actions reported", 0,
                histogramDeltaScreenshotNoAction.getDelta());
        Assert.assertEquals("Should be one share action reported", 1,
                histogramDeltaScreenshotShareAction.getDelta());
        Assert.assertEquals("Should be no download IPH actions reported", 0,
                histogramDeltaScreenshotDownloadIPHAction.getDelta());
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
