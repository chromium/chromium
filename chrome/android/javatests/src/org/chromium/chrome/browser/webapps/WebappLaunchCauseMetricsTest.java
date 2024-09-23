// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics.LaunchCause;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;

/** Tests basic functionality of WebappLaunchCauseMetrics. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class WebappLaunchCauseMetricsTest {
    @Mock private Activity mActivity;
    @Mock private WebappInfo mWebappInfo;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.resetActivitiesForInstrumentationTests();
                    LaunchCauseMetrics.resetForTests();
                });
    }

    @Test
    @SmallTest
    @UiThreadTest
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
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.WEBAPK_CHROME_DISTRIBUTOR, launchCause);

        LaunchCauseMetrics.resetForTests();

        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCause.WEBAPK_OTHER_DISTRIBUTOR);
        Mockito.when(mWebappInfo.distributor()).thenReturn(WebApkDistributor.OTHER);
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.WEBAPK_OTHER_DISTRIBUTOR, launchCause);

        LaunchCauseMetrics.resetForTests();

        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM,
                        LaunchCause.WEBAPK_CHROME_DISTRIBUTOR);
        Mockito.when(mWebappInfo.isForWebApk()).thenReturn(false);
        metrics.onReceivedIntent();
        launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.WEBAPK_CHROME_DISTRIBUTOR, launchCause);
    }

    @Test
    @SmallTest
    @UiThreadTest
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
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.EXTERNAL_VIEW_INTENT, launchCause);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testNullWebAppInfo() throws Throwable {
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, LaunchCause.OTHER);

        WebappLaunchCauseMetrics metrics = new WebappLaunchCauseMetrics(mActivity, null);

        metrics.onReceivedIntent();
        int launchCause = metrics.recordLaunchCause();
        histogram.assertExpected();
        Assert.assertEquals(LaunchCauseMetrics.LaunchCause.OTHER, launchCause);
    }
}
