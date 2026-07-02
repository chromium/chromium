// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Pushes Send Tab To Self label updates to UI for tabs. */
@NullMarked
public class SendTabToSelfTabLabeller implements TabModelObserver {
    private final TabListNotificationHandler mNotificationHandler;
    private final NullableObservableSupplier<TabModel> mTabModelSupplier;
    private final Callback<@Nullable TabModel> mOnTabModelChange = this::onTabModelChange;
    private final Set<Integer> mLabelledTabIds = new HashSet<>();
    private @Nullable TabModel mCurrentTabModel;

    /**
     * Constructs a new {@link SendTabToSelfTabLabeller}.
     *
     * @param notificationHandler Handler responsible for updating tab card labels in the UI.
     * @param tabModelSupplier Supplier for the currently active {@link TabModel}.
     */
    public SendTabToSelfTabLabeller(
            TabListNotificationHandler notificationHandler,
            NullableObservableSupplier<TabModel> tabModelSupplier) {
        mNotificationHandler = notificationHandler;
        mTabModelSupplier = tabModelSupplier;
        mTabModelSupplier.addSyncObserverAndCallIfNonNull(mOnTabModelChange);
    }

    /** Cleans up observers and unregisters from the active {@link TabModel}. */
    public void destroy() {
        mTabModelSupplier.removeObserver(mOnTabModelChange);
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeObserver(this);
            mCurrentTabModel = null;
        }
    }

    /**
     * Updates the UI with Send Tab To Self labels for the specified tabs.
     *
     * @param tabs The list of tabs to update. If null, updates all tabs in the current {@link
     *     TabModel}.
     */
    public void showAll(@Nullable List<Tab> tabs) {
        PostTask.postTask(TaskTraits.UI_DEFAULT, () -> showAllInternal(tabs));
    }

    private void showAllInternal(@Nullable List<Tab> tabs) {
        if (tabs == null) {
            tabs = getTabsFromTabModel();
        }
        Map<Integer, TabCardLabelData> cardLabels = new HashMap<>();
        for (Tab tab : tabs) {
            if (tab == null || tab.isDestroyed() || tab.getUserDataHost() == null) continue;
            // First check the in-memory UserDataHost synchronously. This avoids posting tasks to
            // the UI thread when the data is already loaded.
            SendTabToSelfTabCardLabelData data = SendTabToSelfTabCardLabelData.get(tab);
            int tabId = tab.getId();
            if (data != null) {
                TabCardLabelData label = buildLabel(data);
                if (label != null) {
                    mLabelledTabIds.add(tabId);
                    cardLabels.put(tabId, label);
                } else if (mLabelledTabIds.remove(tabId)) {
                    cardLabels.put(tabId, null);
                }
            } else {
                // Asynchronously restore from LevelDB. If no data is found, push null to clear.
                SendTabToSelfTabCardLabelData.from(
                        tab,
                        loadedData -> {
                            TabCardLabelData label = buildLabel(loadedData);
                            if (label != null) {
                                mLabelledTabIds.add(tabId);
                                mNotificationHandler.updateTabCardLabels(
                                        Collections.singletonMap(tabId, label));
                            } else if (mLabelledTabIds.remove(tabId)) {
                                mNotificationHandler.updateTabCardLabels(
                                        Collections.singletonMap(tabId, null));
                            }
                        });
            }
        }
        if (!cardLabels.isEmpty()) {
            mNotificationHandler.updateTabCardLabels(cardLabels);
        }
    }

    /**
     * Handles changes to the active {@link TabModel} by unregistering from the old model and
     * registering to the new one.
     *
     * @param newTabModel The newly selected {@link TabModel}.
     */
    private void onTabModelChange(@Nullable TabModel newTabModel) {
        if (mCurrentTabModel == newTabModel) return;
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeObserver(this);
        }
        mCurrentTabModel = newTabModel;
        if (mCurrentTabModel != null) {
            mCurrentTabModel.addObserver(this);
        }
    }

    /**
     * Retrieves a list of all valid tabs from the currently active {@link TabModel}.
     *
     * @return A list of {@link Tab} instances.
     */
    private List<Tab> getTabsFromTabModel() {
        List<Tab> tabs = new ArrayList<>();
        if (mCurrentTabModel == null) {
            return tabs;
        }
        for (int i = 0; i < mCurrentTabModel.getCount(); i++) {
            Tab tab = mCurrentTabModel.getTabAt(i);
            if (tab != null) {
                tabs.add(tab);
            }
        }
        return tabs;
    }

    /**
     * Builds the {@link TabCardLabelData} for a given tab if it has active Send Tab To Self data.
     *
     * @param data The Send Tab To Self data to create the label for.
     * @return The {@link TabCardLabelData} containing the label details, or null if no label
     *     applies.
     */
    private @Nullable TabCardLabelData buildLabel(@Nullable SendTabToSelfTabCardLabelData data) {
        if (data == null || data.isNegativeCache() || !data.shouldShowLabel()) {
            return null;
        }
        return new TabCardLabelData(
                TabCardLabelType.ACTIVITY_UPDATE,
                data::getLabelText,
                /* asyncImageFactory= */ null,
                /* contentDescriptionResolver= */ null);
    }
}
