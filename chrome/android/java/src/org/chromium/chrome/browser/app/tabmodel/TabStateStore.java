// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.SystemClock;

import org.chromium.base.Log;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageService.LoadedTabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabRegistrationObserver;

import java.util.HashSet;
import java.util.Set;

/** Orchestrates saving of tabs to the {@link TabStateStorageService}. */
@NullMarked
public class TabStateStore {
    private static final String TAG = "TabStateStore";

    private final TabStateStorageService mTabStateStorageService;
    private final TabStateAttributes.Observer mAttributesObserver =
            this::onTabStateDirtinessChanged;
    private final TabModelSelectorTabRegistrationObserver mTabRegistrationObserver;
    private final TabMoveObserver mTabMoveObserver;

    private class InnerRegistrationObserver
            implements TabModelSelectorTabRegistrationObserver.Observer {
        @Override
        public void onTabRegistered(Tab tab) {
            TabStateStore.this.onTabRegistered(tab);
        }

        @Override
        public void onTabUnregistered(Tab tab) {
            TabStateStore.this.onTabUnregistered(tab);
        }
    }

    private class TabMoveObserver implements TabModelObserver {
        private final TabModel mTabModel;

        private TabMoveObserver(TabModel tabModel) {
            mTabModel = tabModel;
            mTabModel.addObserver(this);
        }

        private void destroy() {
            mTabModel.removeObserver(this);
        }

        @Override
        public void didMoveTab(Tab tab, int newIndex, int curIndex) {
            onMoveTab(mTabModel, newIndex, curIndex);
        }
    }

    /**
     * @param tabStateStorageService The {@link TabStateStorageService} to save to.
     * @param tabModelSelector The {@link TabModelSelector} to observe changes in.
     */
    public TabStateStore(
            TabStateStorageService tabStateStorageService, TabModelSelector tabModelSelector) {
        mTabStateStorageService = tabStateStorageService;
        mTabRegistrationObserver = new TabModelSelectorTabRegistrationObserver(tabModelSelector);
        mTabRegistrationObserver.addObserverAndNotifyExistingTabRegistration(
                new InnerRegistrationObserver());

        mTabMoveObserver = new TabMoveObserver(tabModelSelector.getModel(/* incognito= */ false));
        // TODO(https://crbug.com/427254267): Watch for incognito as well eventually. But before
        // things are fully functional, do not write any incognito data to avoid regressing on
        // privacy.

        loadAllTabsFromService();
    }

    /** Cleans up observation. */
    public void destroy() {
        mTabRegistrationObserver.destroy();
        mTabMoveObserver.destroy();
    }

    private void onTabStateDirtinessChanged(Tab tab, @DirtinessState int dirtiness) {
        if (dirtiness == DirtinessState.DIRTY && !tab.isDestroyed()) {
            saveTab(tab);
        }
    }

    private void saveTab(Tab tab) {
        mTabStateStorageService.saveTabData(tab);
    }

    private void onTabRegistered(Tab tab) {
        TabStateAttributes attributes = TabStateAttributes.from(tab);
        assumeNonNull(attributes);
        if (attributes.addObserver(mAttributesObserver) == DirtinessState.DIRTY) {
            saveTab(tab);
        }
    }

    private void onTabUnregistered(Tab tab) {
        assumeNonNull(TabStateAttributes.from(tab)).removeObserver(mAttributesObserver);
        // TODO(https://crbug.com/430996004): Delete the tab record.
    }

    private void onMoveTab(TabModel tabModel, int newIndex, int curIndex) {
        // TODO(https://crbug.com/427254267): Add some sort of debouncing to avoid duplicate
        // and/or redundant saves when an operation with multiple events/moves.
        // TODO(https://crbug.com/427254267): A collections implementation will need pinned
        // and unpinned collections, but this is at the wrong scope to know about that.
        int start = Math.max(0, Math.min(newIndex, curIndex));
        int end = Math.min(tabModel.getCount() - 1, Math.max(newIndex, curIndex));
        Set<Token> tabGroupsToSave = new HashSet<>();
        for (int i = start; i <= end; i++) {
            Tab child = tabModel.getTabAt(i);
            Token groupId = child == null ? null : child.getTabGroupId();
            if (groupId != null) {
                tabGroupsToSave.add(groupId);
            }
        }
        for (Token groupId : tabGroupsToSave) {
            // TODO(https://crbug.com/427254267): Save the tab group's children index list.

            // Useless call to avoid compiler complaining until actually used.
            groupId.toBundle();
        }
        // TODO(https://crbug.com/427254267): Save the tab model's children index list.
    }

    private void loadAllTabsFromService() {
        long loadStartTime = SystemClock.elapsedRealtime();
        mTabStateStorageService.loadAllTabs(
                (loadedTabStates) -> onTabsLoaded(loadedTabStates, loadStartTime));
    }

    private void onTabsLoaded(LoadedTabState[] loadedTabStates, long loadStartTime) {
        long duration = SystemClock.elapsedRealtime() - loadStartTime;
        Log.i(TAG, "Loaded %d tabs in %dms", loadedTabStates.length, duration);

        for (int i = 0; i < loadedTabStates.length; i++) {
            TabState tabState = loadedTabStates[i].tabState;
            if (tabState.contentsState == null || tabState.contentsState.buffer().limit() <= 0) {
                Log.i(TAG, " Tab %d: no state", i);
                continue;
            }

            WebContentsState contentsState = tabState.contentsState;
            Log.i(
                    TAG,
                    " Tab %d: url: %s, title: %s, state size: %d",
                    i,
                    contentsState.getVirtualUrlFromState(),
                    contentsState.getDisplayTitleFromState(),
                    contentsState.buffer().limit());

            // TODO(https://crbug.com/448150631): Run onTabCreationCallback once tabs are created.
        }
    }
}
