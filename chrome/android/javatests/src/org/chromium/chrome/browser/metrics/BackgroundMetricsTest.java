// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests relating to UMA reported while Chrome is backgrounded.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING})
public final class BackgroundMetricsTest {
    // Note: these rules might conflict and so calls to their methods must be handled carefully.
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() {}

    private void waitForHistogram(String name, int count) {
        CriteriaHelper.pollUiThread(() -> {
            return RecordHistogram.getHistogramTotalCountForTesting(name) >= count;
        }, "waitForHistogram timeout", 10000, 200);
    }

    public void pressHome() {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressHome();
    }

    private void loadNative() {
        final AtomicBoolean mNativeLoaded = new AtomicBoolean();
        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                mNativeLoaded.set(true);
            }
        };
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        });
        CriteriaHelper.pollUiThread(
                () -> mNativeLoaded.get(), "Failed while waiting for starting native.");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"disable-features=UMABackgroundSessions"})
    public void testBackgroundSessionIsRecordedWithBackgroundSessionsDisabled() throws Throwable {
        // Start Chrome.
        mActivityTestRule.startMainActivityOnBlankPage();

        // Background Chrome and wait for a session to be recorded.
        pressHome();
        waitForHistogram("Session.TotalDuration", 1);
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Session.Background.TotalDuration"));

        // Foreground Chrome, and verify a background session is recorded.
        mActivityTestRule.startMainActivityOnBlankPage();
        waitForHistogram("Session.Background.TotalDuration", 1);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=UMABackgroundSessions"})
    public void testBackgroundSessionIsRecordedWithBackgroundSessionsEnabled() throws Throwable {
        // Start Chrome.
        mActivityTestRule.startMainActivityOnBlankPage();

        // Background Chrome and wait for a session to be recorded.
        pressHome();
        waitForHistogram("Session.TotalDuration", 1);
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Session.Background.TotalDuration"));

        // Foreground Chrome, and verify a background session is recorded.
        mActivityTestRule.startMainActivityOnBlankPage();
        waitForHistogram("Session.Background.TotalDuration", 1);

        // UMABackgroundSessions triggers additional UMA logs to be written, but there's currently
        // not a good way to test this because it requires waiting a significant amount of time
        // for some initialization to occur in metrics_service.cc.
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-features=UMABackgroundSessions"})
    public void testStartInBackgroundRecordsBackgroundSession() throws Throwable {
        // Start native, without an activity.
        loadNative();
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Session.Background.TotalDuration"));

        // Start an activity and verify the background session was recorded.
        mActivityTestRule.startMainActivityOnBlankPage();
        waitForHistogram("Session.Background.TotalDuration", 1);
    }
}
