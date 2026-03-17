// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
class ParentApprovalMetrics {
    // Histogram name patterns
    static final String APPROVAL_OUTCOME_NAME_PATTERN = "FamilyLinkUser.LocalApprovalOutcome.%s";
    static final String APPROVAL_PACP_ERROR_CODE_NAME_PATTERN =
            "Android.FamilyLinkUser.LocalApprovalParentAuthenticationError.%s";

    // Flows
    public static final String EXTENSION_FLOW_NAME = "Extension";
    public static final String WEB_FLOW_NAME = "Web";

    // LINT.IfChange(FamilyLinkUserLocalApprovalOutcome)

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // The values need to be in sync with FamilyLinkUserLocalApprovalOutcome in enums.xml.
    @IntDef({
        FamilyLinkUserLocalApprovalOutcome.APPROVED_BY_PARENT,
        FamilyLinkUserLocalApprovalOutcome.DENIED_BY_PARENT,
        FamilyLinkUserLocalApprovalOutcome.PARENT_APPROVAL_CANCELLED,
        FamilyLinkUserLocalApprovalOutcome.VERIFICATION_WIDGET_UNSUPPORTED_API_CALL_EXCEPTION,
        FamilyLinkUserLocalApprovalOutcome.VERIFICATION_WIDGET_UNEXPECTED_EXCEPTION,
        FamilyLinkUserLocalApprovalOutcome.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FamilyLinkUserLocalApprovalOutcome {
        int APPROVED_BY_PARENT = 0;
        int DENIED_BY_PARENT = 1;
        int PARENT_APPROVAL_CANCELLED = 2;
        int VERIFICATION_WIDGET_UNSUPPORTED_API_CALL_EXCEPTION = 3;
        int VERIFICATION_WIDGET_UNEXPECTED_EXCEPTION = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:FamilyLinkUserLocalApprovalOutcome)

    public static void recordOutcomeMetric(
            @FamilyLinkUserLocalApprovalOutcome int outcome, String flow) {
        RecordHistogram.recordEnumeratedHistogram(
                String.format(APPROVAL_OUTCOME_NAME_PATTERN, flow),
                outcome,
                FamilyLinkUserLocalApprovalOutcome.COUNT);
    }

    public static void recordParentAuthenticationErrorCode(int errorCode, String flow) {
        RecordHistogram.recordSparseHistogram(
                String.format(APPROVAL_PACP_ERROR_CODE_NAME_PATTERN, flow), errorCode);
    }
}
