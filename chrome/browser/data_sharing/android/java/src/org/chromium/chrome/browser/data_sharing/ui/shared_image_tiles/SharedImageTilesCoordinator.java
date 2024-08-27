// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * A coordinator for SharedImageTiles component. This component is used to build a view and populate
 * shared image tilese details.
 */
public class SharedImageTilesCoordinator {

    // The maximum amount of tiles that can show, including icon tile and count tile.
    private static final int MAX_TILES_UI_LIMIT = 3;
    // The maximum number appearing on the count number tile.
    private static final int MAX_COUNT_TILE_NUMBER = 99;
    // The maximum single digit number for the count number tile.
    private static final int MAX_SINGLE_DIGIT_NUMBER = 9;

    private final SharedImageTilesMediator mMediator;
    private final Context mContext;
    private final PropertyModel mModel;
    private final SharedImageTilesView mView;
    private final @SharedImageTilesType int mType;
    private final @NonNull DataSharingService mDataSharingService;
    private @NonNull String mCollaborationId;
    private int mAvailableTileCount;
    private int mIconTilesCount;

    /**
     * Constructor for {@link SharedImageTilesCoordinator} component.
     *
     * @param context The Android context used to inflate the views.
     * @param type The {@link SharedImageTilesType} of the SharedImageTiles.
     * @param color The {@link SharedImageTilesColor} of the SharedImageTiles.
     * @param dataSharingService Used to fetch tab group data.
     */
    public SharedImageTilesCoordinator(
            Context context,
            @SharedImageTilesType int type,
            @SharedImageTilesColor int color,
            @NonNull DataSharingService dataSharingService) {
        mModel =
                new PropertyModel.Builder(SharedImageTilesProperties.ALL_KEYS)
                        .with(SharedImageTilesProperties.COLOR_THEME, color)
                        .build();
        mContext = context;
        mType = type;
        mDataSharingService = dataSharingService;

        mView =
                (SharedImageTilesView)
                        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles, null);

        PropertyModelChangeProcessor.create(mModel, mView, SharedImageTilesViewBinder::bind);
        mMediator = new SharedImageTilesMediator(mModel);

        initializeSharedImageTiles();
    }

    /**
     * Update the collaborationId for a SharedImageTiles component.
     *
     * @param collaborationId The new collaborationId.
     */
    public void updateCollaborationId(@NonNull String collaborationId) {
        mCollaborationId = collaborationId;
        // Fetch group information from DataSharingService.
        mDataSharingService.readGroup(
                mCollaborationId,
                (result) -> {
                    if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN) {
                        // Error occurred. Remove all view.
                        updateTilesCount(0);
                        return;
                    }

                    assert result.groupData != null;
                    extractGroupMemberInfo(result.groupData);
                });
    }

    private void extractGroupMemberInfo(GroupData groupData) {
        // TODO(b/361642045): Call showAvatars here.
        updateTilesCount(groupData.members.size());
    }

    /** Populate the shared_image_tiles container with the specific icons. */
    private void initializeSharedImageTiles() {
        if (mAvailableTileCount == 0) {
            return;
        }

        int maxTilesToShowWithNumberTile = MAX_TILES_UI_LIMIT - 1;
        boolean showNumberTile = mAvailableTileCount > MAX_TILES_UI_LIMIT;
        mIconTilesCount =
                showNumberTile
                        ? MAX_TILES_UI_LIMIT - 1
                        : Math.min(mAvailableTileCount, MAX_TILES_UI_LIMIT);

        // Add icon tile(s).
        mModel.set(SharedImageTilesProperties.ICON_TILES, mIconTilesCount);

        // Add number tile.
        if (showNumberTile) {
            // Compute a count bubble.
            mModel.set(
                    SharedImageTilesProperties.REMAINING_TILES,
                    mAvailableTileCount - maxTilesToShowWithNumberTile);
        } else {
            if (mType == SharedImageTilesType.CLICKABLE && mIconTilesCount < MAX_TILES_UI_LIMIT) {
                // Append an add person button.
                mModel.set(SharedImageTilesProperties.SHOW_ADD_BUTTON, true);
            }
        }
    }

    /** Get the view component of SharedImageTiles. */
    public @NonNull SharedImageTilesView getView() {
        return mView;
    }

    /**
     * Update the tiles count for a SharedImageTiles component.
     *
     * @param count The new count number.
     */
    public void updateTilesCount(int count) {
        // TODO(b/325533985): |mAvailableTileCount| should be replace by the actual number of icons
        // needed.
        mAvailableTileCount = count;
        mModel.set(SharedImageTilesProperties.SHOW_ADD_BUTTON, false);
        mModel.set(SharedImageTilesProperties.REMAINING_TILES, 0);
        mModel.set(SharedImageTilesProperties.ICON_TILES, 0);
        initializeSharedImageTiles();
    }

    /** Get all icon views. */
    public List<ViewGroup> getAllIconViews() {
        assert (mView.getChildCount() >= mIconTilesCount);
        List<ViewGroup> list = new ArrayList<>();
        for (int i = 0; i < mIconTilesCount; i++) {
            View view = mView.getChildAt(i);
            list.add((ViewGroup) view);
        }
        return list;
    }

    /** Get the Android context used by the component. */
    public @NonNull Context getContext() {
        return mContext;
    }
}
