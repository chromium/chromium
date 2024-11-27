// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;
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
    private final ModelList mModelList;
    private final MessagingBackendService mMessagingBackendService;
    private final FaviconProvider mFaviconProvider;
    private final AvatarProvider mAvatarProvider;
    private final Callback<Integer> mFocusTabCallback;
    private final Callback<String> mReopenTabCallback;
    private final Runnable mOpenTabGroupEditDialogCallback;
    private final Runnable mManageSharingCallback;
    private final Runnable mCloseBottomSheetCallback;

    /**
     * Constructor.
     *
     * @param context The activity context.
     * @param modelList The {@link ModelList} that will be filled with the list items to be shown.
     * @param messagingBackendService The backed to query for the list of recent activities.
     * @param faviconProvider The backend for providing favicon for URLs.
     * @param avatarProvider The backend for providing avatars for users.
     * @param focusTabCallback Callback to invoke to switch to a tab.
     * @param reopenTabCallback Callback to invoke to reopen a removed tab from group.
     * @param openTabGroupEditDialogCallback Callback to invoke to open the tab group title / color
     *     editor dialog.
     * @param manageSharingCallback Callback to invoke to open the people group management screen.
     */
    public RecentActivityListMediator(
            @NonNull Context context,
            @NonNull ModelList modelList,
            MessagingBackendService messagingBackendService,
            FaviconProvider faviconProvider,
            AvatarProvider avatarProvider,
            Callback<Integer> focusTabCallback,
            Callback<String> reopenTabCallback,
            Runnable openTabGroupEditDialogCallback,
            Runnable manageSharingCallback,
            Runnable closeBottomSheetCallback) {
        mContext = context;
        mModelList = modelList;
        mMessagingBackendService = messagingBackendService;
        mFaviconProvider = faviconProvider;
        mAvatarProvider = avatarProvider;
        mFocusTabCallback = focusTabCallback;
        mReopenTabCallback = reopenTabCallback;
        mOpenTabGroupEditDialogCallback = openTabGroupEditDialogCallback;
        mManageSharingCallback = manageSharingCallback;
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
                            .with(RecentActivityListProperties.TITLE_TEXT, logItem.titleText)
                            .with(
                                    RecentActivityListProperties.DESCRIPTION_TEXT,
                                    logItem.descriptionText)
                            .build();
            propertyModel.set(
                    RecentActivityListProperties.ON_CLICK_LISTENER,
                    createActivityLogItemOnClickListener(logItem));

            // Set favicon provider.
            GURL tabUrl = new GURL(getTabLastKnownUrl(logItem));
            Callback<ImageView> faviconCallback =
                    faviconView -> {
                        mFaviconProvider.fetchFavicon(tabUrl, faviconView::setImageDrawable);
                    };
            propertyModel.set(RecentActivityListProperties.FAVICON_PROVIDER, faviconCallback);

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
            // TODO(crbug.com/380962101): Move this switch case to native and provide an action enum
            // in the ActivityLogItem itself.
            switch (logItem.collaborationEvent) {
                case CollaborationEvent.TAB_ADDED:
                case CollaborationEvent.TAB_UPDATED:
                    int tabId = getTabMetadata(logItem).localTabId;
                    mFocusTabCallback.onResult(tabId);
                    break;
                case CollaborationEvent.TAB_REMOVED:
                    mReopenTabCallback.onResult(getTabLastKnownUrl(logItem));
                    break;
                case CollaborationEvent.TAB_GROUP_NAME_UPDATED:
                case CollaborationEvent.TAB_GROUP_COLOR_UPDATED:
                    mOpenTabGroupEditDialogCallback.run();
                    break;
                case CollaborationEvent.COLLABORATION_MEMBER_ADDED:
                case CollaborationEvent.COLLABORATION_MEMBER_REMOVED:
                    mManageSharingCallback.run();
                    break;
                default:
                    assert false
                            : "No handler for collaboration event " + logItem.collaborationEvent;
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
}
