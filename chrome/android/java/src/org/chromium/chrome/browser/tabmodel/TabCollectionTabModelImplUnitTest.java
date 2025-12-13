// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
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

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.components.visited_url_ranking.url_grouping.TabSelectionType;

import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link TabCollectionTabModelImpl}. More substantive tests are in {@link
 * TabCollectionTabModelImplTest}.
 */
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
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabModelDelegate mTabModelDelegate;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;
    @Mock private AsyncTabParamsManager mAsyncTabParamsManager;
    @Mock private TabRemover mTabRemover;
    @Mock private TabUngrouper mTabUngrouper;
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
                        any(TabModelJniBridge.class),
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
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        /* supportUndo= */ false);
        mTabModel.addObserver(mTabModelObserver);
    }

    @After
    public void tearDown() {
        mTabModel.destroy();
        verify(mTabModelJniBridgeJni).destroy(eq(TAB_MODEL_JNI_BRIDGE_PTR));
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

        verify(mTabModelJniBridgeJni).broadcastSessionRestoreComplete(eq(TAB_MODEL_JNI_BRIDGE_PTR));
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
        verify(mTabModelObserver, atLeastOnce()).onDestroy();
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

    @Test
    public void testIsTabInTabGroup() {
        MockTab tab = MockTab.createAndInitialize(123, mProfile);
        tab.setIsInitialized(true);
        assertFalse(mTabModel.isTabInTabGroup(tab));
        tab.setTabGroupId(new Token(1L, 2L));
        assertTrue(mTabModel.isTabInTabGroup(tab));
    }

    @Test
    public void testWillMergingCreateNewGroup() {
        MockTab tab1 = MockTab.createAndInitialize(123, mProfile);
        MockTab tab2 = MockTab.createAndInitialize(123, mProfile);
        tab1.setIsInitialized(true);
        tab2.setIsInitialized(true);

        assertTrue(mTabModel.willMergingCreateNewGroup(List.of(tab1)));
        assertTrue(mTabModel.willMergingCreateNewGroup(List.of(tab1, tab2)));
        assertTrue(mTabModel.willMergingCreateNewGroup(List.of(tab2)));

        tab1.setTabGroupId(new Token(1L, 2L));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab1)));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab1, tab2)));
        assertTrue(mTabModel.willMergingCreateNewGroup(List.of(tab2)));

        tab2.setTabGroupId(new Token(3L, 4L));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab1)));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab1, tab2)));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab2)));

        tab1.setTabGroupId(null);
        assertTrue(mTabModel.willMergingCreateNewGroup(List.of(tab1)));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab1, tab2)));
        assertFalse(mTabModel.willMergingCreateNewGroup(List.of(tab2)));
    }

    @Test
    public void testGetRelatedTabList_Basic() {
        int tabId = 123;
        MockTab tab1 = MockTab.createAndInitialize(tabId, mProfile);
        tab1.setIsInitialized(true);
        mTabModel.addTab(
                tab1,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        assertEquals(Collections.emptyList(), mTabModel.getRelatedTabList(43789));
        assertEquals(Collections.singletonList(tab1), mTabModel.getRelatedTabList(tabId));
    }

    @Test
    public void testGetTabsInGroup_Null() {
        assertEquals(Collections.emptyList(), mTabModel.getTabsInGroup(null));
    }

    @Test
    public void testSetIndex() {
        // Simulate a regular Profile.
        doReturn(false).when(mProfile).isOffTheRecord();
        doReturn(false).when(mProfile).isIncognitoBranded();

        TabCollectionTabModelImpl model = getModel(mProfile, mTabModelDelegate);

        model.setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelDelegate).selectModel(/* incognito= */ false);

        // Simulate an incognito Profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        doReturn(true).when(mProfile).isIncognitoBranded();
        reset(mTabModelDelegate);

        TabCollectionTabModelImpl incognitoModel = getModel(mProfile, mTabModelDelegate);

        incognitoModel.setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelDelegate).selectModel(/* incognito= */ true);

        // Simulate an ephemeral profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        doReturn(false).when(mProfile).isIncognitoBranded();
        reset(mTabModelDelegate);

        TabCollectionTabModelImpl ephemeralModel = getModel(mProfile, mTabModelDelegate);

        ephemeralModel.setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelDelegate).selectModel(/* incognito= */ true);
    }

    @Test
    public void testConstructor_isIncognito() {
        // Mock a profile that is incognito.
        doReturn(true).when(mOtrProfile).isOffTheRecord();
        doReturn(true).when(mOtrProfile).isIncognitoBranded();

        TabCollectionTabModelImpl incognitoModel =
                new TabCollectionTabModelImpl(
                        mOtrProfile,
                        ActivityType.TABBED,
                        false,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        /* supportUndo= */ true);

        assertFalse(incognitoModel.supportsPendingClosures());
    }

    @Test
    public void testAddTab_willOpenInForeground() {
        // Mock a profile that is incognito.
        doReturn(true).when(mOtrProfile).isOffTheRecord();
        doReturn(true).when(mOtrProfile).isIncognitoBranded();
        when(mTabCollectionTabModelImplJni.init(any(), eq(mOtrProfile)))
                .thenReturn(TAB_COLLECTION_TAB_MODEL_IMPL_PTR);

        TabCollectionTabModelImpl incognitoModel =
                new TabCollectionTabModelImpl(
                        mOtrProfile,
                        ActivityType.TABBED,
                        false,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        false);

        MockTab tab = MockTab.createAndInitialize(123, mOtrProfile);
        tab.setIsInitialized(true);
        incognitoModel.addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mOrderController).willOpenInForeground(TabLaunchType.FROM_CHROME_UI, true);
    }

    @Test
    public void testRemoveTabsAndSelectNext_nextIsInOtherModel() {
        MockTab tabToClose = MockTab.createAndInitialize(123, mProfile);
        tabToClose.setIsInitialized(true);
        when(mOrderController.determineInsertionIndex(anyInt(), anyInt(), any())).thenReturn(0);
        mTabModel.addTab(
                tabToClose, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        MockTab nextTab = MockTab.createAndInitialize(456, mOtrProfile);
        nextTab.setIsInitialized(true);
        doReturn(true).when(mOtrProfile).isOffTheRecord();

        TabModel incognitoTabModel = mock();
        when(mTabModelDelegate.getModel(true)).thenReturn(incognitoTabModel);

        mTabModel.closeTabs(
                TabClosureParams.closeTab(tabToClose)
                        .allowUndo(false)
                        .recommendedNextTab(nextTab)
                        .build());
        verify(mTabModelDelegate).getModel(true);
    }

    @Test
    public void testGetTabCreator_isIncognito() {
        // Mock a profile that is incognito.
        doReturn(true).when(mOtrProfile).isOffTheRecord();
        doReturn(true).when(mOtrProfile).isIncognitoBranded();

        TabCollectionTabModelImpl incognitoModel =
                new TabCollectionTabModelImpl(
                        mOtrProfile,
                        ActivityType.TABBED,
                        false,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        false);

        assertEquals(mIncognitoTabCreator, incognitoModel.getTabCreator());
    }

    private static TabCollectionTabModelImpl getModel(
            Profile profile, TabModelDelegate tabModelDelegate) {
        return new TabCollectionTabModelImpl(
                profile,
                ActivityType.CUSTOM_TAB,
                false,
                null,
                null,
                null,
                null,
                null,
                tabModelDelegate,
                null,
                null,
                null,
                false);
    }
}
