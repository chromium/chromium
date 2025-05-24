// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionEventObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TAB_ID = 123;
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;

    @Mock Profile mProfile;
    @Mock TabModel mTabModel;
    @Mock TabModelSelector mTabModelSelector;
    @Mock GroupSuggestionsService mGroupSuggestionsService;
    @Mock HubManager mHubManager;
    @Mock PaneManager mPaneManager;
    @Mock Tab mTab;
    @Mock Pane mPane;
    @Mock NavigationHandle mNavigationHandle;
    @Mock WebContents mWebContents;
    @Mock NavigationController mNavigationController;
    @Mock NavigationHistory mNavigationHistory;
    @Mock NavigationEntry mNavigationEntry;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private SuggestionEventObserver mSuggestionEventObserver;
    private ObservableSupplierImpl<Boolean> mHubVisibilitySupplier;
    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private OneshotSupplierImpl<HubManager> mHubManagerSupplier;

    @Before
    public void setup() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        ObservableSupplier<Tab> currentTabSupplier =
                new ObservableSupplier<>() {
                    @Override
                    public @Nullable Tab addObserver(Callback<Tab> obs, int behavior) {
                        return null;
                    }

                    @Override
                    public void removeObserver(Callback<Tab> obs) {}

                    @Override
                    public Tab get() {
                        return mTab;
                    }
                };
        doReturn(currentTabSupplier).when(mTabModel).getCurrentTabSupplier();
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.getUrl()).thenReturn(TEST_URL);
        mHubVisibilitySupplier = new ObservableSupplierImpl<>();
        when(mHubManager.getHubVisibilitySupplier()).thenReturn(mHubVisibilitySupplier);
        when(mHubManager.getPaneManager()).thenReturn(mPaneManager);
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier.set(mPane);
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mFocusedPaneSupplier);
        mHubManagerSupplier = new OneshotSupplierImpl<>();
        mHubManagerSupplier.set(mHubManager);
        when(mNavigationHandle.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getNavigationHistory()).thenReturn(mNavigationHistory);
        when(mNavigationHistory.getCurrentEntryIndex()).thenReturn(0);
        when(mNavigationHistory.getEntryAtIndex(0)).thenReturn(mNavigationEntry);
        GroupSuggestionsServiceFactory.setGroupSuggestionsServiceForTesting(
                mGroupSuggestionsService);

        mSuggestionEventObserver =
                new SuggestionEventObserver(mTabModelSelector, mHubManagerSupplier);
    }

    @Test
    public void testDidSelectTab() {
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, TabSelectionType.FROM_USER, 0);

        verify(mGroupSuggestionsService)
                .didSelectTab(eq(TAB_ID), eq(TEST_URL), eq(TabSelectionType.FROM_USER), eq(0));
    }

    @Test
    public void testDidAddTab() {
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        /* markedForSelection= */ true);

        verify(mGroupSuggestionsService).didAddTab(eq(TAB_ID), eq(TabLaunchType.FROM_CHROME_UI));
    }

    @Test
    public void testDidAddTab_IgnoreRestore() {
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.FROZEN_ON_RESTORE,
                        /* markedForSelection= */ true);

        verify(mGroupSuggestionsService, never()).didAddTab(anyInt(), anyInt());
    }

    @Test
    public void testWillCloseTab() {
        mTabModelObserverCaptor.getValue().willCloseTab(mTab, false);

        verify(mGroupSuggestionsService).willCloseTab(eq(TAB_ID));
    }

    @Test
    public void testTabClosureUndone() {
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab);

        verify(mGroupSuggestionsService).tabClosureUndone(eq(TAB_ID));
    }

    @Test
    public void testTabClosureCommitted() {
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab);

        verify(mGroupSuggestionsService).tabClosureCommitted(eq(TAB_ID));
    }

    @Test
    public void testSelectFirstTab() {
        ObservableSupplierImpl<Tab> currentTabSupplier = new ObservableSupplierImpl<>();
        currentTabSupplier.set(mTab);
        doReturn(currentTabSupplier).when(mTabModel).getCurrentTabSupplier();

        mSuggestionEventObserver =
                new SuggestionEventObserver(mTabModelSelector, mHubManagerSupplier);

        // Later current tab changes will be ignored
        Tab tab2 = mock(Tab.class);
        currentTabSupplier.set(tab2);

        verify(mGroupSuggestionsService, times(1))
                .didSelectTab(
                        eq(TAB_ID),
                        eq(TEST_URL),
                        eq(
                                org.chromium.components.visited_url_ranking.url_grouping
                                        .TabSelectionType.FROM_NEW_TAB),
                        eq(Tab.INVALID_TAB_ID));
    }

    @Test
    public void testEnterPane_FocusTabSwitcher() {
        doReturn(PaneId.TAB_SWITCHER).when(mPane).getPaneId();

        mHubVisibilitySupplier.set(true);

        verify(mGroupSuggestionsService).didEnterTabSwitcher();
    }

    @Test
    public void testEnterPane_NotFocusTabSwitcher() {
        doReturn(PaneId.INCOGNITO_TAB_SWITCHER).when(mPane).getPaneId();

        mHubVisibilitySupplier.set(true);

        verify(mGroupSuggestionsService, never()).didEnterTabSwitcher();
    }

    @Test
    public void testEnterPane_InvalidFocusedPane() {
        mFocusedPaneSupplier.set(null);

        mHubVisibilitySupplier.set(true);

        verify(mGroupSuggestionsService, never()).didEnterTabSwitcher();
    }

    @Test
    public void testExitPane() {
        doReturn(PaneId.TAB_SWITCHER).when(mPane).getPaneId();

        mHubVisibilitySupplier.set(false);

        verify(mGroupSuggestionsService, never()).didEnterTabSwitcher();
    }

    @Test
    public void testTabNavigation() {
        when(mTab.isIncognitoBranded()).thenReturn(false);

        when(mNavigationEntry.getTransition()).thenReturn(PageTransition.LINK);
        mSuggestionEventObserver
                .getTabModelSelectorTabObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, mNavigationHandle);

        verify(mGroupSuggestionsService).onDidFinishNavigation(eq(TAB_ID), eq(PageTransition.LINK));
    }

    @Test
    public void testTabNavigation_IncognitoTab() {
        Tab incognitoTab = mock(Tab.class);
        when(incognitoTab.isIncognitoBranded()).thenReturn(true);

        when(mNavigationEntry.getTransition()).thenReturn(PageTransition.LINK);
        mSuggestionEventObserver
                .getTabModelSelectorTabObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(incognitoTab, mNavigationHandle);

        verify(mGroupSuggestionsService, never()).onDidFinishNavigation(anyInt(), anyInt());
    }

    @Test
    public void testDestroy() {
        assertTrue(mHubVisibilitySupplier.hasObservers());

        mSuggestionEventObserver.destroy();

        verify(mTabModel).removeObserver(any());
        assertFalse(mHubVisibilitySupplier.hasObservers());
    }
}
