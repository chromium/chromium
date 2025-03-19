// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;

@RunWith(BaseRobolectricTestRunner.class)
public class SuggestionEventObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int TAB_ID = 123;

    @Mock Profile mProfile;
    @Mock TabModel mTabModel;
    @Mock GroupSuggestionsService mGroupSuggestionsService;
    @Mock ObservableSupplierImpl<Boolean> mHubVisibilitySupplier;
    @Mock ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    @Mock Tab mTab;
    @Mock Pane mPane;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private SuggestionEventObserver mSuggestionEventObserver;

    @Before
    public void setup() {
        mHubVisibilitySupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mFocusedPaneSupplier.set(mPane);
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        doReturn(TAB_ID).when(mTab).getId();
        GroupSuggestionsServiceFactory.setGroupSuggestionsServiceForTesting(
                mGroupSuggestionsService);

        mSuggestionEventObserver =
                new SuggestionEventObserver(
                        mProfile, mTabModel, mHubVisibilitySupplier, mFocusedPaneSupplier);
    }

    @Test
    public void testDidSelectTab() {
        mTabModelObserverCaptor.getValue().didSelectTab(mTab, 0, 0);

        verify(mGroupSuggestionsService).didSelectTab(eq(TAB_ID), eq(0), eq(0));
    }

    @Test
    public void testDidAddTab() {
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        verify(mGroupSuggestionsService).didAddTab(eq(TAB_ID), eq(TabLaunchType.FROM_RESTORE));
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
    public void testDestroy() {
        assertTrue(mHubVisibilitySupplier.hasObservers());

        mSuggestionEventObserver.destroy();

        verify(mTabModel).removeObserver(any());
        assertFalse(mHubVisibilitySupplier.hasObservers());
    }
}
