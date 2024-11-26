// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;
import android.view.View.OnClickListener;

import androidx.annotation.NonNull;

import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.ActivityLogQueryParams;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Core business logic for the recent activity UI. Populates a {@link ModelList} from a list of
 * recent activities obtained from the messaging backend.
 */
class RecentActivityListMediator {
    private final Context mContext;
    private final ModelList mModelList;
    private final MessagingBackendService mMessagingBackendService;
    private final Runnable mCloseBottomSheetCallback;

    /**
     * Constructor.
     *
     * @param context The activity context.
     * @param modelList The {@link ModelList} that will be filled with the list items to be shown.
     * @param messagingBackendService The backed to query for the list of recent activities.
     */
    public RecentActivityListMediator(
            @NonNull Context context,
            @NonNull ModelList modelList,
            MessagingBackendService messagingBackendService,
            Runnable closeBottomSheetCallback) {
        mContext = context;
        mModelList = modelList;
        mMessagingBackendService = messagingBackendService;
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

            // Add the item to the list.
            mModelList.add(new ListItem(0, propertyModel));
        }
    }

    private OnClickListener createActivityLogItemOnClickListener(ActivityLogItem logItem) {
        return view -> {
            assert logItem != null;
            // TODO(crbug.com/380962101): Invoke backend to switch to take action.
            mCloseBottomSheetCallback.run();
        };
    }
}
