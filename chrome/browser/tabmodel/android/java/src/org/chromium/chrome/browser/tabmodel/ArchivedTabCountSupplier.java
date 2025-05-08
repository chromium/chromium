// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupSyncService.Observer;
import org.chromium.components.tab_group_sync.TriggerSource;

/**
 * An {@link ObservableSupplier} which manages the total tab count in the archived surface, which
 * includes tab counts in the archived {@link TabModel} and any tab groups from the {@link
 * TabGroupSyncService}.
 */
@NullMarked
public class ArchivedTabCountSupplier extends ObservableSupplierImpl<Integer>
        implements Destroyable {
    private final TabModel mArchivedTabModel;
    private final ObservableSupplier<Integer> mArchivedTabModelTabCountSupplier;
    private final Callback<Integer> mArchivedTabModelTabCountObserver =
            (tabModelTabCount) -> {
                updateArchivedTabCount();
            };
    private final @Nullable TabGroupSyncService mTabGroupSyncService;

    private final Observer mTabGroupSyncObserver =
            new Observer() {
                @Override
                public void onInitialized() {
                    updateArchivedTabCount();
                }

                @Override
                public void onTabGroupAdded(SavedTabGroup group, @TriggerSource int source) {
                    updateArchivedTabCount();
                }

                @Override
                public void onTabGroupUpdated(SavedTabGroup group, @TriggerSource int source) {
                    updateArchivedTabCount();
                }

                @Override
                public void onTabGroupRemoved(LocalTabGroupId localId, @TriggerSource int source) {
                    updateArchivedTabCount();
                }

                @Override
                public void onTabGroupRemoved(String syncId, @TriggerSource int source) {
                    updateArchivedTabCount();
                }
            };

    /**
     * Creates an instance of {@link ArchivedTabCountSupplier}.
     *
     * @param archivedTabModel The {@link TabModel} representing archived tabs.
     * @param tabGroupSyncService The {@link TabGroupSyncService} governing synced tab groups.
     * @return The supplier that manages tab count updates from both the tab model and sync service.
     */
    public ArchivedTabCountSupplier(
            TabModel archivedTabModel, @Nullable TabGroupSyncService tabGroupSyncService) {
        mArchivedTabModel = archivedTabModel;
        mArchivedTabModelTabCountSupplier = mArchivedTabModel.getTabCountSupplier();
        mArchivedTabModelTabCountSupplier.addObserver(mArchivedTabModelTabCountObserver);
        mTabGroupSyncService = tabGroupSyncService;

        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }

        // Set this supplier once so there is a base value at minimum.
        super.set(mArchivedTabModelTabCountSupplier.get());
    }

    private void updateArchivedTabCount() {
        int totalTabCount = getArchivedTabGroupTabCount();
        totalTabCount += mArchivedTabModel.getCount();
        super.set(totalTabCount);
    }

    private int getArchivedTabGroupTabCount() {
        int archivedTabGroupTabCount = 0;
        if (mTabGroupSyncService != null) {
            for (String syncId : mTabGroupSyncService.getAllGroupIds()) {
                SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncId);

                if (savedTabGroup != null && savedTabGroup.archivalTimeMs != null) {
                    archivedTabGroupTabCount += savedTabGroup.savedTabs.size();
                }
            }
        }

        return archivedTabGroupTabCount;
    }

    @Override
    public void set(Integer tabCount) {
        assert false : "ArchivedTabCountSupplier should only be set through its observers.";
    }

    @Override
    public void destroy() {
        if (mArchivedTabModelTabCountSupplier != null) {
            mArchivedTabModelTabCountSupplier.removeObserver(mArchivedTabModelTabCountObserver);
        }

        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.removeObserver(mTabGroupSyncObserver);
        }
    }
}
