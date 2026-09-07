// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.app.tabmodel.TabCacheManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;

import java.nio.ByteBuffer;

/** Unit tests for {@link BackgroundTabPoolManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BackgroundTabPoolManagerTest {
    private static final @TabId int TAB_ID_1 = 101;
    private static final @TabId int PLACEHOLDER_ID = 999;

    public final @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock ProfileResolver.Natives mProfileResolverNatives;
    private @Mock Profile mProfile;

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
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
        TabStateExtractor.resetTabStatesForTesting();
    }

    @Test
    public void testAcquireCreatesPoolAndIncrementsLease() {
        BackgroundTabPool pool1 = BackgroundTabPoolManager.acquire(mProfile);
        assertNotNull(pool1);
        assertEquals(1, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
        assertSame(pool1, BackgroundTabPoolManager.getPoolForTesting(mProfile));
        assertEquals(
                "mock_token",
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN, null));

        BackgroundTabPool pool2 = BackgroundTabPoolManager.acquire(mProfile);
        assertSame(pool1, pool2);
        assertEquals(2, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
    }

    @Test
    public void testAcquireWithProfileTokenDirectly() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire("custom_token");
        assertNotNull(pool);
        assertEquals("custom_token", pool.getProfileToken());

        BackgroundTabPool poolAgain = BackgroundTabPoolManager.acquire("custom_token");
        assertSame(pool, poolAgain);
        BackgroundTabPoolManager.release(pool);
        BackgroundTabPoolManager.release(poolAgain);
    }

    @Test
    public void testRestorePoolIfTokenExists() {
        assertNull(BackgroundTabPoolManager.restorePoolIfTokenExists());

        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN,
                        "persisted_token");

        BackgroundTabPool restored = BackgroundTabPoolManager.restorePoolIfTokenExists();
        assertNotNull(restored);
        assertEquals("persisted_token", restored.getProfileToken());

        BackgroundTabPoolManager.release(restored);
    }

    @Test
    public void testClearLastUsedProfileToken() {
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN,
                        "persisted_token");
        BackgroundTabPoolManager.clearLastUsedProfileToken();
        assertNull(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN, null));
    }

    @Test
    public void testReleaseDecrementsLeaseAndDestroysWhenEmpty() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        assertEquals(1, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));

        BackgroundTabPoolManager.release(pool);
        assertEquals(0, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
        assertNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
    }

    @Test
    public void testReleaseDoesNotDestroyWhenHoldingTabs() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        pool.addLiveTab(new LiveBackgroundTab(pool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        assertFalse(pool.isEmpty());

        BackgroundTabPoolManager.release(pool);
        assertEquals(0, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
        assertNotNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
        assertSame(pool, BackgroundTabPoolManager.getPoolForTesting(mProfile));
    }

    @Test
    public void testReleaseDoesNotDestroyOrClearTokenWhenColdTabsExist() {
        BackgroundTabPool pool1 = new BackgroundTabPool("mock_token", () -> {});
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        pool1.addLiveTab(new LiveBackgroundTab(pool1, tab, PLACEHOLDER_ID, /* taskId= */ null));
        // Destroy the pool directly so tab state remains persisted in cache
        pool1.destroy();

        // Acquire new pool for same profile - it has cold tabs in cache
        BackgroundTabPool pool2 = BackgroundTabPoolManager.acquire(mProfile);
        assertFalse(pool2.isEmpty());
        assertFalse(pool2.getAllTabIds().isEmpty());

        BackgroundTabPoolManager.release(pool2);
        // Lease count is 0, but cached tabs exist, so pool is retained and token is not cleared
        assertEquals(0, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
        assertNotNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
        assertEquals(
                "mock_token",
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN, null));

        // When cache is cleared, pool is emptied and destroyed, and token is cleared
        pool2.clearAll();
        ShadowLooper.idleMainLooper();
        assertNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
        assertNull(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.BACKGROUND_TAB_POOL_LAST_PROFILE_TOKEN, null));
    }

    @Test
    public void testPoolDestroyedWhenLastTabRemovedAfterRelease() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        pool.addLiveTab(new LiveBackgroundTab(pool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        BackgroundTabPoolManager.release(pool);
        assertNotNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));

        pool.removeTab(TAB_ID_1);
        assertTrue(pool.isEmpty());
        ShadowLooper.idleMainLooper();
        assertNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
    }

    @Test
    public void testPoolNotDestroyedWhenEmptyIfLeaseCountGreaterThanZero() {
        BackgroundTabPool pool1 = BackgroundTabPoolManager.acquire(mProfile);
        BackgroundTabPool pool2 = BackgroundTabPoolManager.acquire(mProfile);
        assertEquals(2, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
        assertTrue(pool1.isEmpty());

        BackgroundTabPoolManager.release(pool1);
        assertEquals(1, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
        assertNotNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
        assertSame(pool1, BackgroundTabPoolManager.getPoolForTesting(mProfile));
    }

    @Test
    public void testDestroyOnProfileTeardown() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        assertNotNull(pool);
        assertEquals(1, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));

        BackgroundTabPoolManager.resetForTesting();
        assertNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
        assertEquals(0, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));
    }

    @Test
    public void testSetPoolForTesting() {
        BackgroundTabPool mockPool = mock(BackgroundTabPool.class);
        BackgroundTabPoolManager.setPoolForTesting(mockPool);

        BackgroundTabPool acquired = BackgroundTabPoolManager.acquire(mProfile);
        assertSame(mockPool, acquired);

        BackgroundTabPoolManager.release(mockPool);
        assertSame(mockPool, BackgroundTabPoolManager.acquire(mProfile));
    }

    @Test(expected = AssertionError.class)
    public void testOffTheRecordProfileThrowsAssertion() {
        Profile otrProfile = mock(Profile.class);
        when(otrProfile.isOffTheRecord()).thenReturn(true);
        BackgroundTabPoolManager.acquire(otrProfile);
    }

    @Test
    public void testUnbalancedReleaseThrowsAssertion() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        BackgroundTabPoolManager.release(pool);
        assertEquals(0, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));

        assertThrows(AssertionError.class, () -> BackgroundTabPoolManager.release(pool));
    }

    @Test
    public void testPoolSurvivesReacquireBeforeAsynchronousEmptyTaskRuns() {
        BackgroundTabPool pool = BackgroundTabPoolManager.acquire(mProfile);
        Tab tab = createMockTab(TAB_ID_1);
        TabState tabState = createMockTabState();
        TabStateExtractor.setTabStateForTesting(TAB_ID_1, tabState);

        pool.addLiveTab(new LiveBackgroundTab(pool, tab, PLACEHOLDER_ID, /* taskId= */ null));
        BackgroundTabPoolManager.release(pool);

        // Removing the tab posts onEmptyCallback asynchronously to UI Looper
        pool.removeTab(TAB_ID_1);
        assertNotNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));

        // Re-acquire before the posted task drains
        BackgroundTabPool reacquired = BackgroundTabPoolManager.acquire(mProfile);
        assertSame(pool, reacquired);
        assertEquals(1, BackgroundTabPoolManager.getLeaseCountForTesting(mProfile));

        // Drain looper - pool should survive because lease count is 1
        ShadowLooper.idleMainLooper();
        assertNotNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));

        BackgroundTabPoolManager.release(reacquired);
        assertNull(BackgroundTabPoolManager.getPoolForTesting(mProfile));
    }

    private Tab createMockTab(@TabId int tabId) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(tabId);
        when(tab.isOffTheRecord()).thenReturn(false);
        when(tab.hasParentCollection()).thenReturn(false);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        return tab;
    }

    private TabState createMockTabState() {
        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        ByteBuffer buffer = ByteBuffer.allocateDirect(10);
        when(tabState.contentsState.buffer()).thenReturn(buffer);
        when(tabState.contentsState.version()).thenReturn(2);
        return tabState;
    }
}
