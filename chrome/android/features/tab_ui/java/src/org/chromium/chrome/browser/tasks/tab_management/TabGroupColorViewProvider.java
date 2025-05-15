// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Provides a view for tab group color dots and shared image tiles if a collaboration. To properly
 * cleanup this class {@link #destroy()} must be invoked in order to remove observers and prevent it
 * from living indefinitely.
 */
@NullMarked
public class TabGroupColorViewProvider implements Destroyable {
    private final Callback<@Nullable List<GroupMember>> mOnGroupMembersChanged =
            this::onGroupMembersChanged;
    private final Callback<@Nullable Integer> mOnGroupSharedStateChanged =
            this::onGroupSharedStateChanged;
    private final Context mContext;
    private final boolean mIsIncognito;
    private final @Nullable DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;
    private final @Nullable TransitiveSharedGroupObserver mTransitiveSharedGroupObserver;

    private EitherGroupId mGroupId;
    private @TabGroupColorId int mColorId;
    private @Nullable FrameLayout mFrameLayout;
    private @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    private SharedImageTilesConfig.@Nullable Builder mSharedImageTilesConfigBuilder;

    /**
     * @param context The context to use to use for creating the view.
     * @param groupId The tab group id or sync id for the group stored as {@link EitherGroupId}.
     * @param isIncognito Whether the tab group is incognito.
     * @param colorId The {@link TabGroupColorId} to show for the main color.
     * @param tabGroupSyncService Used to fetch the current collaboration id of the group.
     * @param dataSharingService Used to fetch and observe current share data.
     * @param collaborationService Used to fetch current service status.
     */
    public TabGroupColorViewProvider(
            Context context,
            EitherGroupId groupId,
            boolean isIncognito,
            @TabGroupColorId int colorId,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService,
            CollaborationService collaborationService) {
        assert groupId != null : "Tab group id cannot be null.";
        mContext = context;
        mGroupId = groupId;
        mIsIncognito = isIncognito;
        mColorId = colorId;
        mCollaborationService = collaborationService;

        if (tabGroupSyncService != null
                && dataSharingService != null
                && mCollaborationService.getServiceStatus().isAllowedToJoin()
                && groupId.isLocalId()) {
            mDataSharingService = dataSharingService;
            mTransitiveSharedGroupObserver =
                    new TransitiveSharedGroupObserver(
                            tabGroupSyncService, dataSharingService, mCollaborationService);
            mTransitiveSharedGroupObserver.setTabGroupId(groupId.getLocalId().tabGroupId);
            mTransitiveSharedGroupObserver
                    .getGroupMembersSupplier()
                    .addObserver(mOnGroupMembersChanged);
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .addObserver(mOnGroupSharedStateChanged);
        } else {
            mDataSharingService = null;
            mTransitiveSharedGroupObserver = null;
        }
    }

    @Override
    public void destroy() {
        if (mTransitiveSharedGroupObserver != null) {
            mTransitiveSharedGroupObserver.destroy();
            mTransitiveSharedGroupObserver
                    .getGroupMembersSupplier()
                    .removeObserver(mOnGroupMembersChanged);
            mTransitiveSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .removeObserver(mOnGroupSharedStateChanged);
            detachAndDestroySharedImageTiles();
        }
    }

    /** Returns whether the group is a collaboration. */
    public boolean hasCollaborationId() {
        if (mTransitiveSharedGroupObserver == null) return false;

        return TabShareUtils.isCollaborationIdValid(
                mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get());
    }

    /**
     * Sets the group id to observer. This {@link EitherGroupId} cannot be null.
     *
     * @param groupId The group id to use.
     */
    public void setTabGroupId(EitherGroupId groupId) {
        mGroupId = groupId;

        if (mTransitiveSharedGroupObserver != null && groupId.isLocalId()) {
            mTransitiveSharedGroupObserver.setTabGroupId(groupId.getLocalId().tabGroupId);
        }
    }

    /**
     * Sets the tab group color by id, this will update the view immediately if it exists.
     *
     * @param colorId The color id to use.
     */
    public void setTabGroupColorId(@TabGroupColorId int colorId) {
        mColorId = colorId;

        if (mFrameLayout != null) {
            updateColorAndSize();
        }
    }

    /** Returns the color dot view, creating it if it does not exist. */
    public View getLazyView() {
        if (mFrameLayout == null) {
            mFrameLayout =
                    (FrameLayout)
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.tab_group_color_container, null);
            assert mFrameLayout != null;

            maybeCreateAndAttachSharedImageTiles();
            updateColorAndSize();
        }
        return mFrameLayout;
    }

    private void updateColorAndSize() {
        assert mFrameLayout != null;

        GradientDrawable drawable = (GradientDrawable) mFrameLayout.getBackground().mutate();
        assert drawable != null;

        final @ColorInt int fillColor =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                        mContext, mColorId, mIsIncognito);
        drawable.setColor(fillColor);

        Resources res = mContext.getResources();
        final float radius;
        final @Px int size;
        if (mSharedImageTilesCoordinator == null) {
            size = res.getDimensionPixelSize(R.dimen.tab_group_color_icon_item_size);
            radius = res.getDimension(R.dimen.tab_group_color_icon_item_radius);
        } else {
            SharedImageTilesConfig config =
                    assumeNonNull(mSharedImageTilesConfigBuilder)
                            .setTabGroupColor(mContext, mColorId)
                            .build();

            mSharedImageTilesCoordinator.updateConfig(config);

            final @Px int stroke = res.getDimensionPixelSize(R.dimen.tab_group_color_icon_stroke);
            size = config.getBorderAndTotalIconSizes(res).second + 2 * stroke;
            // Ceiling division does not exist in the Math package; although there is a JDK proposal
            // for it to be added. Ceiling division is required here to ensure the radius is >= half
            // the size.
            int divCeilRadius = (size + 1) / 2;
            radius = divCeilRadius;
        }

        float[] radii = new float[8];
        Arrays.fill(radii, radius);
        drawable.setCornerRadii(radii);

        if (mFrameLayout.getMinimumWidth() != size) {
            mFrameLayout.setMinimumWidth(size);
            mFrameLayout.setMinimumHeight(size);
        }

        mFrameLayout.invalidate();
    }

    private void maybeCreateAndAttachSharedImageTiles() {
        if (mDataSharingService == null) return;
        assumeNonNull(mTransitiveSharedGroupObserver);

        if (mSharedImageTilesCoordinator != null) {
            assert mFrameLayout != null : "SharedImageTiles should only exist if a view exists.";
            return;
        }

        if (mFrameLayout == null) return;

        @Nullable String collaborationId =
                mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
        if (!TabShareUtils.isCollaborationIdValid(collaborationId)) return;

        @GroupSharedState
        @Nullable Integer groupSharedState =
                mTransitiveSharedGroupObserver.getGroupSharedStateSupplier().get();
        if (!shouldShowSharedImageTiles(groupSharedState)) return;

        mSharedImageTilesConfigBuilder =
                SharedImageTilesConfig.Builder.createForTabGroupColorContext(mContext, mColorId);

        mSharedImageTilesCoordinator =
                new SharedImageTilesCoordinator(
                        mContext,
                        mSharedImageTilesConfigBuilder.build(),
                        mDataSharingService,
                        mCollaborationService);
        mSharedImageTilesCoordinator.fetchImagesForCollaborationId(collaborationId);

        View view = mSharedImageTilesCoordinator.getView();
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT);
        // Margin is required to properly center the view. Using gravity results in inconsistent
        // behaviors between devices.
        final @Px int margin =
                mContext.getResources().getDimensionPixelSize(R.dimen.tab_group_color_icon_stroke);
        params.setMarginStart(margin);
        params.topMargin = margin;
        mFrameLayout.addView(view, params);
        updateColorAndSize();
    }

    private void detachAndDestroySharedImageTiles() {
        if (mSharedImageTilesCoordinator != null) {
            mSharedImageTilesCoordinator.destroy();
            mSharedImageTilesCoordinator = null;
        }
        if (mFrameLayout != null && mFrameLayout.getChildCount() != 0) {
            mFrameLayout.removeAllViews();
            updateColorAndSize();
        }
    }

    private void onGroupMembersChanged(@Nullable List<GroupMember> members) {
        if (mSharedImageTilesCoordinator == null) return;

        assumeNonNull(mTransitiveSharedGroupObserver);
        @Nullable String collaborationId =
                mTransitiveSharedGroupObserver.getCollaborationIdSupplier().get();
        if (members != null && TabShareUtils.isCollaborationIdValid(collaborationId)) {
            mSharedImageTilesCoordinator.onGroupMembersChanged(collaborationId, members);
        } else {
            mSharedImageTilesCoordinator.onGroupMembersChanged(
                    /* collaborationId= */ null, /* members= */ null);
        }
    }

    private void onGroupSharedStateChanged(@Nullable @GroupSharedState Integer groupSharedState) {
        if (shouldShowSharedImageTiles(groupSharedState)) {
            maybeCreateAndAttachSharedImageTiles();
        } else {
            detachAndDestroySharedImageTiles();
        }
    }

    private static boolean shouldShowSharedImageTiles(
            @GroupSharedState @Nullable Integer groupSharedState) {
        return groupSharedState != null
                && groupSharedState != GroupSharedState.NOT_SHARED
                && groupSharedState != GroupSharedState.COLLABORATION_ONLY;
    }

    @TabGroupColorId
    int getTabGroupColorIdForTesting() {
        return mColorId;
    }

    EitherGroupId getTabGroupIdForTesting() {
        return mGroupId;
    }
}
