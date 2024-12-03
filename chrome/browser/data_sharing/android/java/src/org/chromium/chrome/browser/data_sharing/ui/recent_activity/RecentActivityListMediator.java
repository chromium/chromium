// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View.OnClickListener;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.AvatarProvider;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.FaviconProvider;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.ActivityLogQueryParams;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.RecentActivityAction;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Core business logic for the recent activity UI. Populates a {@link ModelList} from a list of
 * recent activities obtained from the messaging backend.
 */
class RecentActivityListMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final MessagingBackendService mMessagingBackendService;
    private final FaviconProvider mFaviconProvider;
    private final AvatarProvider mAvatarProvider;
    private final RecentActivityActionHandler mRecentActivityActionHandler;
    private final Runnable mCloseBottomSheetCallback;

    /**
     * Constructor.
     *
     * @param context The activity context.
     * @param modelList The {@link ModelList} that will be filled with the list items to be shown.
     * @param messagingBackendService The backed to query for the list of recent activities.
     * @param faviconProvider The backend for providing favicon for URLs.
     * @param avatarProvider The backend for providing avatars for users.
     * @param recentActivityActionHandler Click event handler for activity rows.
     */
    public RecentActivityListMediator(
            @NonNull Context context,
            @NonNull ModelList modelList,
            MessagingBackendService messagingBackendService,
            FaviconProvider faviconProvider,
            AvatarProvider avatarProvider,
            RecentActivityActionHandler recentActivityActionHandler,
            Runnable closeBottomSheetCallback) {
        mContext = context;
        mModelList = modelList;
        mMessagingBackendService = messagingBackendService;
        mFaviconProvider = faviconProvider;
        mAvatarProvider = avatarProvider;
        mRecentActivityActionHandler = recentActivityActionHandler;
        mCloseBottomSheetCallback = closeBottomSheetCallback;
        assert mContext != null;
        assert mMessagingBackendService != null;
        assert mCloseBottomSheetCallback != null;
    }

    /**
     * Called to start the UI creation. Populates the {@link ModelList} and notifies coordinator.
     *
     * @param collaborationId The associated collaboration ID.
     * @param callback The callback to run after populating the list.
     */
    void requestShowUI(String collaborationId, Runnable callback) {
        ActivityLogQueryParams activityLogQueryParams = new ActivityLogQueryParams();
        activityLogQueryParams.collaborationId = collaborationId;
        List<ActivityLogItem> activityLogItems =
                mMessagingBackendService.getActivityLog(activityLogQueryParams);
        updateModelList(activityLogItems);
        callback.run();
    }

    /** Called to clear the model when the bottom sheet is closed. */
    void onBottomSheetClosed() {
        mModelList.clear();
    }

    private void updateModelList(List<ActivityLogItem> activityLogItems) {
        for (ActivityLogItem logItem : activityLogItems) {
            if (logItem == null) {
                continue;
            }

            // Create a property model for the item.
            PropertyModel propertyModel =
                    new PropertyModel.Builder(RecentActivityListProperties.ALL_KEYS)
                            .with(RecentActivityListProperties.TITLE_TEXT, getTitleText(logItem))
                            .with(
                                    RecentActivityListProperties.DESCRIPTION_TEXT,
                                    getDescriptionText(logItem))
                            .build();
            propertyModel.set(
                    RecentActivityListProperties.ON_CLICK_LISTENER,
                    createActivityLogItemOnClickListener(logItem));

            // Set favicon provider if favicon should be shown.
            if (logItem.showFavicon) {
                GURL tabUrl = new GURL(getTabLastKnownUrl(logItem));
                Callback<ImageView> faviconCallback =
                        faviconView -> {
                            mFaviconProvider.fetchFavicon(tabUrl, faviconView::setImageDrawable);
                        };
                propertyModel.set(RecentActivityListProperties.FAVICON_PROVIDER, faviconCallback);
            }

            // Set avatar provider.
            Callback<ImageView> avatarCallback =
                    avatarView -> {
                        mAvatarProvider.getAvatarBitmap(
                                getTriggeringUser(logItem), avatarView::setImageDrawable);
                    };
            propertyModel.set(RecentActivityListProperties.AVATAR_PROVIDER, avatarCallback);

            // Add the item to the list.
            mModelList.add(new ListItem(0, propertyModel));
        }
    }

    private OnClickListener createActivityLogItemOnClickListener(ActivityLogItem logItem) {
        return view -> {
            assert logItem != null;
            switch (logItem.action) {
                case RecentActivityAction.FOCUS_TAB:
                    int tabId = getTabMetadata(logItem).localTabId;
                    mRecentActivityActionHandler.focusTab(tabId);
                    break;
                case RecentActivityAction.REOPEN_TAB:
                    mRecentActivityActionHandler.reopenTab(getTabLastKnownUrl(logItem));
                    break;
                case RecentActivityAction.OPEN_TAB_GROUP_EDIT_DIALOG:
                    mRecentActivityActionHandler.openTabGroupEditDialog();
                    break;
                case RecentActivityAction.MANAGE_SHARING:
                    mRecentActivityActionHandler.manageSharing();
                    break;
                default:
                    assert false : "No handler for recent activity action " + logItem.action;
            }
            mCloseBottomSheetCallback.run();
        };
    }

    private @NonNull TabMessageMetadata getTabMetadata(ActivityLogItem logItem) {
        assert logItem.activityMetadata != null : "ActivityMetadata is null";
        assert logItem.activityMetadata.tabMetadata != null : "TabMetadata is null";
        return logItem.activityMetadata.tabMetadata;
    }

    private @NonNull String getTabLastKnownUrl(ActivityLogItem logItem) {
        return getTabMetadata(logItem).lastKnownUrl;
    }

    private @NonNull GroupMember getTriggeringUser(ActivityLogItem logItem) {
        assert logItem.activityMetadata != null : "ActivityMetadata is null";
        assert logItem.activityMetadata.triggeringUser != null : "Triggering user is null";
        return logItem.activityMetadata.triggeringUser;
    }

    private @NonNull String getTitleText(ActivityLogItem logItem) {
        int stringRes = getTitleStringRes(logItem.collaborationEvent);
        String userNameText =
                logItem.userIsSelf
                        ? mContext.getString(R.string.data_sharing_recent_activity_user_self)
                        : logItem.userDisplayName;
        return mContext.getString(stringRes, userNameText);
    }

    private @NonNull String getDescriptionText(ActivityLogItem logItem) {
        String timeDeltaText = getRecencyString(mContext.getResources(), logItem.timeDeltaMs);
        if (TextUtils.isEmpty(logItem.description)) {
            return timeDeltaText;
        }
        return mContext.getString(
                R.string.data_sharing_recent_activity_description_full,
                logItem.description,
                timeDeltaText);
    }

    private static int getTitleStringRes(@CollaborationEvent int collaborationEvent) {
        switch (collaborationEvent) {
            case CollaborationEvent.TAB_ADDED:
                return R.string.data_sharing_recent_activity_tab_added;
            case CollaborationEvent.TAB_REMOVED:
                return R.string.data_sharing_recent_activity_tab_removed;
            case CollaborationEvent.TAB_UPDATED:
                return R.string.data_sharing_recent_activity_tab_updated;
            case CollaborationEvent.TAB_GROUP_NAME_UPDATED:
                return R.string.data_sharing_recent_activity_tab_group_name_updated;
            case CollaborationEvent.TAB_GROUP_COLOR_UPDATED:
                return R.string.data_sharing_recent_activity_tab_group_color_updated;
            case CollaborationEvent.COLLABORATION_MEMBER_ADDED:
                return R.string.data_sharing_recent_activity_user_joined_group;
            case CollaborationEvent.COLLABORATION_MEMBER_REMOVED:
                return R.string.data_sharing_recent_activity_user_left_group;
            case CollaborationEvent.TAB_GROUP_ADDED:
            case CollaborationEvent.COLLABORATION_ADDED:
            case CollaborationEvent.COLLABORATION_REMOVED:
            case CollaborationEvent.UNDEFINED:
                assert false : "No string res for collaboration event " + collaborationEvent;
                return 0;
        }
        return 0;
    }

    /**
     * Computes the string representation of how recent an event was, given the time delta.
     * TODO(crbug.com/380962101): Can we share a common util method with TabResumptionModuleUtils?
     *
     * @param res Resources for string resource retrieval.
     * @param timeDeltaMs Time delta in milliseconds.
     */
    private static String getRecencyString(Resources res, long timeDeltaMs) {
        if (timeDeltaMs < 0L) timeDeltaMs = 0L;

        long daysElapsed = TimeUnit.MILLISECONDS.toDays(timeDeltaMs);
        if (daysElapsed > 0L) {
            return res.getQuantityString(R.plurals.n_days_ago, (int) daysElapsed, daysElapsed);
        }

        long hoursElapsed = TimeUnit.MILLISECONDS.toHours(timeDeltaMs);
        if (hoursElapsed > 0L) {
            return res.getQuantityString(
                    R.plurals.n_hours_ago_narrow, (int) hoursElapsed, hoursElapsed);
        }

        // Bound recency to 1 min.
        long minutesElapsed = Math.max(1L, TimeUnit.MILLISECONDS.toMinutes(timeDeltaMs));
        return res.getQuantityString(
                R.plurals.n_minutes_ago_narrow, (int) minutesElapsed, minutesElapsed);
    }
}
