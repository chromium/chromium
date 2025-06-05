// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * A coordinator for SharedImageTiles component. This component is used to build a view and populate
 * shared image tilese details.
 */
@NullMarked
public class SharedImageTilesCoordinator {

    // The maximum amount of tiles that can show, including icon tile and count tile.
    private static final int MAX_TILES_UI_LIMIT = 3;

    private final Context mContext;
    private final PropertyModel mModel;
    private final SharedImageTilesView mView;
    private final @NonNull DataSharingService mDataSharingService;
    private final @NonNull CollaborationService mCollaborationService;
    private @Nullable String mCollaborationId;
    private int mAvailableMemberCount;
    private int mIconTilesCount;

    private @Nullable UpdateTracker mTracker;

    /**
     * Constructor for {@link SharedImageTilesCoordinator} component.
     *
     * @param context The Android context used to inflate the views.
     * @param config The {@link SharedImageTilesConfig} for styling the component.
     * @param dataSharingService Used to access UI delegate.
     * @param collaborationService Used to fetch collaboration group data.
     */
    public SharedImageTilesCoordinator(
            Context context,
            SharedImageTilesConfig config,
            @NonNull DataSharingService dataSharingService,
            @NonNull CollaborationService collaborationService) {
        mModel =
                new PropertyModel.Builder(SharedImageTilesProperties.ALL_KEYS)
                        .with(SharedImageTilesProperties.VIEW_CONFIG, config)
                        .build();
        mContext = context;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;

        mView =
                (SharedImageTilesView)
                        LayoutInflater.from(mContext).inflate(R.layout.shared_image_tiles, null);

        PropertyModelChangeProcessor.create(mModel, mView, SharedImageTilesViewBinder::bind);
        new SharedImageTilesMediator(mModel);
    }

    /**
     * Update the styling configuration of the current tab group.
     *
     * @param config The {@link SharedImageTilesConfig} for styling the component.
     */
    public void updateConfig(SharedImageTilesConfig config) {
        mModel.set(SharedImageTilesProperties.VIEW_CONFIG, config);
    }

    /** Cleans up any resources or observers this class used. */
    public void destroy() {
        resetTracker();
    }

    /**
     * Fetch new images given a collaboration ID. Should be called again if the members change.
     *
     * @param collaborationId The new collaborationId or null to reset.
     */
    public void fetchImagesForCollaborationId(@Nullable String collaborationId) {
        fetchImagesForCollaborationId(collaborationId, CallbackUtils.emptyCallback());
    }

    /**
     * Fetch new images given a collaboration ID with a finished callback. Should be called again if
     * the members change.
     *
     * @param collaborationId The new collaborationId or null to reset.
     * @param finishedCallback The callback to notify about the SharedImageTiles update status.
     */
    public void fetchImagesForCollaborationId(
            @Nullable String collaborationId, Callback<Boolean> finishedCallback) {
        if (!updateCollaborationIdValid(collaborationId)) {
            resetTracker();
            return;
        }

        resetTracker();

        assumeNonNull(mCollaborationId);
        GroupData groupData = mCollaborationService.getGroupData(mCollaborationId);
        if (groupData == null) {
            // Error occurred. Remove all view.
            updateMembersCount(0);
            finishedCallback.onResult(false);
            return;
        }
        onGroupMembersChangedInternal(groupData.members, finishedCallback);
    }

    /**
     * Updates the group using a list of already read {@link GroupMember} entities.
     *
     * @param collaborationId The collaboration ID for the group the members belong to.
     * @param members The list of group members.
     */
    public void onGroupMembersChanged(
            @Nullable String collaborationId, @Nullable List<GroupMember> members) {
        if (!updateCollaborationIdValid(collaborationId)) return;

        resetTracker();

        onGroupMembersChangedInternal(members, CallbackUtils.emptyCallback());
    }

    /**
     * Get the view component of SharedImageTiles. Note: the imageViews inside the
     * SharedImageTilesView are loaded async and might not be ready yet.
     */
    public SharedImageTilesView getView() {
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
    public Context getContext() {
        return mContext;
    }

    @VisibleForTesting
    void updateMembersCount(int count) {
        mAvailableMemberCount = count;
        mModel.set(SharedImageTilesProperties.REMAINING_TILES, 0);
        mModel.set(SharedImageTilesProperties.ICON_TILES, 0);
        mModel.set(SharedImageTilesProperties.SHOW_MANAGE_TILE, false);
        initializeSharedImageTiles();
    }

    private void resetTracker() {
        if (mTracker == null) return;

        mTracker.reset();
        mTracker = null;
    }

    private boolean updateCollaborationIdValid(@Nullable String collaborationId) {
        mCollaborationId = collaborationId;
        if (mCollaborationId == null) {
            updateMembersCount(0);
            return false;
        }
        return true;
    }

    private void onGroupMembersChangedInternal(
            @Nullable List<GroupMember> members, Callback<Boolean> finishedCallback) {
        List<GroupMember> validMembers = new ArrayList<>();
        if (members != null) {
            for (GroupMember member : members) {
                if (member.email != null && !member.email.isEmpty()) {
                    validMembers.add(member);
                }
            }
        }
        int count = validMembers.size();
        updateMembersCount(count);

        if (count == 0) return;

        int sizeInDp = mModel.get(SharedImageTilesProperties.VIEW_CONFIG).iconSizeDp;

        mTracker =
                new UpdateTracker(
                        mContext,
                        validMembers,
                        getAllIconViews(),
                        getAvatarSizeInPixelsUnscaled(sizeInDp),
                        mDataSharingService.getUiDelegate(),
                        finishedCallback);
    }

    private static class UpdateTracker {
        private @Nullable Callback<Boolean> mFinishedCallback;
        private int mWaitingCount;
        private boolean mReset;

        UpdateTracker(
                Context context,
                List<GroupMember> validMembers,
                List<ImageView> iconViews,
                int sizeInPx,
                DataSharingUIDelegate dataSharingUiDelegate,
                Callback<Boolean> finishedCallback) {
            mFinishedCallback = finishedCallback;
            mReset = false;
            @ColorInt int fallbackColor = SemanticColorUtils.getDefaultIconColorAccent1(context);

            mWaitingCount = iconViews.size();
            assert mWaitingCount <= validMembers.size();
            for (int i = 0; i < iconViews.size(); i++) {
                ImageView imageView = iconViews.get(i);
                GroupMember member = validMembers.get(i);
                DataSharingAvatarBitmapConfig.DataSharingAvatarCallback avatarCallback =
                        (bitmap) -> {
                            if (!mReset) {
                                imageView.setImageBitmap(bitmap);

                                mWaitingCount -= 1;
                                if (mWaitingCount == 0) {
                                    finishedCallback.onResult(true);
                                }
                            }
                        };
                DataSharingAvatarBitmapConfig config =
                        new DataSharingAvatarBitmapConfig.Builder()
                                .setContext(context)
                                .setGroupMember(member)
                                .setAvatarSizeInPixels(sizeInPx)
                                .setAvatarFallbackColor(fallbackColor)
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
            assumeNonNull(mFinishedCallback);
            mFinishedCallback.onResult(false);
            mFinishedCallback = null;
        }
    }

    private int getAvatarSizeInPixelsUnscaled(int sizeInDp) {
        // Returns the given unscaled value converted from dp to px.
        float sizeInPx = mContext.getResources().getDimensionPixelSize(sizeInDp);
        if (DisplayUtil.isUiScaled()) {
            // Unscaling once is needed here because else we apply the scaling factor twice:
            // 1. When converting dp -> px.
            // 2. When displaying the bitmap.
            // This unscaling undoes the 1st scaling and let the avatar show normally. More details
            // at crbug.com/404572952.
            sizeInPx = sizeInPx / DisplayUtil.getCurrentUiScalingFactor(mContext);
        }

        return (int) sizeInPx;
    }

    /** Populate the shared_image_tiles container with the specific icons. */
    private void initializeSharedImageTiles() {
        if (mAvailableMemberCount == 0) {
            mIconTilesCount = 0;
            mModel.set(SharedImageTilesProperties.ICON_TILES, 0);
            mModel.set(SharedImageTilesProperties.REMAINING_TILES, 0);
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

        // Add manage tile.
        mModel.set(SharedImageTilesProperties.SHOW_MANAGE_TILE, mIconTilesCount == 1);

        // Add number tile.
        if (showNumberTile) {
            // Compute a count bubble.
            mModel.set(
                    SharedImageTilesProperties.REMAINING_TILES,
                    mAvailableMemberCount - maxTilesToShowWithNumberTile);
        }
    }
}
