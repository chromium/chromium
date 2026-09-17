// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
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
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.LeaseReason;
import org.chromium.chrome.browser.app.tabmodel.TabStateStore.TabStateStoreCleaner;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl.TabPersistentStoreImplCleaner;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link PersistentStoreCleaner}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE,
    ChromeFeatureList.SCHEDULE_WINDOW_CLEANING
})
public class PersistentStoreCleanerUnitTest {
    private static final int ARCHIVED_TAB_ID = 10;
    private static final int TAB_ID = 11;
    private static final int CUSTOM_TAB_ID = 12;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelOrchestrator mOrchestrator;
    @Mock private TabModelSelectorBase mSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private TabPersistentStoreImplCleaner mLegacyStoreCleaner;
    @Mock private TabStateStoreCleaner mTabStateStoreCleaner;
    @Mock private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    @Mock private Destroyable mLease;

    @Mock private TabContentManager mTabContentManager;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabModelSelector mArchivedTabModelSelector;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModelSelector mCustomTabModelSelector;
    @Mock private TabModel mArchivedTabModel;
    @Mock private Tab mArchivedTab;
    @Mock private Tab mTab;
    @Mock private TabModel mCustomTabModel;
    @Mock private Tab mCustomTab;
    @Mock private TabStateStorageService mTabStateStorageService;

    @Captor private ArgumentCaptor<TabWindowManager.Observer> mObserverCaptor;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mSelectorObserverCaptor;
    @Captor private ArgumentCaptor<int[]> mTabIdsCaptor;
    @Captor private ArgumentCaptor<List<String>> mWindowTagsCaptor;

    private PersistentStoreCleaner mCleaner;

    @Before
    public void setUp() {
        when(mOrchestrator.getTabModelSelector()).thenReturn(mSelector);
        when(mSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
        when(mArchivedTabModelOrchestrator.acquireLeaseInternal(any())).thenReturn(mLease);
        when(mArchivedTabModelOrchestrator.isTabModelInitialized()).thenReturn(true);

        mCleaner = new PersistentStoreCleaner(mProfile, mTabStateStoreCleaner, mLegacyStoreCleaner);

        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        when(mTabContentManager.isDestroyed()).thenReturn(false);

        when(mTabWindowManager.getArchivedTabModelSelector()).thenReturn(mArchivedTabModelSelector);
        when(mTabWindowManager.getAllTabModelSelectors())
                .thenReturn(Collections.singletonList(mTabModelSelector));
        when(mTabWindowManager.getCustomTabsTabModelSelectors())
                .thenReturn(Collections.singletonList(mCustomTabModelSelector));

        when(mTabWindowManager.getWindowIdForSelector(mTabModelSelector)).thenReturn(1);
        when(mTabWindowManager.getTaskIdForCustomTab(mCustomTabModelSelector)).thenReturn(2);

        when(mArchivedTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mCustomTabModelSelector.isTabStateInitialized()).thenReturn(true);

        when(mArchivedTabModelSelector.getModels())
                .thenReturn(Collections.singletonList(mArchivedTabModel));
        when(mTabModelSelector.getModels()).thenReturn(Collections.singletonList(mTabModel));
        when(mCustomTabModelSelector.getModels())
                .thenReturn(Collections.singletonList(mCustomTabModel));

        when(mArchivedTab.getId()).thenReturn(ARCHIVED_TAB_ID);
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mCustomTab.getId()).thenReturn(CUSTOM_TAB_ID);

        when(mArchivedTabModel.iterator())
                .thenReturn(Collections.singletonList(mArchivedTab).iterator());
        when(mTabModel.iterator()).thenReturn(Collections.singletonList(mTab).iterator());
        when(mCustomTabModel.iterator())
                .thenReturn(Collections.singletonList(mCustomTab).iterator());

        TabStateStorageServiceFactory.setForTesting(mTabStateStorageService);
    }

    @After
    public void tearDown() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(null);
        new TabArchiveSettings(ChromeSharedPreferences.getInstance()).resetSettingsForTesting();
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
    }

    @Test
    public void testScheduleCleanUnusedData_AllInitialized() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager).removeAllTabThumbnailsExceptForIds(mTabIdsCaptor.capture());
        assertArrayEquals(
                new int[] {ARCHIVED_TAB_ID, TAB_ID, CUSTOM_TAB_ID}, mTabIdsCaptor.getValue());

        verify(mTabStateStorageService).clearAllWindowsExcept(mWindowTagsCaptor.capture());
        List<String> windowTags = mWindowTagsCaptor.getValue();
        assertArrayEquals(
                new String[] {TabWindowManager.ARCHIVED_WINDOW_TAG, "1", "2"},
                windowTags.toArray());
    }

    @Test
    public void testScheduleCleanUnusedData_NotAllInitialized() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(false);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        verify(mArchivedTabModelOrchestrator, never()).acquireLeaseInternal(any());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mTabStateStorageService, never()).clearAllWindowsExcept(any());

        verify(mTabWindowManager).addObserver(mObserverCaptor.capture());
        TabWindowManager.Observer observer = mObserverCaptor.getValue();
        observer.onAllTabModelStateInitialized();

        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        verify(mArchivedTabModelOrchestrator)
                .acquireLeaseInternal(eq(LeaseReason.PERSISTENT_STORE_CLEANER));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager).removeAllTabThumbnailsExceptForIds(mTabIdsCaptor.capture());
        assertArrayEquals(
                new int[] {ARCHIVED_TAB_ID, TAB_ID, CUSTOM_TAB_ID}, mTabIdsCaptor.getValue());

        verify(mTabStateStorageService).clearAllWindowsExcept(mWindowTagsCaptor.capture());
        List<String> windowTags = mWindowTagsCaptor.getValue();
        assertArrayEquals(
                new String[] {TabWindowManager.ARCHIVED_WINDOW_TAG, "1", "2"},
                windowTags.toArray());

        verify(mLease).destroy();
        verify(mTabWindowManager).removeObserver(observer);
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
    }

    @Test
    public void testCleanUnusedDeps_RemovesTabWindowManagerObserver() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(false);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());

        verify(mTabWindowManager).addObserver(mObserverCaptor.capture());
        TabWindowManager.Observer observer = mObserverCaptor.getValue();

        when(mTabContentManager.isDestroyed()).thenReturn(true);
        observer.onAllTabModelStateInitialized();

        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabWindowManager).removeObserver(observer);
        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testScheduleCleanUnusedData_TabStorageDisabled() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager).removeAllTabThumbnailsExceptForIds(mTabIdsCaptor.capture());
        assertArrayEquals(
                new int[] {ARCHIVED_TAB_ID, TAB_ID, CUSTOM_TAB_ID}, mTabIdsCaptor.getValue());

        verify(mTabStateStorageService, never()).clearAllWindowsExcept(any());
    }

    @Test
    public void testCleanUnusedWindows_ManagerDestroyed() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabContentManager.isDestroyed()).thenReturn(true);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
    }

    @Test
    public void testCleanUnusedWindows_ArchivedSelectorNull() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabWindowManager.getArchivedTabModelSelector()).thenReturn(null);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());

        verify(mTabStateStorageService).clearAllWindowsExcept(mWindowTagsCaptor.capture());
        List<String> windowTags = mWindowTagsCaptor.getValue();
        assertArrayEquals(
                new String[] {TabWindowManager.ARCHIVED_WINDOW_TAG, "1", "2"},
                windowTags.toArray());
        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
    }

    @Test
    public void testCleanUnusedWindows_SelectorsNotInitialized_RetainsLeaseUntilInit() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mLease, never()).destroy();
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());

        // Verify observer was registered on the uninitialized selector.
        verify(mTabModelSelector, atLeastOnce()).addObserver(mSelectorObserverCaptor.capture());

        // Now initialize the selector and notify observer.
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        for (TabModelSelectorObserver observer : mSelectorObserverCaptor.getAllValues()) {
            observer.onTabStateInitialized();
        }
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager).removeAllTabThumbnailsExceptForIds(any());
        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
    }

    @Test
    public void testCleanUnusedWindows_SelectorDestroyedBeforeInit_AbortsAndReleasesLease() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabWindowManager.getArchivedTabModelSelector()).thenReturn(null);
        when(mTabWindowManager.getCustomTabsTabModelSelectors())
                .thenReturn(Collections.emptyList());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabModelSelector, atLeastOnce()).addObserver(mSelectorObserverCaptor.capture());
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(Collections.emptyList());
        for (TabModelSelectorObserver observer : mSelectorObserverCaptor.getAllValues()) {
            observer.onDestroyed();
        }
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mTabStateStorageService, never()).clearAllWindowsExcept(any());
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
        verify(mLease).destroy();
    }

    @Test
    public void
            testCleanUnusedWindows_SelectorDestroyedBeforeInit_OtherSelectorsRemain_ProceedsWithCleanup() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mLease, never()).destroy();

        verify(mTabModelSelector, atLeastOnce()).addObserver(mSelectorObserverCaptor.capture());
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(Collections.emptyList());
        for (TabModelSelectorObserver observer : mSelectorObserverCaptor.getAllValues()) {
            observer.onDestroyed();
        }
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager).removeAllTabThumbnailsExceptForIds(mTabIdsCaptor.capture());
        assertArrayEquals(new int[] {ARCHIVED_TAB_ID, CUSTOM_TAB_ID}, mTabIdsCaptor.getValue());

        verify(mTabStateStorageService).clearAllWindowsExcept(mWindowTagsCaptor.capture());
        List<String> windowTags = mWindowTagsCaptor.getValue();
        assertArrayEquals(
                new String[] {TabWindowManager.ARCHIVED_WINDOW_TAG, "2"}, windowTags.toArray());

        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
        verify(mLease).destroy();
    }

    @Test
    public void testScheduleCleanUnusedData_ZeroArchivedTabs_NotInstantiated() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(/* instance= */ null);
        new TabArchiveSettings(ChromeSharedPreferences.getInstance()).resetSettingsForTesting();
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager).removeAllTabThumbnailsExceptForIds(mTabIdsCaptor.capture());
        assertArrayEquals(
                new int[] {ARCHIVED_TAB_ID, TAB_ID, CUSTOM_TAB_ID}, mTabIdsCaptor.getValue());

        verify(mTabStateStorageService).clearAllWindowsExcept(mWindowTagsCaptor.capture());
        List<String> windowTags = mWindowTagsCaptor.getValue();
        assertArrayEquals(
                new String[] {TabWindowManager.ARCHIVED_WINDOW_TAG, "1", "2"},
                windowTags.toArray());
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
    }

    @Test
    public void testScheduleCleanUnusedData_HasArchivedTabs_AcquiresAndReleasesLease() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
        new TabArchiveSettings(ChromeSharedPreferences.getInstance())
                .setArchivedTabCount(/* count= */ 3);
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        verify(mArchivedTabModelOrchestrator)
                .acquireLeaseInternal(eq(LeaseReason.PERSISTENT_STORE_CLEANER));
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
    }

    @Test
    public void
            testScheduleCleanUnusedData_InstantiatedLater_AcquiresLeaseInMaybeCleanUnusedWindows() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(/* instance= */ null);
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(false);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());

        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
        verify(mTabWindowManager).addObserver(mObserverCaptor.capture());
        TabWindowManager.Observer observer = mObserverCaptor.getValue();
        observer.onAllTabModelStateInitialized();

        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());
        verify(mArchivedTabModelOrchestrator)
                .acquireLeaseInternal(eq(LeaseReason.PERSISTENT_STORE_CLEANER));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mLease).destroy();
        verify(mTabWindowManager).removeObserver(observer);
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
    }

    @Test
    public void
            testScheduleCleanUnusedData_HasArchivedTabs_ArchivedSelectorNull_SkipsThumbnailCleanup() {
        ArchivedTabModelOrchestrator.setInstanceForTesting(mArchivedTabModelOrchestrator);
        new TabArchiveSettings(ChromeSharedPreferences.getInstance())
                .setArchivedTabCount(/* count= */ 3);
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabWindowManager.getArchivedTabModelSelector()).thenReturn(null);

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        assertNotNull(mCleaner.getArchivedTabsLeaseForTesting());

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mTabStateStorageService).clearAllWindowsExcept(mWindowTagsCaptor.capture());
        List<String> windowTags = mWindowTagsCaptor.getValue();
        assertArrayEquals(
                new String[] {TabWindowManager.ARCHIVED_WINDOW_TAG, "1", "2"},
                windowTags.toArray());
        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
    }

    @Test
    public void testCleanUnusedWindows_EmptySelectors_AbortsWithoutDeletingData() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabWindowManager.getArchivedTabModelSelector()).thenReturn(null);
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(Collections.emptyList());
        when(mTabWindowManager.getCustomTabsTabModelSelectors())
                .thenReturn(Collections.emptyList());

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mTabStateStorageService, never()).clearAllWindowsExcept(any());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
    }

    @Test
    public void testCleanUnusedWindows_OnlyArchivedSelector_AbortsWithoutDeletingData() {
        when(mTabWindowManager.isAllTabStateInitialized()).thenReturn(true);
        when(mTabWindowManager.getArchivedTabModelSelector()).thenReturn(mArchivedTabModelSelector);
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(Collections.emptyList());
        when(mTabWindowManager.getCustomTabsTabModelSelectors())
                .thenReturn(Collections.emptyList());

        mCleaner.scheduleCleanUnusedData(mTabContentManager);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabContentManager, never()).removeAllTabThumbnailsExceptForIds(any());
        verify(mTabStateStorageService, never()).clearAllWindowsExcept(any());
        verify(mLease).destroy();
        assertNull(mCleaner.getArchivedTabsLeaseForTesting());
        assertFalse(mCleaner.hasUnusedDataDepsForTesting());
    }

    @Test
    public void testCleanWindow_BothStoresActive() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.LEGACY);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.TAB_STATE_STORE);

        mCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner, never()).cleanupStateFile(anyInt(), any(), any(), any());
        verify(mTabStateStoreCleaner, never()).cleanupStateFile(anyInt(), any());
    }

    @Test
    public void testCleanWindow_LegacyOnly() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.LEGACY);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        mCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner, never()).cleanupStateFile(anyInt(), any(), any(), any());
        verify(mTabStateStoreCleaner).cleanupStateFile(eq(1), eq(mProfile));
    }

    @Test
    public void testCleanWindow_TabStateStoreOnly() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.TAB_STATE_STORE);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        mCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner).cleanupStateFile(eq(1), any(), any(), any());
        verify(mTabStateStoreCleaner, never()).cleanupStateFile(anyInt(), any());
    }

    @Test
    public void testClearState_BothStoresActive() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.LEGACY);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.TAB_STATE_STORE);

        mCleaner.clearState(mOrchestrator);

        verify(mLegacyStoreCleaner, never()).clearState(any(), any());
        verify(mTabStateStoreCleaner, never()).clearState(any());
    }

    @Test
    public void testClearState_NeitherStoreActive() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.INVALID);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        mCleaner.clearState(mOrchestrator);

        verify(mLegacyStoreCleaner).clearState(any(), any());
        verify(mTabStateStoreCleaner).clearState(eq(mProfile));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testCleanWindow_TabStateStoreDisabled() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.INVALID);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        mCleaner.cleanWindowForUnavailableStores(1, mOrchestrator);

        verify(mLegacyStoreCleaner).cleanupStateFile(eq(1), any(), any(), any());
        verify(mTabStateStoreCleaner, never()).cleanupStateFile(anyInt(), any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
    public void testClearState_TabStateStoreDisabled() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.INVALID);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        mCleaner.clearState(mOrchestrator);

        verify(mLegacyStoreCleaner).clearState(any(), any());
        verify(mTabStateStoreCleaner, never()).clearState(any());
    }

    @Test
    public void testClearState_TabStateStoreOnly() {
        when(mOrchestrator.getAuthoritativeStoreType()).thenReturn(StoreType.TAB_STATE_STORE);
        when(mOrchestrator.getShadowStoreType()).thenReturn(StoreType.INVALID);

        mCleaner.clearState(mOrchestrator);

        verify(mLegacyStoreCleaner).clearState(any(), any());
        verify(mTabStateStoreCleaner, never()).clearState(any());
    }
}
