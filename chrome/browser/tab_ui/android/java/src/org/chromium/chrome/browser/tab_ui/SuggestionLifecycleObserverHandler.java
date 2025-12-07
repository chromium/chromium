// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.base.Callback;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    private @Nullable Integer mSuggestionId;
    private @Nullable Callback<UserResponseMetadata> mUserResponseCallback;
    private SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    private boolean mCanCallUserResponseListener;

    /** Constructs an observer handler for a specific tab group suggestion. */
    public SuggestionLifecycleObserverHandler() {
        mCanCallUserResponseListener = false;
    }

    /**
     * Initializes the handler.
     *
     * @param observer Listens for user responses to the suggestion.
     */
    @Initializer
    public void initialize(SuggestionLifecycleObserver observer) {
        mSuggestionLifecycleObserver = observer;
    }

    @Override
    public void onSuggestionAccepted() {
        onSuggestionResponse(
                mSuggestionLifecycleObserver::onSuggestionAccepted, UserResponse.ACCEPTED);
    }

    @Override
    public void onSuggestionDismissed() {
        onSuggestionResponse(
                mSuggestionLifecycleObserver::onSuggestionDismissed, UserResponse.REJECTED);
    }

    @Override
    public void onSuggestionIgnored() {
        onSuggestionResponse(
                mSuggestionLifecycleObserver::onSuggestionIgnored, UserResponse.IGNORED);
    }

    @Override
    public void onShowSuggestion(List<@TabId Integer> tabIdsSortedByIndex) {
        mSuggestionLifecycleObserver.onShowSuggestion(tabIdsSortedByIndex);
    }

    /**
     * Updates the details of the suggestion and allows for user responses to be observed.
     *
     * @param suggestionId The ID for the suggestion.
     * @param userResponseCallback To be invoked with the user's response.
     */
    public void updateSuggestionDetails(
            int suggestionId, Callback<UserResponseMetadata> userResponseCallback) {
        assert mSuggestionId == null
                && mUserResponseCallback == null
                && !mCanCallUserResponseListener;
        mSuggestionId = suggestionId;
        mUserResponseCallback = userResponseCallback;
        mCanCallUserResponseListener = true;
    }

    private void reset() {
        mSuggestionId = null;
        mUserResponseCallback = null;
        mCanCallUserResponseListener = false;
    }

    private void onSuggestionResponse(Runnable listener, @UserResponse int userResponse) {
        if (!mCanCallUserResponseListener) return;
        assert mSuggestionId != null && mUserResponseCallback != null;

        mCanCallUserResponseListener = false;

        listener.run();
        mUserResponseCallback.onResult(new UserResponseMetadata(mSuggestionId, userResponse));
        mSuggestionLifecycleObserver.onAnySuggestionResponse();
        reset();
    }
}
