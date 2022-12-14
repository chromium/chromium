// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Tests the recording of the metrics within the local web approval flow.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class WebsiteParentApprovalMetricsUnitTest {
    @Rule
    public final HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @BeforeClass
    public static void setUpClass() {
        // Needs to load before HistogramTestRule is applied.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void recordOutcomeMetrics() {
        final String histogramName = "FamilyLinkUser.LocalWebApprovalOutcome";

        WebsiteParentApprovalMetrics.recordOutcomeMetric(
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .APPROVED_BY_PARENT);

        int count = mHistogramTestRule.getHistogramValueCount(histogramName,
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .APPROVED_BY_PARENT);
        Assert.assertEquals(1, count);

        WebsiteParentApprovalMetrics.recordOutcomeMetric(
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .PARENT_APPROVAL_CANCELLED);
        WebsiteParentApprovalMetrics.recordOutcomeMetric(
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .PARENT_APPROVAL_CANCELLED);

        count = mHistogramTestRule.getHistogramValueCount(histogramName,
                WebsiteParentApprovalMetrics.FamilyLinkUserLocalWebApprovalOutcome
                        .PARENT_APPROVAL_CANCELLED);
        Assert.assertEquals(2, count);

        count = mHistogramTestRule.getHistogramTotalCount(histogramName);
        Assert.assertEquals(3, count);
    }

    @Test
    @SmallTest
    public void recordParentAuthenticationErrorMetrics() {
        final String histogramName =
                "Android.FamilyLinkUser.LocalWebApprovalParentAuthenticationError";
        final int negativeErrorCode = -1;
        final int lowValueCode = 10; // Example: value of CommonStatusCode.DEVELOPER_ERROR.

        WebsiteParentApprovalMetrics.recordParentAuthenticationErrorCode(negativeErrorCode);
        int count = mHistogramTestRule.getHistogramValueCount(histogramName, negativeErrorCode);
        Assert.assertEquals(1, count);

        WebsiteParentApprovalMetrics.recordParentAuthenticationErrorCode(lowValueCode);
        WebsiteParentApprovalMetrics.recordParentAuthenticationErrorCode(lowValueCode);
        count = mHistogramTestRule.getHistogramValueCount(histogramName, lowValueCode);
        Assert.assertEquals(2, count);

        count = mHistogramTestRule.getHistogramTotalCount(histogramName);
        Assert.assertEquals(3, count);
    }
}