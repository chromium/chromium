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
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;

/** Observer for events that are relevant to TabGroup suggestion triggering or calculation. */
@NullMarked
public class SuggestionEventObserver {

    private final @NonNull TabModel mTabModel;
    private final @NonNull GroupSuggestionsService mGroupSuggestionsService;
    private final @NonNull Callback<Boolean> mHubVisibilityObserver = this::onHubVisibilityChanged;
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
                    mGroupSuggestionsService.didAddTab(tab.getId(), type);
                }
            };

    private @Nullable ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private @Nullable ObservableSupplier<Pane> mFocusedPaneSupplier;

    /** Creates the observer. */
    public SuggestionEventObserver(
            @NonNull TabModel tabModel,
            @NonNull OneshotSupplierImpl<HubManager> hubManagerSupplier) {
        mTabModel = tabModel;
        mGroupSuggestionsService =
                GroupSuggestionsServiceFactory.getForProfile(tabModel.getProfile());
        assert !tabModel.isIncognitoBranded();
        tabModel.addObserver(mTabModelObserver);
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

    /** Destroys the observer. */
    public void destroy() {
        mTabModel.removeObserver(mTabModelObserver);
        if (mHubVisibilitySupplier != null) {
            mHubVisibilitySupplier.removeObserver(mHubVisibilityObserver);
        }
    }
}
