// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric.metrics;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.metrics.AwMetricsUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Unit tests for {@link AwMetricsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AwMetricsUtilsTest {
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHistogramNameHashing() {
        Assert.assertEquals(
                AwMetricsUtils.hashHistogramName("Android.WebView.Visibility.Global"),
                Long.parseUnsignedLong("18367955693041683430"));

        Assert.assertEquals(
                AwMetricsUtils.hashHistogramName(
                        "PageLoad.LayoutInstability.MaxCumulativeShiftScore."
                                + "AfterBackForwardCacheRestore.SessionWindow.Gap1000ms.Max5000ms2"),
                Long.parseUnsignedLong("17564198105882768940"));

        Assert.assertEquals(
                AwMetricsUtils.hashHistogramName(
                        "Android.WebView.VisibleScreenCoverage.PerWebView.https"),
                Long.parseUnsignedLong("4495614277712318663"));

        // The following testcases matches the ones implemented in
        // base/metrics/metrics_hashes_unittest.cc
        Assert.assertEquals(
                AwMetricsUtils.hashHistogramName("Back"),
                Long.decode("0x0557fa923dcee4d0").longValue());

        Assert.assertEquals(
                AwMetricsUtils.hashHistogramName("NewTab"),
                Long.decode("0x290eb683f96572f1").longValue());

        Assert.assertEquals(
                AwMetricsUtils.hashHistogramName("Forward"),
                Long.decode("0x67d2f6740a8eaebf").longValue());
    }
}
