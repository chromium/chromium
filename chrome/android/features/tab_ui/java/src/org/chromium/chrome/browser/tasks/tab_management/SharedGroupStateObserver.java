// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.Objects;

/** Provides a simple interface to watch shared state for a single tab group. */
public class SharedGroupStateObserver implements Destroyable {
    private final DataSharingService.Observer mObserver =
            new DataSharingService.Observer() {
                @Override
                public void onGroupChanged(GroupData groupData) {
                    updateForNonDeletedGroupData(groupData);
                }

                @Override
                public void onGroupAdded(GroupData groupData) {
                    updateForNonDeletedGroupData(groupData);
                }

                @Override
                public void onGroupRemoved(String groupId) {
                    updateForDeletedGroupId(groupId);
                }
            };

    private final ObservableSupplierImpl<Integer> mGroupSharedStateSupplier =
            new ObservableSupplierImpl<>();
    private final LocalTabGroupId mLocalTabGroupId;
    private final DataSharingService mDataSharingService;
    private final TabGroupSyncService mTabGroupSyncService;

    // Track a matching collaboration id because it allows us to not assume sync will still give the
    // old collaboration id when the group is deleted.
    private @Nullable String mCurrentCollaborationId;

    /**
     * @param tabGroupId The id of the tab group.
     * @param tabGroupSyncService Used to fetch the current collaboration id of the group.
     * @param dataSharingService Used to fetch and observer current share data.
     */
    public SharedGroupStateObserver(
            @NonNull Token tabGroupId,
            @NonNull TabGroupSyncService tabGroupSyncService,
            @NonNull DataSharingService dataSharingService) {
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mLocalTabGroupId = new LocalTabGroupId(tabGroupId);

        @Nullable SavedTabGroup group = mTabGroupSyncService.getGroup(mLocalTabGroupId);
        if (group == null || !TabShareUtils.isCollaborationIdValid(group.collaborationId)) {
            mGroupSharedStateSupplier.set(GroupSharedState.NOT_SHARED);
        } else {
            mCurrentCollaborationId = group.collaborationId;
            dataSharingService.readGroup(group.collaborationId, this::onReadGroup);
        }

        dataSharingService.addObserver(mObserver);
    }

    @Override
    public void destroy() {
        mDataSharingService.removeObserver(mObserver);
    }

    /**
     * The held value corresponds to {@link GroupSharedState}. Though upon the initial construction
     * of this class it is possible there's no value set yet. This would be because there is a
     * collaboration id and an outstanding request to read the group from the sharing service.
     */
    public ObservableSupplier<Integer> getGroupSharedStateSupplier() {
        return mGroupSharedStateSupplier;
    }

    private void onReadGroup(@NonNull GroupDataOrFailureOutcome outcome) {
        mGroupSharedStateSupplier.set(TabShareUtils.discernSharedGroupState(outcome));
    }

    private void updateForNonDeletedGroupData(@Nullable GroupData groupData) {
        if (isOurGroup(groupData)) {
            mGroupSharedStateSupplier.set(TabShareUtils.discernSharedGroupState(groupData));
        }
    }

    private void updateForDeletedGroupId(@Nullable String groupId) {
        if (Objects.equals(groupId, mCurrentCollaborationId)) {
            mCurrentCollaborationId = null;
            mGroupSharedStateSupplier.set(GroupSharedState.NOT_SHARED);
        }
    }

    private boolean isOurGroup(@Nullable GroupData groupData) {
        if (groupData == null
                || groupData.groupToken == null
                || !TabShareUtils.isCollaborationIdValid(groupData.groupToken.groupId)) {
            return false;
        } else if (TabShareUtils.isCollaborationIdValid(mCurrentCollaborationId)) {
            return Objects.equals(groupData.groupToken.groupId, mCurrentCollaborationId);
        } else {
            @Nullable SavedTabGroup syncGroup = mTabGroupSyncService.getGroup(mLocalTabGroupId);
            boolean matches =
                    syncGroup != null
                            && Objects.equals(
                                    syncGroup.collaborationId, groupData.groupToken.groupId);
            if (matches) {
                mCurrentCollaborationId = syncGroup.collaborationId;
            }
            return matches;
        }
    }
}
