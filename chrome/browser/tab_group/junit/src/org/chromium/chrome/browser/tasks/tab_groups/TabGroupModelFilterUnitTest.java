// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Tests for {@link TabGroupModelFilter}.
 */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupModelFilterUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 147;
    private static final int TAB5_ID = 258;
    private static final int TAB6_ID = 369;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;
    private static final int TAB4_ROOT_ID = TAB4_ID;
    private static final int TAB5_ROOT_ID = TAB5_ID;
    private static final int TAB6_ROOT_ID = TAB5_ID;
    private static final int TAB1_PARENT_TAB_ID = Tab.INVALID_TAB_ID;
    private static final int TAB2_PARENT_TAB_ID = Tab.INVALID_TAB_ID;
    private static final int TAB3_PARENT_TAB_ID = TAB2_ID;
    private static final int TAB4_PARENT_TAB_ID = Tab.INVALID_TAB_ID;
    private static final int TAB5_PARENT_TAB_ID = Tab.INVALID_TAB_ID;
    private static final int TAB6_PARENT_TAB_ID = TAB5_ID;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int POSITION4 = 3;
    private static final int POSITION5 = 4;
    private static final int POSITION6 = 5;

    private static final int NEW_TAB_ID_0 = 159;
    private static final int NEW_TAB_ID_1 = 160;
    private static final int NEW_TAB_ID_2 = 162;

    @Mock
    TabModel mTabModel;

    @Mock
    TabGroupModelFilter.Observer mTabGroupModelFilterObserver;

    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private Tab mTab5;
    private Tab mTab6;
    private List<Tab> mTabs = new ArrayList<>();

    private TabGroupModelFilter mTabGroupModelFilter;
    private InOrder mTabModelInOrder;

    private final boolean mSupportAutoGroupCreation;

    @ParameterizedRobolectricTestRunner.Parameters(name = "SupportAutoGroupCreation_{0}")
    public static Collection autoGroupCreationParams() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    public TabGroupModelFilterUnitTest(boolean supportAutoGroupCreation) {
        mSupportAutoGroupCreation = supportAutoGroupCreation;
    }

    private Tab prepareTab(int tabId, int rootId, int parentTabId) {
        Tab tab = mock(Tab.class);
        doReturn(true).when(tab).isInitialized();
        CriticalPersistedTabData criticalPersistedTabData = CriticalPersistedTabData.build(tab);
        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        when(tab.getUserDataHost()).thenReturn(userDataHost);
        doReturn(tabId).when(tab).getId();
        CriticalPersistedTabData.from(tab).setRootId(rootId);
        CriticalPersistedTabData.from(tab).setParentId(parentTabId);
        return tab;
    }

    private void setUpTab() {
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID, TAB1_PARENT_TAB_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID, TAB2_PARENT_TAB_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID, TAB3_PARENT_TAB_ID);
        mTab4 = prepareTab(TAB4_ID, TAB4_ROOT_ID, TAB4_PARENT_TAB_ID);
        mTab5 = prepareTab(TAB5_ID, TAB5_ROOT_ID, TAB5_PARENT_TAB_ID);
        mTab6 = prepareTab(TAB6_ID, TAB6_ROOT_ID, TAB6_PARENT_TAB_ID);
    }

    private void setUpTabModel() {
        doAnswer(invocation -> {
            Tab tab = invocation.getArgument(0);
            int index = invocation.getArgument(1);
            index = index == -1 ? mTabs.size() : index;
            mTabs.add(index, tab);
            return null;
        })
                .when(mTabModel)
                .addTab(any(Tab.class), anyInt(), anyInt(), anyInt());

        doAnswer(invocation -> {
            int movedTabId = invocation.getArgument(0);
            int newIndex = invocation.getArgument(1);

            int oldIndex = TabModelUtils.getTabIndexById(mTabModel, movedTabId);
            // Mirror behavior of real tab model here.
            if (oldIndex == newIndex || oldIndex + 1 == newIndex) return null;

            Tab tab = TabModelUtils.getTabById(mTabModel, movedTabId);

            mTabs.remove(tab);
            if (oldIndex < newIndex) --newIndex;
            mTabs.add(newIndex, tab);
            mTabModelObserverCaptor.getValue().didMoveTab(tab, newIndex, oldIndex);
            return null;
        })
                .when(mTabModel)
                .moveTab(anyInt(), anyInt());

        doAnswer(invocation -> {
            int index = invocation.getArgument(0);
            return mTabs.get(index);
        })
                .when(mTabModel)
                .getTabAt(anyInt());

        doAnswer(invocation -> {
            Tab tab = invocation.getArgument(0);
            return mTabs.indexOf(tab);
        })
                .when(mTabModel)
                .indexOf(any(Tab.class));

        doAnswer(invocation -> mTabs.size()).when(mTabModel).getCount();

        doReturn(0).when(mTabModel).index();
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        mTabModelInOrder = inOrder(mTabModel);
    }

    private Tab addTabToTabModel() {
        return addTabToTabModel(-1, null);
    }

    private Tab addTabToTabModel(int index, @Nullable Tab tab) {
        if (tab == null) tab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, Tab.INVALID_TAB_ID);
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                tab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);
        return tab;
    }

    private void setupTabGroupModelFilter(boolean isTabRestoreCompleted, boolean isIncognito) {
        mTabs.clear();
        doReturn(isIncognito).when(mTabModel).isIncognito();
        mTabGroupModelFilter = new TabGroupModelFilter(mTabModel, mSupportAutoGroupCreation);
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);

        doReturn(isIncognito).when(mTab1).isIncognito();
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        doReturn(isIncognito).when(mTab2).isIncognito();
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        doReturn(isIncognito).when(mTab3).isIncognito();
        mTabModel.addTab(
                mTab3, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab3, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        doReturn(isIncognito).when(mTab4).isIncognito();
        mTabModel.addTab(
                mTab4, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab4, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        doReturn(isIncognito).when(mTab5).isIncognito();
        mTabModel.addTab(
                mTab5, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab5, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        doReturn(isIncognito).when(mTab6).isIncognito();
        mTabModel.addTab(
                mTab6, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab6, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        if (isTabRestoreCompleted) {
            mTabGroupModelFilter.restoreCompleted();
            assertTrue(mTabGroupModelFilter.isTabModelRestored());
        }
    }

    @Before
    public void setUp() {
        // After setUp, TabModel has 6 tabs in the following order: mTab1, mTab2, mTab3, mTab4,
        // mTab5, mTab6. While mTab2 and mTab3 are in a group, and mTab5 and mTab6 are in a separate
        // group.

        MockitoAnnotations.initMocks(this);

        setUpTab();
        setUpTabModel();
        setupTabGroupModelFilter(true, false);
    }

    @Test
    public void setIncognito() {
        setupTabGroupModelFilter(true, false);
        setupTabGroupModelFilter(false, true);
        assertThat(mTabGroupModelFilter.isIncognito(), equalTo(true));
        assertThat(mTabModel.getCount(), equalTo(6));
    }

    @Test
    public void addTab_ToExistingGroup() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);
        doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
        assertThat(
                mTabGroupModelFilter.indexOf(newTab), equalTo(mTabGroupModelFilter.indexOf(mTab1)));
    }

    @Test
    public void addTab_ToNewGroup() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, Tab.INVALID_TAB_ID);
        doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(4));

        addTabToTabModel(-1, newTab);

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.indexOf(newTab), equalTo(4));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(5));
    }

    @Test
    public void addTab_ToNewGroup_NotAtEnd() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, Tab.INVALID_TAB_ID);
        doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(4));

        // Add a tab to the model not at the end and ensure the indexes are updated correctly for
        // all other tabs and groups.
        addTabToTabModel(1, newTab);

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(5));
        assertThat(mTabGroupModelFilter.getTotalTabCount(), equalTo(7));

        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(0));
        assertThat(mTabGroupModelFilter.indexOf(newTab), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab2), equalTo(2));
        assertThat(mTabGroupModelFilter.indexOf(mTab3), equalTo(2));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(3));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(4));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(4));
    }

    @Test
    public void addTab_SetRootId() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);

        if (mSupportAutoGroupCreation) {
            doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        } else {
            doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();
        }

        addTabToTabModel(POSITION1 + 1, newTab);
        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB1_ROOT_ID));
    }

    @Test
    public void addTab_TabLaunchedFromTabGroupUi() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);

        doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();
        addTabToTabModel(POSITION1 + 1, newTab);
        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB1_ROOT_ID));
    }

    @Test
    public void addTab_TabLaunchedFromLongPressBackgroundInGroup() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);

        doReturn(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP).when(newTab).getLaunchType();
        addTabToTabModel(POSITION1 + 1, newTab);
        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB1_ROOT_ID));
    }

    @Test
    public void addTab_TabLaunchedFromChromeUi() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);

        doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        addTabToTabModel(POSITION1 + 1, newTab);

        if (mSupportAutoGroupCreation) {
            assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB1_ROOT_ID));
        } else {
            assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(NEW_TAB_ID_0));
        }
    }

    @Test
    public void addTab_DuringRestore() {
        setupTabGroupModelFilter(false, false);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);
        doReturn(TabLaunchType.FROM_RESTORE).when(newTab).getLaunchType();

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(NEW_TAB_ID_0));
    }

    @Test
    public void addTab_ThemeChangeReparenting() {
        // When tab is added due to theme change reparenting, their launch type remains unchanged.
        setupTabGroupModelFilter(false, false);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, TAB1_ID);
        doReturn(TabLaunchType.FROM_LONGPRESS_BACKGROUND).when(newTab).getLaunchType();

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(NEW_TAB_ID_0));
    }

    @Test
    public void addTab_DuringResettingFilterState() {
        mTabGroupModelFilter.resetFilterState();
        verify(mock(CriticalPersistedTabData.class), never()).setRootId(anyInt());
    }

    @Test(expected = IllegalStateException.class)
    public void addTab_ToWrongModel() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab);
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(true).when(newTab).isIncognito();
        mTabGroupModelFilter.addTab(newTab);
    }

    @Test
    public void moveTabOutOfGroup_NonRootTab_NoUpdateTabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB5_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        mTabGroupModelFilter.moveTabOutOfGroup(TAB3_ID);
        mTabGroupModelFilter.moveTabOutOfGroup(TAB6_ID);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab3, TAB2_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab6, TAB5_ROOT_ID);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION5);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB6_ID));
    }

    @Test
    public void moveTabOutOfGroup_RootTab_NoUpdateTabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab6, mTab5));

        // Move Tab2 and Tab5 to the end of respective group so that root tab is the last tab in
        // group. Plus one as offset because we are moving backwards in tab model.
        mTabModel.moveTab(TAB2_ID, POSITION3 + 1);
        mTabModel.moveTab(TAB5_ID, POSITION6 + 1);

        mTabModelInOrder.verify(mTabModel, times(2)).moveTab(anyInt(), anyInt());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB5_ID));

        mTabGroupModelFilter.moveTabOutOfGroup(TAB2_ID);
        mTabGroupModelFilter.moveTabOutOfGroup(TAB5_ID);

        mTabGroupModelFilterObserver.willMoveTabOutOfGroup(mTab2, TAB3_ID);
        mTabGroupModelFilterObserver.willMoveTabOutOfGroup(mTab5, TAB6_ID);
        mTabModelInOrder.verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        mTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION2);
        mTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab5, POSITION5);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertThat(CriticalPersistedTabData.from(mTab5).getRootId(), equalTo(TAB5_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB6_ID));
    }

    @Test
    public void moveTabOutOfGroup_NonRootTab_FirstTab_UpdateTabModel() {
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        // Move Tab3 so that Tab3 is the first tab in group.
        mTabModel.moveTab(TAB3_ID, POSITION2);

        mTabModelInOrder.verify(mTabModel).moveTab(TAB3_ID, POSITION2);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroup(TAB3_ID);

        mTabGroupModelFilterObserver.willMoveTabOutOfGroup(mTab3, TAB2_ROOT_ID);
        // Plus one as offset because we are moving backwards in tab model.
        mTabModelInOrder.verify(mTabModel).moveTab(TAB3_ID, POSITION3 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_NonRootTab_NotFirstTab_UpdateTabModel() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, TAB2_ROOT_ID, TAB2_ID);
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, newTab, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab, mTab3, mTab4, mTab5, mTab6));

        // Add one tab to the end of {Tab2, Tab3} group so that Tab3 is neither the first nor the
        // last tab in group.
        addTabToTabModel(POSITION4, newTab);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB2_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroup(TAB3_ID);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab3, TAB2_ROOT_ID);
        // Plus one as offset because we are moving backwards in tab model.
        mTabModelInOrder.verify(mTabModel).moveTab(TAB3_ID, POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB2_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_RootTab_FirstTab_UpdateTabModel() {
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroup(TAB2_ID);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab2, TAB3_ID);
        // Plus one as offset because we are moving backwards in tab model.
        verify(mTabModel).moveTab(mTab2.getId(), POSITION3 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_RootTab_NotFirstTab_UpdateTabModel() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, TAB2_ROOT_ID, TAB2_ID);
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, newTab, mTab2, mTab3, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, newTab, mTab3, mTab2, mTab4, mTab5, mTab6));

        // Add one tab to {Tab2, Tab3} group as the first tab in group, so that Tab2 is neither the
        // first nor the last tab in group.
        addTabToTabModel(POSITION2, newTab);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(newTab).getRootId(), equalTo(TAB2_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroup(TAB2_ID);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab2, NEW_TAB_ID_0);
        // Plus one as offset because we are moving backwards in tab model.
        verify(mTabModel).moveTab(mTab2.getId(), POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        // NEW_TAB_ID_0 becomes the new root id for {Tab3, newTab} group.
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(NEW_TAB_ID_0));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(NEW_TAB_ID_0));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_LastTab() {
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroup(TAB1_ID);

        // Ungrouping the last tab in group should have no effect on tab model.
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab1, POSITION1);
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_OtherGroupsLastShownIdUnchanged() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ID));

        // By default, the last shown tab is the first tab in group by order in tab model.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB5_ROOT_ID),
                equalTo(TAB5_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB6_ROOT_ID),
                equalTo(TAB5_ID));

        // Specifically select a different tab in (Tab5, Tab6) group to change the last shown id in
        // that group so that it is different from the default setting.
        mTabGroupModelFilter.selectTab(mTab6);
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB5_ROOT_ID),
                equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB6_ROOT_ID),
                equalTo(TAB6_ID));

        mTabGroupModelFilter.moveTabOutOfGroup(TAB2_ID);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab2, TAB3_ID);
        // Plus one as offset because we are moving backwards in tab model.
        verify(mTabModel).moveTab(mTab2.getId(), POSITION3 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // After ungroup, last shown ids in groups that are unrelated to this ungroup should remain
        // unchanged.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB5_ROOT_ID),
                equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB6_ROOT_ID),
                equalTo(TAB6_ID));
    }

    @Test
    public void moveTabOutOfGroupInDirection_NotTrailing() {
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB3_ID, false);

        verify(mTabModel).moveTab(mTab3.getId(), POSITION2);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION3);
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB3_ID));
        assertThat(CriticalPersistedTabData.from(mTab2).getRootId(), equalTo(TAB2_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void mergeTabToGroup_NoUpdateTabModel() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4));

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab2.getId());

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
    }

    @Test
    public void mergeTabToGroup_UpdateTabModel() {
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabModel).moveTab(mTab5.getId(), POSITION3 + 1);
    }

    @Test
    public void mergeTabToGroup_SkipUpdateTabModel() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6));

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId(), true);

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
    }

    @Test
    public void mergeOneTabToTab_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab4));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION1;

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab1.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab4.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeGroupToTab_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab5, mTab6));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab5, mTab6, mTab2, mTab3, mTab4));
        int startIndex = POSITION1;

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab1.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab6, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab5.getId(), ++startIndex);
        verify(mTabModel).moveTab(mTab6.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab1.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeGroupToGroup_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab5, mTab6, mTab4));
        int startIndex = POSITION3;

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab6, TAB2_ROOT_ID);
        verify(mTabModel).moveTab(mTab5.getId(), ++startIndex);
        verify(mTabModel).moveTab(mTab6.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab2.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeOneTabToTab_Backward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab1));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4, mTab1, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab1, TAB4_ROOT_ID);
        verify(mTabModel).moveTab(mTab1.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab4.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab1.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeGroupToTab_Backward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab2, mTab3));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.mergeTabsToGroup(mTab2.getId(), mTab4.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab3, TAB4_ROOT_ID);
        verify(mTabModel).moveTab(mTab2.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab3.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab3, mTab4.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_AllBackward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6, mTab1, mTab4));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, mTab4));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab5, false, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab1, TAB5_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB5_ROOT_ID);
        verify(mTabModel).moveTab(mTab1.getId(), POSITION6 + 1);
        verify(mTabModel).moveTab(mTab4.getId(), POSITION6 + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab5.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab5.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_AllForward() {
        Tab newTab = addTabToTabModel();
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab4, newTab));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, newTab, mTab2, mTab3, mTab5, mTab6));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab1, false, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab4.getId(), POSITION1 + 1);
        verify(mTabModel).moveTab(newTab.getId(), POSITION1 + 2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab, mTab1.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_AnyDirection() {
        Tab newTab = addTabToTabModel();
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4, mTab1, newTab, mTab5, mTab6));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab4, false, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab1, TAB4_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab, TAB4_ROOT_ID);
        verify(mTabModel).moveTab(mTab1.getId(), POSITION4 + 1);
        verify(mTabModel).moveTab(newTab.getId(), POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab4.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab, mTab4.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_InOrder() {
        Tab newTab0 = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab0);
        Tab newTab1 = prepareTab(NEW_TAB_ID_1, NEW_TAB_ID_1, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab1);
        Tab newTab2 = prepareTab(NEW_TAB_ID_2, NEW_TAB_ID_2, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab2);
        List<Tab> expectedTabModel = new ArrayList<>(
                Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6, newTab0, newTab1, newTab2));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(newTab1, newTab2));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, newTab0, false, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab1, newTab0.getId());
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab2, newTab0.getId());
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab1, newTab0.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab2, newTab0.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void mergeListOfTabsToGroup_BackGroup() {
        Tab newTab0 = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab0);
        Tab newTab1 = prepareTab(NEW_TAB_ID_1, NEW_TAB_ID_1, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab1);
        Tab newTab2 = prepareTab(NEW_TAB_ID_2, NEW_TAB_ID_1, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab2);
        List<Tab> expectedTabModel = new ArrayList<>(
                Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6, newTab1, newTab2, newTab0));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(newTab1, newTab2, newTab0));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, newTab1, false, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab1, newTab1.getId());
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab2, newTab1.getId());
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab0, newTab1.getId());
        verify(mTabModel).moveTab(newTab0.getId(), 9);
        // Skip newTab1
        verify(mTabGroupModelFilterObserver).didMoveWithinGroup(newTab2, 8, 8);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab0, newTab1.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void merge_OtherGroupsLastShownIdUnchanged() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab4));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION1;

        // By default, the last shown tab is the first tab in group by order in tab model.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB2_ROOT_ID),
                equalTo(TAB2_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB3_ROOT_ID),
                equalTo(TAB2_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB5_ROOT_ID),
                equalTo(TAB5_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB6_ROOT_ID),
                equalTo(TAB5_ID));

        // Specifically select different tabs in (Tab2, Tab3) group and (Tab5, Tab6) group to change
        // the last shown ids in respective groups so that it is different from the default setting.
        mTabGroupModelFilter.selectTab(mTab3);
        mTabGroupModelFilter.selectTab(mTab6);
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB2_ROOT_ID),
                equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB3_ROOT_ID),
                equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB5_ROOT_ID),
                equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB6_ROOT_ID),
                equalTo(TAB6_ID));

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab1.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab4.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // After merge, last shown ids in groups that are unrelated to this merge should remain
        // unchanged.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB2_ROOT_ID),
                equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB3_ROOT_ID),
                equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB5_ROOT_ID),
                equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabIdForTesting(TAB6_ROOT_ID),
                equalTo(TAB6_ID));
    }

    @Test
    public void moveGroup_Backward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.moveRelatedTabs(mTab2.getId(), startIndex + 1);

        verify(mTabModel).moveTab(mTab2.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab3.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabGroup(mTab3, POSITION3 - 1, startIndex);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void moveGroup_Forward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab5, mTab6, mTab4));
        int startIndex = POSITION3;

        mTabGroupModelFilter.moveRelatedTabs(mTab5.getId(), startIndex + 1);

        verify(mTabModel).moveTab(mTab5.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab6.getId(), startIndex + 2);
        verify(mTabGroupModelFilterObserver).didMoveTabGroup(mTab6, POSITION6, startIndex + 2);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void ignoreUnrelatedMoveTab() {
        // Simulate that the tab restoring is not yet finished.
        setupTabGroupModelFilter(false, false);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());

        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION1, POSITION6);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION6, POSITION1);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION5);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION5, POSITION2);

        // No call should be made here.
        verify(mTabGroupModelFilterObserver, never())
                .didMoveTabOutOfGroup(any(Tab.class), anyInt());
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(any(Tab.class), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didMoveWithinGroup(any(Tab.class), anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didMoveTabGroup(any(Tab.class), anyInt(), anyInt());

        // Ignore any move incognito tabs before TabModel restored.
        setupTabGroupModelFilter(false, true);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());

        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION1, POSITION6);

        // No call should be made here.
        verify(mTabGroupModelFilterObserver, never())
                .didMoveTabOutOfGroup(any(Tab.class), anyInt());
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(any(Tab.class), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didMoveWithinGroup(any(Tab.class), anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didMoveTabGroup(any(Tab.class), anyInt(), anyInt());
    }

    @Test
    public void undoGroupedTab_NoUpdateTabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        // Simulate we just grouped mTab4 with mTab2 and mTab3
        CriticalPersistedTabData.from(mTab4).setRootId(TAB2_ROOT_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(CriticalPersistedTabData.from(mTab4).getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(1));

        // Undo the grouped action
        mTabGroupModelFilter.undoGroupedTab(mTab4, POSITION4, TAB4_ROOT_ID);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab4).getRootId(), equalTo(TAB4_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(2));
    }

    @Test
    public void undoGroupedTab_Forward_UpdateTabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        // Simulate we just grouped mTab1 with mTab4
        CriticalPersistedTabData.from(mTab1).setRootId(TAB4_ROOT_ID);
        mTabModel.moveTab(mTab1.getId(), POSITION4 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(CriticalPersistedTabData.from(mTab1).getRootId(), equalTo(TAB4_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(1));
        assertFalse(Arrays.equals(mTabs.toArray(), expectedTabModel.toArray()));

        // Undo the grouped action.
        mTabGroupModelFilter.undoGroupedTab(mTab1, POSITION1, TAB1_ROOT_ID);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab1).getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(0));
    }

    @Test
    public void undoGroupedTab_Backward_UpdateTabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        // Simulate we just grouped mTab4 with mTab1
        CriticalPersistedTabData.from(mTab4).setRootId(TAB1_ROOT_ID);
        mTabModel.moveTab(mTab4.getId(), POSITION1 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(CriticalPersistedTabData.from(mTab4).getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(0));
        assertFalse(Arrays.equals(mTabs.toArray(), expectedTabModel.toArray()));

        // Undo the grouped action.
        mTabGroupModelFilter.undoGroupedTab(mTab4, POSITION4, TAB4_ROOT_ID);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab4).getRootId(), equalTo(TAB4_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(2));
    }

    @Test
    public void undoGroupedTab_MultipleGroupUndoNoMovement() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        // Simulate we just grouped mTab4 and the group (mTab5, mTab6) with the group (mTab2,
        // mTab3).
        CriticalPersistedTabData.from(mTab4).setRootId(TAB2_ROOT_ID);
        CriticalPersistedTabData.from(mTab5).setRootId(TAB2_ROOT_ID);
        CriticalPersistedTabData.from(mTab6).setRootId(TAB2_ROOT_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(CriticalPersistedTabData.from(mTab4).getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(CriticalPersistedTabData.from(mTab5).getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(1));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Undo the grouped action in reverse order so indexes are correct.
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION6, TAB5_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION2);
        mTabGroupModelFilter.undoGroupedTab(mTab5, POSITION5, TAB5_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab5, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, mTab6.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab4, POSITION4, TAB4_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab4, POSITION2);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(CriticalPersistedTabData.from(mTab4).getRootId(), equalTo(TAB4_ROOT_ID));
        assertThat(CriticalPersistedTabData.from(mTab5).getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(2));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(3));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(3));
    }

    @Test
    public void undoGroupedTab_MultipleGroupUndoWithMovement() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab6, mTab5, mTab4));

        // Simulate we just grouped the group (mTab5, mTab6) with the group (mTab2, mTab3).
        CriticalPersistedTabData.from(mTab5).setRootId(TAB2_ROOT_ID);
        CriticalPersistedTabData.from(mTab6).setRootId(TAB2_ROOT_ID);
        mTabModel.moveTab(mTab5.getId(), POSITION3 + 1);
        mTabModel.moveTab(mTab6.getId(), POSITION3 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(CriticalPersistedTabData.from(mTab5).getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(1));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Undo the grouped action in reverse order so indexes are correct.
        expectedTabModel = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION6, TAB5_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION2);
        mTabGroupModelFilter.undoGroupedTab(mTab5, POSITION5, TAB5_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab5, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, mTab6.getId());

        assertThat(CriticalPersistedTabData.from(mTab5).getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(CriticalPersistedTabData.from(mTab6).getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(3));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(3));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test(expected = AssertionError.class)
    public void undoGroupedTab_AssertTest() {
        // Simulate mTab6 is not in TabModel.
        doReturn(5).when(mTabModel).getCount();

        // Undo the grouped action.
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION1, TAB1_ROOT_ID);
    }

    @Test
    public void moveTab_Incognito() {
        setupTabGroupModelFilter(false, true);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());

        mTabGroupModelFilter.markTabStateInitialized();
        assertTrue(mTabGroupModelFilter.isTabModelRestored());

        // Simulate that tab3 is going to be moved out of group.
        CriticalPersistedTabData.from(mTab3).setRootId(TAB3_ID);

        mTabModelObserverCaptor.getValue().didMoveTab(mTab3, POSITION3, POSITION6);

        // Verify that the signal is not ignored.
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
    }

    @Test
    public void getNonGroupedTab() {
        Tab[] expectedNonGroupedTabs = new Tab[] {mTab1, mTab4};
        List<Tab> nonGroupedTabs = mTabGroupModelFilter.getTabsWithNoOtherRelatedTabs();
        assertArrayEquals(expectedNonGroupedTabs, nonGroupedTabs.toArray());

        // Group mTab4 with mTab2 and mTab3
        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab2.getId());

        Tab[] expectedNonGroupedTabs_AfterGrouping = new Tab[] {mTab1};
        List<Tab> nonGroupedTabs_AfterGrouping =
                mTabGroupModelFilter.getTabsWithNoOtherRelatedTabs();
        assertArrayEquals(
                expectedNonGroupedTabs_AfterGrouping, nonGroupedTabs_AfterGrouping.toArray());

        // UnGroup mTab3 for its group.
        mTabGroupModelFilter.moveTabOutOfGroup(TAB3_ID);
        Tab[] expectedNonGroupedTabs_AfterUnGrouping = new Tab[] {mTab1, mTab3};
        List<Tab> nonGroupedTabs_AfterUnGrouping =
                mTabGroupModelFilter.getTabsWithNoOtherRelatedTabs();
        assertArrayEquals(
                expectedNonGroupedTabs_AfterUnGrouping, nonGroupedTabs_AfterUnGrouping.toArray());
    }

    @Test
    public void resetFilterStateTest() {
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB2_ROOT_ID));
        CriticalPersistedTabData.from(mTab3).setRootId(TAB1_ROOT_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(CriticalPersistedTabData.from(mTab3).getRootId(), equalTo(TAB1_ROOT_ID));
    }

    @Test
    public void testGetRelatedTabListForRootId() {
        Tab[] group1 = new Tab[] {mTab2, mTab3};
        Tab[] group2 = new Tab[] {mTab5, mTab6};
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabListForRootId(TAB2_ROOT_ID).toArray(), group1);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabListForRootId(TAB3_ROOT_ID).toArray(), group1);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabListForRootId(TAB5_ROOT_ID).toArray(), group2);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabListForRootId(TAB6_ROOT_ID).toArray(), group2);
    }

    @Test
    public void testGetRelatedTabCountForRootId() {
        assertEquals("Should have 1 related tab.", 1,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ROOT_ID));
        assertEquals("Should have 2 related tabs.", 2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB2_ROOT_ID));
        assertEquals("Should have 2 related tabs.", 2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB3_ROOT_ID));
        assertEquals("Should have 1 related tab.", 1,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB4_ROOT_ID));
        assertEquals("Should have 2 related tabs.", 2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB5_ROOT_ID));
        assertEquals("Should have 2 related tabs.", 2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB6_ROOT_ID));
    }

    @Test
    public void testIndexOfAnUndoableClosedTabNotCrashing() {
        mTabGroupModelFilter.closeTab(mTab1);
        mTabGroupModelFilter.indexOf(mTab1);
    }

    @Test
    public void testGetTotalTabCount() {
        assertThat("Should have 4 group tabs", mTabGroupModelFilter.getCount(), equalTo(4));

        int totalTabCount = mTabGroupModelFilter.getTotalTabCount();
        assertThat("Should have 6 total tabs", totalTabCount, equalTo(6));
    }

    @Test
    public void mergeGroupToGroupNonAdjacent_notifyFilterObserver() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab5, mTab6, mTab2, mTab3));
        List<Tab> expectedSourceTabs = mTabGroupModelFilter.getRelatedTabList(mTab2.getId());
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();

        for (Tab tab : expectedSourceTabs) {
            // Use tab2 initial index for both related tabs index as the logic moves tab2 after
            // saving its index but before retrieving index for related tab3.
            originalIndexes.add(TabModelUtils.getTabIndexById(
                    mTabGroupModelFilter.getTabModel(), mTab2.getId()));
            originalRootIds.add(mTabGroupModelFilter.getRootId(tab));
        }

        mTabGroupModelFilter.mergeTabsToGroup(mTab2.getId(), mTab5.getId(), false);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(expectedSourceTabs, originalIndexes, originalRootIds);
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());
    }

    @Test
    public void mergeGroupToTabAdjacent_notifyFilterObserver() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab2, mTab3));
        List<Tab> expectedSourceTabs = mTabGroupModelFilter.getRelatedTabList(mTab3.getId());
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();

        for (Tab tab : expectedSourceTabs) {
            originalIndexes.add(TabModelUtils.getTabIndexById(
                    mTabGroupModelFilter.getTabModel(), mTab2.getId()));
            originalRootIds.add(mTabGroupModelFilter.getRootId(tab));
        }

        mTabGroupModelFilter.mergeTabsToGroup(mTab3.getId(), mTab4.getId(), false);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(expectedSourceTabs, originalIndexes, originalRootIds);
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());
    }

    @Test
    public void mergeTabToTab_notifyFilterObserver() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab1));
        List<Tab> expectedSourceTabs = mTabGroupModelFilter.getRelatedTabList(mTab1.getId());
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();

        for (Tab tab : expectedSourceTabs) {
            originalIndexes.add(
                    TabModelUtils.getTabIndexById(mTabGroupModelFilter.getTabModel(), tab.getId()));
            originalRootIds.add(mTabGroupModelFilter.getRootId(tab));
        }

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId(), false);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(expectedSourceTabs, originalIndexes, originalRootIds);
        assertArrayEquals(mTabGroupModelFilter.getRelatedTabList(mTab1.getId()).toArray(),
                expectedGroup.toArray());
    }

    @Test
    public void mergeTabToTab_doNotNotifyFilterObserver() {
        List<Tab> expectedSourceTabs = mTabGroupModelFilter.getRelatedTabList(mTab1.getId());
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();

        for (Tab tab : expectedSourceTabs) {
            originalIndexes.add(
                    TabModelUtils.getTabIndexById(mTabGroupModelFilter.getTabModel(), tab.getId()));
            originalRootIds.add(mTabGroupModelFilter.getRootId(tab));
        }

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId(), true);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateGroup(expectedSourceTabs, originalIndexes, originalRootIds);
    }
}
