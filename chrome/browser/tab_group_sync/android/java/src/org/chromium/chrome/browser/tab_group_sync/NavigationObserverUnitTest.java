// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
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
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
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
    private static final int TAB_ID_1 = 5;
    private static final int TAB_ID_2 = 6;
    private static final Token TOKEN_1 = new Token(2, 3);
    private static final Token TOKEN_2 = new Token(4, 5);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID_1 = new LocalTabGroupId(TOKEN_1);
    private static final GURL CHROME_HISTORY_URL = new GURL("chrome://history");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Tab mTab;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private TabGroupSyncUtilsJni mTabGroupSyncUtilsJni;
    private NavigationObserver mNavigationObserver;
    private NavigationTracker mNavigationTracker;
    @Mock private TabModelSelector mTabModelSelector;
    private List<TabModel> mTabModels = new ArrayList<>();
    private final GURL mTestUrl = new GURL("https://url1.com");
    private final GURL mTestUrl2 = new GURL("https://url2.com");
    private final String mTestTitle = new String("Some title");

    @Before
    public void setUp() {
        mJniMocker.mock(TabGroupSyncUtilsJni.TEST_HOOKS, mTabGroupSyncUtilsJni);
        mTabModels = new ArrayList<>();
        doReturn(mTabModels).when(mTabModelSelector).getModels();

        mNavigationTracker = new NavigationTracker();
        mNavigationObserver =
                new NavigationObserver(mTabModelSelector, mTabGroupSyncService, mNavigationTracker);
    }

    private void mockTab(
            int tabId,
            Token tabGroupId,
            String title,
            GURL url,
            boolean isIncognito,
            boolean isGrouped) {
        when(mTab.isIncognito()).thenReturn(isIncognito);
        when(mTab.getId()).thenReturn(tabId);
        when(mTab.getTabGroupId()).thenReturn(tabGroupId);
        when(mTab.getTitle()).thenReturn(title);
        when(mTab.getUrl()).thenReturn(url);
    }

    private void simulateNavigation(int transition) {
        simulateNavigation(transition, /* isSaveableNavigation= */ true);
    }

    private void simulateNavigation(int transition, boolean isSaveableNavigation) {
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        new GURL("unused"),
                        /* isInPrimaryMainFrame= */ true,
                        /*isSameDocument*/ false,
                        /*isRendererInitiated*/ false,
                        transition,
                        /* hasUserGesture= */ false,
                        /* isReload= */ false,
                        isSaveableNavigation);
        mNavigationObserver.onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);
    }

    @Test
    public void testNavigationObserverBasic() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verify(mTabGroupSyncService)
                .updateTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(mTestTitle),
                        eq(mTestUrl),
                        eq(-1));
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }

    @Test
    public void testMultipleNavigations() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verify(mTabGroupSyncService)
                .updateTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(mTestTitle),
                        eq(mTestUrl),
                        eq(-1));
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());

        mockTab(
                TAB_ID_2,
                TOKEN_2,
                mTestTitle,
                mTestUrl2,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        LocalTabGroupId id2 = new LocalTabGroupId(TOKEN_2);
        verify(mTabGroupSyncService)
                .updateTab(eq(id2), eq(TAB_ID_2), eq(mTestTitle), eq(mTestUrl2), eq(-1));
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(id2), eq(TAB_ID_2), anyLong());
    }

    @Test
    public void testDisableObserver() {
        mNavigationObserver.enableObservers(false);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verifyNoInteractions(mTabGroupSyncService);
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }

    @Test
    public void testIncognito() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ true,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verifyNoInteractions(mTabGroupSyncService);
        verifyNoInteractions(mTabGroupSyncUtilsJni);
    }

    @Test
    public void testChromeInternalUrl() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                CHROME_HISTORY_URL,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verify(mTabGroupSyncService)
                .updateTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(TabGroupSyncUtils.UNSAVEABLE_TAB_TITLE),
                        eq(TabGroupSyncUtils.UNSAVEABLE_URL_OVERRIDE),
                        eq(-1));
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }

    @Test
    public void testNotHttpOrHttpsOrChromeUrl() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                new GURL("ftp://someurl.com"),
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verify(mTabGroupSyncService)
                .updateTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(TabGroupSyncUtils.UNSAVEABLE_TAB_TITLE),
                        eq(TabGroupSyncUtils.UNSAVEABLE_URL_OVERRIDE),
                        eq(-1));
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }

    @Test
    public void testNtpUrl() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                new GURL(UrlConstants.NTP_URL),
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK);
        verify(mTabGroupSyncService)
                .updateTab(
                        eq(LOCAL_TAB_GROUP_ID_1),
                        eq(TAB_ID_1),
                        eq(TabGroupSyncUtils.NEW_TAB_TITLE),
                        eq(TabGroupSyncUtils.NTP_URL),
                        eq(-1));
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }

    @Test
    public void testSyncInitiatedNavigation() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
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
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }

    @Test
    public void testNotSaveableNavigation() {
        mNavigationObserver.enableObservers(true);
        mockTab(
                TAB_ID_1,
                TOKEN_1,
                mTestTitle,
                mTestUrl,
                /* isIncognito= */ false,
                /* isGrouped= */ true);
        simulateNavigation(PageTransition.LINK, false);
        verifyNoInteractions(mTabGroupSyncService);
        verify(mTabGroupSyncUtilsJni)
                .onDidFinishNavigation(any(), eq(LOCAL_TAB_GROUP_ID_1), eq(TAB_ID_1), anyLong());
    }
}
