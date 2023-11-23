// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import androidx.test.filters.SmallTest;

import org.jni_zero.JNINamespace;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.test.util.MemoryMetricsLoggerUtilsJni;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;

/** Tests for memory_metrics_logger.cc. */
@JNINamespace("android_webview")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class MemoryMetricsLoggerTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private HistogramWatcher mHistogramExpectationBrowser;
    private HistogramWatcher mHistogramExpectationRendererMulti;
    private HistogramWatcher mHistogramExpectationRendererSingle;
    private HistogramWatcher mHistogramExpectationTotal;

    public MemoryMetricsLoggerTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mHistogramExpectationBrowser =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes("Memory.Browser.PrivateMemoryFootprint", 1)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();
        mHistogramExpectationRendererMulti =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes("Memory.Renderer.PrivateMemoryFootprint", 1)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();
        mHistogramExpectationRendererSingle =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Memory.Renderer.PrivateMemoryFootprint")
                        .build();
        mHistogramExpectationTotal =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes("Memory.Total.PrivateMemoryFootprint", 1)
                        .allowExtraRecordsForHistogramsAbove()
                        .build();
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);

        mActivityTestRule.loadUrlSync(
                testContainerView.getAwContents(),
                contentsClient.getOnPageFinishedHelper(),
                "about:blank");
        Assert.assertTrue(MemoryMetricsLoggerUtilsJni.get().forceRecordHistograms());
    }

    @After
    public void tearDown() {}

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @SmallTest
    public void testMultiProcessHistograms() {
        mHistogramExpectationBrowser.assertExpected();
        mHistogramExpectationRendererMulti.assertExpected();
        mHistogramExpectationTotal.assertExpected();
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(SINGLE_PROCESS)
    @SmallTest
    public void testSingleProcessHistograms() {
        mHistogramExpectationBrowser.assertExpected();
        // Verify no renderer record in single process mode.
        mHistogramExpectationRendererSingle.assertExpected();
        mHistogramExpectationTotal.assertExpected();
    }
}
