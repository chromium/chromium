// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.test.util.MemoryMetricsLoggerUtilsJni;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;

/**
 * Tests for memory_metrics_logger.cc.
 */
@JNINamespace("android_webview")
@RunWith(AwJUnit4ClassRunner.class)
public class MemoryMetricsLoggerTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Before
    public void setUp() throws Exception {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);

        mActivityTestRule.loadUrlSync(testContainerView.getAwContents(),
                contentsClient.getOnPageFinishedHelper(), "about:blank");
        Assert.assertTrue(MemoryMetricsLoggerUtilsJni.get().forceRecordHistograms());
    }

    @After
    public void tearDown() {}

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @SmallTest
    public void testMultiProcessHistograms() {
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Browser.PrivateMemoryFootprint"));
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Renderer.PrivateMemoryFootprint"));
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Total.PrivateMemoryFootprint"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(SINGLE_PROCESS)
    @SmallTest
    public void testSingleProcessHistograms() {
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Browser.PrivateMemoryFootprint"));
        // Verify no renderer record in single process mode.
        Assert.assertEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Renderer.PrivateMemoryFootprint"));
        Assert.assertNotEquals(0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Memory.Total.PrivateMemoryFootprint"));
    }
}
