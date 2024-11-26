// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

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
        callback.run();
    }

    /** Called to clear the model when the bottom sheet is closed. */
    void onBottomSheetClosed() {
        mModelList.clear();
    }
}
