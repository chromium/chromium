// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabSwitcherGroupSuggestionService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherGroupSuggestionServiceUnitTest {
    private static final @WindowId int WINDOW_ID = 1;
    private static final int SUGGESTION_ID = 123;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private GroupSuggestionsService mGroupSuggestionsService;
    @Mock private SuggestionLifecycleObserver mSuggestionLifecycleObserver;
    @Mock private Callback<UserResponseMetadata> mUserResponseCallback;

    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabSwitcherGroupSuggestionService mService;
    private ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier;

    @Before
    public void setUp() {
        GroupSuggestionsServiceFactory.setGroupSuggestionsServiceForTesting(
                mGroupSuggestionsService);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        mTabGroupModelFilterSupplier = new ObservableSupplierImpl<>(mTabGroupModelFilter);
        mService =
                new TabSwitcherGroupSuggestionService(
                        WINDOW_ID,
                        mTabGroupModelFilterSupplier,
                        mProfile,
                        mSuggestionLifecycleObserver);
    }

    @Test
    public void testMaybeShowSuggestions_noCachedSuggestions() {
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID)).thenReturn(null);

        mService.maybeShowSuggestions();

        verify(mSuggestionLifecycleObserver, never()).onShowSuggestion(any());
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testMaybeShowSuggestions_nullGroupSuggestions() {
        CachedSuggestions cachedSuggestions = new CachedSuggestions(null, mUserResponseCallback);
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);

        mService.maybeShowSuggestions();

        verify(mSuggestionLifecycleObserver, never()).onShowSuggestion(any());
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testMaybeShowSuggestions_nullSuggestionList() {
        GroupSuggestions groupSuggestions = new GroupSuggestions(null);
        CachedSuggestions cachedSuggestions =
                new CachedSuggestions(groupSuggestions, mUserResponseCallback);
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);

        mService.maybeShowSuggestions();

        verify(mSuggestionLifecycleObserver, never()).onShowSuggestion(any());
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testMaybeShowSuggestions_emptySuggestionList() {
        GroupSuggestions groupSuggestions = new GroupSuggestions(Collections.emptyList());
        CachedSuggestions cachedSuggestions =
                new CachedSuggestions(groupSuggestions, mUserResponseCallback);
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);

        mService.maybeShowSuggestions();

        verify(mSuggestionLifecycleObserver, never()).onShowSuggestion(any());
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testMaybeShowSuggestions_discardOtherSuggestions() {
        List<Integer> shownTabIdsList = List.of(1, 2);
        int[] shownTabIdsArray = {1, 2};

        GroupSuggestion suggestion1 = new GroupSuggestion(shownTabIdsArray, 10, 0, "", "", "");
        GroupSuggestion suggestion2 = new GroupSuggestion(new int[] {3, 4}, 11, 0, "", "", "");
        GroupSuggestion suggestion3 = new GroupSuggestion(new int[] {5, 6}, 12, 0, "", "", "");

        List<GroupSuggestion> suggestionList = List.of(suggestion1, suggestion2, suggestion3);
        GroupSuggestions groupSuggestions = new GroupSuggestions(suggestionList);

        CachedSuggestions cachedSuggestions =
                new CachedSuggestions(groupSuggestions, mUserResponseCallback);
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);

        mService.maybeShowSuggestions();
        verify(mSuggestionLifecycleObserver).onShowSuggestion(shownTabIdsList);
        verify(mUserResponseCallback, times(2)).onResult(any(UserResponseMetadata.class));
    }

    @Test
    public void testClearSuggestions() {
        int[] tabIdsArray = {1, 2};

        GroupSuggestion suggestion = new GroupSuggestion(tabIdsArray, 10, 0, "", "", "");
        GroupSuggestions groupSuggestions =
                new GroupSuggestions(Collections.singletonList(suggestion));
        CachedSuggestions cachedSuggestions =
                new CachedSuggestions(groupSuggestions, mUserResponseCallback);

        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);

        mService.maybeShowSuggestions();
        mService.clearSuggestions();

        verify(mSuggestionLifecycleObserver).onSuggestionIgnored();
        verify(mUserResponseCallback).onResult(any(UserResponseMetadata.class));
    }

    @Test
    public void testClearSuggestions_noSuggestionIsShowing() {
        mService.clearSuggestions();

        verify(mSuggestionLifecycleObserver, never()).onSuggestionIgnored();
        verify(mUserResponseCallback, never()).onResult(any());
    }

    @Test
    public void testHandler_onSuggestionAccepted() {
        SuggestionLifecycleObserverHandler handler =
                new SuggestionLifecycleObserverHandler(
                        SUGGESTION_ID, mUserResponseCallback, mSuggestionLifecycleObserver);

        handler.onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback).onResult(any(UserResponseMetadata.class));
    }

    @Test
    public void testHandler_onSuggestionDismissed() {
        SuggestionLifecycleObserverHandler handler =
                new SuggestionLifecycleObserverHandler(
                        SUGGESTION_ID, mUserResponseCallback, mSuggestionLifecycleObserver);

        handler.onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback).onResult(any(UserResponseMetadata.class));
    }

    @Test
    public void testHandler_onSuggestionIgnored() {
        SuggestionLifecycleObserverHandler handler =
                new SuggestionLifecycleObserverHandler(
                        SUGGESTION_ID, mUserResponseCallback, mSuggestionLifecycleObserver);

        handler.onSuggestionIgnored();
        verify(mSuggestionLifecycleObserver).onSuggestionIgnored();
        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback).onResult(any(UserResponseMetadata.class));
    }

    @Test
    public void testHandler_onShowSuggestion() {
        List<Integer> tabIds = List.of(1, 2, 3);
        SuggestionLifecycleObserverHandler handler =
                new SuggestionLifecycleObserverHandler(
                        SUGGESTION_ID, mUserResponseCallback, mSuggestionLifecycleObserver);

        handler.onShowSuggestion(tabIds);
        verify(mSuggestionLifecycleObserver).onShowSuggestion(tabIds);
    }

    @Test
    public void testHandler_onlyOneResponseAllowed() {
        SuggestionLifecycleObserverHandler handler =
                new SuggestionLifecycleObserverHandler(
                        SUGGESTION_ID, mUserResponseCallback, mSuggestionLifecycleObserver);

        handler.onSuggestionAccepted();
        handler.onSuggestionDismissed();
        handler.onSuggestionIgnored();

        verify(mSuggestionLifecycleObserver).onSuggestionAccepted();
        verify(mSuggestionLifecycleObserver, never()).onSuggestionDismissed();
        verify(mSuggestionLifecycleObserver, never()).onSuggestionIgnored();

        verify(mSuggestionLifecycleObserver).onAnySuggestionResponse();
        verify(mUserResponseCallback).onResult(any(UserResponseMetadata.class));
    }

    @Test
    public void testFilterChanged() {
        TabGroupModelFilter newFilter = mock();
        mTabGroupModelFilterSupplier.set(newFilter);
        ShadowLooper.runUiThreadTasks();

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());

        verify(newFilter).addObserver(any());
        verify(newFilter).addTabGroupObserver(any());
    }
}
