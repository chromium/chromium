// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
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
        mSuggestionLifecycleObserverHandler =
                new SuggestionLifecycleObserverHandler(
                        SUGGESTION_ID, mUserResponseCallback, mSuggestionLifecycleObserver);
    }

    @Test
    public void testOnSuggestionAccepted() {
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
        mSuggestionLifecycleObserverHandler.onSuggestionAccepted();

        verify(mSuggestionLifecycleObserver, times(1)).onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver, times(1)).onAnySuggestionResponse();
        verify(mUserResponseCallback, times(1)).onResult(any(UserResponseMetadata.class));

        mSuggestionLifecycleObserverHandler.onSuggestionDismissed();
        mSuggestionLifecycleObserverHandler.onSuggestionIgnored();

        verify(mSuggestionLifecycleObserver, never()).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver, never()).onSuggestionIgnored();
        verify(mSuggestionLifecycleObserver, times(1)).onAnySuggestionResponse();
        verify(mUserResponseCallback, times(1)).onResult(any(UserResponseMetadata.class));
    }
}
