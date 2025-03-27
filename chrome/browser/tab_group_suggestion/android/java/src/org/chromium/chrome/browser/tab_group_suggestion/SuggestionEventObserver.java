// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.url.GURL;

/** Observer for events that are relevant to TabGroup suggestion triggering or calculation. */
@NullMarked
public class SuggestionEventObserver {

    private final @NonNull TabModel mTabModel;
    private final @NonNull GroupSuggestionsService mGroupSuggestionsService;
    private final @NonNull Callback<Boolean> mHubVisibilityObserver = this::onHubVisibilityChanged;
    private final @NonNull TabModelSelectorTabObserver mTabObserver;
    private final @NonNull TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, int type, int lastId) {
                    mGroupSuggestionsService.didSelectTab(tab.getId(), type, lastId);
                }

                @Override
                public void didAddTab(
                        Tab tab,
                        @TabLaunchType int type,
                        @TabCreationState int creationState,
                        boolean markedForSelection) {
                    if (markedForSelection
                            && creationState == TabCreationState.LIVE_IN_FOREGROUND) {
                        mGroupSuggestionsService.didAddTab(tab.getId(), type);
                    }
                }

                @Override
                public void willCloseTab(Tab tab, boolean didCloseAlone) {
                    mGroupSuggestionsService.willCloseTab(tab.getId());
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    mGroupSuggestionsService.tabClosureUndone(tab.getId());
                }

                @Override
                public void tabClosureCommitted(Tab tab) {
                    mGroupSuggestionsService.tabClosureCommitted(tab.getId());
                }
            };

    private @Nullable ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private @Nullable ObservableSupplier<Pane> mFocusedPaneSupplier;

    /** Creates the observer. */
    public SuggestionEventObserver(
            @NonNull TabModelSelector tabModelSelector,
            @NonNull OneshotSupplierImpl<HubManager> hubManagerSupplier) {
        mTabModel = tabModelSelector.getModel(false);
        mTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        if (tab.isIncognitoBranded()) {
                            return;
                        }
                        mGroupSuggestionsService.onPageLoadFinished(tab.getId());
                    }
                };
        mGroupSuggestionsService =
                GroupSuggestionsServiceFactory.getForProfile(mTabModel.getProfile());
        mTabModel.addObserver(mTabModelObserver);
        hubManagerSupplier.runSyncOrOnAvailable(
                hubManager -> {
                    mHubVisibilitySupplier = hubManager.getHubVisibilitySupplier();
                    mHubVisibilitySupplier.addObserver(mHubVisibilityObserver);
                    mFocusedPaneSupplier = hubManager.getPaneManager().getFocusedPaneSupplier();
                });
    }

    private void onHubVisibilityChanged(boolean visible) {
        if (!visible) {
            return;
        }
        assert mFocusedPaneSupplier != null;
        Pane currentPane = mFocusedPaneSupplier.get();
        if (currentPane != null && currentPane.getPaneId() == PaneId.TAB_SWITCHER) {
            mGroupSuggestionsService.didEnterTabSwitcher();
        }
    }

    public @NonNull TabModelSelectorTabObserver getTabModelSelectorTabObserverForTesting() {
        return mTabObserver;
    }

    /** Destroys the observer. */
    public void destroy() {
        mTabModel.removeObserver(mTabModelObserver);
        mTabObserver.destroy();
        if (mHubVisibilitySupplier != null) {
            mHubVisibilitySupplier.removeObserver(mHubVisibilityObserver);
        }
    }
}
