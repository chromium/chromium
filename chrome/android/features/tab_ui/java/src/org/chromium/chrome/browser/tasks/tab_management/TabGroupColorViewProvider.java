// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesColor;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesType;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;

import java.util.Arrays;

/**
 * Provides a view for tab group color dots and shared image tiles if a collaboration. To properly
 * cleanup this class {@link #destroy()} must be invoked in order to remove observers and prevent it
 * from living indefinitely.
 */
public class TabGroupColorViewProvider implements Destroyable {
    private final Callback<String> mOnCollaborationIdChanged = this::onCollaborationIdChanged;
    private final Callback<Integer> mOnGroupSharedStateChanged = this::onGroupSharedStateChanged;
    private final @NonNull Context mContext;
    private final @NonNull Token mTabGroupId;
    private final boolean mIsIncognito;
    private final @Nullable DataSharingService mDataSharingService;
    private final @Nullable SharedGroupObserver mSharedGroupObserver;

    private @TabGroupColorId int mColorId;
    private @Nullable FrameLayout mFrameLayout;
    private @Nullable SharedImageTilesCoordinator mSharedImageTilesCoordinator;

    /**
     * @param context The context to use to use for creating the view.
     * @param tabGroupId The tab group id for the group.
     * @param isIncognito Whether the tab group is incognito.
     * @param colorId The {@link TabGroupColorId} to show for the main color.
     * @param tabGroupSyncService Used to fetch the current collaboration id of the group.
     * @param dataSharingService Used to fetch and observe current share data.
     * @param collaborationService Used to fetch current service status.
     */
    public TabGroupColorViewProvider(
            @NonNull Context context,
            @NonNull Token tabGroupId,
            boolean isIncognito,
            @TabGroupColorId int colorId,
            @Nullable TabGroupSyncService tabGroupSyncService,
            @Nullable DataSharingService dataSharingService,
            @Nullable CollaborationService collaborationService) {
        assert tabGroupId != null : "Tab group id cannot be null.";
        mContext = context;
        mTabGroupId = tabGroupId;
        mIsIncognito = isIncognito;
        mColorId = colorId;

        boolean servicesExist = tabGroupSyncService != null && dataSharingService != null;
        if (servicesExist && collaborationService.getServiceStatus().isAllowedToJoin()) {
            mDataSharingService = dataSharingService;
            mSharedGroupObserver =
                    new SharedGroupObserver(tabGroupId, tabGroupSyncService, dataSharingService);
            mSharedGroupObserver
                    .getCollaborationIdSupplier()
                    .addObserver(mOnCollaborationIdChanged);
            mSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .addObserver(mOnGroupSharedStateChanged);
        } else {
            mDataSharingService = null;
            mSharedGroupObserver = null;
        }
    }

    @Override
    public void destroy() {
        if (mSharedGroupObserver != null) {
            mSharedGroupObserver.destroy();
            mSharedGroupObserver
                    .getCollaborationIdSupplier()
                    .removeObserver(mOnCollaborationIdChanged);
            mSharedGroupObserver
                    .getGroupSharedStateSupplier()
                    .removeObserver(mOnGroupSharedStateChanged);
            detachAndDestroySharedImageTiles();
        }
    }

    /** Returns the tab group id that this tab group color view provider is for. */
    public @NonNull Token getTabGroupId() {
        return mTabGroupId;
    }

    /** Returns whether the group is a collaboration. */
    public boolean hasCollaborationId() {
        if (mSharedGroupObserver == null) return false;

        return TabShareUtils.isCollaborationIdValid(
                mSharedGroupObserver.getCollaborationIdSupplier().get());
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
    public @NonNull View getLazyView() {
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
                ColorPickerUtils.getTabGroupColorPickerItemColor(mContext, mColorId, mIsIncognito);
        drawable.setColor(fillColor);

        boolean isColorDot = mSharedImageTilesCoordinator == null;

        Resources res = mContext.getResources();
        float radius =
                isColorDot
                        ? res.getDimension(R.dimen.tab_group_color_icon_item_radius)
                        : res.getDimension(R.dimen.tab_group_color_icon_with_avatar_item_radius);
        float[] radii = new float[8];
        Arrays.fill(radii, radius);
        drawable.setCornerRadii(radii);

        @Px
        int size =
                isColorDot
                        ? res.getDimensionPixelSize(R.dimen.tab_group_color_icon_item_size)
                        : res.getDimensionPixelSize(
                                R.dimen.tab_group_color_icon_with_avatar_item_size);
        if (mFrameLayout.getMinimumWidth() != size) {
            mFrameLayout.setMinimumWidth(size);
            mFrameLayout.setMinimumHeight(size);
        }

        mFrameLayout.invalidate();

        if (mSharedImageTilesCoordinator != null) {
            mSharedImageTilesCoordinator.updateColorStyle(
                    new SharedImageTilesColor(
                            SharedImageTilesColor.Style.TAB_GROUP,
                            ColorPickerUtils.getTabGroupColorPickerItemColor(
                                    mContext, mColorId, mIsIncognito)));
        }
    }

    private void maybeCreateAndAttachSharedImageTiles() {
        if (mDataSharingService == null) return;

        if (mSharedImageTilesCoordinator != null) {
            assert mFrameLayout != null : "SharedImageTiles should only exist if a view exists.";
            return;
        }

        if (mFrameLayout == null) return;

        @Nullable String collaborationId = mSharedGroupObserver.getCollaborationIdSupplier().get();
        if (!TabShareUtils.isCollaborationIdValid(collaborationId)) return;

        @Nullable
        @GroupSharedState
        Integer groupSharedState = mSharedGroupObserver.getGroupSharedStateSupplier().get();
        if (!shouldShowSharedImageTiles(groupSharedState)) return;

        mSharedImageTilesCoordinator =
                new SharedImageTilesCoordinator(
                        mContext,
                        SharedImageTilesType.SMALL,
                        new SharedImageTilesColor(
                                SharedImageTilesColor.Style.TAB_GROUP,
                                ColorPickerUtils.getTabGroupColorPickerItemColor(
                                        mContext, mColorId, mIsIncognito)),
                        mDataSharingService);
        mSharedImageTilesCoordinator.updateCollaborationId(collaborationId);

        View view = mSharedImageTilesCoordinator.getView();
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT);
        params.gravity = Gravity.CENTER;
        @Px
        int marginStart =
                mContext.getResources().getDimensionPixelSize(R.dimen.tab_group_color_icon_stroke)
                        / 2;
        params.setMarginStart(marginStart);
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

    private void onCollaborationIdChanged(@Nullable String collaborationId) {
        if (mSharedImageTilesCoordinator != null) {
            mSharedImageTilesCoordinator.updateCollaborationId(collaborationId);
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
            @Nullable @GroupSharedState Integer groupSharedState) {
        return groupSharedState != null
                && groupSharedState != GroupSharedState.NOT_SHARED
                && groupSharedState != GroupSharedState.COLLABORATION_ONLY;
    }

    @VisibleForTesting
    @TabGroupColorId
    int getTabGroupColorIdForTesting() {
        return mColorId;
    }
}
