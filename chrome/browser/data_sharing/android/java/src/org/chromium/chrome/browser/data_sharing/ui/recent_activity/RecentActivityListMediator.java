// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;
import android.view.View.OnClickListener;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.AvatarProvider;
import org.chromium.chrome.browser.data_sharing.ui.recent_activity.RecentActivityListCoordinator.FaviconProvider;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.ActivityLogQueryParams;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.RecentActivityAction;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Core business logic for the recent activity UI. Populates a {@link ModelList} from a list of
 * recent activities obtained from the messaging backend.
 */
class RecentActivityListMediator {
    private final Context mContext;
    private final PropertyModel mPropertyModel;
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
     * @param propertyModel The property model of the recent activity list container view.
     * @param modelList The {@link ModelList} that will be filled with the list items to be shown.
     * @param messagingBackendService The backed to query for the list of recent activities.
     * @param faviconProvider The backend for providing favicon for URLs.
     * @param avatarProvider The backend for providing avatars for users.
     * @param recentActivityActionHandler Click event handler for activity rows.
     * @param closeBottomSheetCallback Callback to invoke when bottom sheet is to be closed.
     */
    public RecentActivityListMediator(
            @NonNull Context context,
            @NonNull PropertyModel propertyModel,
            @NonNull ModelList modelList,
            MessagingBackendService messagingBackendService,
            FaviconProvider faviconProvider,
            AvatarProvider avatarProvider,
            RecentActivityActionHandler recentActivityActionHandler,
            Runnable closeBottomSheetCallback) {
        mContext = context;
        mPropertyModel = propertyModel;
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
        mPropertyModel.set(
                RecentActivityContainerProperties.EMPTY_STATE_VISIBLE, activityLogItems.isEmpty());
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
                            .with(RecentActivityListProperties.TITLE_TEXT, logItem.titleText)
                            .build();
            propertyModel.set(
                    RecentActivityListProperties.ON_CLICK_LISTENER,
                    createActivityLogItemOnClickListener(logItem));

            DescriptionAndTimestamp descriptionAndTimestamp = new DescriptionAndTimestamp();
            descriptionAndTimestamp.description = logItem.descriptionText;
            descriptionAndTimestamp.timestamp = logItem.timeDeltaText;
            descriptionAndTimestamp.separator =
                    mContext.getString(R.string.data_sharing_recent_activity_separator);
            descriptionAndTimestamp.descriptionFullTextResId =
                    R.string.data_sharing_recent_activity_description_full;

            propertyModel.set(
                    RecentActivityListProperties.DESCRIPTION_AND_TIMESTAMP_TEXT,
                    descriptionAndTimestamp);

            // Set favicon provider if favicon should be shown.
            Callback<ImageView> faviconCallback = null;
            if (logItem.showFavicon) {
                GURL tabUrl = new GURL(getTabLastKnownUrl(logItem));
                faviconCallback =
                        faviconView -> {
                            mFaviconProvider.fetchFavicon(tabUrl, faviconView::setImageDrawable);
                        };
            }
            propertyModel.set(RecentActivityListProperties.FAVICON_PROVIDER, faviconCallback);

            // Set avatar provider.
            Callback<ImageView> avatarCallback =
                    avatarView -> {
                        mAvatarProvider.getAvatarBitmap(
                                getUserToDisplay(logItem), avatarView::setImageDrawable);
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

    private @Nullable GroupMember getUserToDisplay(ActivityLogItem logItem) {
        assert logItem.activityMetadata != null : "ActivityMetadata is null";
        GroupMember member = logItem.activityMetadata.triggeringUser;
        if (member == null) {
            member = logItem.activityMetadata.affectedUser;
        }
        return member;
    }
}
