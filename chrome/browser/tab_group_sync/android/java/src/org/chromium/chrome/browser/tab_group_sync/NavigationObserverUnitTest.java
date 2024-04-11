// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the {@link NavigationObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NavigationObserverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    private NavigationObserver mNavigationObserver;
    private NavigationTracker mNavigationTracker;
    @Mock private TabModelSelector mTabModelSelector;
    private List<TabModel> mTabModels = new ArrayList<>();
    private final GURL mTestUrl = new GURL("https://url1.com");
    private final GURL mTestUrl2 = new GURL("https://url2.com");
    private final String mTestTitle = new String("Some title");

    @Before
    public void setUp() {
        mTabModels = new ArrayList<>();
        doReturn(mTabModels).when(mTabModelSelector).getModels();

        mNavigationTracker = new NavigationTracker();
        mNavigationObserver =
                new NavigationObserver(mTabModelSelector, mTabGroupSyncService, mNavigationTracker);
    }

    private void mockTab(
            int tabId, int rootId, String title, GURL url, boolean isIncognito, boolean isGrouped) {
        when(mTab.isIncognito()).thenReturn(isIncognito);
        when(mTab.getId()).thenReturn(tabId);
        when((mTab.getRootId())).thenReturn(rootId);
        when(mTab.getTitle()).thenReturn(title);
        when(mTab.getUrl()).thenReturn(url);
        when(mTab.getTabGroupId()).thenReturn(isGrouped ? new Token(2, 3) : null);
    }

    private void simulateNavigation(GURL gurl, int transition) {
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        gurl,
                        /* isInPrimaryMainFrame= */ true,
                        /*isSameDocument*/ false,
                        /*isRendererInitiated*/ false,
                        transition,
                        /* hasUserGesture= */ false,
                        /* isReload= */ false);
        mNavigationObserver.onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);
    }

    @Test
    public void testNavigationObserverBasic() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                /* tabId= */ 5,
                /* rootId= */ 2,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(mTestUrl, PageTransition.LINK);
        verify(mTabGroupSyncService).updateTab(eq(2), eq(5), eq(mTestTitle), eq(mTestUrl), eq(-1));
    }

    @Test
    public void testMultipleNavigations() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                /* tabId= */ 5,
                /* rootId= */ 2,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(mTestUrl, PageTransition.LINK);
        verify(mTabGroupSyncService).updateTab(eq(2), eq(5), eq(mTestTitle), eq(mTestUrl), eq(-1));

        mockTab(
                /* tabId= */ 6,
                /* rootId= */ 3,
                mTestTitle,
                mTestUrl2,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(mTestUrl, PageTransition.LINK);
        verify(mTabGroupSyncService).updateTab(eq(3), eq(6), eq(mTestTitle), eq(mTestUrl2), eq(-1));
    }

    @Test
    public void testDisableObserver() {
        mNavigationObserver.enableObservers(false);
        mockTab(
                /* tabId= */ 5,
                /* rootId= */ 2,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(mTestUrl, PageTransition.LINK);
        verifyNoInteractions(mTabGroupSyncService);
    }

    @Test
    public void testIncognito() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                /* tabId= */ 5,
                /* rootId= */ 2,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ true,
                /* isGrouped= */ true);
        simulateNavigation(mTestUrl, PageTransition.LINK);
        verifyNoInteractions(mTabGroupSyncService);
    }

    @Test
    public void testRedirect() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                /* tabId= */ 5,
                /* rootId= */ 2,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(mTestUrl, PageTransition.SERVER_REDIRECT);
        verifyNoInteractions(mTabGroupSyncService);
    }

    @Test
    public void testSyncInitiatedNavigation() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                /* tabId= */ 5,
                /* rootId= */ 2,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);

        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        mTestUrl,
                        /* isInPrimaryMainFrame= */ true,
                        /*isSameDocument*/ false,
                        /*isRendererInitiated*/ false,
                        PageTransition.LINK,
                        /* hasUserGesture= */ false,
                        /* isReload= */ false);
        mNavigationTracker.setNavigationWasFromSync(navigation.getUserDataHost());
        mNavigationObserver.onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);

        verifyNoInteractions(mTabGroupSyncService);
    }
}
