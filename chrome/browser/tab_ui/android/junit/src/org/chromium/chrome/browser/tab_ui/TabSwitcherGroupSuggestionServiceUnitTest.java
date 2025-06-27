// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TabSwitcherGroupSuggestionService}. */
@EnableFeatures(ChromeFeatureList.TAB_SWITCHER_GROUP_SUGGESTIONS_TEST_MODE_ANDROID)
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherGroupSuggestionServiceUnitTest {
    private static final @WindowId int WINDOW_ID = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private GroupSuggestionsService mGroupSuggestionsService;
    @Mock private SuggestionLifecycleObserverHandler mSuggestionLifecycleObserverHandler;
    @Mock private Callback<UserResponseMetadata> mUserResponseCallback;

    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<UserResponseMetadata> mUserResponseMetadataCaptor;

    @Spy
    private final ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private TabSwitcherGroupSuggestionService mService;

    @Before
    public void setUp() {
        GroupSuggestionsServiceFactory.setGroupSuggestionsServiceForTesting(
                mGroupSuggestionsService);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        mTabGroupModelFilterSupplier.set(mTabGroupModelFilter);

        mService =
                new TabSwitcherGroupSuggestionService(
                        WINDOW_ID,
                        mTabGroupModelFilterSupplier,
                        mProfile,
                        mSuggestionLifecycleObserverHandler);
    }

    @Test
    public void testConstructor_addsObservers() {
        verify(mTabGroupModelFilter).addObserver(any());
        verify(mTabGroupModelFilter).addTabGroupObserver(any());
    }

    @Test
    public void testDestroy_removesObserver() {
        mService.destroy();
        verify(mTabGroupModelFilterSupplier).removeObserver(any());
    }

    @Test
    public void testFilterChanged_switchesObservers() {
        TabGroupModelFilter newFilter = mock();

        mTabGroupModelFilterSupplier.set(newFilter);
        ShadowLooper.runUiThreadTasks();

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());

        verify(newFilter).addObserver(any());
        verify(newFilter).addTabGroupObserver(any());
    }

    @Test
    public void testFilterChanged_toNull() {
        mTabGroupModelFilterSupplier.set(null);
        ShadowLooper.runUiThreadTasks();

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());
    }

    @Test
    public void testMaybeShowSuggestions_clearsPreviousSuggestions() {
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID)).thenReturn(null);
        mService.maybeShowSuggestions();
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
    }

    @Test
    public void testMaybeShowSuggestions_showsFirstAndDiscardsOthers() {
        List<Integer> shownTabIdsList = List.of(1, 2);
        int[] shownTabIdsArray = {1, 2};
        GroupSuggestion suggestion1 = new GroupSuggestion(shownTabIdsArray, 10, 0, "", "", "");
        GroupSuggestion suggestion2 = new GroupSuggestion(new int[] {3, 4}, 11, 0, "", "", "");

        List<GroupSuggestion> suggestionList = List.of(suggestion1, suggestion2);
        GroupSuggestions groupSuggestions = new GroupSuggestions(suggestionList);
        CachedSuggestions cachedSuggestions =
                new CachedSuggestions(groupSuggestions, mUserResponseCallback);
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);

        mService.maybeShowSuggestions();

        verify(mSuggestionLifecycleObserverHandler).onShowSuggestion(shownTabIdsList);
        verify(mSuggestionLifecycleObserverHandler).updateSuggestionDetails(eq(10), any());
        verify(mUserResponseCallback).onResult(mUserResponseMetadataCaptor.capture());

        UserResponseMetadata response = mUserResponseMetadataCaptor.getValue();
        assertEquals(11, response.mSuggestionId);
        assertEquals(UserResponse.NOT_SHOWN, response.mUserResponse);
    }

    @Test
    public void testClearSuggestions_callsHandler() {
        mService.clearSuggestions();
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
    }

    @Test
    public void testTabModelObserver() {
        verify(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        Tab mockTab = mock();

        observer.didMoveTab(mockTab, 0, 1);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.tabClosureUndone(mockTab);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.tabRemoved(mockTab);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willCloseTab(mockTab, false);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willAddTab(mockTab, 0);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.tabPendingClosure(mockTab, 0);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
    }

    @Test
    public void testTabGroupModelFilterObserver() {
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mTabGroupModelFilterObserverCaptor.getValue();

        Tab mockTab = mock();
        List<Tab> mockTabs = Collections.singletonList(mockTab);

        observer.willMergeTabToGroup(mockTab, 0, null);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willMoveTabGroup(0, 1);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willMoveTabOutOfGroup(mockTab, null);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.didCreateGroup(mockTabs, null, null, null, null, 0, false);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.didRemoveTabGroup(0, null, 0);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willCloseTabGroup(null, false);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
    }
}
