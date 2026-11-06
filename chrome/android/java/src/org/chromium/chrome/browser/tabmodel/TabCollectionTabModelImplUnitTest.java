// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.components.visited_url_ranking.url_grouping.TabSelectionCause;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link TabCollectionTabModelImpl}. More substantive tests are in {@link
 * TabCollectionTabModelImplTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS)
public class TabCollectionTabModelImplUnitTest {
    private static final long TAB_MODEL_JNI_BRIDGE_PTR = 875943L;
    private static final long TAB_COLLECTION_TAB_MODEL_IMPL_PTR = 378492L;
    private static final @TabRestoreEntryId int ENTRY_ID = 42;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private TabCollectionTabModelImpl.Natives mTabCollectionTabModelImplJni;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJni;
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
    @Mock private ScopedStorageBatch mScopedStorageBatch;
    @Mock private TabModelObserver mTabModelObserver;
    @Mock private PendingTabClosureManager mPendingTabClosureManager;

    private TabCollectionTabModelImpl mTabModel;
    private List<Tab> mTabs;
    private boolean mExpectNoJniCalls;
    private int mBatchGetCount;

    @Before
    public void setUp() {
        // Required to use MockTab.
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJni);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isIncognitoBranded()).thenReturn(false);
        when(mOtrProfile.isOffTheRecord()).thenReturn(true);
        when(mOtrProfile.isIncognitoBranded()).thenReturn(true);

        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridgeJni);
        when(mTabModelJniBridgeJni.init(
                        any(TabModelJniBridge.class),
                        eq(mProfile),
                        eq(ActivityType.TABBED),
                        eq(null),
                        eq(TabModelType.STANDARD)))
                .thenAnswer(inv -> JniUniquePtr.createForTesting(TAB_MODEL_JNI_BRIDGE_PTR));

        TabCollectionTabModelImplJni.setInstanceForTesting(mTabCollectionTabModelImplJni);
        when(mTabCollectionTabModelImplJni.init(eq(mProfile)))
                .thenReturn(TAB_COLLECTION_TAB_MODEL_IMPL_PTR);

        mTabs = new ArrayList<>();
        mExpectNoJniCalls = false;

        when(mTabCollectionTabModelImplJni.getAllTabs(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR)))
                .thenAnswer(
                        invocation -> {
                            if (mExpectNoJniCalls) {
                                org.junit.Assert.fail("JNI getAllTabs called when not expected");
                            }
                            return mTabs;
                        });

        when(mTabCollectionTabModelImplJni.addTabRecursive(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR),
                        any(Tab.class),
                        anyInt(),
                        any(),
                        anyBoolean(),
                        anyBoolean()))
                .thenAnswer(
                        invocation -> {
                            Tab tab = invocation.getArgument(1);
                            int index = invocation.getArgument(2);
                            mTabs.add(index, tab);
                            return index;
                        });

        doAnswer(
                        invocation -> {
                            Tab tab = invocation.getArgument(1);
                            mTabs.remove(tab);
                            return null;
                        })
                .when(mTabCollectionTabModelImplJni)
                .removeTabRecursive(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), any(Tab.class));

        when(mTabCollectionTabModelImplJni.moveTabRecursive(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR),
                        anyInt(),
                        anyInt(),
                        any(),
                        anyBoolean()))
                .thenAnswer(
                        invocation -> {
                            int oldIndex = invocation.getArgument(1);
                            int newIndex = invocation.getArgument(2);
                            Token newGroupId = invocation.getArgument(3);
                            Tab tab = mTabs.get(oldIndex);
                            mTabs.remove(oldIndex);
                            tab.setTabGroupId(newGroupId);
                            mTabs.add(newIndex, tab);
                            return newIndex;
                        });

        when(mTabCollectionTabModelImplJni.getTabsInGroup(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), any()))
                .thenAnswer(
                        invocation -> {
                            Token groupId = invocation.getArgument(1);
                            if (groupId == null) return Collections.emptyList();
                            List<Tab> result = new ArrayList<>();
                            for (Tab t : mTabs) {
                                if (groupId.equals(t.getTabGroupId())) {
                                    result.add(t);
                                }
                            }
                            return result;
                        });

        when(mTabCollectionTabModelImplJni.getTabCountForGroup(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), any()))
                .thenAnswer(
                        invocation -> {
                            Token groupId = invocation.getArgument(1);
                            if (groupId == null) return 0;
                            int count = 0;
                            for (Tab t : mTabs) {
                                if (groupId.equals(t.getTabGroupId())) {
                                    count++;
                                }
                            }
                            return count;
                        });

        when(mTabCollectionTabModelImplJni.moveTabGroupTo(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), any(), anyInt()))
                .thenAnswer(
                        invocation -> {
                            Token groupId = invocation.getArgument(1);
                            int newIndex = invocation.getArgument(2);
                            List<Tab> groupTabs = new ArrayList<>();
                            for (Tab t : mTabs) {
                                if (groupId.equals(t.getTabGroupId())) {
                                    groupTabs.add(t);
                                }
                            }
                            mTabs.removeAll(groupTabs);
                            mTabs.addAll(newIndex, groupTabs);
                            return newIndex;
                        });

        mTabModel =
                new TabCollectionTabModelImpl(
                        mProfile,
                        ActivityType.TABBED,
                        /* customTabProfileType= */ null,
                        TabModelType.STANDARD,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        () -> {
                            mBatchGetCount++;
                            return mScopedStorageBatch;
                        },
                        /* supportUndo= */ false);
        mTabModel.addObserver(mTabModelObserver);

        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);
    }

    @After
    public void tearDown() {
        mTabModel.destroy();
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

        verify(mTabModelJniBridgeJni).broadcastSessionRestoreComplete(any());
    }

    @Test
    public void testCompleteInitializationTwice() {
        mTabModel.completeInitialization();
        assertThrows(AssertionError.class, mTabModel::completeInitialization);
    }

    @Test
    public void testIsTabStateInitialized() {
        when(mTabModelDelegate.isTabStateInitialized()).thenReturn(false);
        assertFalse(mTabModel.isTabStateInitialized());
        assertTrue(mTabModel.isSessionRestoreInProgress());

        when(mTabModelDelegate.isTabStateInitialized()).thenReturn(true);
        assertTrue(mTabModel.isTabStateInitialized());
        assertFalse(mTabModel.isSessionRestoreInProgress());
    }

    @Test
    public void testAddTabBasic() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);
        tab.setIsInitialized(true);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);

        assertEquals(tab, mTabModel.getTabById(tabId));
        verifyBatchedAndReset();
    }

    @Test
    public void testAddTabDuplicate() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);
        tab.setIsInitialized(true);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();

        assertThrows(
                AssertionError.class,
                () ->
                        mTabModel.addTab(
                                tab,
                                /* index= */ 1,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND));
        verify(mScopedStorageBatch, atLeastOnce()).close();
    }

    @Test
    public void testAddTabWrongModel() {
        @TabId int tabId = 789;
        MockTab otrTab = createMockTab(tabId, mOtrProfile);
        otrTab.setIsInitialized(true);
        assertThrows(
                IllegalStateException.class,
                () ->
                        mTabModel.addTab(
                                otrTab,
                                /* index= */ 1,
                                TabLaunchType.FROM_CHROME_UI,
                                TabCreationState.LIVE_IN_FOREGROUND));
        verifyBatchedAndReset();
    }

    @Test
    public void testAddTab_MigratesRootIdVisualDataAndResetsRootId() {
        @TabId int rootId = 456;
        @TabId int tabId = 789;
        Token tabGroupId = new Token(123L, 456L);
        String groupTitle = "Restored Group";
        int groupColor = 1;
        Context context = ContextUtils.getApplicationContext();
        String rootIdKey = String.valueOf(rootId);

        context.getSharedPreferences(
                        TabGroupVisualDataStore.TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE)
                .edit()
                .putString(rootIdKey, groupTitle)
                .apply();
        context.getSharedPreferences(
                        TabGroupVisualDataStore.TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE)
                .edit()
                .putInt(rootIdKey, groupColor)
                .apply();

        MockTab tab = createMockTab(tabId, mProfile);
        tab.setRootId(rootId);
        tab.setTabGroupId(tabGroupId);
        tab.setIsInitialized(true);

        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_RESTORE,
                TabCreationState.FROZEN_FOR_LAZY_LOAD);

        assertEquals(tabId, tab.getRootId());
        assertEquals(groupTitle, TabGroupVisualDataStore.getTabGroupTitle(tabGroupId));
        assertEquals(groupColor, TabGroupVisualDataStore.getTabGroupColor(tabGroupId));
        verify(mTabCollectionTabModelImplJni)
                .createTabGroup(
                        TAB_COLLECTION_TAB_MODEL_IMPL_PTR,
                        tabGroupId,
                        groupTitle,
                        groupColor,
                        /* isCollapsed= */ false);
        verifyBatchedAndReset();
    }

    @Test
    public void testGetCount() {
        mTabs.add(createMockTab(1, mProfile));
        mTabs.add(createMockTab(2, mProfile));
        mTabs.add(createMockTab(3, mProfile));
        mTabs.add(createMockTab(4, mProfile));
        mTabs.add(createMockTab(5, mProfile));
        assertEquals("Incorrect tab count", 5, mTabModel.getCount());
        verify(mTabCollectionTabModelImplJni).getAllTabs(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR));
    }

    @Test
    public void testGetCount_nativeNotInitialized() {
        mTabModel.destroy();
        mExpectNoJniCalls = true;
        assertEquals(
                "Tab count should be 0 when native is not initialized", 0, mTabModel.getCount());
        verify(mTabModelObserver, atLeastOnce()).onDestroy();
    }

    @Test
    public void testIndexOf() {
        MockTab tab = createMockTab(123, mProfile);
        tab.setIsInitialized(true);
        mTabs.add(createMockTab(1, mProfile));
        mTabs.add(createMockTab(2, mProfile));
        mTabs.add(tab);
        assertEquals("Incorrect tab index", 2, mTabModel.indexOf(tab));
        verify(mTabCollectionTabModelImplJni).getAllTabs(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR));
    }

    @Test
    public void testIndexOf_tabNotFound() {
        MockTab tab = createMockTab(123, mProfile);
        tab.setIsInitialized(true);
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
        verify(mTabCollectionTabModelImplJni, never()).getAllTabs(anyLong());
    }

    @Test
    public void testIndexOf_nativeNotInitialized() {
        mTabModel.destroy(); // Destroys native ptr.
        mExpectNoJniCalls = true;
        assertEquals(
                "Index should be invalid when native is not initialized",
                TabList.INVALID_TAB_INDEX,
                mTabModel.indexOf(MockTab.createAndInitialize(123, mProfile)));
    }

    @Test
    public void testIsTabInTabGroup() {
        MockTab tab = createMockTab(123, mProfile);
        tab.setIsInitialized(true);
        assertFalse(mTabModel.isTabInTabGroup(tab));
        tab.setTabGroupId(new Token(1L, 2L));
        assertTrue(mTabModel.isTabInTabGroup(tab));
    }

    @Test
    public void testGetRelatedTabList_Basic() {
        int tabId = 123;
        MockTab tab1 = createMockTab(tabId, mProfile);
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
    public void testGetTabsInGroup_NativeNotInitialized() {
        mTabModel.destroy();
        mExpectNoJniCalls = true;
        assertEquals(Collections.emptyList(), mTabModel.getTabsInGroup(new Token(1L, 2L)));
    }

    @Test
    public void testMergeListOfTabsToExistingGroup() {
        Token groupId = new Token(1L, 2L);
        MockTab tab0 = createMockTab(0, mProfile);
        MockTab tab1 = createMockTab(1, mProfile);
        MockTab tab2 = createMockTab(2, mProfile);
        MockTab tab3 = createMockTab(3, mProfile);
        MockTab tab4 = createMockTab(4, mProfile);
        MockTab tab5 = createMockTab(5, mProfile);
        tab2.setTabGroupId(groupId);
        tab3.setTabGroupId(groupId);
        mTabs.addAll(List.of(tab0, tab1, tab2, tab3, tab4, tab5));

        when(mTabCollectionTabModelImplJni.tabGroupExists(
                        TAB_COLLECTION_TAB_MODEL_IMPL_PTR, groupId))
                .thenReturn(true);

        // Merging into a non-existent group should no-op.
        mTabModel.mergeListOfTabsToExistingGroup(
                List.of(tab1),
                new Token(9L, 9L),
                /* indexInGroup= */ null,
                TabGroupMergeNotificationType.DONT_NOTIFY);
        assertEquals(List.of(tab0, tab1, tab2, tab3, tab4, tab5), mTabs);

        // Append tab5 to the end of the existing group.
        mTabModel.mergeListOfTabsToExistingGroup(
                List.of(tab5),
                groupId,
                /* indexInGroup= */ null,
                TabGroupMergeNotificationType.DONT_NOTIFY);
        assertEquals(groupId, tab5.getTabGroupId());
        assertEquals(List.of(tab0, tab1, tab2, tab3, tab5, tab4), mTabs);

        // Insert tab1 at index 1 in the existing group.
        mTabModel.mergeListOfTabsToExistingGroup(
                List.of(tab1),
                groupId,
                /* indexInGroup= */ 1,
                TabGroupMergeNotificationType.DONT_NOTIFY);
        assertEquals(groupId, tab1.getTabGroupId());
        assertEquals(List.of(tab0, tab2, tab1, tab3, tab5, tab4), mTabs);
    }

    @Test
    public void testSetIndex() {
        // Simulate a regular Profile.
        doReturn(false).when(mProfile).isOffTheRecord();
        doReturn(false).when(mProfile).isIncognitoBranded();

        TabCollectionTabModelImpl model =
                getModel(mProfile, mTabModelDelegate, mScopedStorageBatch);

        model.setIndex(0, TabSelectionCause.FROM_USER);
        verify(mTabModelDelegate).selectModel(/* incognito= */ false);
        verifyBatchedAndReset();

        // Simulate an incognito Profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        doReturn(true).when(mProfile).isIncognitoBranded();
        reset(mTabModelDelegate);

        TabCollectionTabModelImpl incognitoModel =
                getModel(mProfile, mTabModelDelegate, mScopedStorageBatch);

        incognitoModel.setIndex(0, TabSelectionCause.FROM_USER);
        verify(mTabModelDelegate).selectModel(/* incognito= */ true);
        verifyBatchedAndReset();

        // Simulate an ephemeral profile.
        doReturn(true).when(mProfile).isOffTheRecord();
        doReturn(false).when(mProfile).isIncognitoBranded();
        reset(mTabModelDelegate);

        TabCollectionTabModelImpl ephemeralModel =
                getModel(mProfile, mTabModelDelegate, mScopedStorageBatch);

        ephemeralModel.setIndex(0, TabSelectionCause.FROM_USER);
        verify(mTabModelDelegate).selectModel(/* incognito= */ true);
        verifyBatchedAndReset();
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
                        /* customTabProfileType= */ null,
                        TabModelType.STANDARD,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        () -> mScopedStorageBatch,
                        /* supportUndo= */ true);

        assertFalse(incognitoModel.supportsPendingClosures());
    }

    @Test
    public void testRemoveTabsAndSelectNext_closeLastIncognitoTabWithStubRegularModel() {
        doReturn(true).when(mOtrProfile).isOffTheRecord();
        doReturn(true).when(mOtrProfile).isIncognitoBranded();
        when(mTabCollectionTabModelImplJni.init(eq(mOtrProfile)))
                .thenReturn(TAB_COLLECTION_TAB_MODEL_IMPL_PTR);

        TabCollectionTabModelImpl incognitoModel =
                new TabCollectionTabModelImpl(
                        mOtrProfile,
                        ActivityType.TABBED,
                        /* customTabProfileType= */ null,
                        TabModelType.STANDARD,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        () -> mScopedStorageBatch,
                        /* supportUndo= */ false);

        MockTab tabToClose = createMockTab(123, mOtrProfile);
        tabToClose.setIsInitialized(true);
        when(mOrderController.determineInsertionIndex(anyInt(), anyInt(), any())).thenReturn(0);
        incognitoModel.addTab(
                tabToClose, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();

        StubTabModel stubRegularModel = new StubTabModel(/* isIncognito= */ false, mProfile);
        when(mTabModelDelegate.getModel(false)).thenReturn(stubRegularModel);
        when(mTabModelDelegate.getModel(true)).thenReturn(incognitoModel);

        incognitoModel.closeTabs(TabClosureParams.closeAllTabs().allowUndo(false).build());
        verify(mTabModelDelegate).getModel(true);
    }

    @Test
    public void testAddTab_willOpenInForeground() {
        // Mock a profile that is incognito.
        doReturn(true).when(mOtrProfile).isOffTheRecord();
        doReturn(true).when(mOtrProfile).isIncognitoBranded();
        when(mTabCollectionTabModelImplJni.init(eq(mOtrProfile)))
                .thenReturn(TAB_COLLECTION_TAB_MODEL_IMPL_PTR);

        TabCollectionTabModelImpl incognitoModel =
                new TabCollectionTabModelImpl(
                        mOtrProfile,
                        ActivityType.TABBED,
                        /* customTabProfileType= */ null,
                        TabModelType.STANDARD,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        () -> mScopedStorageBatch,
                        false);

        MockTab tab = createMockTab(123, mOtrProfile);
        tab.setIsInitialized(true);
        incognitoModel.addTab(
                tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mOrderController).willOpenInForeground(TabLaunchType.FROM_CHROME_UI, true);
        verifyBatchedAndReset();
    }

    @Test
    public void testRemoveTabsAndSelectNext_nextIsInOtherModel() {
        MockTab tabToClose = createMockTab(123, mProfile);
        tabToClose.setIsInitialized(true);
        when(mOrderController.determineInsertionIndex(anyInt(), anyInt(), any())).thenReturn(0);
        mTabModel.addTab(
                tabToClose, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();

        MockTab nextTab = createMockTab(456, mOtrProfile);
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
        verifyBatchedAndReset();
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
                        /* customTabProfileType= */ null,
                        TabModelType.STANDARD,
                        mRegularTabCreator,
                        mIncognitoTabCreator,
                        mOrderController,
                        mTabContentManager,
                        mNextTabPolicySupplier,
                        mTabModelDelegate,
                        mAsyncTabParamsManager,
                        mTabRemover,
                        mTabUngrouper,
                        () -> mScopedStorageBatch,
                        false);

        assertEquals(mIncognitoTabCreator, incognitoModel.getTabCreator());
    }

    @Test
    public void testGetMostRecentClosureTime() {
        // With pending closures supported, should return pending closure manager's time.
        when(mPendingTabClosureManager.getMostRecentClosureTime()).thenReturn(10L);
        mTabModel.setPendingTabClosureManagerForTesting(mPendingTabClosureManager);
        assertEquals(10L, mTabModel.getMostRecentClosureTime());

        // With pending closures supported but no time, should return model delegate's time.
        when(mPendingTabClosureManager.getMostRecentClosureTime()).thenReturn(0L);
        when(mTabModelDelegate.getMostRecentClosureTime()).thenReturn(5L);
        assertEquals(5L, mTabModel.getMostRecentClosureTime());

        // Without pending closures, should return model delegate's time.
        mTabModel.setPendingTabClosureManagerForTesting(null);
        assertEquals(5L, mTabModel.getMostRecentClosureTime());
    }

    @Test
    public void testAddTab_NotifyPendingTabClosureManager() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);
        tab.setIsInitialized(true);
        when(mPendingTabClosureManager.getRewoundList()).thenReturn(mock(TabList.class));
        mTabModel.setPendingTabClosureManagerForTesting(mPendingTabClosureManager);

        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        verify(mPendingTabClosureManager).notifyTabAdded(eq(tab), eq(0));
        verify(mPendingTabClosureManager, never()).resetState();
        verifyBatchedAndReset();
    }

    @Test
    public void testIsClosingAllTabs() {
        when(mTabModelDelegate.getModel(false)).thenReturn(mTabModel);

        assertFalse(mTabModel.isClosingAllTabs());

        MockTab tab1 = createMockTab(1, mProfile);
        tab1.setIsInitialized(true);
        mTabModel.addTab(
                tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();

        MockTab tab2 = createMockTab(2, mProfile);
        tab2.setIsInitialized(true);
        mTabModel.addTab(
                tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();

        assertFalse(mTabModel.isClosingAllTabs());

        mTabModel.closeTabs(TabClosureParams.closeTab(tab1).allowUndo(false).build());

        assertFalse(mTabModel.isClosingAllTabs());

        mTabModel.closeTabs(TabClosureParams.closeTab(tab2).allowUndo(false).build());

        assertTrue(mTabModel.isClosingAllTabs());
    }

    @Test
    public void testDetachTabForReparenting_BeforeDestroyPreservesTab() {
        when(mTabModelDelegate.isReparentingInProgress()).thenReturn(true);

        long newCollectionPtr = TAB_COLLECTION_TAB_MODEL_IMPL_PTR + 1;
        List<Tab> newTabs = new ArrayList<>();
        when(mTabCollectionTabModelImplJni.init(eq(mProfile))).thenReturn(newCollectionPtr);
        when(mTabCollectionTabModelImplJni.getAllTabs(eq(newCollectionPtr))).thenReturn(newTabs);
        when(mTabCollectionTabModelImplJni.addTabRecursive(
                        eq(newCollectionPtr),
                        any(Tab.class),
                        anyInt(),
                        any(),
                        anyBoolean(),
                        anyBoolean()))
                .thenAnswer(
                        invocation -> {
                            Tab addedTab = invocation.getArgument(1);
                            int index = invocation.getArgument(2);
                            newTabs.add(index, addedTab);
                            return index;
                        });
        TabCollectionTabModelImpl newTabModel =
                getModel(mProfile, mTabModelDelegate, mScopedStorageBatch);

        MockTab tab = createMockTab(1, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();
        mTabModel.setTabsMultiSelected(Collections.singleton(1), /* isSelected= */ true);

        mTabModel.addObserver(mTabModelObserver);
        mTabModel.detachTabForReparenting(tab);

        assertEquals(0, mTabModel.getCount());
        assertNull(mTabModel.getTabById(1));
        assertNull(mTabModel.getCurrentTabSupplier().get());
        assertFalse(mTabModel.isTabMultiSelected(1));
        verify(mTabCollectionTabModelImplJni)
                .removeTabRecursive(TAB_COLLECTION_TAB_MODEL_IMPL_PTR, tab);
        verify(mTabModelObserver, never()).tabRemoved(any());
        verify(mTabModelObserver, never()).willCloseTab(any(), anyBoolean());

        // Restore the tab into the recreated Activity's model, remove its AsyncTabParams, and run
        // OldActivity.onDestroy(). The tab in newTabModel must remain live (not destroyed).
        newTabModel.addTab(tab, 0, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE);
        when(mAsyncTabParamsManager.hasParamsForTabId(1)).thenReturn(false);
        mTabModel.destroy();
        assertEquals(1, newTabModel.getCount());
        assertEquals(tab, newTabModel.getTabAt(0));
        assertFalse(newTabModel.getTabAt(0).isDestroyed());

        newTabModel.destroy();
    }

    @Test
    public void testSetTabsMultiSelected_Add() {
        addTabsAndResetObserver(/* count= */ 4);

        Set<Integer> tabsToAdd = new HashSet<>(Arrays.asList(1, 2, 3));
        mTabModel.setTabsMultiSelected(tabsToAdd, /* isSelected= */ true);

        for (int tabId : tabsToAdd) {
            assertTrue(mTabModel.isTabMultiSelected(tabId));
        }
        assertFalse(mTabModel.isTabMultiSelected(4));
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    public void testSetTabsMultiSelected_Remove() {
        addTabsAndResetObserver(/* count= */ 4);
        mTabModel.setTabsMultiSelected(
                new HashSet<>(Arrays.asList(1, 2, 3, 4)), /* isSelected= */ true);
        reset(mTabModelObserver);

        mTabModel.setTabsMultiSelected(new HashSet<>(Arrays.asList(2, 4)), /* isSelected= */ false);

        assertFalse(mTabModel.isTabMultiSelected(2));
        assertFalse(mTabModel.isTabMultiSelected(4));
        assertTrue(mTabModel.isTabMultiSelected(1));
        assertTrue(mTabModel.isTabMultiSelected(3));
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    public void testClearMultiSelection_WithNotification() {
        addTabsAndResetObserver(/* count= */ 4);
        mTabModel.setTabsMultiSelected(
                new HashSet<>(Arrays.asList(1, 2, 3)), /* isSelected= */ true);
        reset(mTabModelObserver);

        mTabModel.clearMultiSelection(/* notifyObservers= */ true);

        assertFalse(mTabModel.isTabMultiSelected(2));
        assertFalse(mTabModel.isTabMultiSelected(3));
        verify(mTabModelObserver, times(1)).onTabsSelectionChanged();
    }

    @Test
    public void testClearMultiSelection_WithoutNotification() {
        addTabsAndResetObserver(/* count= */ 4);
        mTabModel.setTabsMultiSelected(
                new HashSet<>(Arrays.asList(1, 2, 3)), /* isSelected= */ true);
        reset(mTabModelObserver);

        mTabModel.clearMultiSelection(/* notifyObservers= */ false);

        assertFalse(mTabModel.isTabMultiSelected(2));
        assertFalse(mTabModel.isTabMultiSelected(3));
        verify(mTabModelObserver, never()).onTabsSelectionChanged();
    }

    @Test
    public void testIsTabMultiSelected() {
        // The first tab added becomes the active tab.
        addTabsAndResetObserver(/* count= */ 3);
        int currentTabId = mTabModel.getCurrentTabSupplier().get().getId();
        int otherSelectedTabId = 2;
        int unselectedTabId = 3;
        assertTrue(
                "The active tab should always be considered selected.",
                mTabModel.isTabMultiSelected(currentTabId));
        mTabModel.setTabsMultiSelected(
                new HashSet<>(Arrays.asList(currentTabId, otherSelectedTabId)),
                /* isSelected= */ true);

        assertTrue(
                "Tab explicitly added to the set should be selected.",
                mTabModel.isTabMultiSelected(otherSelectedTabId));
        assertTrue(
                "The active tab should always be considered selected.",
                mTabModel.isTabMultiSelected(currentTabId));
        assertFalse(
                "A tab not in the set and not active should not be selected.",
                mTabModel.isTabMultiSelected(unselectedTabId));
    }

    @Test
    public void testRestoreClosedTabsForUndo_unrestorableStateNotRestored() {
        UndoClosureSnapshot savedGroupsSnapshot =
                UndoClosureSnapshot.forSavedGroups(
                        List.of(createClosedTab(/* id= */ 5, /* wasSelected= */ false)),
                        List.of("sync_id"),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0);
        assertArrayEquals(
                new int[] {Tab.INVALID_TAB_ID},
                mTabModel.restoreClosedTabsForUndo(savedGroupsSnapshot, /* reselect= */ true));

        UndoClosureSnapshot invalidEntryIdSnapshot =
                createSnapshot(
                        TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID,
                        createClosedTab(/* id= */ 5, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 6, /* wasSelected= */ true));
        assertArrayEquals(
                new int[] {Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID},
                mTabModel.restoreClosedTabsForUndo(invalidEntryIdSnapshot, /* reselect= */ true));

        UndoClosureSnapshot validSnapshot =
                createSnapshot(ENTRY_ID, createClosedTab(/* id= */ 5, /* wasSelected= */ true));
        mTabModel.destroy();
        mExpectNoJniCalls = true;
        assertArrayEquals(
                new int[] {Tab.INVALID_TAB_ID},
                mTabModel.restoreClosedTabsForUndo(validSnapshot, /* reselect= */ true));

        verify(mTabModelDelegate, never()).restoreForUndo(any(), anyInt(), any());
        verify(mScopedStorageBatch, never()).close();
    }

    @Test
    public void testRestoreClosedTabsForUndo_restoresTabsInStorageBatchAndNoReselect() {
        mTabModel.setActive(/* active= */ true);
        MockTab selectedTab = addSelectedTab(/* tabId= */ 1);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        ENTRY_ID,
                        createClosedTab(/* id= */ 5, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 6, /* wasSelected= */ true));
        MockTab restoredTab1 = createMockTab(/* tabId= */ 105, mProfile);
        MockTab restoredTab2 = createMockTab(/* tabId= */ 106, mProfile);
        int batchGetCountBeforeRestore = mBatchGetCount;
        when(mTabModelDelegate.restoreForUndo(mTabModel, ENTRY_ID, snapshot.tabs))
                .thenAnswer(
                        invocation -> {
                            // The outer restore batch is open while the delegate restores tabs.
                            assertEquals(batchGetCountBeforeRestore + 1, mBatchGetCount);
                            verify(mScopedStorageBatch, never()).close();
                            addRestoredTab(restoredTab1);
                            addRestoredTab(restoredTab2);
                            // Drop the nested addTab batches so only the outer restore batch's
                            // close() is verified.
                            reset(mScopedStorageBatch);
                            return new int[] {105, 106};
                        });

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ false);

        assertArrayEquals(new int[] {105, 106}, restoredIds);
        assertEquals(restoredTab1, mTabModel.getTabById(105));
        assertEquals(restoredTab2, mTabModel.getTabById(106));
        assertEquals(selectedTab, mTabModel.getCurrentTabSupplier().get());
        verify(mTabModelObserver, never())
                .didSelectTab(any(), eq(TabSelectionType.FROM_UNDO), anyInt());
        verify(mTabModelDelegate).restoreForUndo(mTabModel, ENTRY_ID, snapshot.tabs);
        verifyBatchedAndReset();
    }

    @Test
    public void testRestoreClosedTabsForUndo_reselectsPreviouslySelectedTab() {
        mTabModel.setActive(/* active= */ true);
        addSelectedTab(/* tabId= */ 1);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        ENTRY_ID,
                        createClosedTab(/* id= */ 5, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 6, /* wasSelected= */ true));
        MockTab restoredTab1 = createMockTab(/* tabId= */ 105, mProfile);
        MockTab restoredTab2 = createMockTab(/* tabId= */ 106, mProfile);
        stubRestoreForUndo(snapshot, restoredTab1, restoredTab2);

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true);

        assertArrayEquals(new int[] {105, 106}, restoredIds);
        assertEquals(restoredTab2, mTabModel.getCurrentTabSupplier().get());
        assertEquals(mTabModel.indexOf(restoredTab2), mTabModel.index());
        verify(mTabModelObserver)
                .didSelectTab(restoredTab2, TabSelectionType.FROM_UNDO, /* lastId= */ 1);
    }

    @Test
    public void testRestoreClosedTabsForUndo_selectedTabNotRestoredKeepsSelection() {
        mTabModel.setActive(/* active= */ true);
        MockTab selectedTab = addSelectedTab(/* tabId= */ 1);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        ENTRY_ID,
                        createClosedTab(/* id= */ 5, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 6, /* wasSelected= */ true));
        stubRestoreForUndo(snapshot, createMockTab(/* tabId= */ 105, mProfile), null);

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true);

        assertArrayEquals(new int[] {105, Tab.INVALID_TAB_ID}, restoredIds);
        assertEquals(selectedTab, mTabModel.getCurrentTabSupplier().get());
        verify(mTabModelObserver, never())
                .didSelectTab(any(), eq(TabSelectionType.FROM_UNDO), anyInt());
    }

    @Test
    public void testRestoreClosedTabsForUndo_noSelectedTabSelectsFirstRestoredTab() {
        mTabModel.setActive(/* active= */ true);
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        ENTRY_ID,
                        createClosedTab(/* id= */ 5, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 6, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 7, /* wasSelected= */ false));
        MockTab restoredTab2 = createMockTab(/* tabId= */ 106, mProfile);
        stubRestoreForUndo(snapshot, null, restoredTab2, createMockTab(/* tabId= */ 107, mProfile));
        assertEquals(TabList.INVALID_TAB_INDEX, mTabModel.index());

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true);

        assertArrayEquals(new int[] {Tab.INVALID_TAB_ID, 106, 107}, restoredIds);
        assertEquals(restoredTab2, mTabModel.getCurrentTabSupplier().get());
        verify(mTabModelObserver)
                .didSelectTab(
                        restoredTab2, TabSelectionType.FROM_UNDO, /* lastId= */ Tab.INVALID_TAB_ID);
    }

    @Test
    public void testRestoreClosedTabsForUndo_inactiveModelDoesNotFallbackSelect() {
        mTabModel.setActive(/* active= */ false);
        UndoClosureSnapshot snapshot =
                createSnapshot(ENTRY_ID, createClosedTab(/* id= */ 5, /* wasSelected= */ false));
        MockTab restoredTab = createMockTab(/* tabId= */ 105, mProfile);
        stubRestoreForUndo(snapshot, restoredTab);

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true);

        assertArrayEquals(new int[] {105}, restoredIds);
        assertEquals(restoredTab, mTabModel.getCurrentTabSupplier().get());
        verify(mTabModelDelegate, never()).selectModel(anyBoolean());
        verify(mTabModelObserver, never())
                .didSelectTab(any(), eq(TabSelectionType.FROM_UNDO), anyInt());
    }

    @Test
    public void testRestoreClosedTabsForUndo_wrongLengthDelegateResultReturnsInvalidIds() {
        UndoClosureSnapshot snapshot =
                createSnapshot(
                        ENTRY_ID,
                        createClosedTab(/* id= */ 5, /* wasSelected= */ false),
                        createClosedTab(/* id= */ 6, /* wasSelected= */ true));
        when(mTabModelDelegate.restoreForUndo(mTabModel, ENTRY_ID, snapshot.tabs))
                .thenReturn(new int[] {105});

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true);

        assertArrayEquals(new int[] {Tab.INVALID_TAB_ID, Tab.INVALID_TAB_ID}, restoredIds);
        verify(mTabModelObserver, never())
                .didSelectTab(any(), eq(TabSelectionType.FROM_UNDO), anyInt());
        verifyBatchedAndReset();
    }

    @Test
    public void testRestoreClosedTabsForUndo_restoresTabGroupBeforeSelection() {
        mTabModel.setActive(/* active= */ true);
        Token groupId = new Token(1L, 2L);
        ClosedTab closedTab =
                new ClosedTab(
                        /* id= */ 5,
                        /* title= */ "Tab 5",
                        groupId,
                        /* indexInGroup= */ 0,
                        /* modelIndex= */ 0,
                        /* pinned= */ false,
                        /* wasSelected= */ true);
        UndoClosureSnapshot snapshot = createSnapshot(ENTRY_ID, closedTab);
        MockTab restoredTab = createMockTab(/* tabId= */ 105, mProfile);
        stubRestoreForUndo(snapshot, restoredTab);
        doAnswer(
                        invocation -> {
                            assertEquals(groupId, restoredTab.getTabGroupId());
                            return null;
                        })
                .when(mTabModelObserver)
                .didSelectTab(
                        restoredTab, TabSelectionType.FROM_UNDO, /* lastId= */ Tab.INVALID_TAB_ID);

        int[] restoredIds = mTabModel.restoreClosedTabsForUndo(snapshot, /* reselect= */ true);

        assertArrayEquals(new int[] {105}, restoredIds);
        assertEquals(groupId, restoredTab.getTabGroupId());
        assertEquals(restoredTab, mTabModel.getCurrentTabSupplier().get());
        verify(mTabCollectionTabModelImplJni)
                .createTabGroup(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR),
                        eq(groupId),
                        any(),
                        anyInt(),
                        anyBoolean());
        verify(mTabModelObserver)
                .didSelectTab(
                        restoredTab, TabSelectionType.FROM_UNDO, /* lastId= */ Tab.INVALID_TAB_ID);
        verify(mScopedStorageBatch, atLeastOnce()).close();
    }

    private void addTabsAndResetObserver(int count) {
        for (int i = 1; i <= count; i++) {
            MockTab tab = createMockTab(i, mProfile);
            tab.setIsInitialized(/* isInitialized= */ true);
            mTabModel.addTab(
                    tab,
                    /* index= */ i - 1,
                    TabLaunchType.FROM_CHROME_UI,
                    TabCreationState.LIVE_IN_FOREGROUND);
            verifyBatchedAndReset();
        }
        reset(mTabModelObserver);
    }

    private void verifyBatchedAndReset() {
        verify(mScopedStorageBatch).close();
        reset(mScopedStorageBatch);
    }

    private MockTab createMockTab(@TabId int tabId, Profile profile) {
        MockTab tab = MockTab.createAndInitialize(tabId, profile);
        tab.setWebContentsOverrideForTesting(mock(WebContents.class));
        return tab;
    }

    private TabCollectionTabModelImpl getModel(
            Profile profile, TabModelDelegate tabModelDelegate, ScopedStorageBatch batch) {
        return new TabCollectionTabModelImpl(
                profile,
                ActivityType.CUSTOM_TAB,
                CustomTabProfileType.REGULAR,
                TabModelType.STANDARD,
                null,
                null,
                mOrderController,
                null,
                null,
                tabModelDelegate,
                mAsyncTabParamsManager,
                null,
                mTabUngrouper,
                () -> batch,
                false);
    }

    private MockTab addSelectedTab(@TabId int tabId) {
        MockTab tab = createMockTab(tabId, mProfile);
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        verifyBatchedAndReset();
        mTabModel.setIndex(mTabModel.indexOf(tab), TabSelectionType.FROM_USER);
        verifyBatchedAndReset();
        assertEquals(tab, mTabModel.getCurrentTabSupplier().get());
        return tab;
    }

    /**
     * Stubs the delegate to add {@code restoredTabs} to the model, using the same indexing as
     * {@code snapshot.tabs}. A null entry means the tab at that placement index is not restored.
     */
    private void stubRestoreForUndo(UndoClosureSnapshot snapshot, MockTab... restoredTabs) {
        when(mTabModelDelegate.restoreForUndo(mTabModel, snapshot.entryId, snapshot.tabs))
                .thenAnswer(
                        invocation -> {
                            int[] restoredIds = new int[restoredTabs.length];
                            for (int i = 0; i < restoredTabs.length; i++) {
                                MockTab tab = restoredTabs[i];
                                if (tab == null) {
                                    restoredIds[i] = Tab.INVALID_TAB_ID;
                                    continue;
                                }
                                addRestoredTab(tab);
                                restoredIds[i] = tab.getId();
                            }
                            // Drop the nested addTab batches so only the outer restore batch's
                            // close() is verified.
                            reset(mScopedStorageBatch);
                            return restoredIds;
                        });
    }

    private void addRestoredTab(MockTab tab) {
        // The mocked order controller inserts at index 0 regardless of the requested index.
        mTabModel.addTab(
                tab,
                /* index= */ 0,
                TabLaunchType.FROM_UNDO_CLOSURE,
                TabCreationState.LIVE_IN_BACKGROUND);
    }

    private static UndoClosureSnapshot createSnapshot(
            @TabRestoreEntryId int entryId, ClosedTab... tabs) {
        return UndoClosureSnapshot.forTabModel(
                List.of(tabs),
                /* groups= */ Collections.emptyMap(),
                /* isTabGroupSyncEnabled= */ false,
                /* isAllTabs= */ false,
                entryId,
                TabClosingSource.UNKNOWN,
                /* undoRunnable= */ null);
    }

    private static ClosedTab createClosedTab(@TabId int id, boolean wasSelected) {
        return new ClosedTab(
                id,
                /* title= */ "Tab " + id,
                /* groupId= */ null,
                /* indexInGroup= */ TabList.INVALID_TAB_INDEX,
                /* modelIndex= */ id,
                /* pinned= */ false,
                wasSelected);
    }
}
