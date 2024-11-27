// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.PeopleGroupActionFailure;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
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

    private final Context mContext;
    private final PropertyModel mModel;
    private final SharedImageTilesView mView;
    private final @SharedImageTilesType int mType;
    private final @NonNull DataSharingService mDataSharingService;
    private @NonNull String mCollaborationId;
    private int mAvailableMemberCount;
    private int mIconTilesCount;

    private UpdateTracker mTracker;

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
            SharedImageTilesColor color,
            @NonNull DataSharingService dataSharingService) {
        mModel =
                new PropertyModel.Builder(SharedImageTilesProperties.ALL_KEYS)
                        .with(SharedImageTilesProperties.TYPE, type)
                        .with(SharedImageTilesProperties.COLOR_STYLE, color)
                        .build();
        mContext = context;
        mDataSharingService = dataSharingService;
        mType = type;

        mView =
                (SharedImageTilesView)
                        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles, null);

        PropertyModelChangeProcessor.create(mModel, mView, SharedImageTilesViewBinder::bind);
        new SharedImageTilesMediator(mModel);
    }

    /**
     * Update the color style of the current view.
     *
     * @param color The updated {@link SharedImageTilesColor}.
     */
    public void updateColorStyle(SharedImageTilesColor color) {
        mModel.set(SharedImageTilesProperties.COLOR_STYLE, color);
    }

    /** Cleans up any resources or observers this class used. */
    public void destroy() {}

    /**
     * Update the collaborationId for a SharedImageTiles component.
     *
     * @param collaborationId The new collaborationId or null to reset.
     */
    public void updateCollaborationId(@Nullable String collaborationId) {
        updateCollaborationId(collaborationId, CallbackUtils.emptyCallback());
    }

    /**
     * Update the collaborationId for a SharedImageTiles component with a finished callback.
     *
     * @param collaborationId The new collaborationId or null to reset.
     * @param finishedCallback The callback to notify about the SharedImageTiles update status.
     */
    public void updateCollaborationId(
            @Nullable String collaborationId, Callback<Boolean> finishedCallback) {
        mCollaborationId = collaborationId;
        if (mCollaborationId == null) {
            updateMembersCount(0);
            return;
        }

        if (mTracker != null) {
            mTracker.reset();
            mTracker = null;
        }

        // Fetch group information from DataSharingService.
        // TODO(crbug.com/381138936): Migrate to cached readGroup.
        mDataSharingService.readGroup(
                mCollaborationId,
                (result) -> {
                    if (result.actionFailure != PeopleGroupActionFailure.UNKNOWN) {
                        // Error occurred. Remove all view.
                        updateMembersCount(0);
                        finishedCallback.onResult(false);
                        return;
                    }

                    assert result.groupData != null;
                    List<GroupMember> validMembers = new ArrayList<>();
                    for (GroupMember member : result.groupData.members) {
                        if (member.email != null && !member.email.isEmpty()) {
                            validMembers.add(member);
                        }
                    }
                    updateMembersCount(validMembers.size());

                    int sizeInDp =
                            (mType == SharedImageTilesType.SMALL)
                                    ? R.dimen.small_shared_image_tiles_icon_height
                                    : R.dimen.shared_image_tiles_icon_height;
                    mTracker =
                            new UpdateTracker(
                                    mContext,
                                    validMembers,
                                    getAllIconViews(),
                                    getAvatarSizeInPixels(sizeInDp),
                                    mDataSharingService.getUiDelegate(),
                                    finishedCallback);
                });
    }

    /**
     * Get the view component of SharedImageTiles. Note: the imageViews inside the
     * SharedImageTilesView are loaded async and might not be ready yet.
     */
    public @NonNull SharedImageTilesView getView() {
        return mView;
    }

    /** Get all icon views. */
    public List<ImageView> getAllIconViews() {
        assert (mView.getChildCount() >= mIconTilesCount);
        List<ImageView> list = new ArrayList<>();
        for (int i = 0; i < mIconTilesCount; i++) {
            ViewGroup view_group = (ViewGroup) mView.getChildAt(i);
            assert view_group.getChildCount() == 1;
            ImageView view = (ImageView) view_group.getChildAt(0);
            list.add(view);
        }
        return list;
    }

    /** Get the Android context used by the component. */
    public @NonNull Context getContext() {
        return mContext;
    }

    @VisibleForTesting
    void updateMembersCount(int count) {
        mAvailableMemberCount = count;
        mModel.set(SharedImageTilesProperties.REMAINING_TILES, 0);
        mModel.set(SharedImageTilesProperties.ICON_TILES, 0);
        initializeSharedImageTiles();
    }

    private static class UpdateTracker {
        private Callback<Boolean> mFinishedCallback;
        private int mWaitingCount;
        private boolean mReset;

        UpdateTracker(
                Context context,
                List<GroupMember> validMembers,
                List<ImageView> iconViews,
                int sizeInPx,
                @NonNull DataSharingUIDelegate dataSharingUiDelegate,
                Callback<Boolean> finishedCallback) {
            mFinishedCallback = finishedCallback;
            mReset = false;

            mWaitingCount = iconViews.size();
            assert mWaitingCount <= validMembers.size();
            for (int i = 0; i < iconViews.size(); i++) {
                ImageView imageView = iconViews.get(i);
                GroupMember member = validMembers.get(i);
                DataSharingAvatarBitmapConfig.DataSharingAvatarCallback avatarCallback =
                        new DataSharingAvatarBitmapConfig.DataSharingAvatarCallback() {
                            @Override
                            public void onAvatarLoaded(Bitmap bitmap) {
                                if (!mReset) {
                                    imageView.setImageBitmap(bitmap);

                                    mWaitingCount -= 1;
                                    if (mWaitingCount == 0) {
                                        finishedCallback.onResult(true);
                                    }
                                }
                            }
                        };
                DataSharingAvatarBitmapConfig config =
                        new DataSharingAvatarBitmapConfig.Builder()
                                .setContext(context)
                                .setGroupMember(member)
                                .setAvatarSizeInPixels(sizeInPx)
                                .setDataSharingAvatarCallback(avatarCallback)
                                .build();
                dataSharingUiDelegate.getAvatarBitmap(config);
            }
        }

        void reset() {
            if (mReset) {
                return;
            }
            mReset = true;
            mFinishedCallback.onResult(false);
            mFinishedCallback = null;
        }
    }

    private int getAvatarSizeInPixels(int sizeInDp) {
        return mContext.getResources().getDimensionPixelSize(sizeInDp);
    }

    /** Populate the shared_image_tiles container with the specific icons. */
    private void initializeSharedImageTiles() {
        if (mAvailableMemberCount == 0) {
            return;
        }

        int maxTilesToShowWithNumberTile = MAX_TILES_UI_LIMIT - 1;
        boolean showNumberTile = mAvailableMemberCount > MAX_TILES_UI_LIMIT;
        mIconTilesCount =
                showNumberTile
                        ? MAX_TILES_UI_LIMIT - 1
                        : Math.min(mAvailableMemberCount, MAX_TILES_UI_LIMIT);

        // Add icon tile(s).
        mModel.set(SharedImageTilesProperties.ICON_TILES, mIconTilesCount);

        // Add number tile.
        if (showNumberTile) {
            // Compute a count bubble.
            mModel.set(
                    SharedImageTilesProperties.REMAINING_TILES,
                    mAvailableMemberCount - maxTilesToShowWithNumberTile);
        }
    }
}
