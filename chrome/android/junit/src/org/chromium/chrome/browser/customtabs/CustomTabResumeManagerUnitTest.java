// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.android.util.concurrent.PausedExecutorService;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link CustomTabResumeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabResumeManagerUnitTest {
    private static final int TEST_TAB_ID = 42;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final PausedExecutorService mExecutor = new PausedExecutorService();

    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    private CipherFactory mCipherFactory;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private TabDelegateFactory mTabDelegateFactory;
    @Mock private Tab mTab;
    @Mock private Tab mRestoredTab;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private CustomTabResumeManager mResumeManager;

    @Before
    public void setUp() {
        PostTask.setPrenativeThreadPoolExecutorForTesting(mExecutor);
        mCipherFactory = new CipherFactory();
        setUpTabModelMocks();
        clearStorageDirectory();

        when(mRestoredTab.getUserDataHost()).thenReturn(new UserDataHost());
        TabBuilder.setTabForTesting(mRestoredTab);

        mResumeManager = new CustomTabResumeManager(mIntentDataProvider, mCipherFactory);
    }

    @After
    public void tearDown() {
        mExecutor.runAll();
        TabStateExtractor.resetTabStatesForTesting();
        CustomTabResumeManager.getInMemoryStateCacheForTesting().clear();
        clearStorageDirectory();
    }

    // =========================================================================
    // shouldCreateTabResumeManager tests
    // =========================================================================

    @Test
    public void testShouldCreateTabResumeManager_nullIntentData_returnsFalse() {
        assertFalse(CustomTabResumeManager.shouldCreateTabResumeManager(null));
    }

    @Test
    public void testShouldCreateTabResumeManager_missingEmbedderTabId_returnsFalse() {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn(null);
        assertFalse(CustomTabResumeManager.shouldCreateTabResumeManager(mIntentDataProvider));
    }

    @Test
    public void testShouldCreateTabResumeManager_offTheRecord_returnsFalse() {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("test-session-id");
        when(mIntentDataProvider.isOffTheRecord()).thenReturn(true);
        assertFalse(CustomTabResumeManager.shouldCreateTabResumeManager(mIntentDataProvider));
    }

    @Test
    public void testShouldCreateTabResumeManager_hasEmbedderTabId_returnsTrue() {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("test-session-id");
        when(mIntentDataProvider.isOffTheRecord()).thenReturn(false);
        assertTrue(CustomTabResumeManager.shouldCreateTabResumeManager(mIntentDataProvider));
    }

    // =========================================================================
    // isTabResumptionRequested tests
    // =========================================================================

    @Test
    public void testIsTabResumptionRequested_nullIntentData_returnsFalse() {
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(null));
    }

    @Test
    public void testIsTabResumptionRequested_emptyIntentData_returnsFalse() {
        BrowserServicesIntentDataProvider emptyProvider =
                mockIntentProvider(null, /* restoreTab= */ false);
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(emptyProvider));
    }

    @Test
    public void testIsTabResumptionRequested_hasRestoreTabOnly_returnsFalse() {
        BrowserServicesIntentDataProvider provider =
                mockIntentProvider(null, /* restoreTab= */ true);
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(provider));
    }

    @Test
    public void testIsTabResumptionRequested_hasEmbedderTabIdOnly_returnsFalse() {
        BrowserServicesIntentDataProvider provider =
                mockIntentProvider("test-session-id", /* restoreTab= */ false);
        assertFalse(CustomTabResumeManager.isTabResumptionRequested(provider));
    }

    @Test
    public void testIsTabResumptionRequested_hasRestoreTabAndEmbedderTabId_returnsTrue() {
        BrowserServicesIntentDataProvider provider =
                mockIntentProvider("test-session-id", /* restoreTab= */ true);
        assertTrue(CustomTabResumeManager.isTabResumptionRequested(provider));
    }

    // =========================================================================
    // shouldForceRelaunchForResumption tests
    // =========================================================================

    @Test
    public void testShouldForceRelaunchForResumption_nullIntentData_returnsFalse() {
        assertFalse(
                CustomTabResumeManager.shouldForceRelaunchForResumption(null, mIntentDataProvider));
    }

    @Test
    public void testShouldForceRelaunchForResumption_resumptionRequested_returnsTrue() {
        BrowserServicesIntentDataProvider newProvider =
                mockIntentProvider("test-session-id", /* restoreTab= */ true);

        assertTrue(CustomTabResumeManager.shouldForceRelaunchForResumption(newProvider, null));
    }

    @Test
    public void
            testShouldForceRelaunchForResumption_noResumptionRequested_nullCurrentIntent_returnsFalse() {
        BrowserServicesIntentDataProvider newProvider =
                mockIntentProvider(null, /* restoreTab= */ false);

        assertFalse(CustomTabResumeManager.shouldForceRelaunchForResumption(newProvider, null));
    }

    @Test
    public void testShouldForceRelaunchForResumption_differentSessionIds_returnsTrue() {
        BrowserServicesIntentDataProvider newProvider =
                mockIntentProvider("session-b", /* restoreTab= */ false);
        BrowserServicesIntentDataProvider currentProvider =
                mockIntentProvider("session-a", /* restoreTab= */ false);

        assertTrue(
                CustomTabResumeManager.shouldForceRelaunchForResumption(
                        newProvider, currentProvider));
    }

    @Test
    public void testShouldForceRelaunchForResumption_sameSessionIds_returnsFalse() {
        BrowserServicesIntentDataProvider newProvider =
                mockIntentProvider("session-a", /* restoreTab= */ false);
        BrowserServicesIntentDataProvider currentProvider =
                mockIntentProvider("session-a", /* restoreTab= */ false);

        assertFalse(
                CustomTabResumeManager.shouldForceRelaunchForResumption(
                        newProvider, currentProvider));
    }

    // =========================================================================
    // maybeRestoreTab tests
    // =========================================================================

    @Test
    public void testMaybeRestoreTab_noResumptionRequested_returnsNull() {
        when(mIntentDataProvider.isRestoreTabRequested()).thenReturn(false);
        assertNull(
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        /* isOffTheRecord= */ false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id"));
    }

    @Test
    public void testMaybeRestoreTab_cacheHit() {
        when(mIntentDataProvider.isRestoreTabRequested()).thenReturn(true);
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-123");

        // Seed the static cache.
        TabState state = createValidTabState(1000L);
        CustomTabResumeManager.getInMemoryStateCacheForTesting().put("session-123", state);

        Tab restoredTab =
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        /* isOffTheRecord= */ false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id");

        assertSame(mRestoredTab, restoredTab);
        assertEquals("app-id", TabAssociatedApp.getAppId(restoredTab));
    }

    @Test
    public void testMaybeRestoreTab_restoreFromDisk() throws Exception {
        when(mIntentDataProvider.isRestoreTabRequested()).thenReturn(true);
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-456");

        // Create and save a TabState to disk.
        TabState state = createValidTabState(2000L);
        File storageDir = ensureStorageDirectory();
        int tabId = Math.max(1, "session-456".hashCode() & 0x7fffffff);
        TabStateFileManager.saveState(
                storageDir, state, tabId, /* isEncrypted= */ true, mCipherFactory);

        // Try to restore it.
        Tab restoredTab =
                mResumeManager.maybeRestoreTab(
                        mCipherFactory,
                        mProfile,
                        mTabModelSelector,
                        /* isOffTheRecord= */ false,
                        mWindowAndroid,
                        mTabDelegateFactory,
                        "app-id");

        assertSame(mRestoredTab, restoredTab);
        assertEquals("app-id", TabAssociatedApp.getAppId(restoredTab));
        assertEquals(
                state.timestampMillis,
                CustomTabResumeManager.getInMemoryStateCacheForTesting()
                        .get("session-456")
                        .timestampMillis);
    }

    // =========================================================================
    // registerTabIfResumptionEnabled & saveTabStateDeferred tests
    // =========================================================================

    @Test
    public void testRegisterTabIfResumptionEnabled() {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-123");

        mResumeManager.registerTabIfResumptionEnabled(mTab);

        verify(mTab).addObserver(any());
    }

    @Test
    public void testDestroy_removesObserverFromRegisteredTabs() {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-123");

        mResumeManager.registerTabIfResumptionEnabled(mTab);
        mResumeManager.destroy();

        verify(mTab).removeObserver(any());
    }

    @Test
    public void testOnActivityAttachmentChanged_nullWindowUnregistersObserver() {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-123");
        TabStateExtractor.setTabStateForTesting(TEST_TAB_ID, new TabState());

        mResumeManager.registerTabIfResumptionEnabled(mTab);
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        observer.onActivityAttachmentChanged(mTab, /* window= */ null);
        verify(mTab).removeObserver(observer);

        observer.onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        assertFalse(
                CustomTabResumeManager.getInMemoryStateCacheForTesting()
                        .containsKey("session-123"));
    }

    @Test
    public void testSaveTabStateDeferred_nullLimits_usesDefaults() throws Exception {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-default");
        when(mIntentDataProvider.getMaxResumptionTabs()).thenReturn(null);
        when(mIntentDataProvider.getResumptionTabTtlMs()).thenReturn(null);

        TabStateExtractor.setTabStateForTesting(TEST_TAB_ID, createValidTabState(3000L));

        File storageDir = ensureStorageDirectory();
        long now = System.currentTimeMillis();
        File expiredFile =
                createSeedFile(
                        storageDir,
                        "expired_state",
                        now
                                - CustomTabResumeManager.DEFAULT_RESUMPTION_TAB_TTL_MS
                                - TimeUnit.HOURS.toMillis(2));
        File oldestQuotaFile =
                createSeedFile(
                        storageDir,
                        "oldest_quota_state",
                        now - TimeUnit.DAYS.toMillis(4) - TimeUnit.HOURS.toMillis(2));
        File fourDayOldFile =
                createSeedFile(storageDir, "four_day_old_state", now - TimeUnit.DAYS.toMillis(4));
        for (int i = 0; i < CustomTabResumeManager.DEFAULT_MAX_RESUMPTION_TABS - 2; i++) {
            createSeedFile(storageDir, "recent_state_" + i, now - TimeUnit.HOURS.toMillis(1));
        }

        ArgumentCaptor<TabObserver> observerCaptor = ArgumentCaptor.forClass(TabObserver.class);
        mResumeManager.registerTabIfResumptionEnabled(mTab);
        verify(mTab).addObserver(observerCaptor.capture());

        observerCaptor.getValue().onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        mExecutor.runAll();

        verify(mIntentDataProvider).getMaxResumptionTabs();
        verify(mIntentDataProvider).getResumptionTabTtlMs();
        assertNotNull(
                CustomTabResumeManager.getInMemoryStateCacheForTesting().get("session-default"));
        assertFalse(expiredFile.exists());
        assertFalse(oldestQuotaFile.exists());
        assertTrue(fourDayOldFile.exists());
        File[] remainingFiles = storageDir.listFiles();
        assertNotNull(remainingFiles);
        assertEquals(CustomTabResumeManager.DEFAULT_MAX_RESUMPTION_TABS, remainingFiles.length);
    }

    @Test
    public void testSaveTabStateDeferred_customLimits_usesProvidedValues() throws Exception {
        when(mIntentDataProvider.getEmbedderTabId()).thenReturn("session-custom");
        when(mIntentDataProvider.getMaxResumptionTabs()).thenReturn(2);
        when(mIntentDataProvider.getResumptionTabTtlMs()).thenReturn(TimeUnit.HOURS.toMillis(1));

        TabStateExtractor.setTabStateForTesting(TEST_TAB_ID, createValidTabState(4000L));

        File storageDir = ensureStorageDirectory();
        long now = System.currentTimeMillis();
        File expiredByCustomTtlFile =
                createSeedFile(storageDir, "expired_custom_ttl", now - TimeUnit.HOURS.toMillis(2));
        File oldestValidFile =
                createSeedFile(storageDir, "oldest_valid", now - TimeUnit.MINUTES.toMillis(30));
        File newerValidFile =
                createSeedFile(storageDir, "newer_valid", now - TimeUnit.MINUTES.toMillis(10));

        ArgumentCaptor<TabObserver> observerCaptor = ArgumentCaptor.forClass(TabObserver.class);
        mResumeManager.registerTabIfResumptionEnabled(mTab);
        verify(mTab).addObserver(observerCaptor.capture());

        observerCaptor.getValue().onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);
        mExecutor.runAll();

        verify(mIntentDataProvider).getMaxResumptionTabs();
        verify(mIntentDataProvider).getResumptionTabTtlMs();
        assertNotNull(
                CustomTabResumeManager.getInMemoryStateCacheForTesting().get("session-custom"));
        assertFalse(expiredByCustomTtlFile.exists());
        assertFalse(oldestValidFile.exists());
        assertTrue(newerValidFile.exists());
        File[] remainingFiles = storageDir.listFiles();
        assertNotNull(remainingFiles);
        assertEquals(2, remainingFiles.length);
    }

    // =========================================================================
    // Private Setup & Mocking Helpers
    // =========================================================================

    private void setUpTabModelMocks() {
        when(mTabModelSelector.getModel(any(Boolean.class))).thenReturn(mTabModel);
        TabRemover tabRemover = mock(TabRemover.class);
        when(mTabModel.getTabRemover()).thenReturn(tabRemover);
        when(mTab.getId()).thenReturn(TEST_TAB_ID);
    }

    private BrowserServicesIntentDataProvider mockIntentProvider(
            String embedderTabId, boolean restoreTab) {
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        when(provider.getEmbedderTabId()).thenReturn(embedderTabId);
        when(provider.isRestoreTabRequested()).thenReturn(restoreTab);
        return provider;
    }

    private static TabState createValidTabState(long timestampMillis) {
        TabState state = new TabState();
        state.timestampMillis = timestampMillis;
        state.url = GURL.emptyGURL();
        state.contentsState =
                new WebContentsState(
                        ByteBuffer.allocateDirect(10),
                        WebContentsState.CONTENTS_STATE_CURRENT_VERSION);
        return state;
    }

    private static File ensureStorageDirectory() {
        File storageDir =
                new File(ContextUtils.getApplicationContext().getFilesDir(), "cct_tab_data");
        if (!storageDir.exists()) {
            assertTrue(storageDir.mkdirs());
        }
        return storageDir;
    }

    private static void clearStorageDirectory() {
        File storageDir =
                new File(ContextUtils.getApplicationContext().getFilesDir(), "cct_tab_data");
        File[] files = storageDir.listFiles();
        if (files != null) {
            for (File file : files) {
                file.delete();
            }
        }
    }

    private static File createSeedFile(File dir, String name, long lastModifiedMs)
            throws Exception {
        File file = new File(dir, name);
        assertTrue(file.createNewFile());
        assertTrue(file.setLastModified(lastModifiedMs));
        return file;
    }
}
