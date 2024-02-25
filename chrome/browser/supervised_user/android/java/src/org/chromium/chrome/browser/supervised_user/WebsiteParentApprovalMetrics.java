// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

class WebsiteParentApprovalMetrics {
    // Histogram name
    static final String WEB_APPOVAL_OUTCOME_NAME = "FamilyLinkUser.LocalWebApprovalOutcome";
    static final String WEB_APPOVAL_PACP_ERROR_CODE =
            "Android.FamilyLinkUser.LocalWebApprovalParentAuthenticationError";

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // The values need to be in sync with FamilyLinkUserLocalWebApprovalOutcome in enums.xml.
    @IntDef({
        FamilyLinkUserLocalWebApprovalOutcome.APPROVED_BY_PARENT,
        FamilyLinkUserLocalWebApprovalOutcome.DENIED_BY_PARENT,
        FamilyLinkUserLocalWebApprovalOutcome.PARENT_APPROVAL_CANCELLED,
        FamilyLinkUserLocalWebApprovalOutcome.VERIFICATION_WIDGET_UNSUPPORTED_API_CALL_EXCEPTION,
        FamilyLinkUserLocalWebApprovalOutcome.VERIFICATION_WIDGET_UNEXPECTED_EXCEPTION,
        FamilyLinkUserLocalWebApprovalOutcome.COUNT
    })
    public @interface FamilyLinkUserLocalWebApprovalOutcome {
        int APPROVED_BY_PARENT = 0;
        int DENIED_BY_PARENT = 1;
        int PARENT_APPROVAL_CANCELLED = 2;
        int VERIFICATION_WIDGET_UNSUPPORTED_API_CALL_EXCEPTION = 3;
        int VERIFICATION_WIDGET_UNEXPECTED_EXCEPTION = 7;
        int COUNT = 8;
    }

    public static void recordOutcomeMetric(@FamilyLinkUserLocalWebApprovalOutcome int outcome) {
        RecordHistogram.recordEnumeratedHistogram(
                WEB_APPOVAL_OUTCOME_NAME, outcome, FamilyLinkUserLocalWebApprovalOutcome.COUNT);
    }

    public static void recordParentAuthenticationErrorCode(int errorCode) {
        RecordHistogram.recordSparseHistogram(WEB_APPOVAL_PACP_ERROR_CODE, errorCode);
    }
}
