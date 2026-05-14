// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ModelTrackingOrchestrator.CollectionKeyedFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.CollectionSaveForwarder;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.StorageCollectionSynchronizer;
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabGroupCollectionData;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateStorageService;
import org.chromium.chrome.browser.tab.TabStateStorageServiceFactory;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.PersistentStoreMigrationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.tabs.TabStripCollection;

import java.util.Arrays;

/** Unit tests for {@link ModelTrackingOrchestrator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TAB_STORAGE_SQLITE_PROTOTYPE)
public class ModelTrackingOrchestratorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String WINDOW_TAG = "window_1";

    @Captor private ArgumentCaptor<IncognitoTabModelObserver> mIncognitoObserverCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupObserverCaptor;

    @Mock private PersistentStoreMigrationManager mMigrationManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mRegularTabModel;
    @Mock private IncognitoTabModel mIncognitoTabModel;
    @Mock private Profile mProfile;
    @Mock private TabStripCollection mRegularTabStripCollection;
    @Mock private TabStripCollection mIncognitoTabStripCollection;
    @Mock private ActiveTabCache mActiveTabCache;
    @Mock private StorageLoadedData mRegularData;
    @Mock private TabStateStorageService mTabStateStorageService;

    @Mock private CollectionKeyedFactory<StorageCollectionSynchronizer> mSynchronizerFactory;
    @Mock private StorageCollectionSynchronizer mRegularSynchronizer;
    @Mock private StorageCollectionSynchronizer mIncognitoSynchronizer;

    @Mock private CollectionKeyedFactory<CollectionSaveForwarder> mSaveForwarderFactory;
    @Mock private CollectionSaveForwarder mRegularSaveForwarder;
    @Mock private CollectionSaveForwarder mIncognitoSaveForwarder;

    private final SettableNullableObservableSupplier<Tab> mRegularTabSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNullableObservableSupplier<Tab> mIncognitoTabSupplier =
            ObservableSuppliers.createNullable();

    private ModelTrackingOrchestrator mOrchestrator;

    @Before
    public void setUp() {
        TabStateStorageServiceFactory.setForTesting(mTabStateStorageService);

        when(mTabModelSelector.getModel(false)).thenReturn(mRegularTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);

        when(mRegularTabModel.getProfile()).thenReturn(mProfile);
        when(mIncognitoTabModel.getProfile()).thenReturn(mProfile);

        when(mRegularTabModel.getTabStripCollection()).thenReturn(mRegularTabStripCollection);
        when(mIncognitoTabModel.getTabStripCollection()).thenReturn(mIncognitoTabStripCollection);

        when(mRegularTabModel.getCurrentTabSupplier()).thenReturn(mRegularTabSupplier);
        when(mIncognitoTabModel.getCurrentTabSupplier()).thenReturn(mIncognitoTabSupplier);

        when(mTabModelSelector.getModels())
                .thenReturn(Arrays.asList(mRegularTabModel, mIncognitoTabModel));

        when(mSynchronizerFactory.build(any(), eq(mRegularTabStripCollection)))
                .thenReturn(mRegularSynchronizer);
        when(mSynchronizerFactory.build(any(), eq(mIncognitoTabStripCollection)))
                .thenReturn(mIncognitoSynchronizer);

        when(mSaveForwarderFactory.build(any(), eq(mRegularTabStripCollection)))
                .thenReturn(mRegularSaveForwarder);
        when(mSaveForwarderFactory.build(any(), eq(mIncognitoTabStripCollection)))
                .thenReturn(mIncognitoSaveForwarder);

        when(mRegularTabModel.isOffTheRecord()).thenReturn(false);
        when(mIncognitoTabModel.isOffTheRecord()).thenReturn(true);
        when(mRegularData.getGroupsData()).thenReturn(new TabGroupCollectionData[0]);
    }

    private void createOrchestrator(boolean hasCipherFactory, boolean isAuthoritative) {
        mOrchestrator =
                new ModelTrackingOrchestrator(
                        WINDOW_TAG,
                        mMigrationManager,
                        mTabModelSelector,
                        mActiveTabCache,
                        hasCipherFactory,
                        isAuthoritative,
                        mSynchronizerFactory,
                        mSaveForwarderFactory);
    }

    @Test
    public void testConstructor_HasCipherFactory_IncognitoObserver() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        verify(mIncognitoTabModel).addIncognitoObserver(any());
    }

    @Test
    public void testConstructor_NoCipherFactory_NoIncognitoObserver() {
        createOrchestrator(/* hasCipherFactory= */ false, /* isAuthoritative= */ true);

        verify(mIncognitoTabModel, never()).addIncognitoObserver(any());
    }

    @Test
    public void testRegularLifecycle_Authoritative_LoadedAndRestored() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        assertTrue(mOrchestrator.isSynchronizerPresent(/* incognito= */ false));

        // Synchronizer is initialized on restore finished.
        mOrchestrator.onRestoredForModel(/* incognito= */ false);
        assertTrue(mOrchestrator.isSynchronizerPresent(/* incognito= */ false));
    }

    @Test
    public void testShadowStoreCatchUpLifecycle_simpleNoIncognito() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ false);

        mOrchestrator.onAuthoritativeStateLoaded();
        mOrchestrator.setLoadIncognitoTabsOnStart(false);

        // Confirm not caught up yet.
        verify(mMigrationManager, never()).onShadowStoreCaughtUp();

        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        ArgumentCaptor<Runnable> callbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mRegularSynchronizer).fullSave(callbackCaptor.capture());
        callbackCaptor.getValue().run();

        ShadowLooper.runUiThreadTasks();

        // Shadow store catching up complete.
        verify(mMigrationManager).onShadowStoreCaughtUp();
    }

    @Test
    public void testShadowStoreCatchUpLifecycle_withIncognitoLoad() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ false);

        mOrchestrator.onAuthoritativeStateLoaded();
        mOrchestrator.setLoadIncognitoTabsOnStart(true);

        // Caught up blocked because Regular, Incognito models are not caught up.
        verify(mMigrationManager, never()).onShadowStoreCaughtUp();

        // Caught up Regular.
        mOrchestrator.onRestoredForModel(/* incognito= */ false);
        ArgumentCaptor<Runnable> regularCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mRegularSynchronizer).fullSave(regularCallbackCaptor.capture());
        regularCallbackCaptor.getValue().run();
        ShadowLooper.runUiThreadTasks();

        verify(mMigrationManager, never()).onShadowStoreCaughtUp();

        // Caught up Incognito.
        mOrchestrator.onRestoredForModel(/* incognito= */ true);
        ArgumentCaptor<Runnable> incognitoCallbackCaptor = ArgumentCaptor.forClass(Runnable.class);
        verify(mIncognitoSynchronizer).fullSave(incognitoCallbackCaptor.capture());
        incognitoCallbackCaptor.getValue().run();
        ShadowLooper.runUiThreadTasks();

        // Now both models caught up.
        verify(mMigrationManager).onShadowStoreCaughtUp();
    }

    @Test
    public void testIncognitoLifecycle_OnModelCreated() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoObserverCaptor.capture());
        IncognitoTabModelObserver observer = mIncognitoObserverCaptor.getValue();

        StorageLoadedData data = mock(StorageLoadedData.class);
        when(data.getLoadedTabStates()).thenReturn(new StorageLoadedData.LoadedTabState[1]);
        when(data.getGroupsData()).thenReturn(new TabGroupCollectionData[0]);
        mOrchestrator.onDataLoaded(data, /* incognito= */ true);

        observer.onIncognitoModelCreated();

        assertTrue(mOrchestrator.isSynchronizerPresent(/* incognito= */ true));
    }

    @Test
    public void testIncognitoLifecycle_DidBecomeEmptyResetsSynchronizer() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoObserverCaptor.capture());
        IncognitoTabModelObserver observer = mIncognitoObserverCaptor.getValue();

        StorageLoadedData data = mock(StorageLoadedData.class);
        when(data.getLoadedTabStates()).thenReturn(new StorageLoadedData.LoadedTabState[1]);
        when(data.getGroupsData()).thenReturn(new TabGroupCollectionData[0]);
        mOrchestrator.onDataLoaded(data, /* incognito= */ true);

        observer.onIncognitoModelCreated();
        assertTrue(mOrchestrator.isSynchronizerPresent(/* incognito= */ true));

        observer.didBecomeEmpty();
        assertFalse(mOrchestrator.isSynchronizerPresent(/* incognito= */ true));
    }

    @Test
    public void testRestoreCancelled() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.setLoadIncognitoTabsOnStart(true);
        mOrchestrator.onRestoreCancelled();

        verify(mIncognitoSynchronizer).cancelRestore();
    }

    @Test
    public void testRestoreFinished_ClearUnusedNodes() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onRestoreFinished();

        // Processes the chained task sequence.
        ShadowLooper.runUiThreadTasks();

        verify(mTabStateStorageService)
                .clearUnusedNodesForWindow(
                        eq(WINDOW_TAG), eq(false), eq(mRegularTabStripCollection));
        verify(mTabStateStorageService)
                .clearUnusedNodesForWindow(
                        eq(WINDOW_TAG), eq(true), eq(mIncognitoTabStripCollection));
    }

    @Test
    public void testSaveTab_RegularTab() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        MockTab tab = new MockTab(1, mProfile);
        TabStateAttributes.createForTab(tab, TabCreationState.LIVE_IN_FOREGROUND);

        mRegularTabSupplier.set(tab);
        mOrchestrator.saveTab(tab);

        verify(mRegularSynchronizer).saveTab(eq(tab));
        verify(mActiveTabCache).saveActiveTab(tab);
    }

    @Test
    public void testSaveTabGroupPayload() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        verify(mRegularTabModel).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mTabGroupObserverCaptor.getValue();

        Token groupId = Token.createRandom();
        MockTab tab = new MockTab(1, mProfile);
        tab.setTabGroupId(groupId);

        when(mRegularTabModel.isOffTheRecord()).thenReturn(false);
        observer.didCreateNewGroup(tab, mRegularTabModel);

        observer.didChangeTabGroupColor(groupId, 0);

        verify(mRegularSynchronizer).saveTabGroupPayload(eq(groupId));
    }

    @Test
    public void testSaveTabGroupPayload_metadataChanges() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        verify(mRegularTabModel).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mTabGroupObserverCaptor.getValue();

        Token groupId = Token.createRandom();
        MockTab tab = new MockTab(1, mProfile);
        tab.setTabGroupId(groupId);

        when(mRegularTabModel.isOffTheRecord()).thenReturn(false);
        observer.didCreateNewGroup(tab, mRegularTabModel);

        observer.didChangeTabGroupCollapsed(groupId, true, false);
        observer.didChangeTabGroupTitle(groupId, "New Title");

        // Verifies saveTabGroupPayload is called for collapsing and title changes.
        verify(mRegularSynchronizer, times(2)).saveTabGroupPayload(eq(groupId));
    }

    @Test
    public void testSaveTabGroupPayload_didRemoveTabGroup_noSave() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        verify(mRegularTabModel).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        TabGroupModelFilterObserver observer = mTabGroupObserverCaptor.getValue();

        Token groupId = Token.createRandom();
        MockTab tab = new MockTab(1, mProfile);
        tab.setTabGroupId(groupId);

        when(mRegularTabModel.isOffTheRecord()).thenReturn(false);
        observer.didCreateNewGroup(tab, mRegularTabModel);

        observer.didRemoveTabGroup(1, groupId, 0);

        observer.didChangeTabGroupColor(groupId, 0);

        // Verifies saveTabGroupPayload is NOT called after the group has been removed.
        verify(mRegularSynchronizer, never()).saveTabGroupPayload(eq(groupId));
    }

    @Test
    public void testDestroy() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        mOrchestrator.destroy();

        verify(mIncognitoTabModel).removeIncognitoObserver(any());
        verify(mRegularTabModel).removeTabGroupObserver(any());
        verify(mIncognitoSynchronizer, never()).destroy();
        verify(mRegularSynchronizer).destroy();
        verify(mRegularSaveForwarder).destroy();
    }

    @Test
    public void testDestroy_WithIncognitoSynchronizerReady() {
        createOrchestrator(/* hasCipherFactory= */ true, /* isAuthoritative= */ true);

        mOrchestrator.onDataLoaded(mRegularData, /* incognito= */ false);
        mOrchestrator.onRestoredForModel(/* incognito= */ false);

        verify(mIncognitoTabModel).addIncognitoObserver(mIncognitoObserverCaptor.capture());
        IncognitoTabModelObserver observer = mIncognitoObserverCaptor.getValue();
        observer.onIncognitoModelCreated();

        mOrchestrator.destroy();

        verify(mIncognitoSynchronizer).destroy();
        verify(mRegularSynchronizer).destroy();
        verify(mIncognitoSaveForwarder).destroy();
        verify(mRegularSaveForwarder).destroy();
    }
}
