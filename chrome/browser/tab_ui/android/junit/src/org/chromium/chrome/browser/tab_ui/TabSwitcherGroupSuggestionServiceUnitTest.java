// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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

import org.chromium.base.JniOnceCallback;
import org.chromium.base.Token;
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
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.ArrayList;
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
    @Mock private JniOnceCallback<UserResponseMetadata> mUserResponseCallback;

    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<UserResponseMetadata> mUserResponseMetadataCaptor;

    @Spy
    private final ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private final ArrayList<Tab> mTabs = new ArrayList<>();

    private TabSwitcherGroupSuggestionService mService;

    @Before
    public void setUp() {
        GroupSuggestionsServiceFactory.setGroupSuggestionsServiceForTesting(
                mGroupSuggestionsService);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModel.iterator()).thenAnswer(inv -> mTabs.iterator());

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
    public void testDestroy() {
        mService.destroy();
        verify(mTabGroupModelFilterSupplier).removeObserver(any());
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
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

        // Mock a full tab model state to prevent NPEs.
        mockTab(1, 0, true); // One tab must be active
        mockTab(2, 1, false);
        mockTab(3, 2, false);
        mockTab(4, 3, false);
        when(mTabModel.getCount()).thenReturn(4);

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
        verify(mUserResponseCallback, never()).onResult(mUserResponseMetadataCaptor.capture());
    }

    @Test
    public void testMaybeShowSuggestions_tabsContiguous() {
        int[] tabIds = {2, 1};
        GroupSuggestion suggestion = new GroupSuggestion(tabIds, 10, 0, "", "", "");
        setupCachedSuggestion(suggestion);
        mockTab(1, 0, true);
        mockTab(2, 1, false);
        when(mTabModel.getCount()).thenReturn(2);

        mService.maybeShowSuggestions();
        verify(mSuggestionLifecycleObserverHandler).onShowSuggestion(List.of(1, 2));
    }

    @Test
    public void testMaybeShowSuggestions_tabsNotContiguous_smallGap() {
        int[] tabIds = {1, 3}; // Gap between tab 1 and 3.
        GroupSuggestion suggestion = new GroupSuggestion(tabIds, 10, 0, "", "", "");
        setupCachedSuggestion(suggestion);
        mockTab(1, 0, true);
        mockTab(2, 1, false);
        mockTab(3, 2, false);
        when(mTabModel.getCount()).thenReturn(3);

        mService.maybeShowSuggestions();
        verify(mSuggestionLifecycleObserverHandler).onShowSuggestion(List.of(1, 3));
    }

    @Test
    public void testMaybeShowSuggestions_tabsNotContiguous_bigGap() {
        int[] tabIds = {1, 4}; // Gap between tab 1 and 4.
        GroupSuggestion suggestion = new GroupSuggestion(tabIds, 10, 0, "", "", "");
        setupCachedSuggestion(suggestion);
        mockTab(1, 0, true);
        mockTab(2, 1, false);
        mockTab(3, 2, false);
        mockTab(4, 3, false);
        when(mTabModel.getCount()).thenReturn(4);

        mService.maybeShowSuggestions();
        verify(mSuggestionLifecycleObserverHandler, never()).onShowSuggestion(any());
        verify(mUserResponseCallback).onResult(any());
    }

    @Test
    public void testMaybeShowSuggestions_oneTabPinned() {
        int[] tabIds = {1, 2};
        GroupSuggestion suggestion = new GroupSuggestion(tabIds, 10, 0, "", "", "");
        setupCachedSuggestion(suggestion);
        mockTab(1, 0, true);
        Tab pinnedTab = mockTab(2, 1, false);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getCount()).thenReturn(2);

        mService.maybeShowSuggestions();
        verify(mSuggestionLifecycleObserverHandler, never()).onShowSuggestion(any());
        verify(mUserResponseCallback).onResult(any());
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
        observer.onTabClosePending(Collections.singletonList(mockTab), /* isAllTabs= */ false, 0);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
    }

    @Test
    public void testTabGroupModelFilterObserver() {
        verify(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mTabGroupModelFilterObserverCaptor.getValue();

        Tab mockTab = mock();

        observer.willMergeTabToGroup(mockTab, 0, null);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willMoveTabGroup(new Token(1L, 2L), 0);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willMoveTabOutOfGroup(mockTab, null);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.didCreateNewGroup(mockTab, mTabGroupModelFilter);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.didRemoveTabGroup(0, null, 0);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();

        reset(mSuggestionLifecycleObserverHandler);
        observer.willCloseTabGroup(null, false);
        verify(mSuggestionLifecycleObserverHandler).onSuggestionIgnored();
    }

    private Tab mockTab(int tabId, int index, boolean isActive) {
        Tab tab = mock();

        when(tab.getId()).thenReturn(tabId);
        when(tab.isFrozen()).thenReturn(false);
        when(tab.isClosing()).thenReturn(false);
        when(tab.isActivated()).thenReturn(isActive);

        when(mTabModel.getTabById(tabId)).thenReturn(tab);
        when(mTabModel.indexOf(tab)).thenReturn(index);
        when(mTabModel.getTabAt(index)).thenReturn(tab);
        mTabs.add(index, tab);
        return tab;
    }

    private void setupCachedSuggestion(GroupSuggestion suggestion) {
        GroupSuggestions groupSuggestions =
                new GroupSuggestions(Collections.singletonList(suggestion));
        CachedSuggestions cachedSuggestions =
                new CachedSuggestions(groupSuggestions, mUserResponseCallback);
        when(mGroupSuggestionsService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(cachedSuggestions);
    }
}
