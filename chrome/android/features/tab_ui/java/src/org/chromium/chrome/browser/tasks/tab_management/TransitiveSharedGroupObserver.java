// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.tab_group_sync.TabGroupSyncService;

import java.util.List;
import java.util.Objects;

/**
 * A wrapper for {@link SharedGroupObserver} that supports changing the observed tab group id while
 * continuing to observe a single set of {@link ObservableSupplier} for updates. If only a single
 * tab group is of interest, prefer {@link SharedGroupObserver}.
 *
 * <p>This class abstracts away the record keeping that would otherwise be required to register and
 * unregister observers and create a new {@link SharedGroupObserver} whenever a different tab group
 * id needs to be observed.
 */
@NullMarked
public class TransitiveSharedGroupObserver implements Destroyable {
    private final ObservableSupplierImpl<@Nullable SharedGroupObserver>
            mCurrentSharedGroupObserverSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplier<@Nullable Integer> mGroupSharedStateSupplier;
    private final ObservableSupplier<@Nullable List<GroupMember>> mGroupMembersSupplier;
    private final ObservableSupplier<@Nullable String> mCollaborationIdSupplier;
    private final TabGroupSyncService mTabGroupSyncService;
    private final DataSharingService mDataSharingService;
    private final CollaborationService mCollaborationService;

    private @Nullable Token mCurrentTabGroupId;

    /**
     * @param tabGroupSyncService Used to fetch the current collaboration id of the group.
     * @param dataSharingService Used to observe current share data.
     * @param collaborationService Used to fetch current shared data.
     */
    @SuppressWarnings("NullAway") // https://github.com/uber/NullAway/issues/1128
    public TransitiveSharedGroupObserver(
            TabGroupSyncService tabGroupSyncService,
            DataSharingService dataSharingService,
            CollaborationService collaborationService) {
        mTabGroupSyncService = tabGroupSyncService;
        mDataSharingService = dataSharingService;
        mCollaborationService = collaborationService;

        mGroupSharedStateSupplier =
                mCurrentSharedGroupObserverSupplier.createTransitive(
                        SharedGroupObserver::getGroupSharedStateSupplier);
        mGroupMembersSupplier =
                mCurrentSharedGroupObserverSupplier.createTransitive(
                        SharedGroupObserver::getGroupMembersSupplier);
        mCollaborationIdSupplier =
                mCurrentSharedGroupObserverSupplier.createTransitive(
                        SharedGroupObserver::getCollaborationIdSupplier);
    }

    @Override
    public void destroy() {
        mCurrentTabGroupId = null;
        swapSharedGroupObserver(/* newObserver= */ null);
    }

    /** Sets the tab group id to observe and create a corresponding observer. */
    public void setTabGroupId(@Nullable Token tabGroupId) {
        if (Objects.equals(tabGroupId, mCurrentTabGroupId)) return;

        mCurrentTabGroupId = tabGroupId;

        @Nullable SharedGroupObserver newObserver =
                tabGroupId == null
                        ? null
                        : new SharedGroupObserver(
                                tabGroupId,
                                mTabGroupSyncService,
                                mDataSharingService,
                                mCollaborationService);

        swapSharedGroupObserver(newObserver);
    }

    /** The held value corresponds to {@link GroupSharedState}. */
    public ObservableSupplier<@Nullable Integer> getGroupSharedStateSupplier() {
        return mGroupSharedStateSupplier;
    }

    /** The held value corresponds to the list of {@link GroupMember} for the group. */
    public ObservableSupplier<@Nullable List<GroupMember>> getGroupMembersSupplier() {
        return mGroupMembersSupplier;
    }

    /** The held value corresponds to the collaboration id for the group. */
    public ObservableSupplier<@Nullable String> getCollaborationIdSupplier() {
        return mCollaborationIdSupplier;
    }

    private void swapSharedGroupObserver(@Nullable SharedGroupObserver newObserver) {
        var currentSharedGroupObserver = mCurrentSharedGroupObserverSupplier.get();
        if (currentSharedGroupObserver != null) {
            assumeNonNull(currentSharedGroupObserver).destroy();
        }

        mCurrentSharedGroupObserverSupplier.set(newObserver);
    }
}
