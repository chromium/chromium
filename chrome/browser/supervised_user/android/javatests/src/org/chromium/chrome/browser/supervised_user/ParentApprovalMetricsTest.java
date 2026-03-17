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

/** Tests the recording of the metrics within the local parent approval flow. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ParentApprovalMetricsTest {
    @Test
    @SmallTest
    public void recordWebOutcomeMetrics() {
        final String histogramName = "FamilyLinkUser.LocalApprovalOutcome.Web";

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName,
                        ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome
                                .APPROVED_BY_PARENT);
        ParentApprovalMetrics.recordOutcomeMetric(
                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome.APPROVED_BY_PARENT,
                ParentApprovalMetrics.WEB_FLOW_NAME);
        histogram.assertExpected();

        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                histogramName,
                                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome
                                        .PARENT_APPROVAL_CANCELLED,
                                2)
                        .build();
        ParentApprovalMetrics.recordOutcomeMetric(
                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome.PARENT_APPROVAL_CANCELLED,
                ParentApprovalMetrics.WEB_FLOW_NAME);
        ParentApprovalMetrics.recordOutcomeMetric(
                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome.PARENT_APPROVAL_CANCELLED,
                ParentApprovalMetrics.WEB_FLOW_NAME);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void recordWebParentAuthenticationErrorMetrics() {
        final String histogramName =
                "Android.FamilyLinkUser.LocalApprovalParentAuthenticationError.Web";
        final int negativeErrorCode = -1;
        final int lowValueCode = 10; // Example: value of CommonStatusCode.DEVELOPER_ERROR.

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, negativeErrorCode);
        ParentApprovalMetrics.recordParentAuthenticationErrorCode(
                negativeErrorCode, ParentApprovalMetrics.WEB_FLOW_NAME);
        histogram.assertExpected();

        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(histogramName, lowValueCode, 2)
                        .build();
        ParentApprovalMetrics.recordParentAuthenticationErrorCode(
                lowValueCode, ParentApprovalMetrics.WEB_FLOW_NAME);
        ParentApprovalMetrics.recordParentAuthenticationErrorCode(
                lowValueCode, ParentApprovalMetrics.WEB_FLOW_NAME);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void recordExtensionOutcomeMetrics() {
        final String histogramName = "FamilyLinkUser.LocalApprovalOutcome.Extension";

        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        histogramName,
                        ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome
                                .APPROVED_BY_PARENT);
        ParentApprovalMetrics.recordOutcomeMetric(
                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome.APPROVED_BY_PARENT,
                ParentApprovalMetrics.EXTENSION_FLOW_NAME);
        histogram.assertExpected();

        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                histogramName,
                                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome
                                        .PARENT_APPROVAL_CANCELLED,
                                2)
                        .build();
        ParentApprovalMetrics.recordOutcomeMetric(
                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome.PARENT_APPROVAL_CANCELLED,
                ParentApprovalMetrics.EXTENSION_FLOW_NAME);
        ParentApprovalMetrics.recordOutcomeMetric(
                ParentApprovalMetrics.FamilyLinkUserLocalApprovalOutcome.PARENT_APPROVAL_CANCELLED,
                ParentApprovalMetrics.EXTENSION_FLOW_NAME);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void recordExtensionParentAuthenticationErrorMetrics() {
        final String histogramName =
                "Android.FamilyLinkUser.LocalApprovalParentAuthenticationError.Extension";
        final int negativeErrorCode = -1;
        final int lowValueCode = 10; // Example: value of CommonStatusCode.DEVELOPER_ERROR.

        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, negativeErrorCode);
        ParentApprovalMetrics.recordParentAuthenticationErrorCode(
                negativeErrorCode, ParentApprovalMetrics.EXTENSION_FLOW_NAME);
        histogram.assertExpected();

        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(histogramName, lowValueCode, 2)
                        .build();
        ParentApprovalMetrics.recordParentAuthenticationErrorCode(
                lowValueCode, ParentApprovalMetrics.EXTENSION_FLOW_NAME);
        ParentApprovalMetrics.recordParentAuthenticationErrorCode(
                lowValueCode, ParentApprovalMetrics.EXTENSION_FLOW_NAME);
        histogram.assertExpected();
    }
}
