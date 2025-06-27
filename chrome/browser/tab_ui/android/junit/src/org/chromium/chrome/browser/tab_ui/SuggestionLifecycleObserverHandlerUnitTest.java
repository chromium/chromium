// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link SuggestionLifecycleObserverHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionLifecycleObserverHandlerUnitTest {
    private static final int SUGGESTION_ID = 123;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    @Mock private Callback<UserResponseMetadata> mUserResponseCallback;
    private SuggestionLifecycleObserverHandler mSuggestionLifecycleObserverHandler;

    @Before
    public void setUp() {
        mSuggestionLifecycleObserverHandler = new SuggestionLifecycleObserverHandler();
        mSuggestionLifecycleObserverHandler.initialize(mSuggestionLifecycleObserver);
    }

    @Test
    public void testOnSuggestionAccepted() {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionAccepted();

        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback)
                .onResult(
                        argThat(
                                metadata ->
                                        metadata.mSuggestionId == SUGGESTION_ID
                                                && metadata.mUserResponse
                                                        == UserResponse.ACCEPTED));
    }

    @Test
    public void testOnSuggestionDismissed() {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionDismissed();

        verify(mSuggestionLifecycleObserver).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback)
                .onResult(
                        argThat(
                                metadata ->
                                        metadata.mSuggestionId == SUGGESTION_ID
                                                && metadata.mUserResponse
                                                        == UserResponse.REJECTED));
    }

    @Test
    public void testOnSuggestionIgnored() {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionIgnored();

        verify(mSuggestionLifecycleObserver).onSuggestionIgnored();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback)
                .onResult(
                        argThat(
                                metadata ->
                                        metadata.mSuggestionId == SUGGESTION_ID
                                                && metadata.mUserResponse == UserResponse.IGNORED));
    }

    @Test
    public void testOnShowSuggestion() {
        List<Integer> tabIds = Arrays.asList(1, 2, 3);
        mSuggestionLifecycleObserverHandler.onShowSuggestion(tabIds);

        verify(mSuggestionLifecycleObserver).onShowSuggestion(eq(tabIds));
        verify(mSuggestionLifecycleObserver, never()).onAnySuggestionResponse();
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testOnlyFirstUserResponseListenerIsProcessed() {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionAccepted();

        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback).onResult(any());

        mSuggestionLifecycleObserverHandler.onSuggestionDismissed();
        mSuggestionLifecycleObserverHandler.onSuggestionIgnored();

        verify(mSuggestionLifecycleObserver, never()).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver, never()).onSuggestionIgnored();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback).onResult(any());
    }

    @Test
    public void testNoResponseBeforeUpdateDetails() {
        mSuggestionLifecycleObserverHandler.onSuggestionAccepted();
        mSuggestionLifecycleObserverHandler.onSuggestionDismissed();
        mSuggestionLifecycleObserverHandler.onSuggestionIgnored();

        verify(mSuggestionLifecycleObserver, never()).onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver, never()).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver, never()).onSuggestionIgnored();
        verify(mSuggestionLifecycleObserver, never()).onAnySuggestionResponse();
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testReset_disablesResponses() {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionAccepted();
        verify(mUserResponseCallback).onResult(any());

        mSuggestionLifecycleObserverHandler.onSuggestionDismissed();

        verify(mSuggestionLifecycleObserver, never()).onSuggestionDismissed();
        verify(mUserResponseCallback).onResult(any());
    }

    @Test
    public void testReset_newSuggestion() {
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionAccepted();
        verify(mUserResponseCallback).onResult(any());

        reset(mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.updateSuggestionDetails(
                SUGGESTION_ID, mUserResponseCallback);
        mSuggestionLifecycleObserverHandler.onSuggestionDismissed();

        verify(mUserResponseCallback).onResult(any());
        verify(mUserResponseCallback).onResult(any());
        verify(mSuggestionLifecycleObserver).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver, times(2)).onAnySuggestionResponse();
    }
}
