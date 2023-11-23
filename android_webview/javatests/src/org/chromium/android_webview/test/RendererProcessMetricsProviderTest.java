// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.MULTI_PROCESS;
import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import androidx.test.filters.SmallTest;

import org.jni_zero.JNINamespace;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.test.util.RendererProcessMetricsProviderUtilsJni;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;

/** Tests for renderer_process_metrics_provider.cc. */
@JNINamespace("android_webview")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class RendererProcessMetricsProviderTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private HistogramWatcher mHistogramExpectationSingleProcess;
    private HistogramWatcher mHistogramExpectationMultiProcess;

    public RendererProcessMetricsProviderTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mHistogramExpectationSingleProcess =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.WebView.SingleOrMultiProcess",
                                /* sample=single process */ 0,
                                1)
                        .build();
        mHistogramExpectationMultiProcess =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.WebView.SingleOrMultiProcess",
                                /* sample=multi process */ 1,
                                1)
                        .build();
        RendererProcessMetricsProviderUtilsJni.get().forceRecordHistograms();
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(SINGLE_PROCESS)
    @SmallTest
    public void testSingleProcessHistograms() throws Throwable {
        mHistogramExpectationSingleProcess.assertExpected();
    }

    @Test
    @Feature({"AndroidWebView"})
    @OnlyRunIn(MULTI_PROCESS)
    @SmallTest
    public void testMultiProcessHistograms() throws Throwable {
        mHistogramExpectationMultiProcess.assertExpected();
    }
}
