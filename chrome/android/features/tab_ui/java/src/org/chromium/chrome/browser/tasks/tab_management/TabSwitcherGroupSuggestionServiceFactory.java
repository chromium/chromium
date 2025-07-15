// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;

import org.chromium.base.CallbackUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.SuggestionLifecycleObserverHandler;
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
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            Profile profile,
            TabListHighlighter tabListHighlighter,
            TabGroupSuggestionMessageService messageService) {
        assert ChromeFeatureList.sTabSwitcherGroupSuggestionsAndroid.isEnabled();
        assert !profile.isOffTheRecord();

        @WindowId int windowId = TabWindowManagerSingleton.getInstance().getIdForWindow(activity);

        SuggestionLifecycleObserverHandler handler =
                initObserver(tabListHighlighter, messageService);

        return new TabSwitcherGroupSuggestionService(
                windowId, currentTabGroupModelFilterSupplier, profile, handler);
    }

    private static SuggestionLifecycleObserverHandler initObserver(
            TabListHighlighter tabListHighlighter,
            TabGroupSuggestionMessageService messageService) {
        SuggestionLifecycleObserverHandler handler = new SuggestionLifecycleObserverHandler();
        SuggestionLifecycleObserver observer =
                new SuggestionLifecycleObserver() {
                    @Override
                    public void onAnySuggestionResponse() {
                        tabListHighlighter.unhighlightTabs();
                    }

                    @Override
                    public void onSuggestionAccepted() {
                        RecordUserAction.record(
                                TabSwitcherGroupSuggestionService.USER_ACTION_PREFIX + ".Accepted");
                    }

                    @Override
                    public void onSuggestionDismissed() {
                        RecordUserAction.record(
                                TabSwitcherGroupSuggestionService.USER_ACTION_PREFIX
                                        + ".Dismissed");
                    }

                    @Override
                    public void onSuggestionIgnored() {
                        messageService.dismissMessage(CallbackUtils.emptyRunnable());
                        RecordUserAction.record(
                                TabSwitcherGroupSuggestionService.USER_ACTION_PREFIX + ".Ignored");
                    }

                    @Override
                    public void onShowSuggestion(List<@TabId Integer> tabIdsSortedByIndex) {
                        tabListHighlighter.highlightTabs(new HashSet<>(tabIdsSortedByIndex));
                        messageService.addGroupMessageForTabs(tabIdsSortedByIndex, handler);
                        RecordUserAction.record(
                                TabSwitcherGroupSuggestionService.USER_ACTION_PREFIX + ".Shown");
                    }
                };
        handler.initialize(observer);
        return handler;
    }
}
