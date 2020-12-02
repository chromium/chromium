// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashMap;
import java.util.Map;

class FeedV2TestHelper {
    private FeedV2TestHelper() {}

    private static Map<String, Integer> getEnumHistogramValues(
            String histogramName, Map<String, Integer> enumNames) {
        HashMap<String, Integer> counts = new HashMap<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (Map.Entry<String, Integer> entry : enumNames.entrySet()) {
                int count = RecordHistogram.getHistogramValueCountForTesting(
                        histogramName, entry.getValue());
                if (count > 0) {
                    counts.put(entry.getKey(), count);
                }
            }
        });
        return counts;
    }

    public static Map<String, Integer> getFeedUserActionsHistogramValues() {
        // Histogram enum values from components/feed/core/v2/metrics_reporter.h.
        HashMap<String, Integer> enumNames = new HashMap<>();
        enumNames.put("kTappedOnCard", 0);
        enumNames.put("kShownCard", 1);
        enumNames.put("kTappedSendFeedback", 2);
        enumNames.put("kTappedLearnMore", 3);
        enumNames.put("kTappedHideStory", 4);
        enumNames.put("kTappedNotInterestedIn", 5);
        enumNames.put("kTappedManageInterests", 6);
        enumNames.put("kTappedDownload", 7);
        enumNames.put("kTappedOpenInNewTab", 8);
        enumNames.put("kOpenedContextMenu", 9);
        enumNames.put("kOpenedFeedSurface", 10);
        enumNames.put("kTappedOpenInNewIncognitoTab", 11);
        enumNames.put("kEphemeralChange", 12);
        enumNames.put("kEphemeralChangeRejected", 13);
        enumNames.put("kTappedTurnOn", 14);
        enumNames.put("kTappedTurnOff", 15);
        enumNames.put("kTappedManageActivity", 16);
        enumNames.put("kAddedToReadLater", 17);
        enumNames.put("kClosedContextMenu", 18);
        enumNames.put("kEphemeralChangeCommited", 19);
        enumNames.put("kOpenedDialog", 20);
        enumNames.put("kClosedDialog", 21);
        enumNames.put("kShowSnackbar", 22);
        enumNames.put("kOpenedNativeContextMenu", 23);
        return getEnumHistogramValues("ContentSuggestions.Feed.UserActions", enumNames);
    }

    public static Map<String, Integer> getUploadActionsStatusValues() {
        // Histogram enum values from UploadActionsStatus in components/feed/core/v2/enums.h.
        HashMap<String, Integer> enumNames = new HashMap<>();
        enumNames.put("kNoStatus", 0);
        enumNames.put("kNoPendingActions", 1);
        enumNames.put("kFailedToStorePendingAction", 2);
        enumNames.put("kStoredPendingAction", 3);
        enumNames.put("kUpdatedConsistencyToken", 4);
        enumNames.put("kFinishedWithoutUpdatingConsistencyToken", 5);
        enumNames.put("kAbortUploadForSignedOutUser", 6);
        enumNames.put("kAbortUploadBecauseDisabled", 7);
        return getEnumHistogramValues("ContentSuggestions.Feed.UploadActionsStatus", enumNames);
    }

    public static Map<String, Integer> getLoadStreamStatusInitialValues() {
        return getEnumHistogramValues(
                "ContentSuggestions.Feed.LoadStreamStatus.Initial", loadStreamEnums());
    }

    private static HashMap<String, Integer> loadStreamEnums() {
        HashMap<String, Integer> enumNames = new HashMap<>();
        enumNames.put("kNoStatus", 0);
        enumNames.put("kLoadedFromStore", 1);
        enumNames.put("kLoadedFromNetwork", 2);
        enumNames.put("kFailedWithStoreError", 3);
        enumNames.put("kNoStreamDataInStore", 4);
        enumNames.put("kModelAlreadyLoaded", 5);
        enumNames.put("kNoResponseBody", 6);
        enumNames.put("kProtoTranslationFailed", 7);
        enumNames.put("kDataInStoreIsStale", 8);
        enumNames.put("kDataInStoreIsStaleTimestampInFuture", 9);
        enumNames.put("kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED", 10);
        enumNames.put("kCannotLoadFromNetworkOffline", 11);
        enumNames.put("kCannotLoadFromNetworkThrottled", 12);
        enumNames.put("kLoadNotAllowedEulaNotAccepted", 13);
        enumNames.put("kLoadNotAllowedArticlesListHidden", 14);
        enumNames.put("kCannotParseNetworkResponseBody", 15);
        enumNames.put("kLoadMoreModelIsNotLoaded", 16);
        enumNames.put("kLoadNotAllowedDisabledByEnterprisePolicy", 17);
        enumNames.put("kNetworkFetchFailed", 18);
        enumNames.put("kCannotLoadMoreNoNextPageToken", 19);
        return enumNames;
    }
}
