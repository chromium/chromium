// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * A coordinator for the recent activity UI. This includes (1) creating and managing bottom sheet,
 * and (2) building and showing the list of recent activity UI.
 */
public class RecentActivityListCoordinator {
    private final BottomSheetController mBottomSheetController;
    private final ModelList mModelList;
    private final View mContentContainer;
    private final RecyclerView mContentRecyclerView;
    private final RecentActivityListMediator mMediator;
    private RecentActivityBottomSheetContent mBottomSheetContent;

    /**
     * Constructor.
     *
     * @param context The context for creating views and loading resources.
     * @param bottomSheetController The controller for showing bottom sheet.
     * @param messagingBackendService The backend for providing the recent activity list.
     */
    public RecentActivityListCoordinator(
            Context context,
            @NonNull BottomSheetController bottomSheetController,
            MessagingBackendService messagingBackendService) {
        mModelList = new ModelList();
        mBottomSheetController = bottomSheetController;

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(mModelList);
        adapter.registerType(
                0,
                new LayoutViewBuilder(R.layout.recent_activity_log_item),
                RecentActivityListViewBinder::bind);

        mContentContainer =
                LayoutInflater.from(context)
                        .inflate(R.layout.recent_activity_bottom_sheet, /* root= */ null);
        mContentRecyclerView = mContentContainer.findViewById(R.id.recent_activity_recycler_view);
        mContentRecyclerView.setAdapter(adapter);

        mBottomSheetController.addObserver(
                new EmptyBottomSheetObserver() {
                    @Override
                    public void onSheetClosed(int reason) {
                        mBottomSheetController.removeObserver(this);
                        mMediator.onBottomSheetClosed();
                    }
                });

        mMediator =
                new RecentActivityListMediator(
                        context, mModelList, messagingBackendService, this::closeBottomSheet);
    }

    /**
     * Called to create the UI creation. Builds the list UI and after the list is created pulls up
     * the bottom sheet UI.
     *
     * @param collaborationId The collaboration ID for which recent activities are to be shown.
     */
    public void requestShowUI(String collaborationId) {
        mMediator.requestShowUI(collaborationId, this::showBottomSheet);
    }

    private void showBottomSheet() {
        mBottomSheetContent = new RecentActivityBottomSheetContent(mContentContainer);
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    private void closeBottomSheet() {
        mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
    }
}
