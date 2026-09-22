// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.TabCacheManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabOrchestratorType;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

import java.util.Collections;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link BackgroundTabRestorationHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BackgroundTabRestorationHelperTest {
    private static final @TabId int TAB_ID = 101;
    private static final int DESTINATION_INDEX = 2;

    public final @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ProfileResolver.Natives mProfileResolverNatives;
    private @Mock TabModelSelector mTabModelSelector;
    private @Mock TabModel mNormalTabModel;
    private @Mock Profile mProfile;
    private @Mock BackgroundTabPool mBackgroundTabPool;
    private @Mock BackgroundPoolTab mBackgroundPoolTab;
    private @Mock Tab mTab;
    private @Mock WebContentsState mWebContentsState;

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        when(mTabModelSelector.getModel(false)).thenReturn(mNormalTabModel);
        when(mNormalTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mProfileResolverNatives.tokenizeProfile(mProfile)).thenReturn("mock_token");
        BackgroundTabPoolManager.resetForTesting();
        TabCacheManager.resetForTesting();
    }

    @After
    public void tearDown() {
        BackgroundTabPoolManager.resetForTesting();
        TabCacheManager.resetForTesting();
        NotificationProxyUtils.setNotificationEnabledForTest(null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_flagEnabled() {
        assertTrue(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_nonAuthoritativeStore() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_flagDisabled() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_notificationsDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_incognito() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED,
                        /* isIncognito= */ true,
                        /* isAuthoritativeStore= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_nonTabbedOrchestrator() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.CUSTOM,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true));
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.ARCHIVED,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true));
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.HEADLESS,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_nullSelector() {
        assertNull(BackgroundTabRestorationHelper.acquirePool(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_preNative_fallsBackToPersistedToken() {
        when(mProfile.isNativeInitialized()).thenReturn(false);
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN,
                        "persisted_token");

        BackgroundTabPool pool = BackgroundTabRestorationHelper.acquirePool(mTabModelSelector);
        assertNotNull(pool);
        assertEquals("persisted_token", pool.getProfileToken());
        BackgroundTabPoolManager.release(pool);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_nullProfile_restoresFromToken() {
        when(mNormalTabModel.getProfile()).thenReturn(null);
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN,
                        "persisted_token");

        BackgroundTabPool pool = BackgroundTabRestorationHelper.acquirePool(mTabModelSelector);
        assertNotNull(pool);
        assertEquals("persisted_token", pool.getProfileToken());
        BackgroundTabPoolManager.release(pool);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_nullProfile_noToken() {
        when(mNormalTabModel.getProfile()).thenReturn(null);
        assertNull(BackgroundTabRestorationHelper.acquirePool(mTabModelSelector));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_offTheRecordProfile() {
        when(mProfile.isOffTheRecord()).thenReturn(true);
        assertNull(BackgroundTabRestorationHelper.acquirePool(mTabModelSelector));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_success() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        BackgroundTabPool pool = BackgroundTabRestorationHelper.acquirePool(mTabModelSelector);
        assertNotNull(pool);
        assertEquals(mBackgroundTabPool, pool);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_flagDisabled() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllPlaceholderTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_nonAuthoritativeStore() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ false);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllPlaceholderTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_incognito() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ true,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_nonTabbedOrchestrator() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.CUSTOM,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllPlaceholderTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_poolReturnsIds() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.getAllPlaceholderTabIds()).thenReturn(Set.of(1, 2, 3));

        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertEquals(3, ids.size());
        assertTrue(ids.contains(1));
        assertTrue(ids.contains(2));
        assertTrue(ids.contains(3));
        verify(mBackgroundTabPool).getAllPlaceholderTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_poolAcquireFails() {
        when(mNormalTabModel.getProfile()).thenReturn(null);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_success_destroysPlaceholderContentsState() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        when(mBackgroundTabPool.loadTabByPlaceholderId(TAB_ID)).thenReturn(mBackgroundPoolTab);
        when(mBackgroundPoolTab.attachTab(eq(mNormalTabModel), eq(DESTINATION_INDEX), eq(tabState)))
                .thenReturn(mTab);

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState,
                        /* isAuthoritativeStore= */ true);

        assertEquals(mTab, restoredTab);
        verify(mBackgroundTabPool).loadTabByPlaceholderId(TAB_ID);
        verify(mBackgroundPoolTab).prepareForForeground(mTabModelSelector);
        verify(mBackgroundPoolTab)
                .attachTab(eq(mNormalTabModel), eq(DESTINATION_INDEX), eq(tabState));
        verify(mWebContentsState).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_nonAuthoritativeStore() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState,
                        /* isAuthoritativeStore= */ false);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTabByPlaceholderId(anyInt());
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_nonTabbedOrchestrator() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.CUSTOM,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState,
                        /* isAuthoritativeStore= */ true);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTabByPlaceholderId(anyInt());
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_flagDisabled() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState,
                        /* isAuthoritativeStore= */ true);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTabByPlaceholderId(anyInt());
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_notificationsDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState,
                        /* isAuthoritativeStore= */ true);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTabByPlaceholderId(anyInt());
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_tabNotFoundInPool() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.loadTabByPlaceholderId(TAB_ID)).thenReturn(null);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState,
                        /* isAuthoritativeStore= */ true);

        assertNull(restoredTab);
        verify(mBackgroundTabPool).loadTabByPlaceholderId(TAB_ID);
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_nullSelector() {
        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        null,
                        TAB_ID,
                        DESTINATION_INDEX,
                        null,
                        /* isAuthoritativeStore= */ true);
        assertNull(restoredTab);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_nullProfile() {
        when(mNormalTabModel.getProfile()).thenReturn(null);
        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        null,
                        /* isAuthoritativeStore= */ true);
        assertNull(restoredTab);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testClaimRemainingBackgroundTabIds_success() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.claimTabIdsWithoutPlaceholders()).thenReturn(Set.of(1, 2));

        Set<Integer> ids =
                BackgroundTabRestorationHelper.claimRemainingBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertEquals(Set.of(1, 2), ids);
        verify(mBackgroundTabPool).claimTabIdsWithoutPlaceholders();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testClaimRemainingBackgroundTabIds_nonAuthoritativeStore() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        Set<Integer> ids =
                BackgroundTabRestorationHelper.claimRemainingBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ false);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).claimTabIdsWithoutPlaceholders();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testClaimRemainingBackgroundTabIds_incognito() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        Set<Integer> ids =
                BackgroundTabRestorationHelper.claimRemainingBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ true,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).claimTabIdsWithoutPlaceholders();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testClaimRemainingBackgroundTabIds_flagDisabled() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        Set<Integer> ids =
                BackgroundTabRestorationHelper.claimRemainingBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).claimTabIdsWithoutPlaceholders();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testClaimRemainingBackgroundTabIds_nullPool() {
        when(mNormalTabModel.getProfile()).thenReturn(null);

        Set<Integer> ids =
                BackgroundTabRestorationHelper.claimRemainingBackgroundTabIds(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        /* isIncognito= */ false,
                        /* isAuthoritativeStore= */ true);
        assertTrue(ids.isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreRemainingBackgroundTabs_success() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        BackgroundPoolTab coldTab1 = mock(BackgroundPoolTab.class);
        BackgroundPoolTab coldTab2 = mock(BackgroundPoolTab.class);
        Tab restoredTab1 = mock(Tab.class);
        Tab restoredTab2 = mock(Tab.class);
        when(restoredTab1.getId()).thenReturn(1);
        when(restoredTab2.getId()).thenReturn(2);

        when(mNormalTabModel.getTabById(1)).thenReturn(null);
        when(mNormalTabModel.getTabById(2)).thenReturn(null);
        when(mBackgroundTabPool.getLiveTab(1)).thenReturn(null);
        when(mBackgroundTabPool.getLiveTab(2)).thenReturn(null);
        when(mBackgroundTabPool.loadTabByOriginalId(1)).thenReturn(coldTab1);
        when(mBackgroundTabPool.loadTabByOriginalId(2)).thenReturn(coldTab2);
        when(mNormalTabModel.getCount()).thenReturn(0).thenReturn(1);
        when(coldTab1.attachTab(eq(mNormalTabModel), eq(0))).thenReturn(restoredTab1);
        when(coldTab2.attachTab(eq(mNormalTabModel), eq(1))).thenReturn(restoredTab2);

        List<Tab> restoredTabs =
                BackgroundTabRestorationHelper.restoreRemainingBackgroundTabs(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        List.of(1, 2),
                        /* isAuthoritativeStore= */ true);

        assertEquals(2, restoredTabs.size());
        assertEquals(restoredTab1, restoredTabs.get(0));
        assertEquals(restoredTab2, restoredTabs.get(1));
        verify(mBackgroundTabPool).loadTabByOriginalId(1);
        verify(mBackgroundTabPool).loadTabByOriginalId(2);
        verify(coldTab1).attachTab(mNormalTabModel, 0);
        verify(coldTab2).attachTab(mNormalTabModel, 1);
        verify(mBackgroundTabPool).cleanupPostRestore();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreRemainingBackgroundTabs_emptyTabIds() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        List<Tab> restoredTabs =
                BackgroundTabRestorationHelper.restoreRemainingBackgroundTabs(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        Collections.emptySet(),
                        /* isAuthoritativeStore= */ true);

        assertTrue(restoredTabs.isEmpty());
        verify(mBackgroundTabPool, never()).loadTabByOriginalId(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreRemainingBackgroundTabs_nonAuthoritativeStore() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        List<Tab> restoredTabs =
                BackgroundTabRestorationHelper.restoreRemainingBackgroundTabs(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        Set.of(1),
                        /* isAuthoritativeStore= */ false);

        assertTrue(restoredTabs.isEmpty());
        verify(mBackgroundTabPool, never()).loadTabByOriginalId(anyInt());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreRemainingBackgroundTabs_flagDisabled() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);

        List<Tab> restoredTabs =
                BackgroundTabRestorationHelper.restoreRemainingBackgroundTabs(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        Set.of(1),
                        /* isAuthoritativeStore= */ true);

        assertTrue(restoredTabs.isEmpty());
        verify(mBackgroundTabPool, never()).loadTabByOriginalId(anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreRemainingBackgroundTabs_assertsNoLiveTabs() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mNormalTabModel.getTabById(5)).thenReturn(null);
        LiveBackgroundTab liveTab = mock(LiveBackgroundTab.class);
        when(mBackgroundTabPool.getLiveTab(5)).thenReturn(liveTab);

        Set<Integer> tabIds = Set.of(5);
        assertThrows(
                AssertionError.class,
                () ->
                        BackgroundTabRestorationHelper.restoreRemainingBackgroundTabs(
                                TabOrchestratorType.TABBED,
                                mTabModelSelector,
                                tabIds,
                                /* isAuthoritativeStore= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testRestoreRemainingBackgroundTabs_tabAlreadyInModel() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Tab existingTab = mock(Tab.class);
        when(mNormalTabModel.getTabById(10)).thenReturn(existingTab);

        List<Tab> restoredTabs =
                BackgroundTabRestorationHelper.restoreRemainingBackgroundTabs(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        Set.of(10),
                        /* isAuthoritativeStore= */ true);

        assertTrue(restoredTabs.isEmpty());
        verify(mBackgroundTabPool).removeTabById(10);
        verify(mBackgroundTabPool, never()).loadTabByOriginalId(10);
        verify(mBackgroundTabPool).cleanupPostRestore();
    }
}
