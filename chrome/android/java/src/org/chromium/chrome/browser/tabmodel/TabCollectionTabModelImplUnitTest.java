// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
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
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;

/** Unit tests for {@link TabCollectionTabModelImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCollectionTabModelImplUnitTest {
    private static final long TAB_MODEL_JNI_BRIDGE_PTR = 875943L;
    private static final long TAB_COLLECTION_TAB_MODEL_IMPL_PTR = 378492L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private TabCollectionTabModelImpl.Natives mTabCollectionTabModelImplJni;
    @Mock private Profile mProfile;
    @Mock private Profile mOtrProfile;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabModelOrderController mOrderController;
    @Mock private TabModelDelegate mTabModelDelegate;
    @Mock private AsyncTabParamsManager mAsyncTabParamsManager;
    @Mock private TabRemover mTabRemover;
    @Mock private TabModelObserver mTabModelObserver;

    private TabCollectionTabModelImpl mTabModel;

    @Before
    public void setUp() {
        // Required to use MockTab.
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isIncognitoBranded()).thenReturn(false);
        when(mOtrProfile.isOffTheRecord()).thenReturn(true);
        when(mOtrProfile.isIncognitoBranded()).thenReturn(true);

        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridgeJni);
        when(mTabModelJniBridgeJni.init(
                        any(),
                        eq(mProfile),
                        eq(ActivityType.TABBED),
                        /* isArchivedTabModel= */ eq(false)))
                .thenReturn(TAB_MODEL_JNI_BRIDGE_PTR);

        TabCollectionTabModelImplJni.setInstanceForTesting(mTabCollectionTabModelImplJni);
        when(mTabCollectionTabModelImplJni.init(any(), eq(mProfile)))
                .thenReturn(TAB_COLLECTION_TAB_MODEL_IMPL_PTR);

        mTabModel =
                new TabCollectionTabModelImpl(
                        mProfile,
                        ActivityType.TABBED,
                        /* isArchivedTabModel= */ false,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover);
        mTabModel.addObserver(mTabModelObserver);
    }

    @After
    public void tearDown() {
        mTabModel.destroy();
        verify(mTabModelJniBridgeJni).destroy(eq(TAB_MODEL_JNI_BRIDGE_PTR), any());
        verify(mTabCollectionTabModelImplJni).destroy(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR));
    }

    @Test
    public void testGetTabRemover() {
        assertEquals(mTabRemover, mTabModel.getTabRemover());
    }

    @Test
    public void testGetTabCreator() {
        assertEquals(mRegularTabCreator, mTabModel.getTabCreator());
    }

    @Test
    public void testBroadcastSessionRestoreComplete() {
        mTabModel.completeInitialization();

        verify(mTabModelObserver).restoreCompleted();
        assertTrue(mTabModel.isInitializationComplete());

        mTabModel.broadcastSessionRestoreComplete();

        verify(mTabModelJniBridgeJni)
                .broadcastSessionRestoreComplete(eq(TAB_MODEL_JNI_BRIDGE_PTR), any());
    }

    @Test
    public void testCompleteInitializationTwice() {
        mTabModel.completeInitialization();
        assertThrows(AssertionError.class, mTabModel::completeInitialization);
    }

    @Test
    public void testIsTabModelRestored() {
        when(mTabModelDelegate.isTabModelRestored()).thenReturn(false);
        assertFalse(mTabModel.isTabModelRestored());
        assertTrue(mTabModel.isSessionRestoreInProgress());

        when(mTabModelDelegate.isTabModelRestored()).thenReturn(true);
        assertTrue(mTabModel.isTabModelRestored());
        assertFalse(mTabModel.isSessionRestoreInProgress());
    }

    @Test
    public void testAddTabBasic() {
        @TabId int tabId = 789;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        tab.setIsInitialized(true);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        assertEquals(tab, mTabModel.getTabById(tabId));
    }

    @Test
    public void testAddTabDuplicate() {
        @TabId int tabId = 789;
        MockTab tab = MockTab.createAndInitialize(tabId, mProfile);
        tab.setIsInitialized(true);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        assertThrows(
                AssertionError.class,
                () ->
                        mTabModel.addTab(
                                tab,
                                /* index= */ 1,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND));
    }

    @Test
    public void testAddTabWrongModel() {
        @TabId int tabId = 789;
        MockTab otrTab = MockTab.createAndInitialize(tabId, mOtrProfile);
        otrTab.setIsInitialized(true);
        assertThrows(
                IllegalStateException.class,
                () ->
                        mTabModel.addTab(
                                otrTab,
                                /* index= */ 1,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND));
    }

    @Test
    public void testGetCount() {
        when(mTabCollectionTabModelImplJni.getTabCountRecursive(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR)))
                .thenReturn(5);
        assertEquals("Incorrect tab count", 5, mTabModel.getCount());
        verify(mTabCollectionTabModelImplJni)
                .getTabCountRecursive(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR));
    }

    @Test
    public void testGetCount_nativeNotInitialized() {
        mTabModel.destroy();
        assertEquals(
                "Tab count should be 0 when native is not initialized", 0, mTabModel.getCount());
        verify(mTabCollectionTabModelImplJni, never()).getTabCountRecursive(anyLong());
    }

    @Test
    public void testIndexOf() {
        MockTab tab = MockTab.createAndInitialize(123, mProfile);
        tab.setIsInitialized(true);
        when(mTabCollectionTabModelImplJni.getIndexOfTabRecursive(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(tab)))
                .thenReturn(2);
        assertEquals("Incorrect tab index", 2, mTabModel.indexOf(tab));
        verify(mTabCollectionTabModelImplJni)
                .getIndexOfTabRecursive(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(tab));
    }

    @Test
    public void testIndexOf_tabNotFound() {
        MockTab tab = MockTab.createAndInitialize(123, mProfile);
        tab.setIsInitialized(true);
        when(mTabCollectionTabModelImplJni.getIndexOfTabRecursive(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(tab)))
                .thenReturn(TabList.INVALID_TAB_INDEX);
        assertEquals(
                "Incorrect tab index for non-existent tab",
                TabList.INVALID_TAB_INDEX,
                mTabModel.indexOf(tab));
    }

    @Test
    public void testIndexOf_nullTab() {
        assertEquals(
                "Index of null tab should be invalid",
                TabList.INVALID_TAB_INDEX,
                mTabModel.indexOf(null));
        verify(mTabCollectionTabModelImplJni, never()).getIndexOfTabRecursive(anyLong(), any());
    }

    @Test
    public void testIndexOf_nativeNotInitialized() {
        mTabModel.destroy(); // Destroys native ptr.
        assertEquals(
                "Index should be invalid when native is not initialized",
                TabList.INVALID_TAB_INDEX,
                mTabModel.indexOf(MockTab.createAndInitialize(123, mProfile)));
        verify(mTabCollectionTabModelImplJni, never()).getIndexOfTabRecursive(anyLong(), any());
    }
}
