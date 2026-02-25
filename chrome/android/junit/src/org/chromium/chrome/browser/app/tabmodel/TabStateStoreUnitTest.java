// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.StorageLoadingStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabStateStore}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabStateStoreUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String WINDOW_TAG = "window_1";
    private static final int CLEANUP_WINDOW_ID = 123;
    private static final String CLEANUP_WINDOW_TAG = String.valueOf(CLEANUP_WINDOW_ID);

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mRegularTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Profile mProfile;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabCreator mTabCreator;
    @Mock private TabPersistencePolicy mTabPersistencePolicy;
    @Mock private PersistentStoreMigrationManager mMigrationManager;
    @Mock private CipherFactory mCipherFactory;
    @Mock private TabStateStorageService mTabStateStorageService;
    @Mock private TabPersistentStoreObserver mObserver;
    @Mock private ModelTrackingOrchestrator mModelTrackingOrchestrator;
    @Mock private TabCountTracker mTabCountTracker;
    @Mock private StorageLoadedData mRegularData;
    @Mock private StorageLoadedData mIncognitoData;
    @Mock private TabList mComprehensiveTabList;
    @Captor private ArgumentCaptor<Callback<StorageLoadedData>> mCallbackCaptor;

    private final ModelTrackingOrchestrator.Factory mFactory =
            (a, b, c, d, e) -> mModelTrackingOrchestrator;
    private final SettableNullableObservableSupplier<Tab> mRegularTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Tab> mIncognitoTabSupplier =
            ObservableSuppliers.createNullable();
    private TabStateStore mTabStateStore;

    private Tab createMockTabWithParentCollection(int id, Profile profile) {
        return new MockTab(id, profile) {
            @Override
            public boolean hasParentCollection() {
                return true;
            }

            @Override
            public boolean isInitialized() {
                return true;
            }
        };
    }

    @Before
    public void setUp() throws Exception {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mRegularTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        TabStateStorageServiceFactory.setForTesting(mTabStateStorageService);

        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mFactory,
                        /* isAuthoritative= */ true);
        mTabStateStore.addObserver(mObserver);

        when(mRegularTabModel.getCurrentTabSupplier()).thenReturn(mRegularTabSupplier);
        when(mIncognitoTabModel.getCurrentTabSupplier()).thenReturn(mIncognitoTabSupplier);

        when(mRegularData.getLoadingStatus()).thenReturn(StorageLoadingStatus.SUCCESS);
        when(mRegularData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        when(mIncognitoData.getLoadingStatus()).thenReturn(StorageLoadingStatus.SUCCESS);
        when(mIncognitoData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
    }

    @Test
    public void testClearState() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.clearState();
        verify(mTabStateStorageService).clearState();
    }

    @Test
    public void testCleanupStateFile() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.cleanupStateFile(CLEANUP_WINDOW_ID);
        verify(mTabStateStorageService).clearWindow(CLEANUP_WINDOW_TAG);
    }

    @Test
    public void testClearCurrentWindow() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.clearCurrentWindow();
        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
    }

    @Test
    public void testOnNativeLibraryReady_withKey() {
        byte[] key = new byte[] {1, 2, 3};
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(key);

        mTabStateStore.onNativeLibraryReady();

        verify(mTabStateStorageService).setKey(WINDOW_TAG, key);
        verify(mRegularTabModel).addObserver(any());
        verify(mIncognitoTabModel).addObserver(any());
    }

    @Test
    public void testOnNativeLibraryReady_generatesKey() {
        mTabStateStore.onNativeLibraryReady();
        byte[] newKey = new byte[] {4, 5, 6};
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(null);
        when(mTabStateStorageService.generateKey(WINDOW_TAG)).thenReturn(newKey);

        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mFactory,
                        /* isAuthoritative= */ true);
        mTabStateStore.onNativeLibraryReady();

        verify(mCipherFactory).setKeyForTabStateStorage(newKey);
        verify(mTabStateStorageService, never()).setKey(any(), any());
    }

    @Test
    public void testSaveState_SavesDirtyTab() {
        mTabStateStore.onNativeLibraryReady();
        Tab tab = createMockTabWithParentCollection(1, mProfile);

        TabStateAttributes.createForTab(tab, TabCreationState.LIVE_IN_FOREGROUND);

        // Force dirtiness update.
        tab.setIsPinned(true);
        tab.setIsPinned(false);

        mRegularTabSupplier.set(tab);
        mIncognitoTabSupplier.set(null);

        mTabStateStore.saveState();

        verify(mModelTrackingOrchestrator).saveTab(tab);
    }

    @Test
    public void testSaveState_DoesNotSaveCleanTab() {
        mTabStateStore.onNativeLibraryReady();
        Tab tab = MockTab.createAndInitialize(1, mProfile);
        TabStateAttributes.createForTab(tab, TabCreationState.LIVE_IN_FOREGROUND);
        mRegularTabSupplier.set(tab);
        mIncognitoTabSupplier.set(null);

        mTabStateStore.saveState();
        mTabStateStore.saveState();

        verify(mModelTrackingOrchestrator, never()).saveTab(tab);
    }

    @Test
    public void testLoadAndRestore_Success() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        callbacks.get(0).onResult(mRegularData);
        callbacks.get(1).onResult(mIncognitoData);

        verify(mObserver).onInitialized(0);

        mTabStateStore.restoreTabs(true);
        ShadowLooper.runUiThreadTasks();

        verify(mObserver).onStateLoaded();
        verify(mModelTrackingOrchestrator).onRestoredForModel(false);
        verify(mModelTrackingOrchestrator).onRestoredForModel(true);
        verify(mModelTrackingOrchestrator).onRestoreFinished();
    }

    @Test
    public void testLoadAndRestore_Cancel() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);
        when(mTabCreatorManager.getTabCreator(anyBoolean())).thenReturn(mTabCreator);

        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        LoadedTabState loadedTabState = new LoadedTabState(0, tabState);
        when(mRegularData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {loadedTabState});

        mTabStateStore.destroy();
        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        callbacks.get(0).onResult(mRegularData);
        callbacks.get(1).onResult(mIncognitoData);

        verify(mModelTrackingOrchestrator, times(2)).onRestoreCancelled();
    }

    @Test
    public void testLoadState_Failure() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        Callback<StorageLoadedData> regularCallback = mCallbackCaptor.getAllValues().get(0);

        when(mRegularData.getLoadingStatus()).thenReturn(StorageLoadingStatus.PARSE_ERROR);
        when(mRegularData.getErrorMessage()).thenReturn("error");

        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        LoadedTabState loadedTabState = new LoadedTabState(0, tabState);
        when(mRegularData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {loadedTabState});

        Assert.assertThrows(AssertionError.class, () -> regularCallback.onResult(mRegularData));

        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, false, null);
        verify(tabState.contentsState).destroy();
        verify(mRegularData).destroy();
    }

    @Test
    public void testDestroy() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        mTabStateStore.destroy();

        verify(mRegularTabModel).removeObserver(any());
        verify(mIncognitoTabModel).removeObserver(any());
        verify(mModelTrackingOrchestrator).destroy();

        Callback<StorageLoadedData> regularCallback = callbacks.get(0);
        regularCallback.onResult(mRegularData);
        verify(mObserver, never()).onInitialized(anyInt());
        verify(mRegularData).destroy();
    }

    @Test
    public void testTabCountTracker_getRestoredTabCount() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(false);

        verify(mTabCountTracker).getRestoredTabCount(false);
        verify(mTabCountTracker).getRestoredTabCount(true);
    }

    @Test
    public void testTabCountTracker_clearTabCount() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(true);

        verify(mTabCountTracker).getRestoredTabCount(false);
        verify(mTabCountTracker, never()).getRestoredTabCount(true);
        verify(mTabCountTracker).clearTabCount(true);
    }

    @Test
    public void testClearCurrentWindow_Authoritative() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        StorageLoadedData regularData = mock(StorageLoadedData.class);
        when(regularData.getLoadingStatus()).thenReturn(StorageLoadingStatus.SUCCESS);
        when(regularData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(0).onResult(regularData);

        StorageLoadedData incognitoData = mock(StorageLoadedData.class);
        when(incognitoData.getLoadingStatus()).thenReturn(StorageLoadingStatus.SUCCESS);
        when(incognitoData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(1).onResult(incognitoData);

        verify(mObserver).onInitialized(0);

        mTabStateStore.restoreTabs(true);
        ShadowLooper.runUiThreadTasks();

        verify(mObserver).onStateLoaded();
        verify(mModelTrackingOrchestrator).onRestoreFinished();
        verify(mTabStateStorageService, never()).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker, never()).clearCurrentWindow();
    }

    @Test
    public void testClearCurrentWindow_NonAuthoritative() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mFactory,
                        /* isAuthoritative= */ false);
        mTabStateStore.addObserver(mObserver);

        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        StorageLoadedData regularData = mock(StorageLoadedData.class);
        when(regularData.getLoadingStatus()).thenReturn(StorageLoadingStatus.SUCCESS);
        when(regularData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(0).onResult(regularData);

        StorageLoadedData incognitoData = mock(StorageLoadedData.class);
        when(incognitoData.getLoadingStatus()).thenReturn(StorageLoadingStatus.SUCCESS);
        when(incognitoData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(1).onResult(incognitoData);

        verify(mObserver).onInitialized(0);

        mTabStateStore.restoreTabs(true);
        ShadowLooper.runUiThreadTasks();

        verify(mObserver).onStateLoaded();
        verify(mModelTrackingOrchestrator).onRestoreFinished();
        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker).clearCurrentWindow();
    }

    @Test
    public void testSaveCleanTabOnRegistration_Authoritative() {
        Tab tab = createMockTabWithParentCollection(1, mProfile);
        TabStateAttributes.createForTab(tab, TabCreationState.LIVE_IN_FOREGROUND);
        when(mRegularTabModel.getCount()).thenReturn(1);
        when(mRegularTabModel.getTabAt(0)).thenReturn(tab);
        when(mIncognitoTabModel.getCount()).thenReturn(0);

        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);
        when(mTabModelSelector.getModels())
                .thenReturn(Arrays.asList(mRegularTabModel, mIncognitoTabModel));
        when(mRegularTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);
        when(mIncognitoTabModel.getComprehensiveModel()).thenReturn(mComprehensiveTabList);
        when(mComprehensiveTabList.iterator()).thenReturn(List.of(tab).iterator());

        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        callbacks.get(0).onResult(mRegularData);
        callbacks.get(1).onResult(mIncognitoData);

        verify(mModelTrackingOrchestrator).saveTab(tab);
    }

    @Test
    public void testDoNotSaveCleanTabOnRegistration_NonAuthoritative() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mFactory,
                        /* isAuthoritative= */ false);
        mTabStateStore.addObserver(mObserver);

        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        Tab tab = createMockTabWithParentCollection(1, mProfile);
        TabStateAttributes.createForTab(tab, TabCreationState.LIVE_IN_FOREGROUND);
        when(mRegularTabModel.getCount()).thenReturn(1);
        when(mRegularTabModel.getTabAt(0)).thenReturn(tab);

        mTabStateStore.loadState(false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        callbacks.get(0).onResult(mRegularData);
        callbacks.get(1).onResult(mIncognitoData);

        mTabStateStore.restoreTabs(true);
        ShadowLooper.runUiThreadTasks();

        verify(mModelTrackingOrchestrator, never()).saveTab(tab);
    }

    @Test
    public void testLoadState_IgnoreIncognito() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(true);

        verify(mModelTrackingOrchestrator).setLoadIncognitoTabsOnStart(false);
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService, never()).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker).clearTabCount(true);
        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, true, null);
    }

    @Test
    public void testLoadState_LoadIncognito() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(false);

        verify(mModelTrackingOrchestrator).setLoadIncognitoTabsOnStart(true);
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker, never()).clearTabCount(true);
        verify(mTabStateStorageService, never()).clearUnusedNodesForWindow(any(), eq(true), any());
    }

    @Test
    public void testLoadState_NoCipherFactory() {
        // Create a new TabStateStore instance without a CipherFactory
        TabStateStore noCipherTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        /* cipherFactory= */ null,
                        mTabCountTracker,
                        mFactory,
                        /* isAuthoritative= */ true);
        noCipherTabStateStore.addObserver(mObserver);
        noCipherTabStateStore.onNativeLibraryReady();

        noCipherTabStateStore.loadState(false);

        // Incognito data should not be loaded.
        verify(mModelTrackingOrchestrator).setLoadIncognitoTabsOnStart(false);
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService, never()).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker).clearTabCount(true);
        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, true, null);
    }
}
