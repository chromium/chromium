// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
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
                        TabOrchestratorType.TABBED, /* isIncognito= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_flagDisabled() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED, /* isIncognito= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_notificationsDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED, /* isIncognito= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_incognito() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.TABBED, /* isIncognito= */ true));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testShouldIntercept_nonTabbedOrchestrator() {
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.CUSTOM, /* isIncognito= */ false));
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.ARCHIVED, /* isIncognito= */ false));
        assertFalse(
                BackgroundTabRestorationHelper.shouldIntercept(
                        TabOrchestratorType.HEADLESS, /* isIncognito= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_nullSelector() {
        assertNull(BackgroundTabRestorationHelper.acquirePool(null));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testAcquirePool_nullModel() {
        when(mTabModelSelector.getModel(false)).thenReturn(null);
        assertNull(BackgroundTabRestorationHelper.acquirePool(mTabModelSelector));
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
                        TabOrchestratorType.TABBED, mTabModelSelector, /* isIncognito= */ false);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_incognito() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED, mTabModelSelector, /* isIncognito= */ true);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_nonTabbedOrchestrator() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.CUSTOM, mTabModelSelector, /* isIncognito= */ false);
        assertTrue(ids.isEmpty());
        verify(mBackgroundTabPool, never()).getAllTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_poolReturnsIds() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.getAllTabIds()).thenReturn(Set.of(1, 2, 3));

        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED, mTabModelSelector, /* isIncognito= */ false);
        assertEquals(3, ids.size());
        assertTrue(ids.contains(1));
        assertTrue(ids.contains(2));
        assertTrue(ids.contains(3));
        verify(mBackgroundTabPool).getAllTabIds();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testFetchBackgroundTabIds_poolAcquireFails() {
        when(mTabModelSelector.getModel(false)).thenReturn(null);
        Set<Integer> ids =
                BackgroundTabRestorationHelper.fetchBackgroundTabIds(
                        TabOrchestratorType.TABBED, mTabModelSelector, /* isIncognito= */ false);
        assertTrue(ids.isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_success_destroysPlaceholderContentsState() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.loadTab(TAB_ID, TAB_ID)).thenReturn(mBackgroundPoolTab);
        when(mBackgroundPoolTab.attachTab(eq(mNormalTabModel), eq(DESTINATION_INDEX)))
                .thenReturn(mTab);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState);

        assertEquals(mTab, restoredTab);
        verify(mBackgroundTabPool).loadTab(TAB_ID, TAB_ID);
        verify(mBackgroundPoolTab).attachTab(eq(mNormalTabModel), eq(DESTINATION_INDEX));
        verify(mWebContentsState).destroy();
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
                        tabState);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTab(anyInt(), anyInt());
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
                        tabState);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTab(anyInt(), anyInt());
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
                        tabState);

        assertNull(restoredTab);
        verify(mBackgroundTabPool, never()).loadTab(anyInt(), anyInt());
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_tabNotFoundInPool() {
        BackgroundTabPoolManager.setPoolForTesting(mBackgroundTabPool);
        when(mBackgroundTabPool.loadTab(TAB_ID, TAB_ID)).thenReturn(null);

        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;

        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        tabState);

        assertNull(restoredTab);
        verify(mBackgroundTabPool).loadTab(TAB_ID, TAB_ID);
        verify(mWebContentsState, never()).destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_nullSelector() {
        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED, null, TAB_ID, DESTINATION_INDEX, null);
        assertNull(restoredTab);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testMaybeRestoreBackgroundTab_nullModel() {
        when(mTabModelSelector.getModel(false)).thenReturn(null);
        Tab restoredTab =
                BackgroundTabRestorationHelper.maybeRestoreBackgroundTab(
                        TabOrchestratorType.TABBED,
                        mTabModelSelector,
                        TAB_ID,
                        DESTINATION_INDEX,
                        null);
        assertNull(restoredTab);
    }
}
