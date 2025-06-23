// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsServiceFactory;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Orchestrates fetching and showing tab group suggestions in the Tab Switcher. */
@NullMarked
public class TabSwitcherGroupSuggestionService {

    /** Observes lifecycle events for tab group suggestions. */
    public interface SuggestionLifecycleObserver {
        /** Called when the user accepts the suggestion. */
        default void onSuggestionAccepted() {}

        /** Called when the user explicitly dismisses the suggestion. */
        default void onSuggestionDismissed() {}

        /** Called when the suggestion is ignored. */
        default void onSuggestionIgnored() {}

        /** Called when any response is received. */
        default void onAnySuggestionResponse() {}

        /**
         * Called when a suggestion is shown.
         *
         * @param tabIds The tab IDs included in the group suggestion.
         */
        default void onShowSuggestion(List<@TabId Integer> tabIds) {}
    }

    /**
     * Handles observer lifecycle events for tab group suggestions.
     *
     * <p>After a listener for a user response to a group suggestion in this handler has been called
     * once, subsequent calls for such events are ignored.
     */
    public static class SuggestionLifecycleObserverHandler implements SuggestionLifecycleObserver {
        private final int mSuggestionId;
        private final Callback<UserResponseMetadata> mUserResponseCallback;
        private final SuggestionLifecycleObserver mSuggestionLifecycleObserver;
        private boolean mResponseObserverAlreadyCalled;

        /**
         * Constructs an observer handler for a specific tab group suggestion.
         *
         * @param suggestionId The ID for the suggestion.
         * @param userResponseCallback To be invoked with the user's response.
         * @param suggestionResponseListener Listens for user responses to the suggestion.
         */
        public SuggestionLifecycleObserverHandler(
                int suggestionId,
                Callback<UserResponseMetadata> userResponseCallback,
                SuggestionLifecycleObserver suggestionResponseListener) {
            mSuggestionId = suggestionId;
            mUserResponseCallback = userResponseCallback;
            mSuggestionLifecycleObserver = suggestionResponseListener;
        }

        @Override
        public void onSuggestionAccepted() {
            onSuggestionResponse(
                    mSuggestionLifecycleObserver::onSuggestionAccepted,
                    new UserResponseMetadata(mSuggestionId, UserResponse.ACCEPTED));
        }

        @Override
        public void onSuggestionDismissed() {
            onSuggestionResponse(
                    mSuggestionLifecycleObserver::onSuggestionDismissed,
                    new UserResponseMetadata(mSuggestionId, UserResponse.REJECTED));
        }

        @Override
        public void onSuggestionIgnored() {
            onSuggestionResponse(
                    mSuggestionLifecycleObserver::onSuggestionIgnored,
                    new UserResponseMetadata(mSuggestionId, UserResponse.IGNORED));
        }

        @Override
        public void onShowSuggestion(List<@TabId Integer> tabIds) {
            mSuggestionLifecycleObserver.onShowSuggestion(tabIds);
        }

        private void onSuggestionResponse(
                Runnable listener, UserResponseMetadata userResponseMetadata) {
            if (mResponseObserverAlreadyCalled) return;
            mResponseObserverAlreadyCalled = true;

            listener.run();
            mUserResponseCallback.onResult(userResponseMetadata);
            mSuggestionLifecycleObserver.onAnySuggestionResponse();
        }
    }

    private final int mWindowId;
    private final SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    private final GroupSuggestionsService mGroupSuggestionsService;
    private @Nullable SuggestionLifecycleObserverHandler mSuggestionLifecycleObserverHandler;

    /**
     * @param windowId The ID of the current window.
     * @param suggestionLifecycleObserver Listens for user responses to a group suggestion.
     */
    public TabSwitcherGroupSuggestionService(
            @WindowId int windowId,
            Profile profile,
            SuggestionLifecycleObserver suggestionLifecycleObserver) {
        mWindowId = windowId;
        mSuggestionLifecycleObserver = suggestionLifecycleObserver;

        mGroupSuggestionsService = GroupSuggestionsServiceFactory.getForProfile(profile);
    }

    /** Shows tab group suggestions if needed. */
    public void maybeShowSuggestions() {
        clearSuggestions();

        CachedSuggestions cachedSuggestions =
                mGroupSuggestionsService.getCachedSuggestions(mWindowId);

        if (cachedSuggestions == null) return;
        GroupSuggestions groupSuggestions = cachedSuggestions.groupSuggestions;

        if (groupSuggestions == null
                || groupSuggestions.groupSuggestions == null
                || groupSuggestions.groupSuggestions.isEmpty()) {
            return;
        }

        Callback<UserResponseMetadata> userResponseCallback =
                cachedSuggestions.userResponseMetadataCallback;

        List<GroupSuggestion> groupSuggestionsList = groupSuggestions.groupSuggestions;

        // Mark all suggestions except the first one as "not shown".
        for (int i = 1; i < groupSuggestionsList.size(); i++) {
            GroupSuggestion groupSuggestion = groupSuggestionsList.get(i);
            userResponseCallback.onResult(
                    new UserResponseMetadata(groupSuggestion.suggestionId, UserResponse.NOT_SHOWN));
        }
        showSuggestion(groupSuggestionsList.get(0), userResponseCallback);
    }

    /** Clears tab group suggestions if present. */
    public void clearSuggestions() {
        if (mSuggestionLifecycleObserverHandler != null) {
            mSuggestionLifecycleObserverHandler.onSuggestionIgnored();
            mSuggestionLifecycleObserverHandler = null;
        }
    }

    /**
     * Shows a single tab group suggestion to the user.
     *
     * @param suggestion The suggestion to show.
     * @param callback The callback to invoke with the user's response.
     */
    private void showSuggestion(
            GroupSuggestion suggestion, Callback<UserResponseMetadata> callback) {
        Set<Integer> suggestionTabIds = new HashSet<>();
        for (int tabId : suggestion.tabIds) {
            suggestionTabIds.add(tabId);
        }

        mSuggestionLifecycleObserverHandler =
                new SuggestionLifecycleObserverHandler(
                        suggestion.suggestionId, callback, mSuggestionLifecycleObserver);
        mSuggestionLifecycleObserverHandler.onShowSuggestion(new ArrayList<>(suggestionTabIds));
    }
}
