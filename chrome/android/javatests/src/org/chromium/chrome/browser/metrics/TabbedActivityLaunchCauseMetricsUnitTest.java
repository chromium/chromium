// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.speech.RecognizerResultsIntent;

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
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Unit tests for TabbedActivityLaunchCauseMetrics.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class TabbedActivityLaunchCauseMetricsUnitTest {
    @Mock
    private Activity mActivity;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (!ApplicationStatus.isInitialized()) {
                ApplicationStatus.initialize(BaseJUnit4ClassRunner.getApplication());
            }
            ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        });
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ApplicationStatus.destroyForJUnitTests();
        ThreadUtils.runOnUiThreadBlocking(() -> LaunchCauseMetrics.resetForTests());
    }

    private static int histogramCountForValue(int value) {
        if (!LibraryLoader.getInstance().isInitialized()) return 0;
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    @Test
    @SmallTest
    public void testOpenInBrowserMetrics() throws Throwable {
        int count =
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("about:blank"));
        intent.putExtra(IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, true);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        // Tests the case where Chrome is backgrounded either by the intent picker, or by
        // cross-channel Open In Browser.
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));

        // Ensures we record this metric even when Chrome has already recorded a launch.
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));

        // Ensures we don't record this metric again without a new Intent having been received.
        metrics.recordLaunchCause();
        Assert.assertEquals(count,
                histogramCountForValue(LaunchCauseMetrics.LaunchCause.OPEN_IN_BROWSER_FROM_MENU));

        // Ensures other metrics still aren't recorded when Chrome has already recorded a launch.
        int total = RecordHistogram.getHistogramTotalCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM);
        intent.putExtra(IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, false);
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        Assert.assertEquals(total,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));
    }

    @Test
    @SmallTest
    public void testVoiceSearchResultsMetrics() throws Throwable {
        int count = histogramCountForValue(
                LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT);
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        Mockito.when(mActivity.getIntent()).thenReturn(intent);

        TabbedActivityLaunchCauseMetrics metrics = new TabbedActivityLaunchCauseMetrics(mActivity);

        // Tests the case where Chrome is backgrounded either by the intent picker, or by
        // cross-channel Open In Browser.
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(count,
                histogramCountForValue(
                        LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT));

        // Ensures we don't record this metric when Chrome has already recorded a launch.
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        Assert.assertEquals(count,
                histogramCountForValue(
                        LaunchCauseMetrics.LaunchCause.EXTERNAL_SEARCH_ACTION_INTENT));
    }
}
