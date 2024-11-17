// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class DataSharingMetrics {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(JoinActionStateAndroid)
    @IntDef({
        JoinActionStateAndroid.JOIN_TRIGGERED,
        JoinActionStateAndroid.PROFILE_AVAILABLE,
        JoinActionStateAndroid.PARSE_URL_FAILED,
        JoinActionStateAndroid.SYNCED_TAB_GROUP_EXISTS,
        JoinActionStateAndroid.LOCAL_TAB_GROUP_EXISTS,
        JoinActionStateAndroid.LOCAL_TAB_GROUP_ADDED,
        JoinActionStateAndroid.LOCAL_TAB_GROUP_OPENED,
        JoinActionStateAndroid.ADD_MEMBER_FAILED,
        JoinActionStateAndroid.ADD_MEMBER_SUCCESS,
        JoinActionStateAndroid.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface JoinActionStateAndroid {
        int JOIN_TRIGGERED = 0;
        int PROFILE_AVAILABLE = 1;
        int PARSE_URL_FAILED = 2;
        int SYNCED_TAB_GROUP_EXISTS = 3;
        int LOCAL_TAB_GROUP_EXISTS = 4;
        int LOCAL_TAB_GROUP_ADDED = 5;
        int LOCAL_TAB_GROUP_OPENED = 6;
        int ADD_MEMBER_FAILED = 7;
        int ADD_MEMBER_SUCCESS = 8;
        int COUNT = 9;
    }

    // LINT.ThenChange(//tools/metrics/histograms/data_sharing/enums.xml:JoinActionStateAndroid)

    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange(ShareActionStateAndroid)
    @IntDef({
        ShareActionStateAndroid.SHARE_TRIGGERED,
        ShareActionStateAndroid.GROUP_EXISTS,
        ShareActionStateAndroid.ENSURE_VISIBILITY_FAILED,
        ShareActionStateAndroid.BOTTOM_SHEET_DISMISSED,
        ShareActionStateAndroid.GROUP_CREATE_SUCCESS,
        ShareActionStateAndroid.GROUP_CREATE_FAILED,
        ShareActionStateAndroid.URL_CREATION_FAILED,
        ShareActionStateAndroid.SHARE_SHEET_SHOWN,
        ShareActionStateAndroid.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShareActionStateAndroid {
        int SHARE_TRIGGERED = 0;
        int GROUP_EXISTS = 1;
        int ENSURE_VISIBILITY_FAILED = 2;
        int BOTTOM_SHEET_DISMISSED = 3;
        int GROUP_CREATE_SUCCESS = 4;
        int GROUP_CREATE_FAILED = 5;
        int URL_CREATION_FAILED = 6;
        int SHARE_SHEET_SHOWN = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/data_sharing/enums.xml:ShareActionStateAndroid)

    public static void recordJoinActionFlowState(@JoinActionStateAndroid int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "DataSharing.Android.JoinActionFlowState", state, JoinActionStateAndroid.COUNT);
    }

    public static void recordShareActionFlowState(@ShareActionStateAndroid int state) {
        RecordHistogram.recordEnumeratedHistogram(
                "DataSharing.Android.ShareActionFlowState", state, ShareActionStateAndroid.COUNT);
    }

    public static void recordJoinFlowLatency(String stepName, long durationMs) {
        RecordHistogram.recordTimesHistogram(
                "DataSharing.Android.JoinFlow." + stepName, durationMs);
    }
}
