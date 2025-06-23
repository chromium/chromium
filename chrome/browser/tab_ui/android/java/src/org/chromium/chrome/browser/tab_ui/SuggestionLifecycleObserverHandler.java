// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.List;

/**
 * Handles observer lifecycle events for tab group suggestions.
 *
 * <p>After a listener for a user response to a group suggestion in this handler has been called
 * once, subsequent calls for such events are ignored.
 */
@NullMarked
public class SuggestionLifecycleObserverHandler implements SuggestionLifecycleObserver {
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
