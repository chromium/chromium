// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.test.util.RendererProcessMetricsProviderUtilsJni;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;

/**
 * Tests for renderer_process_metrics_provider.cc.
 */
@JNINamespace("android_webview")
@RunWith(AwJUnit4ClassRunner.class)
public class RendererProcessMetricsProviderTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Before
    public void setUp() throws Exception {
        RendererProcessMetricsProviderUtilsJni.get().forceRecordHistograms();
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(SINGLE_PROCESS)
    @SmallTest
    public void testSingleProcessHistograms() throws Throwable {
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SingleOrMultiProcess", /* sample=single process */ 0));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SingleOrMultiProcess", /* sample=multi process */ 1));
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @SmallTest
    public void testMultiProcessHistograms() throws Throwable {
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SingleOrMultiProcess", /* sample=single process */ 0));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SingleOrMultiProcess", /* sample=multi process */ 1));
    }
}
