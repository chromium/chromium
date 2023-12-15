// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;

/** Helper for recording metrics related to the share sheet link toggle feature. */
final class ShareSheetLinkToggleMetricsHelper {
    static final class LinkToggleMetricsDetails {
        @LinkToggleState int mLinkToggleState;
        @DetailedContentType int mDetailedContentType;

        LinkToggleMetricsDetails(
                @LinkToggleState int linkToggleState,
                @DetailedContentType int detailedContentType) {
            mLinkToggleState = linkToggleState;
            mDetailedContentType = detailedContentType;
        }
    }

    /** Records the metrics about the state of the link toggle when users complete a share. */
    static void recordLinkToggleSharedStateMetric(
            LinkToggleMetricsDetails linkToggleMetricsDetails) {
        recordLinkToggleMetric(linkToggleMetricsDetails, "Completed");
    }

    /** Records metrics for when the link toggle is toggled. */
    static void recordLinkToggleToggledMetric(LinkToggleMetricsDetails linkToggleMetricsDetails) {
        recordLinkToggleMetric(linkToggleMetricsDetails, "InProgress");
    }

    private static void recordLinkToggleMetric(
            LinkToggleMetricsDetails linkToggleMetricsDetails, String shareState) {
        if (linkToggleMetricsDetails.mLinkToggleState == LinkToggleState.COUNT) {
            return;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.SharingHubAndroid."
                        + getDetailedContentTypeAsString(
                                linkToggleMetricsDetails.mDetailedContentType)
                        + "."
                        + shareState,
                linkToggleMetricsDetails.mLinkToggleState,
                LinkToggleState.COUNT);
    }

    private static String getDetailedContentTypeAsString(
            @DetailedContentType int detailedContentType) {
        switch (detailedContentType) {
            case DetailedContentType.IMAGE:
                return "Image";
            case DetailedContentType.GIF:
                return "Gif";
            case DetailedContentType.HIGHLIGHTED_TEXT:
                return "HighlightedText";
            case DetailedContentType.SCREENSHOT:
                return "Screenshot";
            case DetailedContentType.NOT_SPECIFIED:
                return "NotSpecified";
        }
        return "";
    }

    private ShareSheetLinkToggleMetricsHelper() {}
}
