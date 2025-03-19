// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.profiles.Profile;
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
    private final @NonNull ObservableSupplier<Boolean> mHubVisibilitySupplier;
    private final @NonNull ObservableSupplier<Pane> mFocusedPaneSupplier;
    private final @NonNull Callback<Boolean> mHubVisiblityObserver = this::onHubVisiblilityChanged;
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

    /** Creates the observer. */
    public SuggestionEventObserver(
            @NonNull Profile profile,
            @NonNull TabModel tabModel,
            @NonNull ObservableSupplier<Boolean> hubVisibilitySupplier,
            @NonNull ObservableSupplier<Pane> focusedPaneSupplier) {
        mTabModel = tabModel;
        mHubVisibilitySupplier = hubVisibilitySupplier;
        mFocusedPaneSupplier = focusedPaneSupplier;
        mGroupSuggestionsService = GroupSuggestionsServiceFactory.getForProfile(profile);
        assert !tabModel.isIncognitoBranded();
        tabModel.addObserver(mTabModelObserver);
        hubVisibilitySupplier.addObserver(mHubVisiblityObserver);
    }

    private void onHubVisiblilityChanged(boolean visible) {
        if (!visible) {
            return;
        }
        Pane currentPane = mFocusedPaneSupplier.get();
        if (currentPane != null && currentPane.getPaneId() == PaneId.TAB_SWITCHER) {
            mGroupSuggestionsService.didEnterTabSwitcher();
        }
    }

    /** Destroys the observer. */
    public void destroy() {
        mTabModel.removeObserver(mTabModelObserver);
        mHubVisibilitySupplier.removeObserver(mHubVisiblityObserver);
    }
}
