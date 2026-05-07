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
import static org.mockito.Mockito.reset;
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
import org.chromium.chrome.browser.tab.StorageLoadWarningCode;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.StorageLoadedData.StorageLoadWarning;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tab.WebContentsState;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager.StoreType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.TabPersistentStoreObserver;

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
    @Mock private ActiveTabCache mActiveTabCache;
    @Mock private TabCountTracker mTabCountTracker;
    @Mock private StorageLoadedData mRegularData;
    @Mock private StorageLoadedData mIncognitoData;
    @Captor private ArgumentCaptor<Callback<StorageLoadedData>> mCallbackCaptor;

    private final ModelTrackingOrchestrator.Factory mModelTrackingOrchestratorFactory =
            (a, b, c, d, e, f) -> mModelTrackingOrchestrator;
    private final ActiveTabCache.Factory mActiveTabCacheFactory = (a, b, c) -> mActiveTabCache;
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
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ true,
                        /* isFromRecreating= */ false);
        mTabStateStore.addObserver(mObserver);

        when(mRegularTabModel.getCurrentTabSupplier()).thenReturn(mRegularTabSupplier);
        when(mIncognitoTabModel.getCurrentTabSupplier()).thenReturn(mIncognitoTabSupplier);

        when(mRegularData.getWarnings()).thenReturn(new StorageLoadWarning[0]);
        when(mRegularData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        when(mIncognitoData.getWarnings()).thenReturn(new StorageLoadWarning[0]);
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
        verify(mTabCountTracker).clearCurrentWindow();
        verify(mActiveTabCache).clearCurrentWindow();
        verify(mMigrationManager).onWindowCleared();
    }

    @Test
    public void testOnNativeLibraryReady_Authoritative_Raze() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ true,
                        /* isFromRecreating= */ false);
        when(mMigrationManager.shouldRazeStoreForWindow(true)).thenReturn(true);

        mTabStateStore.onNativeLibraryReady();

        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker).clearCurrentWindow();
        verify(mActiveTabCache).clearCurrentWindow();
        verify(mMigrationManager).onWindowCleared();
        verify(mMigrationManager).onAuthoritativeStoreInitialized(StoreType.TAB_STATE_STORE);
    }

    @Test
    public void testOnNativeLibraryReady_Authoritative_NoRaze() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ true,
                        /* isFromRecreating= */ false);
        when(mMigrationManager.shouldRazeStoreForWindow(true)).thenReturn(false);

        mTabStateStore.onNativeLibraryReady();

        verify(mTabStateStorageService, never()).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker, never()).clearCurrentWindow();
        verify(mActiveTabCache, never()).clearCurrentWindow();
        verify(mMigrationManager, never()).onWindowCleared();
        verify(mMigrationManager).onAuthoritativeStoreInitialized(StoreType.TAB_STATE_STORE);
    }

    @Test
    public void testOnNativeLibraryReady_NonAuthoritative_Raze() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);
        when(mMigrationManager.shouldRazeStoreForWindow(false)).thenReturn(true);

        mTabStateStore.onNativeLibraryReady();

        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker).clearCurrentWindow();
        verify(mActiveTabCache).clearCurrentWindow();
        verify(mMigrationManager).onShadowStoreRazed();
        verify(mMigrationManager, never()).onAuthoritativeStoreInitialized(anyInt());
    }

    @Test
    public void testOnNativeLibraryReady_NonAuthoritative_NoRaze() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);
        when(mMigrationManager.shouldRazeStoreForWindow(false)).thenReturn(false);

        mTabStateStore.onNativeLibraryReady();

        verify(mTabStateStorageService, never()).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker, never()).clearCurrentWindow();
        verify(mActiveTabCache, never()).clearCurrentWindow();
        verify(mMigrationManager, never()).onShadowStoreRazed();
        verify(mMigrationManager, never()).onAuthoritativeStoreInitialized(anyInt());
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
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ true,
                        /* isFromRecreating= */ false);
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

        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

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
        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        callbacks.get(0).onResult(mRegularData);
        callbacks.get(1).onResult(mIncognitoData);

        verify(mModelTrackingOrchestrator, times(2)).onRestoreCancelled();
    }

    @Test
    public void testLoadStateFailure_Authoritative() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        Callback<StorageLoadedData> regularCallback = mCallbackCaptor.getAllValues().get(0);

        when(mRegularData.getWarnings())
                .thenReturn(
                        new StorageLoadWarning[] {
                            new StorageLoadWarning(StorageLoadWarningCode.PARSE_ERROR, "error")
                        });

        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        LoadedTabState loadedTabState = new LoadedTabState(0, tabState);
        when(mRegularData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {loadedTabState});

        regularCallback.onResult(mRegularData);

        verify(mTabStateStorageService, never())
                .clearUnusedNodesForWindow(any(), anyBoolean(), any());
        verify(mTabCountTracker, never()).clearTabCount(anyBoolean());
        verify(mActiveTabCache, never()).clearActiveTab(anyBoolean());
        verify(mMigrationManager, never()).onShadowStoreRazed();
    }

    @Test
    public void testLoadStateFailure_NonAuthoritative() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);
        mTabStateStore.addObserver(mObserver);

        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        Callback<StorageLoadedData> regularCallback = mCallbackCaptor.getAllValues().get(0);

        when(mRegularData.getWarnings())
                .thenReturn(
                        new StorageLoadWarning[] {
                            new StorageLoadWarning(StorageLoadWarningCode.PARSE_ERROR, "error")
                        });

        TabState tabState = new TabState();
        tabState.contentsState = mock(WebContentsState.class);
        LoadedTabState loadedTabState = new LoadedTabState(0, tabState);
        when(mRegularData.getLoadedTabStates()).thenReturn(new LoadedTabState[] {loadedTabState});

        Assert.assertThrows(AssertionError.class, () -> regularCallback.onResult(mRegularData));

        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, false, null);
        verify(mTabCountTracker).clearTabCount(false);
        verify(mActiveTabCache).clearActiveTab(false);
        verify(tabState.contentsState).destroy();
        verify(mRegularData).destroy();
    }

    @Test
    public void testOnAuthoritativeStateLoaded() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);

        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.onAuthoritativeStateLoaded();
        verify(mModelTrackingOrchestrator).onAuthoritativeStateLoaded();
    }

    @Test
    public void testDestroy() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

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

        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabCountTracker).getRestoredTabCount(false);
        verify(mTabCountTracker).getRestoredTabCount(true);
    }

    @Test
    public void testTabCountTracker_clearTabCount() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(/* ignoreIncognitoFiles= */ true, /* ignoreRegularFiles= */ false);

        verify(mTabCountTracker).getRestoredTabCount(false);
        verify(mTabCountTracker, never()).getRestoredTabCount(true);
        verify(mTabCountTracker).clearTabCount(true);
    }

    @Test
    public void testClearCurrentWindowOnRestore_Authoritative() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);
        reset(
                mMigrationManager,
                mModelTrackingOrchestrator,
                mTabStateStorageService,
                mTabCountTracker);

        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        StorageLoadedData regularData = mock(StorageLoadedData.class);
        when(regularData.getWarnings()).thenReturn(new StorageLoadWarning[0]);
        when(regularData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(0).onResult(regularData);

        StorageLoadedData incognitoData = mock(StorageLoadedData.class);
        when(incognitoData.getWarnings()).thenReturn(new StorageLoadWarning[0]);
        when(incognitoData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(1).onResult(incognitoData);

        verify(mObserver).onInitialized(0);

        mTabStateStore.restoreTabs(true);
        ShadowLooper.runUiThreadTasks();

        verify(mObserver).onStateLoaded();
        verify(mModelTrackingOrchestrator).onRestoreFinished();
        verify(mTabStateStorageService, never()).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker, never()).clearCurrentWindow();
        verify(mActiveTabCache, never()).clearCurrentWindow();
        verify(mMigrationManager, never()).onWindowCleared();
        verify(mMigrationManager, never()).onShadowStoreRazed();
    }

    @Test
    public void testClearCurrentWindowOnRestore_NonAuthoritative() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);
        mTabStateStore.addObserver(mObserver);

        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);
        reset(
                mMigrationManager,
                mModelTrackingOrchestrator,
                mTabStateStorageService,
                mTabCountTracker);

        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabStateStorageService, times(2))
                .loadAllData(eq(WINDOW_TAG), anyBoolean(), mCallbackCaptor.capture());

        List<Callback<StorageLoadedData>> callbacks = mCallbackCaptor.getAllValues();

        StorageLoadedData regularData = mock(StorageLoadedData.class);
        when(regularData.getWarnings()).thenReturn(new StorageLoadWarning[0]);
        when(regularData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(0).onResult(regularData);

        StorageLoadedData incognitoData = mock(StorageLoadedData.class);
        when(incognitoData.getWarnings()).thenReturn(new StorageLoadWarning[0]);
        when(incognitoData.getLoadedTabStates()).thenReturn(new LoadedTabState[0]);
        callbacks.get(1).onResult(incognitoData);

        verify(mObserver).onInitialized(0);

        mTabStateStore.restoreTabs(true);
        ShadowLooper.runUiThreadTasks();

        verify(mObserver).onStateLoaded();
        verify(mModelTrackingOrchestrator).onRestoreFinished();
        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker).clearCurrentWindow();
        verify(mActiveTabCache).clearCurrentWindow();
        verify(mMigrationManager).onShadowStoreRazed();
    }

    @Test
    public void testDoNotSaveCleanTabOnRegistration_Authoritative() {
        mTabStateStore.onNativeLibraryReady();

        Tab tab = createMockTabWithParentCollection(1, mProfile);
        TabStateAttributes.createForTab(tab, TabCreationState.FROZEN_ON_RESTORE);

        mTabStateStore.onTabRegistered(tab);

        verify(mModelTrackingOrchestrator, never()).saveTab(tab);
    }

    @Test
    public void testSaveCleanTabOnRegistration_NonAuthoritative() {
        mTabStateStore =
                new TabStateStore(
                        mTabModelSelector,
                        WINDOW_TAG,
                        mTabCreatorManager,
                        mTabPersistencePolicy,
                        mMigrationManager,
                        mCipherFactory,
                        mTabCountTracker,
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);
        mTabStateStore.addObserver(mObserver);

        mTabStateStore.onNativeLibraryReady();

        Tab tab = createMockTabWithParentCollection(1, mProfile);
        TabStateAttributes.createForTab(tab, TabCreationState.FROZEN_ON_RESTORE);

        mTabStateStore.onTabRegistered(tab);

        verify(mModelTrackingOrchestrator).saveTab(tab);
    }

    @Test
    public void testLoadState_IgnoreRegular() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(/* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ true);

        verify(mTabStateStorageService, never()).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker).clearTabCount(false);
        verify(mActiveTabCache).clearActiveTab(false);
        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, false, null);
    }

    @Test
    public void testLoadState_NoIgnoreRegular() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker, never()).clearTabCount(false);
        verify(mActiveTabCache, never()).clearActiveTab(false);
    }

    @Test
    public void testLoadState_IgnoreIncognito() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(/* ignoreIncognitoFiles= */ true, /* ignoreRegularFiles= */ false);

        verify(mModelTrackingOrchestrator).setLoadIncognitoTabsOnStart(false);
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService, never()).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker).clearTabCount(true);
        verify(mActiveTabCache).clearActiveTab(true);
        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, true, null);
    }

    @Test
    public void testLoadState_LoadIncognito() {
        mTabStateStore.onNativeLibraryReady();
        when(mCipherFactory.getKeyForTabStateStorage()).thenReturn(new byte[1]);

        mTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

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
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ true,
                        /* isFromRecreating= */ false);
        noCipherTabStateStore.addObserver(mObserver);
        noCipherTabStateStore.onNativeLibraryReady();

        noCipherTabStateStore.loadState(
                /* ignoreIncognitoFiles= */ false, /* ignoreRegularFiles= */ false);

        // Incognito data should not be loaded.
        verify(mModelTrackingOrchestrator).setLoadIncognitoTabsOnStart(false);
        verify(mTabStateStorageService).loadAllData(eq(WINDOW_TAG), eq(false), any());
        verify(mTabStateStorageService, never()).loadAllData(eq(WINDOW_TAG), eq(true), any());
        verify(mTabCountTracker).clearTabCount(true);
        verify(mTabStateStorageService).clearUnusedNodesForWindow(WINDOW_TAG, true, null);
    }

    @Test
    public void testClearCurrentWindow_Authoritative() {
        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.clearCurrentWindow();

        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker).clearCurrentWindow();
        verify(mActiveTabCache).clearCurrentWindow();
        verify(mMigrationManager).onWindowCleared();
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
                        mModelTrackingOrchestratorFactory,
                        mActiveTabCacheFactory,
                        /* isAuthoritative= */ false,
                        /* isFromRecreating= */ false);

        mTabStateStore.onNativeLibraryReady();
        mTabStateStore.clearCurrentWindow();

        verify(mTabStateStorageService).clearWindow(WINDOW_TAG);
        verify(mTabCountTracker).clearCurrentWindow();
        verify(mActiveTabCache).clearCurrentWindow();
        verify(mMigrationManager).onShadowStoreRazed();
    }
}
