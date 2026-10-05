// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import android.text.TextUtils;
import android.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * Observes navigations on every tab in the given tab model. Filters to navigations for tabs in tab
 * groups and notifies sync of them.
 */
@NullMarked
public class NavigationObserver extends TabModelSelectorTabObserver {
    private static final String TAG = "TG.NavObserver";
    private final TabGroupSyncService mTabGroupSyncService;
    private final NavigationTracker mNavigationTracker;
    private boolean mEnableObservers;

    /**
     * Constructor.
     *
     * @param tabModelSelector The {@link TabModelSelector} whose tabs are to be observed.
     * @param tabGroupSyncService The sync backend to be notified of navigations.
     * @param navigationTracker Tracker for identifying sync initiated navigations.
     */
    public NavigationObserver(
            TabModelSelector tabModelSelector,
            TabGroupSyncService tabGroupSyncService,
            NavigationTracker navigationTracker) {
        super(tabModelSelector);
        mTabGroupSyncService = tabGroupSyncService;
        mNavigationTracker = navigationTracker;
    }

    /**
     * Called to enable or disable this observer. When disabled, the navigations will not be
     * propagated to sync. Typically invoked when chrome is in the middle of applying remote updates
     * to the local tab model.
     *
     * @param enableObservers Whether to enable the observer.
     */
    public void enableObservers(boolean enableObservers) {
        mEnableObservers = enableObservers;
    }

    @Override
    public void onDidFinishNavigationInPrimaryMainFrame(
            Tab tab, NavigationHandle navigationHandle) {
        LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
        if (tab.isIncognito() || localTabGroupId == null) {
            return;
        }

        TabGroupSyncUtils.onDidFinishNavigation(tab, navigationHandle);

        if (!isValidTabForSync(tab)) return;

        SavedTabGroup group = mTabGroupSyncService.getGroup(localTabGroupId);
        boolean isExtensionNavigationAllowed = (group == null) || (group.collaborationId == null);
        if (!TabGroupSyncUtils.isSaveableNavigation(
                isExtensionNavigationAllowed, navigationHandle)) {
            return;
        }

        TabGroupSyncUtils.updateTabRedirectChain(tab, navigationHandle);

        // Avoid loops if the navigation was initiated from sync.
        if (mNavigationTracker.wasNavigationFromSync(navigationHandle.getUserDataHost())) {
            return;
        }

        // Propagate the update to sync.
        LogUtils.log(
                TAG,
                "Navigation wasn't from sync, notify sync, url = "
                        + tab.getUrl().getValidSpecOrEmpty());
        Pair<GURL, String> urlAndTitle =
                TabGroupSyncUtils.getFilteredUrlAndTitle(tab.getUrl(), tab.getTitle());
        updateSync(localTabGroupId, tab.getId(), urlAndTitle.second, urlAndTitle.first);
    }

    @Override
    public void onTitleUpdated(Tab tab) {
        if (!isValidTabForSync(tab)) {
            return;
        }

        LocalTabGroupId localTabGroupId = TabGroupSyncUtils.getLocalTabGroupId(tab);
        if (localTabGroupId == null) {
            return;
        }

        if (!TabGroupSyncUtils.isSavableUrl(tab.getUrl())) {
            return;
        }

        SavedTabGroup group = mTabGroupSyncService.getGroup(localTabGroupId);
        if (group == null || group.collaborationId != null) {
            return;
        }

        SavedTabGroupTab matchingSavedTab = null;
        for (SavedTabGroupTab savedTab : group.savedTabs) {
            if (savedTab.localId != null && savedTab.localId == tab.getId()) {
                matchingSavedTab = savedTab;
                break;
            }
        }
        if (matchingSavedTab == null) {
            return;
        }

        Pair<GURL, String> filteredUrlAndTitle =
                TabGroupSyncUtils.getFilteredUrlAndTitle(tab.getUrl(), tab.getTitle());

        // Only update if the URL matches what sync currently has, but the title has changed.
        // If the URL differs, the navigation was either ignored (e.g. un-saveable or from sync)
        // or not yet saved; onTitleUpdated should never inadvertently push an un-saveable URL.
        if (!Objects.equals(matchingSavedTab.url, filteredUrlAndTitle.first)) {
            return;
        }

        if (TextUtils.equals(matchingSavedTab.title, filteredUrlAndTitle.second)) {
            return;
        }

        LogUtils.log(TAG, "onTitleUpdated, notify sync, title = " + tab.getTitle());
        updateSync(
                localTabGroupId,
                tab.getId(),
                filteredUrlAndTitle.second,
                filteredUrlAndTitle.first);
    }

    private boolean isValidTabForSync(Tab tab) {
        return mEnableObservers && !tab.isIncognito() && !tab.isFrozen();
    }

    private void updateSync(LocalTabGroupId localTabGroupId, int tabId, String title, GURL url) {
        // We set the position argument as -1 so that it can be ignored in native.
        mTabGroupSyncService.updateTab(localTabGroupId, tabId, title, url, /* position= */ -1);
    }
}
