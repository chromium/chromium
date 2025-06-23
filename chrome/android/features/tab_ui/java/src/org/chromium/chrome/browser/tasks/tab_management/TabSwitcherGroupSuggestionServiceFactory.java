// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;

import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabwindow.WindowId;

import java.util.HashSet;
import java.util.List;

/** Factory class for {@link TabSwitcherGroupSuggestionService}. */
@NullMarked
public class TabSwitcherGroupSuggestionServiceFactory {
    /**
     * Creates a {@link TabSwitcherGroupSuggestionService} instance for the given activity.
     *
     * @param activity The activity to create the service for.
     * @param currentTabGroupModelFilterSupplier Supplies the current tab group model filter.
     * @param profile The profile to use for the service.
     * @param tabListHighlighter Used for highlighting tabs when a suggestion is shown.
     * @param messageService Used for showing a suggestion message.
     */
    public static TabSwitcherGroupSuggestionService build(
            Activity activity,
            ObservableSupplier<TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            Profile profile,
            TabListHighlighter tabListHighlighter,
            TabGroupSuggestionMessageService messageService) {
        assert ChromeFeatureList.sTabSwitcherGroupSuggestionsAndroid.isEnabled();
        @WindowId int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);

        SuggestionLifecycleObserver lifecycleObserver =
                getObserver(tabListHighlighter, messageService);

        return new TabSwitcherGroupSuggestionService(
                windowId, currentTabGroupModelFilterSupplier, profile, lifecycleObserver);
    }

    private static SuggestionLifecycleObserver getObserver(
            TabListHighlighter tabListHighlighter,
            TabGroupSuggestionMessageService messageService) {
        return new SuggestionLifecycleObserver() {
            @Override
            public void onAnySuggestionResponse() {
                tabListHighlighter.unhighlightTabs();
            }

            @Override
            public void onSuggestionIgnored() {
                messageService.dismissMessage(CallbackUtils.emptyRunnable());
            }

            @Override
            public void onShowSuggestion(List<@TabId Integer> tabIds) {
                tabListHighlighter.highlightTabs(new HashSet<>(tabIds));
                messageService.addGroupMessageForTabs(tabIds, this);
            }
        };
    }
}
