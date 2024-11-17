// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;

/** Helper for metrics related to the Link to Text feature. */
public final class LinkToTextMetricsHelper {
    @IntDef({
        LinkToTextDiagnoseStatus.SHOW_SHARINGHUB_FOR_HIGHLIGHT,
        LinkToTextDiagnoseStatus.REQUEST_SELECTOR,
        LinkToTextDiagnoseStatus.SELECTOR_RECEIVED,
        LinkToTextDiagnoseStatus.MAX
    })
    public @interface LinkToTextDiagnoseStatus {
        int SHOW_SHARINGHUB_FOR_HIGHLIGHT = 0;
        int REQUEST_SELECTOR = 1;
        int SELECTOR_RECEIVED = 2;
        int MAX = 3;
    }

    /** Private constructor since all the methods in this class are static. */
    private LinkToTextMetricsHelper() {}

    /**
     * Records the metrics about the state of the link generation when users are sharing it.
     *
     * @param linkGenerationStatus The state of the link generation that ended up being shared.
     */
    public static void recordSharedHighlightStateMetrics(@LinkGeneration int linkGenerationStatus) {
        if (linkGenerationStatus == LinkGeneration.MAX) return;
        switch (linkGenerationStatus) {
            case LinkGeneration.LINK:
                RecordUserAction.record(
                        "SharingHubAndroid.LinkGeneration.Success.LinkToTextShared");
                break;
            case LinkGeneration.TEXT:
                RecordUserAction.record("SharingHubAndroid.LinkGeneration.Success.TextShared");
                break;
            case LinkGeneration.FAILURE:
                RecordUserAction.record("SharingHubAndroid.LinkGeneration.Failure.TextShared");
                break;
            default:
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "SharedHighlights.AndroidShareSheet.SharedState",
                linkGenerationStatus,
                LinkGeneration.MAX);
    }

    /**
     * Records the metrics about the status of link to text flow.
     *
     * @param linkToTextDiagnoseStatus The status of link to text flow.
     */
    public static void recordLinkToTextDiagnoseStatus(
            @LinkToTextDiagnoseStatus int linkToTextDiagnoseStatus) {
        RecordHistogram.recordEnumeratedHistogram(
                "SharedHighlights.LinkToTextDiagnoseStatus",
                linkToTextDiagnoseStatus,
                LinkToTextDiagnoseStatus.MAX);
    }
}
