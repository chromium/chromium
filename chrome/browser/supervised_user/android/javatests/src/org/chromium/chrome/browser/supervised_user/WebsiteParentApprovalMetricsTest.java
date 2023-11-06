// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Tests the recording of the metrics within the local web approval flow. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebsiteParentApprovalMetricsTest {
    @Test
    @SmallTest
    public void recordOutcomeMetrics() {
        final String histogramName = "FamilyLinkUser.LocalWebApprovalOutcome";

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName,
                        WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                                .APPROVED_BY_PARENT);
        WebsiteParentApprovalMetrics.recordOutcomeMetric(
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .APPROVED_BY_PARENT);
        histogram.assertExpected();

        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                histogramName,
                                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                                        .PARENT_APPROVAL_CANCELLED,
                                2)
                        .build();
        WebsiteParentApprovalMetrics.recordOutcomeMetric(
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .PARENT_APPROVAL_CANCELLED);
        WebsiteParentApprovalMetrics.recordOutcomeMetric(
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .PARENT_APPROVAL_CANCELLED);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void recordParentAuthenticationErrorMetrics() {
        final String histogramName =
                "Android.FamilyLinkUser.LocalWebApprovalParentAuthenticationError";
        final int negativeErrorCode = -1;
        final int lowValueCode = 10; // Example: value of CommonStatusCode.DEVELOPER_ERROR.

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, negativeErrorCode);
        WebsiteParentApprovalMetrics.recordParentAuthenticationErrorCode(negativeErrorCode);
        histogram.assertExpected();

        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(histogramName, lowValueCode, 2)
                        .build();
        WebsiteParentApprovalMetrics.recordParentAuthenticationErrorCode(lowValueCode);
        WebsiteParentApprovalMetrics.recordParentAuthenticationErrorCode(lowValueCode);
        histogram.assertExpected();
    }
}
