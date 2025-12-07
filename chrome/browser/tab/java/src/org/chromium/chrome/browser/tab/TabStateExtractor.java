// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;
import java.util.Map;

/** Extracts a {@link TabState} from a {@link Tab}. */
@NullMarked
public class TabStateExtractor {
    private static @Nullable Map<Integer, TabState> sTabStatesForTesting;

    /**
     * Returns an opaque "state" object that can be persisted to storage.
     *
     * @param tab The {@link Tab} from which to extract the state.
     * @return The state object, or null if the tab is not initialized.
     */
    public static @Nullable TabState from(Tab tab) {
        if (sTabStatesForTesting != null && sTabStatesForTesting.containsKey(tab.getId())) {
            return sTabStatesForTesting.get(tab.getId());
        }

        if (!tab.isInitialized()) return null;
        TabState tabState = new TabState();
        tabState.contentsState = getWebContentsState(tab);
        tabState.openerAppId = TabAssociatedApp.getAppId(tab);
        tabState.parentId = tab.getParentId();
        tabState.timestampMillis = tab.getTimestampMillis();
        tabState.tabLaunchTypeAtCreation = tab.getTabLaunchTypeAtCreation();
        // Don't save the actual default theme color because it could change on night mode state
        // changed.
        tabState.themeColor =
                tab.isThemingAllowed() && !tab.isNativePage()
                        ? tab.getThemeColor()
                        : TabState.UNSPECIFIED_THEME_COLOR;
        tabState.rootId = tab.getRootId();
        tabState.userAgent = tab.getUserAgent();
        tabState.lastNavigationCommittedTimestampMillis =
                tab.getLastNavigationCommittedTimestampMillis();
        tabState.tabGroupId = tab.getTabGroupId();
        tabState.tabHasSensitiveContent = tab.getTabHasSensitiveContent();
        tabState.isPinned = tab.getIsPinned();
        return tabState;
    }

    /**
     * Returns an object representing the state of the Tab's WebContents.
     *
     * @param tab The {@link Tab} from which to extract the WebContents state.
     * @return The web contents state object, or null if the native call returns null (e.g.
     *     out-of-memory).
     */
    public static @Nullable WebContentsState getWebContentsState(Tab tab) {
        LoadUrlParams pendingLoadParams = tab.getPendingLoadParams();

        // Case 1: The tab is still frozen we can just use the existing state.
        if (tab.getWebContentsState() != null) {
            assert pendingLoadParams == null;
            return tab.getWebContentsState();
        }

        // The tab is not frozen we need to create a new state. This may be null if buffer
        // allocation fails.
        WebContents webContents = tab.getWebContents();
        WebContentsState webContentsState = null;
        if (webContents != null) {
            webContentsState = WebContentsState.getWebContentsStateFromWebContents(webContents);
        }

        // Case 2: We extracted a state from the WebContents and there is no pending load we can
        // just use it.
        if (pendingLoadParams == null) {
            return webContentsState;
        }

        // Case 3: There was a pending load, and we have a state from the WebContents, we can append
        // the pending load to the state and use it.
        if (webContentsState != null) {
            webContentsState.appendPendingNavigation(
                    tab.getProfile(),
                    tab.getTitle(),
                    pendingLoadParams,
                    /* trackLastEntryWasPending= */ false);
            return webContentsState;
        }

        // Case 4: We have a pending load but no state from the WebContents, we can create a new
        // state with just the pending load.
        return WebContentsState.createSingleNavigationWebContentsState(
                tab.getProfile(), tab.getTitle(), pendingLoadParams);
    }

    public static void setTabStateForTesting(int tabId, TabState tabState) {
        if (sTabStatesForTesting == null) {
            sTabStatesForTesting = new HashMap<>();
        }
        sTabStatesForTesting.put(tabId, tabState);
    }

    public static void resetTabStatesForTesting() {
        sTabStatesForTesting = null;
    }
}
