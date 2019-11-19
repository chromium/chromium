// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Tests for ScreenshotTabObserver class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ScreenshotTabObserverTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

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

    // Disabled due to flakiness. https://crbug.com/901856
    @Test
    @SmallTest
    @DisabledTest
    public void testScreenshotNumberReportingOne() {
        MetricsUtils.HistogramDelta histogramDeltaZeroScreenshots =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 0);
        MetricsUtils.HistogramDelta histogramDeltaOneScreenshot =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 1);
        MetricsUtils.HistogramDelta histogramDeltaTwoScreenshots =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 2);
        mObserver.onScreenshotTaken();
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> mTab.destroy());
        // Check the first 3 buckets of the NumberOfScrenshots metric.
        Assert.assertEquals("Should be no pages with zero snapshots reported", 0,
                histogramDeltaZeroScreenshots.getDelta());
        Assert.assertEquals("Should be one page with one snapshot reported", 1,
                histogramDeltaOneScreenshot.getDelta());
        Assert.assertEquals("Should be no pages with two snapshots reported", 0,
                histogramDeltaTwoScreenshots.getDelta());
    }

    // Disabled due to flakiness. https://crbug.com/901856
    @Test
    @SmallTest
    @DisabledTest
    public void testScreenshotNumberReportingTwo() {
        MetricsUtils.HistogramDelta histogramDeltaTwoScreenshots =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.ScreenshotsPerPage", 2);
        mObserver.onScreenshotTaken();
        mObserver.onScreenshotTaken();
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> mTab.destroy());
        Assert.assertEquals("Should be one page with two snapshots reported", 1,
                histogramDeltaTwoScreenshots.getDelta());
    }

    // Disabled due to flakiness. https://crbug.com/901856
    @Test
    @SmallTest
    @DisabledTest
    public void testScreenshotActionReporting() {
        MetricsUtils.HistogramDelta histogramDeltaScreenshotNoAction =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.Action", 0);
        MetricsUtils.HistogramDelta histogramDeltaScreenshotShareAction =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.Action", 1);
        MetricsUtils.HistogramDelta histogramDeltaScreenshotDownloadIPHAction =
                new MetricsUtils.HistogramDelta("Tab.Screenshot.Action", 2);
        mObserver.onScreenshotTaken();
        mObserver.onActionPerformedAfterScreenshot(ScreenshotTabObserver.SCREENSHOT_ACTION_SHARE);
        TestThreadUtils.runOnUiThreadBlocking((Runnable) () -> mTab.destroy());
        Assert.assertEquals("Should be no none actions reported", 0,
                histogramDeltaScreenshotNoAction.getDelta());
        Assert.assertEquals("Should be one share action reported", 1,
                histogramDeltaScreenshotShareAction.getDelta());
        Assert.assertEquals("Should be no download IPH actions reported", 0,
                histogramDeltaScreenshotDownloadIPHAction.getDelta());
    }
}
