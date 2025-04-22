// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import org.chromium.components.visited_url_ranking.url_grouping.TabSelectionType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;

/** Observer for events that are relevant to TabGroup suggestion triggering or calculation. */
@NullMarked
public class SuggestionEventObserver {

    private final TabModel mTabModel;
    private final GroupSuggestionsService mGroupSuggestionsService;
    private final Callback<Boolean> mHubVisibilityObserver = this::onHubVisibilityChanged;
    private final TabModelSelectorTabObserver mTabObserver;
    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void didSelectTab(Tab tab, int type, int lastId) {
                    @TabSelectionType
                    int selectionType =
                            switch (type) {
                                case org.chromium.chrome.browser.tab.TabSelectionType
                                        .FROM_CLOSE -> TabSelectionType.FROM_CLOSE_ACTIVE_TAB;
                                case org.chromium.chrome.browser.tab.TabSelectionType
                                        .FROM_EXIT -> TabSelectionType.FROM_APP_EXIT;
                                case org.chromium.chrome.browser.tab.TabSelectionType
                                        .FROM_NEW -> TabSelectionType.FROM_NEW_TAB;
                                case org.chromium.chrome.browser.tab.TabSelectionType
                                        .FROM_USER -> TabSelectionType.FROM_USER;
                                case org.chromium.chrome.browser.tab.TabSelectionType
                                        .FROM_OMNIBOX -> TabSelectionType.FROM_OMNIBOX;
                                case org.chromium.chrome.browser.tab.TabSelectionType
                                        .FROM_UNDO -> TabSelectionType.FROM_UNDO_CLOSURE;
                                default -> throw new IllegalArgumentException(
                                        "Unknown selection typ: " + type);
                            };
                    mGroupSuggestionsService.didSelectTab(
                            tab.getId(), tab.getUrl(), selectionType, lastId);
                }

                @Override
                public void didAddTab(
                        Tab tab,
                        @TabLaunchType int type,
                        @TabCreationState int creationState,
                        boolean markedForSelection) {
                    if (creationState != TabCreationState.FROZEN_ON_RESTORE) {
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
            TabModelSelector tabModelSelector, OneshotSupplierImpl<HubManager> hubManagerSupplier) {
        mTabModel = tabModelSelector.getModel(false);
        mTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {
                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigationHandle) {
                        NavigationController controller =
                                navigationHandle.getWebContents().getNavigationController();
                        if (tab.isIncognitoBranded() || controller == null) {
                            return;
                        }
                        NavigationHistory history = controller.getNavigationHistory();
                        if (history == null) {
                            return;
                        }
                        int transitionType =
                                history.getEntryAtIndex(history.getCurrentEntryIndex())
                                        .getTransition();
                        mGroupSuggestionsService.onDidFinishNavigation(tab.getId(), transitionType);
                    }
                };
        mGroupSuggestionsService =
                GroupSuggestionsServiceFactory.getForProfile(assumeNonNull(mTabModel.getProfile()));
        mTabModel
                .getCurrentTabSupplier()
                .addSyncObserverAndCallIfNonNull(
                        new Callback<@Nullable Tab>() {
                            @Override
                            public void onResult(@Nullable Tab tab) {
                                assumeNonNull(tab);
                                mGroupSuggestionsService.didSelectTab(
                                        tab.getId(),
                                        tab.getUrl(),
                                        TabSelectionType.FROM_NEW_TAB,
                                        Tab.INVALID_TAB_ID);

                                // TODO(crbug.com/389129271): Get rid of redundant cast after
                                // https://github.com/uber/NullAway/issues/1155 is fixed.
                                mTabModel
                                        .getCurrentTabSupplier()
                                        .removeObserver((Callback<@Nullable Tab>) this);
                            }
                        });
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

    public TabModelSelectorTabObserver getTabModelSelectorTabObserverForTesting() {
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
