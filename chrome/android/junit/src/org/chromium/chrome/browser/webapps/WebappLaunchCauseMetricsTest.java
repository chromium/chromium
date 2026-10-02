// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import android.app.Activity;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics.LaunchCause;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;

/** Tests basic functionality of WebappLaunchCauseMetrics. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public final class WebappLaunchCauseMetricsTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Activity mActivity;
    @Mock private WebappInfo mWebappInfo;

    @Before
    public void setUp() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
    }

    @After
    public void tearDown() {
        ApplicationStatus.resetActivitiesForInstrumentationTests();
        LaunchCauseMetrics.resetForTests();
    }

    @Test
    public void testHomescreenLaunch() throws Throwable {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCause.WEBAPK_CHROME_DISTRIBUTOR);
        Mockito.when(mWebappInfo.isLaunchedFromHomescreen()).thenReturn(true);
        Mockito.when(mWebappInfo.isForWebApk()).thenReturn(true);
        Mockito.when(mWebappInfo.distributor()).thenReturn(WebApkDistributor.BROWSER);

        WebappLaunchCauseMetrics metrics = new WebappLaunchCauseMetrics(mActivity, mWebappInfo);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        assertEquals(LaunchCause.WEBAPK_CHROME_DISTRIBUTOR, launchCause);

        LaunchCauseMetrics.resetForTests();

        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCause.WEBAPK_OTHER_DISTRIBUTOR);
        Mockito.when(mWebappInfo.distributor()).thenReturn(WebApkDistributor.OTHER);
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        assertEquals(LaunchCause.WEBAPK_OTHER_DISTRIBUTOR, launchCause);

        LaunchCauseMetrics.resetForTests();

        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCause.WEBAPK_CHROME_DISTRIBUTOR);
        Mockito.when(mWebappInfo.isForWebApk()).thenReturn(false);
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        assertEquals(LaunchCause.WEBAPK_CHROME_DISTRIBUTOR, launchCause);
    }

    @Test
    public void testViewIntentLaunch() throws Throwable {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCause.EXTERNAL_VIEW_INTENT);
        Mockito.when(mWebappInfo.isLaunchedFromHomescreen()).thenReturn(false);
        Mockito.when(mWebappInfo.source()).thenReturn(ShortcutSource.EXTERNAL_INTENT);

        WebappLaunchCauseMetrics metrics = new WebappLaunchCauseMetrics(mActivity, mWebappInfo);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        assertEquals(LaunchCause.EXTERNAL_VIEW_INTENT, launchCause);
    }

    @Test
    public void testNullWebAppInfo() throws Throwable {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, LaunchCause.OTHER);

        WebappLaunchCauseMetrics metrics = new WebappLaunchCauseMetrics(mActivity, null);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        assertEquals(LaunchCause.OTHER, launchCause);
    }
}
