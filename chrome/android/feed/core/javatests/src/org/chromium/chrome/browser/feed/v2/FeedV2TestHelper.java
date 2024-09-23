// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import androidx.recyclerview.widget.RecyclerView;

import org.hamcrest.Matchers;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;

import java.util.HashMap;
import java.util.Map;

/** Helpers for Feed V2 browser tests. */
public class FeedV2TestHelper {
    private FeedV2TestHelper() {}

    private static Map<String, Integer> getEnumHistogramValues(
            String histogramName, Map<String, Integer> enumNames) {
        HashMap<String, Integer> counts = new HashMap<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Map.Entry<String, Integer> entry : enumNames.entrySet()) {
                        int count =
                                RecordHistogram.getHistogramValueCountForTesting(
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
        enumNames.put("kTappedOnCard", FeedUserActionType.TAPPED_ON_CARD);
        enumNames.put("kTappedSendFeedback", FeedUserActionType.TAPPED_SEND_FEEDBACK);
        enumNames.put("kTappedLearnMore", FeedUserActionType.TAPPED_LEARN_MORE);
        enumNames.put("kTappedHideStory", FeedUserActionType.TAPPED_HIDE_STORY);
        enumNames.put("kTappedNotInterestedIn", FeedUserActionType.TAPPED_NOT_INTERESTED_IN);
        enumNames.put("kTappedManageInterests", FeedUserActionType.TAPPED_MANAGE_INTERESTS);
        enumNames.put("kTappedDownload", FeedUserActionType.TAPPED_DOWNLOAD);
        enumNames.put("kTappedOpenInNewTab", FeedUserActionType.TAPPED_OPEN_IN_NEW_TAB);
        enumNames.put("kOpenedContextMenu", FeedUserActionType.OPENED_CONTEXT_MENU);
        enumNames.put("kOpenedFeedSurface", FeedUserActionType.OPENED_FEED_SURFACE);
        enumNames.put(
                "kTappedOpenInNewIncognitoTab",
                FeedUserActionType.TAPPED_OPEN_IN_NEW_INCOGNITO_TAB);
        enumNames.put("kEphemeralChange", FeedUserActionType.EPHEMERAL_CHANGE);
        enumNames.put("kEphemeralChangeRejected", FeedUserActionType.EPHEMERAL_CHANGE_REJECTED);
        enumNames.put("kTappedTurnOn", FeedUserActionType.TAPPED_TURN_ON);
        enumNames.put("kTappedTurnOff", FeedUserActionType.TAPPED_TURN_OFF);
        enumNames.put("kTappedManageActivity", FeedUserActionType.TAPPED_MANAGE_ACTIVITY);
        enumNames.put("kAddedToReadLater", FeedUserActionType.ADDED_TO_READ_LATER);
        enumNames.put("kClosedContextMenu", FeedUserActionType.CLOSED_CONTEXT_MENU);
        enumNames.put("kEphemeralChangeCommited", FeedUserActionType.EPHEMERAL_CHANGE_COMMITED);
        enumNames.put("kOpenedDialog", FeedUserActionType.OPENED_DIALOG);
        enumNames.put("kClosedDialog", FeedUserActionType.CLOSED_DIALOG);
        enumNames.put("kShowSnackbar", FeedUserActionType.SHOW_SNACKBAR);
        enumNames.put("kOpenedNativeContextMenu", FeedUserActionType.OPENED_NATIVE_CONTEXT_MENU);
        enumNames.put("kTappedFollowButton", FeedUserActionType.TAPPED_FOLLOW_BUTTON);
        return getEnumHistogramValues("ContentSuggestions.Feed.UserActions", enumNames);
    }

    public static Map<String, Integer> getUploadActionsStatusValues() {
        // Histogram enum values from UploadActionsStatus in components/feed/core/v2/enums.h.
        HashMap<String, Integer> enumNames = new HashMap<>();
        enumNames.put("kNoStatus", UploadActionsStatus.NO_STATUS);
        enumNames.put("kNoPendingActions", UploadActionsStatus.NO_PENDING_ACTIONS);
        enumNames.put(
                "kFailedToStorePendingAction", UploadActionsStatus.FAILED_TO_STORE_PENDING_ACTION);
        enumNames.put("kStoredPendingAction", UploadActionsStatus.STORED_PENDING_ACTION);
        enumNames.put("kUpdatedConsistencyToken", UploadActionsStatus.UPDATED_CONSISTENCY_TOKEN);
        enumNames.put(
                "kFinishedWithoutUpdatingConsistencyToken",
                UploadActionsStatus.FINISHED_WITHOUT_UPDATING_CONSISTENCY_TOKEN);
        enumNames.put(
                "kAbortUploadForSignedOutUser",
                UploadActionsStatus.ABORT_UPLOAD_FOR_SIGNED_OUT_USER);
        enumNames.put(
                "kAbortUploadBecauseDisabled", UploadActionsStatus.ABORT_UPLOAD_BECAUSE_DISABLED);
        return getEnumHistogramValues("ContentSuggestions.Feed.UploadActionsStatus", enumNames);
    }

    public static Map<String, Integer> getLoadStreamStatusInitialValues() {
        return getEnumHistogramValues(
                "ContentSuggestions.Feed.LoadStreamStatus.Initial", loadStreamEnums());
    }

    private static HashMap<String, Integer> loadStreamEnums() {
        HashMap<String, Integer> enumNames = new HashMap<>();
        enumNames.put("kNoStatus", LoadStreamStatus.NO_STATUS);
        enumNames.put("kLoadedFromStore", LoadStreamStatus.LOADED_FROM_STORE);
        enumNames.put("kLoadedFromNetwork", LoadStreamStatus.LOADED_FROM_NETWORK);
        enumNames.put("kFailedWithStoreError", LoadStreamStatus.FAILED_WITH_STORE_ERROR);
        enumNames.put("kNoStreamDataInStore", LoadStreamStatus.NO_STREAM_DATA_IN_STORE);
        enumNames.put("kModelAlreadyLoaded", LoadStreamStatus.MODEL_ALREADY_LOADED);
        enumNames.put("kNoResponseBody", LoadStreamStatus.NO_RESPONSE_BODY);
        enumNames.put("kProtoTranslationFailed", LoadStreamStatus.PROTO_TRANSLATION_FAILED);
        enumNames.put("kDataInStoreIsStale", LoadStreamStatus.DATA_IN_STORE_IS_STALE);
        enumNames.put(
                "kDataInStoreIsStaleTimestampInFuture",
                LoadStreamStatus.DATA_IN_STORE_IS_STALE_TIMESTAMP_IN_FUTURE);
        enumNames.put(
                "kCannotLoadFromNetworkSupressedForHistoryDelete_DEPRECATED",
                LoadStreamStatus.CANNOT_LOAD_FROM_NETWORK_SUPRESSED_FOR_HISTORY_DELETE_DEPRECATED);
        enumNames.put(
                "kCannotLoadFromNetworkOffline", LoadStreamStatus.CANNOT_LOAD_FROM_NETWORK_OFFLINE);
        enumNames.put(
                "kCannotLoadFromNetworkThrottled",
                LoadStreamStatus.CANNOT_LOAD_FROM_NETWORK_THROTTLED);
        enumNames.put(
                "kLoadNotAllowedEulaNotAccepted",
                LoadStreamStatus.LOAD_NOT_ALLOWED_EULA_NOT_ACCEPTED);
        enumNames.put(
                "kLoadNotAllowedArticlesListHidden",
                LoadStreamStatus.LOAD_NOT_ALLOWED_ARTICLES_LIST_HIDDEN);
        enumNames.put(
                "kCannotParseNetworkResponseBody",
                LoadStreamStatus.CANNOT_PARSE_NETWORK_RESPONSE_BODY);
        enumNames.put("kLoadMoreModelIsNotLoaded", LoadStreamStatus.LOAD_MORE_MODEL_IS_NOT_LOADED);
        enumNames.put(
                "kLoadNotAllowedDisabledByEnterprisePolicy",
                LoadStreamStatus.LOAD_NOT_ALLOWED_DISABLED_BY_ENTERPRISE_POLICY);
        enumNames.put("kNetworkFetchFailed", LoadStreamStatus.NETWORK_FETCH_FAILED);
        enumNames.put(
                "kCannotLoadMoreNoNextPageToken",
                LoadStreamStatus.CANNOT_LOAD_MORE_NO_NEXT_PAGE_TOKEN);
        enumNames.put(
                "kDataInStoreStaleMissedLastRefresh",
                LoadStreamStatus.DATA_IN_STORE_STALE_MISSED_LAST_REFRESH);
        enumNames.put(
                "kLoadedStaleDataFromStoreDueToNetworkFailure",
                LoadStreamStatus.LOADED_STALE_DATA_FROM_STORE_DUE_TO_NETWORK_FAILURE);
        enumNames.put("kDataInStoreIsExpired", LoadStreamStatus.DATA_IN_STORE_IS_EXPIRED);
        return enumNames;
    }

    public static void waitForRecyclerItems(int minItems, RecyclerView recyclerView) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Recycler view exists", recyclerView, Matchers.notNullValue());
                    Criteria.checkThat(
                            "Items are loaded",
                            recyclerView.getAdapter().getItemCount(),
                            Matchers.greaterThan(minItems));
                });
    }
}
