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
import org.chromium.components.tab_group_sync.TriggerSource;

/**
 * An {@link ObservableSupplier} which manages the total item count in the archived surface, which
 * includes tab counts in the archived {@link TabModel} and any tab groups from the {@link
 * TabGroupSyncService}.
 */
@NullMarked
public class ArchivedTabCountSupplier extends ObservableSupplierImpl<Integer>
        implements Destroyable {
    private static final int INITIAL_TAB_COUNT = 0;

    private final Callback<Integer> mArchivedTabModelTabCountObserver =
            (tabModelTabCount) -> {
                updateArchivedTabCount();
            };
    private @Nullable TabModel mArchivedTabModel;
    private @Nullable ObservableSupplier<Integer> mArchivedTabModelTabCountSupplier;
    private @Nullable TabGroupSyncService mTabGroupSyncService;

    private final TabGroupSyncService.Observer mTabGroupSyncObserver =
            new TabGroupSyncService.Observer() {
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
     * @return The supplier that manages tab count updates from both the tab model and sync service.
     */
    public ArchivedTabCountSupplier() {
        // Set this supplier once so there is a base value at minimum.
        super.set(INITIAL_TAB_COUNT);
    }

    /**
     * Setup the observers for tab counts when the tab model and sync service are available.
     *
     * @param archivedTabModel The {@link TabModel} representing archived tabs.
     * @param tabGroupSyncService The {@link TabGroupSyncService} governing synced tab groups.
     */
    public void setupInternalObservers(
            TabModel archivedTabModel, @Nullable TabGroupSyncService tabGroupSyncService) {
        mArchivedTabModel = archivedTabModel;
        mArchivedTabModelTabCountSupplier = mArchivedTabModel.getTabCountSupplier();
        mArchivedTabModelTabCountSupplier.addObserver(mArchivedTabModelTabCountObserver);
        mTabGroupSyncService = tabGroupSyncService;

        if (mTabGroupSyncService != null) {
            mTabGroupSyncService.addObserver(mTabGroupSyncObserver);
        }
    }

    private void updateArchivedTabCount() {
        int totalTabCount = getArchivedTabGroupCount();
        if (mArchivedTabModel != null) {
            totalTabCount += mArchivedTabModel.getCount();
        }
        super.set(totalTabCount);
    }

    private int getArchivedTabGroupCount() {
        int archivedTabGroupCount = 0;
        if (mTabGroupSyncService != null) {
            for (String syncId : mTabGroupSyncService.getAllGroupIds()) {
                SavedTabGroup savedTabGroup = mTabGroupSyncService.getGroup(syncId);

                if (savedTabGroup != null && savedTabGroup.archivalTimeMs != null) {
                    archivedTabGroupCount += 1;
                }
            }
        }

        return archivedTabGroupCount;
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
