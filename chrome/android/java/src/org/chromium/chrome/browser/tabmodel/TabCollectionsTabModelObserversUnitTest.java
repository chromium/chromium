// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroidJni;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.ScopedStorageBatch;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link TabModelObserver} callbacks in {@link TabCollectionTabModelImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS)
public class TabCollectionsTabModelObserversUnitTest {
    private static final long TAB_MODEL_JNI_BRIDGE_PTR = 875943L;
    private static final long TAB_COLLECTION_TAB_MODEL_IMPL_PTR = 378492L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelJniBridge.Natives mTabModelJniBridgeJni;
    @Mock private TabCollectionTabModelImpl.Natives mTabCollectionTabModelImplJni;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureDevicesDispatcherAndroidJni;
    @Mock private UkmRecorder.Natives mUkmRecorderJni;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJni;
    @Mock private Profile mProfile;
    @Mock private TabCreator mRegularTabCreator;
    @Mock private TabCreator mIncognitoTabCreator;
    @Mock private TabModelOrderController mOrderController;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabModelDelegate mTabModelDelegate;
    @Mock private NextTabPolicySupplier mNextTabPolicySupplier;
    @Mock private AsyncTabParamsManager mAsyncTabParamsManager;
    @Mock private TabRemover mTabRemover;
    @Mock private ScopedStorageBatch mScopedStorageBatch;
    @Mock private TabModelObserver mTabModelObserver;

    private TabCollectionTabModelImpl mTabModel;
    private List<Tab> mTabs;

    @Before
    public void setUp() {
        // Required to use MockTab.
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(
                mMediaCaptureDevicesDispatcherAndroidJni);
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJni);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJni);
        when(mTabGroupSyncFeaturesJni.isTabGroupSyncEnabled(any())).thenReturn(false);

        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mProfile.isIncognitoBranded()).thenReturn(false);

        TabModelJniBridgeJni.setInstanceForTesting(mTabModelJniBridgeJni);
        when(mTabModelJniBridgeJni.init(
                        any(TabModelJniBridge.class),
                        eq(mProfile),
                        eq(ActivityType.TABBED),
                        eq(null),
                        eq(TabModelType.STANDARD)))
                .thenReturn(TAB_MODEL_JNI_BRIDGE_PTR);

        TabCollectionTabModelImplJni.setInstanceForTesting(mTabCollectionTabModelImplJni);
        when(mTabCollectionTabModelImplJni.init(any(), eq(mProfile)))
                .thenReturn(TAB_COLLECTION_TAB_MODEL_IMPL_PTR);

        mTabs = new ArrayList<>();

        when(mTabCollectionTabModelImplJni.getAllTabs(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR)))
                .thenAnswer(invocation -> new ArrayList<>(mTabs));

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
                            boolean isPinned = invocation.getArgument(4);
                            Tab tab = mTabs.remove(oldIndex);
                            tab.setTabGroupId(newGroupId);
                            tab.setIsPinned(isPinned);
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

        when(mTabCollectionTabModelImplJni.getAllTabGroupIds(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR)))
                .thenAnswer(
                        invocation -> {
                            Set<Token> groupIds = new HashSet<>();
                            for (Tab t : mTabs) {
                                if (t.getTabGroupId() != null) {
                                    groupIds.add(t.getTabGroupId());
                                }
                            }
                            return new ArrayList<>(groupIds);
                        });

        when(mTabCollectionTabModelImplJni.tabGroupExists(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), any()))
                .thenAnswer(
                        invocation -> {
                            Token groupId = invocation.getArgument(1);
                            if (groupId == null) return false;
                            for (Tab t : mTabs) {
                                if (groupId.equals(t.getTabGroupId())) {
                                    return true;
                                }
                            }
                            return false;
                        });

        when(mOrderController.determineInsertionIndex(anyInt(), anyInt(), any()))
                .thenAnswer(invocation -> invocation.getArgument(1));
        when(mOrderController.willOpenInForeground(anyInt(), anyBoolean())).thenReturn(true);

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
                        /* isIncognitoBranded= */ false,
                        (unused, tabModelSupplier) -> new PassthroughTabUngrouper(tabModelSupplier),
                        () -> mScopedStorageBatch,
                        /* supportUndo= */ true);
        mTabModel.addObserver(mTabModelObserver);

        when(mTabModelDelegate.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelDelegate.getModel(anyBoolean())).thenReturn(mTabModel);
    }

    @After
    public void tearDown() {
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(null);
        UkmRecorderJni.setInstanceForTesting(null);
        TabGroupSyncFeaturesJni.setInstanceForTesting(null);
        mTabModel.destroy();
        verify(mTabModelJniBridgeJni).destroy(eq(TAB_MODEL_JNI_BRIDGE_PTR));
        verify(mTabCollectionTabModelImplJni).destroy(eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR));
    }

    // 1. willAddTab

    @Test
    public void testWillAddTab_foreground() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mTabModelObserver).willAddTab(eq(tab), eq(TabLaunchType.FROM_CHROME_UI));
    }

    @Test
    public void testWillAddTab_background() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(
                tab, 0, TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabCreationState.LIVE_IN_BACKGROUND);
        verify(mTabModelObserver).willAddTab(eq(tab), eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND));
    }

    // 2. didAddTab

    @Test
    public void testDidAddTab_selectTab() {
        MockTab tab = createMockTab(101, mProfile);
        when(mOrderController.willOpenInForeground(eq(TabLaunchType.FROM_CHROME_UI), anyBoolean()))
                .thenReturn(true);

        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mTabModelObserver)
                .didAddTab(
                        eq(tab),
                        eq(TabLaunchType.FROM_CHROME_UI),
                        eq(TabCreationState.LIVE_IN_FOREGROUND),
                        eq(true));
    }

    @Test
    public void testDidAddTab_doNotSelectTab() {
        MockTab tab1 = createMockTab(101, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        MockTab tab2 = createMockTab(102, mProfile);
        when(mOrderController.willOpenInForeground(
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND), anyBoolean()))
                .thenReturn(false);

        mTabModel.addTab(
                tab2, 1, TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabCreationState.LIVE_IN_BACKGROUND);
        verify(mTabModelObserver)
                .didAddTab(
                        eq(tab2),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND),
                        eq(TabCreationState.LIVE_IN_BACKGROUND),
                        eq(false));
    }

    // 3. didSelectTab

    @Test
    public void testDidSelectTab_fromUser() {
        MockTab tab1 = createMockTab(101, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelObserver)
                .didSelectTab(eq(tab1), eq(TabSelectionType.FROM_USER), eq(tab2.getId()));
    }

    @Test
    public void testDidSelectTab_fromClose() {
        MockTab tab1 = createMockTab(101, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.setIndex(0, TabSelectionType.FROM_CLOSE);
        verify(mTabModelObserver)
                .didSelectTab(eq(tab1), eq(TabSelectionType.FROM_CLOSE), eq(tab2.getId()));
    }

    @Test
    public void testDidSelectTab_fromNew() {
        MockTab tab = createMockTab(101, mProfile);
        when(mOrderController.willOpenInForeground(eq(TabLaunchType.FROM_CHROME_UI), anyBoolean()))
                .thenReturn(true);

        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verify(mTabModelObserver)
                .didSelectTab(eq(tab), eq(TabSelectionType.FROM_NEW), eq(Tab.INVALID_TAB_ID));
    }

    // 4. onTabsSelectionChanged

    @Test
    public void testOnTabsSelectionChanged_setIndex() {
        MockTab tab1 = createMockTab(101, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);
        verify(mTabModelObserver).onTabsSelectionChanged();
    }

    @Test
    public void testOnTabsSelectionChanged_setTabsMultiSelected() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.setTabsMultiSelected(Set.of(101), true);
        verify(mTabModelObserver).onTabsSelectionChanged();
    }

    @Test
    public void testOnTabsSelectionChanged_clearMultiSelection() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.setTabsMultiSelected(Set.of(101), true);

        reset(mTabModelObserver);
        mTabModel.clearMultiSelection(/* notifyObservers= */ true);
        verify(mTabModelObserver).onTabsSelectionChanged();
    }

    // 5. didMoveTab

    @Test
    public void testDidMoveTab_individualTab() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.moveTab(tab1.getId(), 1);
        verify(mTabModelObserver).didMoveTab(eq(tab1), eq(1), eq(0));
    }

    @Test
    public void testDidMoveTab_tabGroup() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        MockTab tab3 = createMockTab(103, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        Token groupId = new Token(1L, 2L);
        tab1.setTabGroupId(groupId);
        tab2.setTabGroupId(groupId);

        reset(mTabModelObserver);
        mTabModel.moveGroupToIndex(groupId, 1);
        verify(mTabModelObserver).didMoveTab(eq(tab1), eq(1), eq(0));
        verify(mTabModelObserver).didMoveTab(eq(tab2), eq(2), eq(1));
    }

    // 6. willChangePinState

    @Test
    public void testWillChangePinState_pinTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);
        verify(mTabModelObserver).willChangePinState(eq(tab));
    }

    @Test
    public void testWillChangePinState_unpinTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);

        reset(mTabModelObserver);
        mTabModel.unpinTab(tab.getId());
        verify(mTabModelObserver).willChangePinState(eq(tab));
    }

    // 7. didChangePinState

    @Test
    public void testDidChangePinState_pinTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);
        verify(mTabModelObserver).didChangePinState(eq(tab));
    }

    @Test
    public void testDidChangePinState_unpinTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.pinTab(tab.getId(), /* showUngroupDialog= */ false);

        reset(mTabModelObserver);
        mTabModel.unpinTab(tab.getId());
        verify(mTabModelObserver).didChangePinState(eq(tab));
    }

    // 8. tabRemoved

    @Test
    public void testTabRemoved() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.removeTab(tab);
        verify(mTabModelObserver).tabRemoved(eq(tab));
    }

    // 9. willCloseTab

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseTab_singleTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
        verify(mTabModelObserver).willCloseTab(eq(tab), eq(true));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseTab_multipleTabs() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(false).build());
        verify(mTabModelObserver).willCloseTab(eq(tab1), eq(false));
        verify(mTabModelObserver).willCloseTab(eq(tab2), eq(false));
    }

    // 10. willCloseMultipleTabs

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseMultipleTabs_allowUndo() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(true).build());
        verify(mTabModelObserver).willCloseMultipleTabs(eq(true), eq(List.of(tab1, tab2)));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseMultipleTabs_noUndo() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(false).build());
        verify(mTabModelObserver).willCloseMultipleTabs(eq(false), eq(List.of(tab1, tab2)));
    }

    // 11. willCloseAllTabs

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseAllTabs() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(TabClosureParams.closeAllTabs().allowUndo(false).build());
        verify(mTabModelObserver).willCloseAllTabs(eq(false));
    }

    // 12. allTabsAreClosing

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testAllTabsAreClosing_closeAllTabs() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);

        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeAllTabs().allowUndo(false).build());

        verify(mTabModelObserver).allTabsAreClosing();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testAllTabsAreClosing_closeOneTab() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        assertEquals(1, mTabModel.getCount());

        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
        verify(mTabModelObserver).allTabsAreClosing();
    }

    // 13. willCloseTabs

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseTabs_closeAllTabs() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);

        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeAllTabs().allowUndo(false).build());

        verify(mTabModelObserver).willCloseTabs(eq(List.of(tab)), eq(true), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseTabs_allTabsAreClosing() {
        @TabId int tabId = 789;
        MockTab tab = createMockTab(tabId, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        assertEquals(1, mTabModel.getCount());

        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());

        // When closing the last tab, isAllTabs should be true, mirroring `allTabsAreClosing()`.
        verify(mTabModelObserver).willCloseTabs(eq(List.of(tab)), eq(true), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseTabs_closeOneTab() {
        @TabId int tabId1 = 789;
        MockTab tab1 = createMockTab(tabId1, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        @TabId int tabId2 = 456;
        MockTab tab2 = createMockTab(tabId2, mProfile);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mTabModel.closeTabs(TabClosureParams.closeTab(tab1).allowUndo(false).build());

        verify(mTabModelObserver).willCloseTabs(eq(List.of(tab1)), eq(false), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testWillCloseTabs_closeMultipleTabs() {
        @TabId int tabId1 = 789;
        MockTab tab1 = createMockTab(tabId1, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        @TabId int tabId2 = 456;
        MockTab tab2 = createMockTab(tabId2, mProfile);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        @TabId int tabId3 = 123;
        MockTab tab3 = createMockTab(tabId3, mProfile);
        mTabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(false).build());

        verify(mTabModelObserver).willCloseTabs(eq(List.of(tab1, tab2)), eq(false), eq(false));
    }

    // 14. didRemoveTabForClosure

    @Test
    public void testDidRemoveTabForClosure_singleTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
        verify(mTabModelObserver).didRemoveTabForClosure(eq(tab));
    }

    @Test
    public void testDidRemoveTabForClosure_multipleTabs() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(false).build());
        verify(mTabModelObserver).didRemoveTabForClosure(eq(tab1));
        verify(mTabModelObserver).didRemoveTabForClosure(eq(tab2));
    }

    // 15. onTabClosePending

    @Test
    public void testOnTabClosePending_singleTab() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());
        verify(mTabModelObserver)
                .onTabClosePending(eq(List.of(tab)), eq(false), eq(TabClosingSource.UNKNOWN));
    }

    @Test
    public void testOnTabClosePending_multipleTabs() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        MockTab tab3 = createMockTab(103, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(true).build());
        verify(mTabModelObserver)
                .onTabClosePending(
                        eq(List.of(tab1, tab2)), eq(false), eq(TabClosingSource.UNKNOWN));
    }

    @Test
    public void testOnTabClosePending_allTabs() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(TabClosureParams.closeAllTabs().allowUndo(true).build());
        verify(mTabModelObserver)
                .onTabClosePending(
                        eq(List.of(tab1, tab2)), eq(true), eq(TabClosingSource.UNKNOWN));
    }

    // 16. willUndoTabClosure

    @Test
    public void testWillUndoTabClosure() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());

        reset(mTabModelObserver);
        mTabModel.cancelTabClosure(tab.getId());
        verify(mTabModelObserver).willUndoTabClosure(eq(List.of(tab)), eq(false));
    }

    // 17. tabClosureUndone

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testTabClosureUndone() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());

        reset(mTabModelObserver);
        mTabModel.cancelTabClosure(tab.getId());
        verify(mTabModelObserver).tabClosureUndone(eq(tab));
    }

    // 20. tabClosureCommitted

    @Test
    public void testTabClosureCommitted_commitTabClosure() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());

        reset(mTabModelObserver);
        mTabModel.commitTabClosure(tab.getId());
        verify(mTabModelObserver).tabClosureCommitted(eq(tab));
    }

    @Test
    public void testTabClosureCommitted_commitAllTabClosures() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(true).build());

        reset(mTabModelObserver);
        mTabModel.commitAllTabClosures();
        verify(mTabModelObserver).tabClosureCommitted(eq(tab1));
        verify(mTabModelObserver).tabClosureCommitted(eq(tab2));
    }

    // 21. allTabsClosureCommitted

    @Test
    public void testAllTabsClosureCommitted() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());

        reset(mTabModelObserver);
        mTabModel.commitAllTabClosures();
        verify(mTabModelObserver).allTabsClosureCommitted(eq(false));
    }

    // 23. onFinishingTabClosure

    @Test
    public void testOnFinishingTabClosure_withoutUndo() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
        verify(mTabModelObserver).onFinishingTabClosure(eq(tab), eq(TabClosingSource.UNKNOWN));
    }

    @Test
    public void testOnFinishingTabClosure_onCommit() {
        MockTab tab = createMockTab(101, mProfile);
        mTabModel.addTab(tab, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(true).build());

        reset(mTabModelObserver);
        mTabModel.commitTabClosure(tab.getId());
        verify(mTabModelObserver).onFinishingTabClosure(eq(tab), eq(TabClosingSource.UNKNOWN));
    }

    // 24. onFinishingMultipleTabClosure

    @Test
    public void testOnFinishingMultipleTabClosure_canRestore() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2))
                        .allowUndo(false)
                        .saveToTabRestoreService(true)
                        .build());
        verify(mTabModelObserver).onFinishingMultipleTabClosure(eq(List.of(tab1, tab2)), eq(true));
    }

    @Test
    public void testOnFinishingMultipleTabClosure_cannotRestore() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2))
                        .allowUndo(false)
                        .saveToTabRestoreService(false)
                        .build());
        verify(mTabModelObserver).onFinishingMultipleTabClosure(eq(List.of(tab1, tab2)), eq(false));
    }

    // 25. onTabGroupCreated

    @Test
    public void testOnTabGroupCreated() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        reset(mTabModelObserver);
        Token groupId = mTabModel.createTabGroup(List.of(tab1, tab2));
        verify(mTabModelObserver).onTabGroupCreated(eq(groupId));
    }

    // 26. onTabGroupRemoving

    @Test
    public void testOnTabGroupRemoving() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        Token groupId = new Token(1L, 2L);
        tab1.setTabGroupId(groupId);
        tab2.setTabGroupId(groupId);

        reset(mTabModelObserver);
        mTabModel.closeTabs(
                TabClosureParams.closeTabs(List.of(tab1, tab2)).allowUndo(false).build());
        verify(mTabModelObserver).onTabGroupRemoving(eq(groupId));
    }

    // 27. onTabGroupMoved

    @Test
    public void testOnTabGroupMoved() {
        MockTab tab1 = createMockTab(101, mProfile);
        MockTab tab2 = createMockTab(102, mProfile);
        MockTab tab3 = createMockTab(103, mProfile);
        mTabModel.addTab(tab1, 0, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab2, 1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.addTab(tab3, 2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        Token groupId = new Token(1L, 2L);
        tab1.setTabGroupId(groupId);
        tab2.setTabGroupId(groupId);

        reset(mTabModelObserver);
        mTabModel.moveGroupToIndex(groupId, 1);
        verify(mTabModelObserver).onTabGroupMoved(eq(groupId), eq(0));
    }

    // 28. onTabGroupVisualsChanged

    @Test
    public void testOnTabGroupVisualsChanged_title() {
        Token groupId = new Token(1L, 2L);
        when(mTabCollectionTabModelImplJni.getTabGroupTitle(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(groupId)))
                .thenReturn("Old Title");

        mTabModel.setTabGroupTitle(groupId, "New Title");
        verify(mTabModelObserver).onTabGroupVisualsChanged(eq(groupId));
    }

    @Test
    public void testOnTabGroupVisualsChanged_color() {
        Token groupId = new Token(1L, 2L);
        when(mTabCollectionTabModelImplJni.getTabGroupColor(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(groupId)))
                .thenReturn(TabGroupColorId.GREY);

        mTabModel.setTabGroupColor(groupId, TabGroupColorId.BLUE);
        verify(mTabModelObserver).onTabGroupVisualsChanged(eq(groupId));
    }

    @Test
    public void testOnTabGroupVisualsChanged_collapsed() {
        Token groupId = new Token(1L, 2L);
        when(mTabCollectionTabModelImplJni.getTabGroupCollapsed(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(groupId)))
                .thenReturn(false);

        mTabModel.setTabGroupCollapsed(groupId, true);
        verify(mTabModelObserver).onTabGroupVisualsChanged(eq(groupId));
    }

    @Test
    public void testOnTabGroupVisualsChanged_visualData() {
        Token groupId = new Token(1L, 2L);
        when(mTabCollectionTabModelImplJni.getTabGroupTitle(
                        eq(TAB_COLLECTION_TAB_MODEL_IMPL_PTR), eq(groupId)))
                .thenReturn("Old Title");

        mTabModel.setTabGroupVisualData(groupId, "New Title", TabGroupColorId.BLUE, true, false);
        verify(mTabModelObserver).onTabGroupVisualsChanged(eq(groupId));
    }

    // 29. restoreCompleted

    @Test
    public void testRestoreCompleted() {
        mTabModel.completeInitialization();
        mTabModel.broadcastSessionRestoreComplete();
        verify(mTabModelObserver).restoreCompleted();
    }

    // 30. onDestroy

    @Test
    public void testOnDestroy() {
        mTabModel.destroy();
        verify(mTabModelObserver, atLeastOnce()).onDestroy();
    }

    private MockTab createMockTab(int tabId, Profile profile) {
        MockTab tab = MockTab.createAndInitialize(tabId, profile);
        tab.setWebContentsOverrideForTesting(mock(WebContents.class));
        return tab;
    }
}
