// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doFunction;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;
import androidx.collection.ArraySet;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.GarbageCollectionTestUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.TokenJni;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabStateAttributes;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver.DidRemoveTabGroupReason;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Tests for {@link TabGroupModelFilter}. */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
    ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE
})
public class TabGroupModelFilterUnitTest {
    private static final int TAB1_ID = 11;
    private static final int TAB2_ID = 12;
    private static final int TAB3_ID = 13;
    private static final int TAB4_ID = 14;
    private static final int TAB5_ID = 15;
    private static final int TAB6_ID = 16;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;
    private static final int TAB4_ROOT_ID = TAB4_ID;
    private static final int TAB5_ROOT_ID = TAB5_ID;
    private static final int TAB6_ROOT_ID = TAB5_ID;
    private static final Token TAB1_TAB_GROUP_ID = null;
    private static final Token TAB2_TAB_GROUP_ID = new Token(2L, 2L);
    private static final Token TAB3_TAB_GROUP_ID = TAB2_TAB_GROUP_ID;
    private static final Token TAB4_TAB_GROUP_ID = null;
    private static final Token TAB5_TAB_GROUP_ID = new Token(5L, 2L);
    private static final Token TAB6_TAB_GROUP_ID = TAB5_TAB_GROUP_ID;
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

    private static final int NEW_TAB_ID_0 = 20;
    private static final int NEW_TAB_ID_1 = 21;
    private static final int NEW_TAB_ID_2 = 22;

    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB_TITLE = "Tab";

    private static final String TAB_GROUP_COLORS_FILE_NAME = "tab_group_colors";
    private static final int COLOR_ID = 0;

    private static final String TAB_GROUP_SYNC_IDS_FILE_NAME = "tab_group_sync_ids";
    private static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock Profile mProfile;
    @Mock Token.Natives mTokenJniMock;
    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock TabModel mTabModel;
    @Mock TabList mComprehensiveModel;
    @Mock TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    @Mock Context mContext;
    @Mock SharedPreferences mSharedPreferencesTitle;
    @Mock SharedPreferences mSharedPreferencesColor;
    @Mock SharedPreferences mSharedPreferencesSyncId;
    @Mock SharedPreferences mSharedPreferencesCollapsed;
    @Mock SharedPreferences.Editor mEditor;
    @Mock TabStateAttributes.Observer mAttributesObserver;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private Tab mTab4;
    private Tab mTab5;
    private Tab mTab6;
    private List<Tab> mTabs = new ArrayList<>();

    private TabGroupModelFilter mTabGroupModelFilter;
    private InOrder mTabModelInOrder;
    private InOrder mModelAndObserverInOrder;

    private Tab prepareTab(int tabId, int rootId, @Nullable Token tabGroupId, int parentTabId) {
        Tab tab = mock(Tab.class, "Tab " + tabId);
        doReturn(true).when(tab).isInitialized();
        doReturn(tabId).when(tab).getId();

        ObserverList<TabObserver> tabObserverList = new ObserverList<>();
        MockitoHelper.doCallback((TabObserver obs) -> tabObserverList.addObserver(obs))
                .when(tab)
                .addObserver(any());
        MockitoHelper.doCallback((TabObserver obs) -> tabObserverList.removeObserver(obs))
                .when(tab)
                .removeObserver(any());
        doAnswer(
                        invocation -> {
                            int newRootId = invocation.getArgument(0);
                            when(tab.getRootId()).thenReturn(newRootId);
                            for (TabObserver observer : tabObserverList) {
                                observer.onRootIdChanged(tab, newRootId);
                            }
                            return null;
                        })
                .when(tab)
                .setRootId(anyInt());
        tab.setRootId(rootId);
        doAnswer(
                        invocation -> {
                            Token newTabGroupId = invocation.getArgument(0);
                            when(tab.getTabGroupId()).thenReturn(newTabGroupId);
                            for (TabObserver observer : tabObserverList) {
                                observer.onTabGroupIdChanged(tab, newTabGroupId);
                            }
                            return null;
                        })
                .when(tab)
                .setTabGroupId(any());
        tab.setTabGroupId(tabGroupId);
        doReturn(parentTabId).when(tab).getParentId();

        when(tab.getUrl()).thenReturn(GURL.emptyGURL());
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        TabStateAttributes.createForTab(tab, TabCreationState.LIVE_IN_FOREGROUND);
        TabStateAttributes.from(tab).addObserver(mAttributesObserver);

        return tab;
    }

    private void setUpTab() {
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID, TAB1_TAB_GROUP_ID, TAB1_PARENT_TAB_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID, TAB2_PARENT_TAB_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID, TAB3_TAB_GROUP_ID, TAB3_PARENT_TAB_ID);
        mTab4 = prepareTab(TAB4_ID, TAB4_ROOT_ID, TAB4_TAB_GROUP_ID, TAB4_PARENT_TAB_ID);
        mTab5 = prepareTab(TAB5_ID, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID, TAB5_PARENT_TAB_ID);
        mTab6 = prepareTab(TAB6_ID, TAB6_ROOT_ID, TAB6_TAB_GROUP_ID, TAB6_PARENT_TAB_ID);
    }

    private void setUpTabModel() {
        doAnswer(
                        invocation -> {
                            Tab tab = invocation.getArgument(0);
                            int index = invocation.getArgument(1);
                            index = index == -1 ? mTabs.size() : index;
                            mTabs.add(index, tab);
                            return null;
                        })
                .when(mTabModel)
                .addTab(any(Tab.class), anyInt(), anyInt(), anyInt());

        doAnswer(
                        invocation -> {
                            int movedTabId = invocation.getArgument(0);
                            int newIndex = invocation.getArgument(1);

                            int oldIndex = TabModelUtils.getTabIndexById(mTabModel, movedTabId);
                            // Mirror behavior of real tab model here.
                            if (oldIndex == newIndex || oldIndex + 1 == newIndex) return null;

                            Tab tab = mTabModel.getTabById(movedTabId);

                            mTabs.remove(tab);
                            if (oldIndex < newIndex) --newIndex;
                            mTabs.add(newIndex, tab);
                            mTabModelObserverCaptor.getValue().didMoveTab(tab, newIndex, oldIndex);
                            return null;
                        })
                .when(mTabModel)
                .moveTab(anyInt(), anyInt());

        doFunction(mTabs::get).when(mTabModel).getTabAt(anyInt());
        doAnswer(
                        invocation -> {
                            int tabId = invocation.getArgument(0);
                            return mTabs.stream()
                                    .filter(t -> t.getId() == tabId)
                                    .findAny()
                                    .orElse(null);
                        })
                .when(mTabModel)
                .getTabById(anyInt());
        doFunction(mTabs::indexOf).when(mTabModel).indexOf(any(Tab.class));

        doAnswer(invocation -> mTabs.size()).when(mTabModel).getCount();

        doReturn(0).when(mTabModel).index();
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());

        doReturn(mComprehensiveModel).when(mTabModel).getComprehensiveModel();
        doAnswer(invocation -> mTabs.size()).when(mComprehensiveModel).getCount();
        doFunction(mTabs::get).when(mComprehensiveModel).getTabAt(anyInt());

        mTabModelInOrder = inOrder(mTabModel);
    }

    private Tab addTabToTabModel() {
        return addTabToTabModel(-1, null);
    }

    private Tab addTabToTabModel(int index, @Nullable Tab tab) {
        if (tab == null) tab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, Tab.INVALID_TAB_ID);
        mTabModel.addTab(
                tab, index, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        tab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        return tab;
    }

    private void setupTabGroupModelFilter(boolean isTabRestoreCompleted, boolean isIncognito) {
        mTabs.clear();
        doReturn(isIncognito).when(mTabModel).isIncognito();
        mTabGroupModelFilter = new TabGroupModelFilter(mTabModel);
        mTabGroupModelFilter.addTabGroupObserver(mTabGroupModelFilterObserver);

        doReturn(isIncognito).when(mTab1).isIncognito();
        mTabModel.addTab(
                mTab1, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab1,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        doReturn(isIncognito).when(mTab2).isIncognito();
        mTabModel.addTab(
                mTab2, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab2,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        doReturn(isIncognito).when(mTab3).isIncognito();
        mTabModel.addTab(
                mTab3, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab3,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        doReturn(isIncognito).when(mTab4).isIncognito();
        mTabModel.addTab(
                mTab4, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab4,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        doReturn(isIncognito).when(mTab5).isIncognito();
        mTabModel.addTab(
                mTab5, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab5,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        doReturn(isIncognito).when(mTab6).isIncognito();
        mTabModel.addTab(
                mTab6, -1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab6,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        if (isTabRestoreCompleted) {
            mTabGroupModelFilter.restoreCompleted();
            assertTrue(mTabGroupModelFilter.isTabModelRestored());
        }

        doReturn(mSharedPreferencesTitle)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mSharedPreferencesColor)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLORS_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mSharedPreferencesSyncId)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_SYNC_IDS_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mSharedPreferencesCollapsed)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);
        ContextUtils.initApplicationContextForTests(mContext);
        when(mSharedPreferencesTitle.getString(anyString(), any())).thenReturn(TAB_TITLE);
        when(mSharedPreferencesColor.getInt(anyString(), anyInt()))
                .thenReturn(TabGroupColorUtils.INVALID_COLOR_ID);
        when(mSharedPreferencesCollapsed.getBoolean(anyString(), anyBoolean())).thenReturn(true);
        when(mSharedPreferencesTitle.edit()).thenReturn(mEditor);
        when(mSharedPreferencesColor.edit()).thenReturn(mEditor);
        when(mSharedPreferencesSyncId.edit()).thenReturn(mEditor);
        when(mSharedPreferencesCollapsed.edit()).thenReturn(mEditor);
        when(mEditor.putString(anyString(), anyString())).thenReturn(mEditor);
        when(mEditor.putInt(anyString(), anyInt())).thenReturn(mEditor);
        when(mEditor.putBoolean(anyString(), anyBoolean())).thenReturn(mEditor);
        when(mEditor.remove(anyString())).thenReturn(mEditor);

        mModelAndObserverInOrder = inOrder(mTabModel, mTabGroupModelFilterObserver);
    }

    @Before
    public void setUp() {
        TabGroupModelFilter.SKIP_TAB_GROUP_CREATION_DIALOG.setForTesting(false);
        // After setUp, TabModel has 6 tabs in the following order: mTab1, mTab2, mTab3, mTab4,
        // mTab5, mTab6. While mTab2 and mTab3 are in a group, and mTab5 and mTab6 are in a separate
        // group.

        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(TokenJni.TEST_HOOKS, mTokenJniMock);
        mJniMocker.mock(TabGroupSyncFeaturesJni.TEST_HOOKS, mTabGroupSyncFeaturesJniMock);

        when(mProfile.isNativeInitialized()).thenReturn(true);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        setUpTab();
        setUpTabModel();
        setupTabGroupModelFilter(true, false);
    }

    @Test
    public void testDestroy() {
        TabModelObserver tabModelObserver = new TabModelObserver() {};
        mTabGroupModelFilter.addObserver(tabModelObserver);

        WeakReference ref = new WeakReference(tabModelObserver);
        tabModelObserver = null;
        assertFalse(GarbageCollectionTestUtils.canBeGarbageCollected(ref));

        mTabGroupModelFilter.destroy();

        verify(mTabModel).removeObserver(any());
        assertTrue(GarbageCollectionTestUtils.canBeGarbageCollected(ref));
    }

    @Test
    public void setIncognito() {
        setupTabGroupModelFilter(true, false);
        setupTabGroupModelFilter(false, true);
        assertThat(mTabGroupModelFilter.isIncognito(), equalTo(true));
        assertThat(mTabModel.getCount(), equalTo(6));
    }

    @Test
    public void addTab_ToExistingGroupSingleTab() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));

        Token tabGroupId = new Token(93L, 42L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab1.getTabGroupId(), newTab.getTabGroupId());
        assertThat(
                mTabGroupModelFilter.indexOf(newTab), equalTo(mTabGroupModelFilter.indexOf(mTab1)));
    }

    @Test
    public void addTab_ToExistingGroupedTab() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB2_ID);
        doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertEquals(mTab2.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertEquals(mTab2.getTabGroupId(), newTab.getTabGroupId());
        assertThat(
                mTabGroupModelFilter.indexOf(newTab), equalTo(mTabGroupModelFilter.indexOf(mTab2)));
    }

    @Test
    public void addTab_ToNewGroup() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, Tab.INVALID_TAB_ID);
        doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(4));

        addTabToTabModel(-1, newTab);

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.indexOf(newTab), equalTo(4));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(5));
        assertNull(newTab.getTabGroupId());
    }

    @Test
    public void addTab_ToNewGroup_NotAtEnd() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, Tab.INVALID_TAB_ID);
        doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        assertThat(mTabGroupModelFilter.getCount(), equalTo(4));

        // Add a tab to the model not at the end and ensure the indexes are updated correctly for
        // all other tabs and groups.
        addTabToTabModel(1, newTab);

        assertNull(newTab.getTabGroupId());

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
    public void addTab_SetRootIdAndTabGroupId() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);

        doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();

        Token tabGroupId = new Token(93L, 42L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        addTabToTabModel(POSITION1 + 1, newTab);
        assertThat(newTab.getRootId(), equalTo(TAB1_ROOT_ID));
        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab1.getTabGroupId(), newTab.getTabGroupId());
    }

    @Test
    public void rootIdToStableIdAndBackConversion() {
        // Test existing IDs.
        assertEquals(TAB2_ROOT_ID, mTabGroupModelFilter.getRootIdFromStableId(TAB2_TAB_GROUP_ID));
        assertEquals(TAB2_TAB_GROUP_ID, mTabGroupModelFilter.getStableIdFromRootId(TAB2_ROOT_ID));

        assertEquals(null, mTabGroupModelFilter.getStableIdFromRootId(TAB1_ROOT_ID));

        // Test non-existing IDs.
        assertEquals(
                Tab.INVALID_TAB_ID,
                mTabGroupModelFilter.getRootIdFromStableId(new Token(93L, 42L)));
        assertEquals(null, mTabGroupModelFilter.getStableIdFromRootId(1000));
    }

    @Test
    public void addTab_TabLaunchedFromTabGroupUi() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_TAB_GROUP_UI).when(newTab).getLaunchType();

        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        Token tabGroupId = new Token(93L, 42L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(TAB1_ROOT_ID));
        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab1.getTabGroupId(), newTab.getTabGroupId());
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
    }

    @Test
    public void addTab_TabLaunchedFromLongPressBackgroundInGroup() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP).when(newTab).getLaunchType();
        assertNull(mTab1.getTabGroupId());
        assertNull(newTab.getTabGroupId());
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));

        Token tabGroupId = new Token(93L, 42L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);
        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(TAB1_ROOT_ID));
        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab1.getTabGroupId(), newTab.getTabGroupId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(newTab, mTabGroupModelFilter);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
    }

    @Test
    public void addTab_TabLaunchedFromLongPressBackgroundInGroup_NotRestoredToGroupOnUndo() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP).when(newTab).getLaunchType();
        assertNull(mTab1.getTabGroupId());
        assertNull(newTab.getTabGroupId());
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));

        Token tabGroupId = new Token(93L, 42L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        // Create a new tab in the tab group via launch type.
        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(TAB1_ROOT_ID));
        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab1.getTabGroupId(), newTab.getTabGroupId());
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(newTab));
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(newTab, mTabGroupModelFilter);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));

        // Move the new tab out of the tab group.
        mTabGroupModelFilter.moveTabOutOfGroupInDirection(newTab.getId(), /* trailing= */ true);

        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(newTab, POSITION1);
        assertFalse(mTabGroupModelFilter.isTabInTabGroup(newTab));
        assertThat(newTab.getRootId(), equalTo(NEW_TAB_ID_0));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab1));

        // Start to close the new tab.
        mTabGroupModelFilter.willCloseTab(newTab, /* didCloseAlone= */ true);

        // Undo the closure.
        mTabGroupModelFilter.tabClosureUndone(newTab);

        // Assert on undo the new tab is not re-added to the tab group it was originally in.
        assertThat(newTab.getRootId(), equalTo(NEW_TAB_ID_0));
        assertFalse(mTabGroupModelFilter.isTabInTabGroup(newTab));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void addTab_TabLaunchedFromLongPressBackgroundInGroupToExistingGroup() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP).when(newTab).getLaunchType();

        Token tabGroupId = new Token(93L, 42L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        mTabGroupModelFilter.createSingleTabGroup(mTab1, true);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab1, mTabGroupModelFilter);

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(TAB1_ROOT_ID));
        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab1.getTabGroupId(), newTab.getTabGroupId());

        // Verify this isn't called a second time.
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab1, mTabGroupModelFilter);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
    }

    @Test
    public void addTab_TabLaunchedFromChromeUi() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);

        doReturn(TabLaunchType.FROM_CHROME_UI).when(newTab).getLaunchType();
        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(NEW_TAB_ID_0));
        assertNull(newTab.getTabGroupId());
    }

    @Test
    public void addTab_DuringRestore() {
        setupTabGroupModelFilter(false, false);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_RESTORE).when(newTab).getLaunchType();

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(NEW_TAB_ID_0));
        assertNull(newTab.getTabGroupId());
    }

    @Test
    public void addTab_ThemeChangeReparenting() {
        // When tab is added due to theme change reparenting, their launch type remains unchanged.
        setupTabGroupModelFilter(false, false);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, TAB1_ID);
        doReturn(TabLaunchType.FROM_LONGPRESS_BACKGROUND).when(newTab).getLaunchType();

        addTabToTabModel(POSITION1 + 1, newTab);

        assertThat(newTab.getRootId(), equalTo(NEW_TAB_ID_0));
        assertNull(newTab.getTabGroupId());
    }

    @Test
    public void addTab_DuringResettingFilterState() {
        mTabGroupModelFilter.resetFilterState();
        verify(mock(Tab.class), never()).setRootId(anyInt());
        verify(mock(Tab.class), never()).setTabGroupId(any());
    }

    @Test(expected = IllegalStateException.class)
    public void addTab_ToWrongModel() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab);
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(true).when(newTab).isIncognito();
        mTabGroupModelFilter.addTab(newTab, /* fromUndo= */ false);
    }

    @Test
    public void isTabInTabGroup() {
        assertFalse(mTabGroupModelFilter.isTabInTabGroup(mTab1));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab2));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab3));
        assertFalse(mTabGroupModelFilter.isTabInTabGroup(mTab4));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab5));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab6));
    }

    @Test
    public void testGroupMembershipOfTabAfterClose() {
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab2));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab3));
        assertEquals(TAB2_ID, mTab2.getRootId());
        assertEquals(TAB2_ID, mTab3.getRootId());

        mTabGroupModelFilter.closeTab(mTab2);

        assertFalse(mTabGroupModelFilter.isTabInTabGroup(mTab2));
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab3));
        assertEquals(TAB3_ID, mTab2.getRootId());
        assertEquals(TAB3_ID, mTab3.getRootId());
        verify(mTabGroupModelFilterObserver).didChangeGroupRootId(TAB2_ID, TAB3_ID);
    }

    @Test
    public void moveTabOutOfGroup_NonRootTab_NoUpdateTabModel() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB5_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB3_ID, /* trailing= */ true);
        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB6_ID, /* trailing= */ true);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab3, TAB2_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab6, TAB5_ROOT_ID);
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION5);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB6_ID));
        assertEquals(mTab2.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertNull(mTab3.getTabGroupId());
        assertEquals(mTab5.getTabGroupId(), TAB5_TAB_GROUP_ID);
        assertNull(mTab6.getTabGroupId());
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
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB5_ID));

        assertEquals(mTab2.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertEquals(mTab3.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertEquals(mTab5.getTabGroupId(), TAB5_TAB_GROUP_ID);
        assertEquals(mTab6.getTabGroupId(), TAB5_TAB_GROUP_ID);

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB2_ID, /* trailing= */ true);
        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB5_ID, /* trailing= */ true);

        mTabGroupModelFilterObserver.willMoveTabOutOfGroup(mTab2, TAB3_ID);
        mTabGroupModelFilterObserver.willMoveTabOutOfGroup(mTab5, TAB6_ID);
        mTabModelInOrder.verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        mTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION2);
        mTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab5, POSITION5);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertThat(mTab5.getRootId(), equalTo(TAB5_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB6_ID));

        assertNull(mTab2.getTabGroupId());
        assertEquals(mTab3.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertNull(mTab5.getTabGroupId());
        assertEquals(mTab6.getTabGroupId(), TAB5_TAB_GROUP_ID);
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
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB3_ID, /* trailing= */ true);

        mTabGroupModelFilterObserver.willMoveTabOutOfGroup(mTab3, TAB2_ROOT_ID);
        // Plus one as offset because we are moving backwards in tab model.
        mTabModelInOrder.verify(mTabModel).moveTab(TAB3_ID, POSITION3 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertNull(mTab3.getTabGroupId());
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_NonRootTab_NotFirstTab_UpdateTabModel() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID, TAB2_ID);
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, newTab, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab, mTab3, mTab4, mTab5, mTab6));

        // Add one tab to the end of {Tab2, Tab3} group so that Tab3 is neither the first nor the
        // last tab in group.
        addTabToTabModel(POSITION4, newTab);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(newTab.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(newTab.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB3_ID, /* trailing= */ true);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab3, TAB2_ROOT_ID);
        // Plus one as offset because we are moving backwards in tab model.
        mTabModelInOrder.verify(mTabModel).moveTab(TAB3_ID, POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(newTab.getRootId(), equalTo(TAB2_ROOT_ID));
        assertNull(mTab3.getTabGroupId());
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(newTab.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_RootTab_FirstTab_UpdateTabModel() {
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB2_ID, /* trailing= */ true);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab2, TAB3_ID);
        // Plus one as offset because we are moving backwards in tab model.
        verify(mTabModel).moveTab(mTab2.getId(), POSITION3 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertNull(mTab2.getTabGroupId());
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_RootTab_NotFirstTab_UpdateTabModel() {
        Tab newTab = prepareTab(NEW_TAB_ID_0, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID, TAB2_ID);
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, newTab, mTab2, mTab3, mTab4, mTab5, mTab6));
        List<Tab> expectedTabModelAfterUngroup =
                new ArrayList<>(Arrays.asList(mTab1, newTab, mTab3, mTab2, mTab4, mTab5, mTab6));

        // Add one tab to {Tab2, Tab3} group as the first tab in group, so that Tab2 is neither the
        // first nor the last tab in group.
        addTabToTabModel(POSITION2, newTab);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));
        assertThat(newTab.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(newTab.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB2_ID, /* trailing= */ true);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab2, NEW_TAB_ID_0);
        // Plus one as offset because we are moving backwards in tab model.
        verify(mTabModel).moveTab(mTab2.getId(), POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertNull(mTab2.getTabGroupId());
        // NEW_TAB_ID_0 becomes the new root id for {Tab3, newTab} group.
        assertThat(mTab3.getRootId(), equalTo(NEW_TAB_ID_0));
        assertThat(mTab3.getRootId(), equalTo(NEW_TAB_ID_0));
        // The TabGroupId is stable and does not change.
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(newTab.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
    }

    @Test
    public void moveTabOutOfGroup_LastTab() {
        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        Token tabGroupId = new Token(374893L, 83942L);
        mTab1.setTabGroupId(tabGroupId);
        assertNotNull(mTab1.getTabGroupId());
        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB1_ID, /* trailing= */ true);

        // Ungrouping the last tab in group should have no effect on tab model.
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab1, POSITION1);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab1.getRootId(), tabGroupId, DidRemoveTabGroupReason.UNGROUP);
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());
        assertNull(mTab1.getTabGroupId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void moveTabOutOfGroup_LastTab_WithTabGroupId() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        List<Tab> expectedTabModelBeforeUngroup =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());

        Token tabGroupId = new Token(374893L, 83942L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        mTabGroupModelFilter.createSingleTabGroup(mTab1, true);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));

        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab1, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab1.getId());
        verify(mTabGroupModelFilterObserver, never())
                .didCreateGroup(
                        anyList(),
                        anyList(),
                        anyList(),
                        anyList(),
                        anyString(),
                        anyInt(),
                        anyBoolean());
        assertTrue(mTabGroupModelFilter.isTabInTabGroup(mTab1));

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB1_ID, /* trailing= */ true);

        // Ungrouping the last tab in group should have no effect on tab model.
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab1, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab1, POSITION1);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab1.getRootId(), tabGroupId, DidRemoveTabGroupReason.UNGROUP);
        assertArrayEquals(mTabs.toArray(), expectedTabModelBeforeUngroup.toArray());
        assertNull(mTab1.getTabGroupId());
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
    }

    @Test
    public void moveTabOutOfGroup_OtherGroupsLastShownIdUnchanged() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab3, mTab2, mTab4, mTab5, mTab6));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ID));

        // By default, the last shown tab is the first tab in group by order in tab model.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB5_ROOT_ID), equalTo(TAB5_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB6_ROOT_ID), equalTo(TAB5_ID));

        // Specifically select a different tab in (Tab5, Tab6) group to change the last shown id in
        // that group so that it is different from the default setting.
        mTabGroupModelFilter.selectTab(mTab6);
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB5_ROOT_ID), equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB6_ROOT_ID), equalTo(TAB6_ID));

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB2_ID, /* trailing= */ true);

        verify(mTabGroupModelFilterObserver).willMoveTabOutOfGroup(mTab2, TAB3_ID);
        // Plus one as offset because we are moving backwards in tab model.
        verify(mTabModel).moveTab(mTab2.getId(), POSITION3 + 1);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION2);
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertNull(mTab2.getTabGroupId());
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // After ungroup, last shown ids in groups that are unrelated to this ungroup should remain
        // unchanged.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB5_ROOT_ID), equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB6_ROOT_ID), equalTo(TAB6_ID));
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
        verify(mTabGroupModelFilterObserver, never()).didChangeGroupRootId(anyInt(), anyInt());
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertNull(mTab3.getTabGroupId());
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertArrayEquals(mTabs.toArray(), expectedTabModelAfterUngroup.toArray());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab3, DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
    }

    @Test
    public void moveTabOutOfGroupInDirection_NewRootId() {
        assertEquals(TAB2_ID, mTab2.getRootId());
        assertEquals(TAB2_ID, mTab3.getRootId());

        mTabGroupModelFilter.moveTabOutOfGroupInDirection(TAB2_ID, false);

        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION3);
        verify(mTabGroupModelFilterObserver).didChangeGroupRootId(TAB2_ID, TAB3_ID);
        assertThat(mTab3.getRootId(), equalTo(TAB3_ID));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ID));
        assertNull(mTab2.getTabGroupId());
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab2, DirtinessState.DIRTY);
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab3, DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
    }

    @Test
    public void mergeTabToGroup_NoUpdateTabModel() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4));

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab2.getId());

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab4, mTabGroupModelFilter);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());

        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab4.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));

        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab4, DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
    }

    @Test
    public void mergeTabToGroup_UpdateTabModel() {
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabModel).moveTab(mTab5.getId(), POSITION3 + 1);

        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab5, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab5.getId(), TAB5_TAB_GROUP_ID, DidRemoveTabGroupReason.MERGE);

        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
    }

    @Test
    public void mergeTabToGroup_SkipUpdateTabModel() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6));

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId(), true);

        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab5, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab5.getId(), TAB5_TAB_GROUP_ID, DidRemoveTabGroupReason.MERGE);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());

        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeOneTabToTab_Forward() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab4));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION1;

        Token tabGroupId = new Token(38294L, 2191L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab1.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab4.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab1, mTabGroupModelFilter);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        assertThat(mTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(mTab4.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    public void mergeGroupToTab_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab5, mTab6));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab5, mTab6, mTab2, mTab3, mTab4));
        int startIndex = POSITION1;

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab1.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab5, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab6, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab5.getId(), ++startIndex);
        verify(mTabModel).moveTab(mTab6.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab6, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(TAB5_ROOT_ID, null, DidRemoveTabGroupReason.MERGE);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        assertThat(mTab1.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
    }

    @Test
    public void mergeGroupToGroup_Forward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab5, mTab6, mTab4));
        int startIndex = POSITION3;

        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab5, TAB2_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab6, TAB2_ROOT_ID);
        verify(mTabModel).moveTab(mTab5.getId(), ++startIndex);
        verify(mTabModel).moveTab(mTab6.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, mTab2.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab2.getId());
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab6, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab5.getId(), TAB5_TAB_GROUP_ID, DidRemoveTabGroupReason.MERGE);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab5.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeOneTabToTab_Backward() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab1));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4, mTab1, mTab5, mTab6));
        int startIndex = POSITION4;

        Token tabGroupId = new Token(94321L, 7328L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab1, TAB4_ROOT_ID);
        verify(mTabModel).moveTab(mTab1.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab4.getId());
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab4, mTabGroupModelFilter);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab1.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        assertThat(mTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(mTab4.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    public void mergeGroupToTab_Backward() {
        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab2, mTab3));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.mergeTabsToGroup(mTab2.getId(), mTab4.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab2, TAB4_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab3, TAB4_ROOT_ID);
        verify(mTabModel).moveTab(mTab2.getId(), startIndex + 1);
        verify(mTabModel).moveTab(mTab3.getId(), startIndex + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab2, mTab4.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab3, mTab4.getId());
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab2, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab2.getId(), TAB4_TAB_GROUP_ID, DidRemoveTabGroupReason.MERGE);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab4.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
    }

    @Test
    public void mergeTabsToGroup_Collapsed() {
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(true));
    }

    @Test
    public void mergeTabsToGroup_SourceExpanded() {
        when(mSharedPreferencesCollapsed.getBoolean(eq(String.valueOf(TAB5_ROOT_ID)), anyBoolean()))
                .thenReturn(false);
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(true));
    }

    @Test
    public void mergeTabsToGroup_DestinationExpanded() {
        when(mSharedPreferencesCollapsed.getBoolean(eq(String.valueOf(TAB2_ROOT_ID)), anyBoolean()))
                .thenReturn(false);
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE)
    public void mergeTabsToGroup_CollapsedWithoutFeature() {
        mTabGroupModelFilter.mergeTabsToGroup(mTab5.getId(), mTab2.getId());
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(false));
    }

    @Test
    public void mergeListOfTabsToGroup_AllBackward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab5, mTab6, mTab1, mTab4));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, mTab4));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab5, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab1, TAB5_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB5_ROOT_ID);
        verify(mTabModel).moveTab(mTab1.getId(), POSITION6 + 1);
        verify(mTabModel).moveTab(mTab4.getId(), POSITION6 + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab5.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab5.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Attempt to merge single tabs with group tabs.
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab5, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver, never()).didRemoveTabGroup(anyInt(), any(), anyInt());

        assertThat(mTab5.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab4.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab1, DirtinessState.DIRTY);
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab4, DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeListOfTabsToGroup_AllForward() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        Tab newTab = addTabToTabModel();
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab4, newTab));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, newTab, mTab2, mTab3, mTab5, mTab6));

        Token tabGroupId = new Token(123L, 567L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab1, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB1_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab4.getId(), POSITION1 + 1);
        verify(mTabModel).moveTab(newTab.getId(), POSITION1 + 2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab, mTab1.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Attempt to merge all single tabs, resulting in a new group creation.
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab1, mTabGroupModelFilter);

        assertThat(mTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(mTab4.getTabGroupId(), equalTo(tabGroupId));
        assertThat(newTab.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeListOfTabsToGroup_AnyDirection() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        Tab newTab = addTabToTabModel();
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab2, mTab3, mTab4, mTab1, newTab, mTab5, mTab6));

        Token tabGroupId = new Token(1234L, 4567L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab4, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab1, TAB4_ROOT_ID);
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab, TAB4_ROOT_ID);
        verify(mTabModel).moveTab(mTab1.getId(), POSITION4 + 1);
        verify(mTabModel).moveTab(newTab.getId(), POSITION4 + 1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab1, mTab4.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab, mTab4.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Attempt to merge all single tabs, resulting in a new group creation.
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab4, mTabGroupModelFilter);

        assertThat(mTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(mTab4.getTabGroupId(), equalTo(tabGroupId));
        assertThat(newTab.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeListOfTabsToGroup_InOrder() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        Tab newTab0 = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab0);
        Tab newTab1 = prepareTab(NEW_TAB_ID_1, NEW_TAB_ID_1, null, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab1);
        Tab newTab2 = prepareTab(NEW_TAB_ID_2, NEW_TAB_ID_2, null, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab2);
        List<Tab> expectedTabModel =
                new ArrayList<>(
                        Arrays.asList(
                                mTab1, mTab2, mTab3, mTab4, mTab5, mTab6, newTab0, newTab1,
                                newTab2));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(newTab1, newTab2));

        Token tabGroupId = new Token(91234L, 84567L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, newTab0, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab1, newTab0.getId());
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab2, newTab0.getId());
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab1, newTab0.getId());
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab2, newTab0.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Attempt to merge all single tabs, resulting in a new group creation.
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(newTab0, mTabGroupModelFilter);

        assertThat(newTab0.getTabGroupId(), equalTo(tabGroupId));
        assertThat(newTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(newTab2.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    public void mergeListOfTabsToGroup_BackGroup() {
        Token tabGroupId = new Token(234L, 342L);

        Tab newTab0 = prepareTab(NEW_TAB_ID_0, NEW_TAB_ID_0, null, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab0);
        Tab newTab1 = prepareTab(NEW_TAB_ID_1, NEW_TAB_ID_1, tabGroupId, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab1);
        Tab newTab2 = prepareTab(NEW_TAB_ID_2, NEW_TAB_ID_1, tabGroupId, Tab.INVALID_TAB_ID);
        addTabToTabModel(-1, newTab2);
        List<Tab> expectedTabModel =
                new ArrayList<>(
                        Arrays.asList(
                                mTab1, mTab2, mTab3, mTab4, mTab5, mTab6, newTab1, newTab2,
                                newTab0));
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(newTab1, newTab2, newTab0));

        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, newTab1, false);

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab1, newTab1.getId());
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab2, newTab1.getId());
        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(newTab0, newTab1.getId());
        verify(mTabModel).moveTab(newTab0.getId(), 9);
        // Skip newTab1
        verify(mTabGroupModelFilterObserver).didMoveWithinGroup(newTab2, 8, 8);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(newTab0, newTab1.getId());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Attempt to merge a single tab with group tabs.
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(newTab1, mTabGroupModelFilter);

        assertThat(newTab0.getTabGroupId(), equalTo(tabGroupId));
        assertThat(newTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(newTab2.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    public void mergeListOfTabsToGroup_MultipleGroups() {
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4));
        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab5, false);

        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab2.getId(), TAB2_TAB_GROUP_ID, DidRemoveTabGroupReason.MERGE);
        assertEquals(mTab5.getId(), mTab1.getRootId());
        assertEquals(mTab5.getId(), mTab2.getRootId());
        assertEquals(mTab5.getId(), mTab3.getRootId());
        assertEquals(mTab5.getId(), mTab4.getRootId());
        assertEquals(mTab5.getId(), mTab5.getRootId());
        assertEquals(mTab5.getId(), mTab6.getRootId());
        assertEquals(mTab5.getTabGroupId(), mTab1.getTabGroupId());
        assertEquals(mTab5.getTabGroupId(), mTab2.getTabGroupId());
        assertEquals(mTab5.getTabGroupId(), mTab3.getTabGroupId());
        assertEquals(mTab5.getTabGroupId(), mTab4.getTabGroupId());
        assertEquals(mTab5.getTabGroupId(), mTab5.getTabGroupId());
        assertEquals(mTab5.getTabGroupId(), mTab6.getTabGroupId());
    }

    @Test
    public void mergeListOfTabsToGroup_Collapsed() {
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab5, mTab6));
        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab4, true);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(true));
    }

    @Test
    public void mergeListOfTabsToGroup_SourceExpanded() {
        when(mSharedPreferencesCollapsed.getBoolean(eq(String.valueOf(TAB5_ROOT_ID)), anyBoolean()))
                .thenReturn(false);
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab5, mTab6));
        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab4, true);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(true));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_COLLAPSE)
    public void mergeListOfTabsToGroup_CollapsedWithoutFeature() {
        List<Tab> tabsToMerge = new ArrayList<>(Arrays.asList(mTab5, mTab6));
        mTabGroupModelFilter.mergeListOfTabsToGroup(tabsToMerge, mTab4, true);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), eq(false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void merge_OtherGroupsLastShownIdUnchanged() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab1, mTab4));
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION1;

        // By default, the last shown tab is the first tab in group by order in tab model.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB2_ROOT_ID), equalTo(TAB2_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB3_ROOT_ID), equalTo(TAB2_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB5_ROOT_ID), equalTo(TAB5_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB6_ROOT_ID), equalTo(TAB5_ID));

        // Specifically select different tabs in (Tab2, Tab3) group and (Tab5, Tab6) group to change
        // the last shown ids in respective groups so that it is different from the default setting.
        mTabGroupModelFilter.selectTab(mTab3);
        mTabGroupModelFilter.selectTab(mTab6);
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB2_ROOT_ID), equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB3_ROOT_ID), equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB5_ROOT_ID), equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB6_ROOT_ID), equalTo(TAB6_ID));

        Token tabGroupId = new Token(91234L, 84567L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeTabsToGroup(mTab4.getId(), mTab1.getId());

        verify(mTabGroupModelFilterObserver).willMergeTabToGroup(mTab4, TAB1_ROOT_ID);
        verify(mTabModel).moveTab(mTab4.getId(), ++startIndex);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab1.getId());
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab1, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver, never()).didRemoveTabGroup(anyInt(), any(), anyInt());
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab4.getId()).toArray(),
                expectedGroup.toArray());
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // After merge, last shown ids in groups that are unrelated to this merge should remain
        // unchanged.
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB2_ROOT_ID), equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB3_ROOT_ID), equalTo(TAB3_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB5_ROOT_ID), equalTo(TAB6_ID));
        assertThat(mTabGroupModelFilter.getGroupLastShownTabId(TAB6_ROOT_ID), equalTo(TAB6_ID));

        assertThat(mTab1.getTabGroupId(), equalTo(tabGroupId));
        assertThat(mTab4.getTabGroupId(), equalTo(tabGroupId));
    }

    @Test
    public void moveGroup_Backward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab4, mTab2, mTab3, mTab5, mTab6));
        int startIndex = POSITION4;

        mTabGroupModelFilter.moveRelatedTabs(mTab2.getId(), startIndex + 1);

        mModelAndObserverInOrder
                .verify(mTabGroupModelFilterObserver)
                .willMoveTabGroup(POSITION2, startIndex + 1);
        mModelAndObserverInOrder.verify(mTabModel).moveTab(mTab2.getId(), startIndex + 1);
        mModelAndObserverInOrder.verify(mTabModel).moveTab(mTab3.getId(), startIndex + 1);
        mModelAndObserverInOrder
                .verify(mTabGroupModelFilterObserver)
                .didMoveTabGroup(mTab3, POSITION3 - 1, startIndex);
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void moveGroup_Forward() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab5, mTab6, mTab4));
        int startIndex = POSITION3;

        mTabGroupModelFilter.moveRelatedTabs(mTab5.getId(), startIndex + 1);

        mModelAndObserverInOrder
                .verify(mTabGroupModelFilterObserver)
                .willMoveTabGroup(POSITION5, startIndex + 1);
        mModelAndObserverInOrder.verify(mTabModel).moveTab(mTab5.getId(), startIndex + 1);
        mModelAndObserverInOrder.verify(mTabModel).moveTab(mTab6.getId(), startIndex + 2);
        mModelAndObserverInOrder
                .verify(mTabGroupModelFilterObserver)
                .didMoveTabGroup(mTab6, POSITION6, startIndex + 2);
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
        mTab4.setRootId(TAB2_ROOT_ID);
        mTab4.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab4.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(1));

        TabStateAttributes.from(mTab4).clearTabStateDirtiness();
        reset(mAttributesObserver);

        // Undo the grouped action
        mTabGroupModelFilter.undoGroupedTab(mTab4, POSITION4, TAB4_ROOT_ID, TAB4_TAB_GROUP_ID);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab4.getRootId(), equalTo(TAB4_ROOT_ID));
        assertNull(mTab4.getTabGroupId());
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(2));
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab4, DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab4, POSITION2);
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab4), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void undoGroupedTab_Forward_UpdateTabModel() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        Token tabGroupId = new Token(91234L, 84567L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        mTabGroupModelFilter.createSingleTabGroup(mTab4, false);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab4, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab4, mTab4.getId());
        verify(mTabGroupModelFilterObserver, never())
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), anyBoolean());

        assertThat(mTab4.getTabGroupId(), equalTo(tabGroupId));

        // Simulate we just grouped mTab1 with mTab4
        mTab1.setRootId(TAB4_ROOT_ID);
        mTab1.setTabGroupId(tabGroupId);
        mTabModel.moveTab(mTab1.getId(), POSITION4 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab1.getRootId(), equalTo(TAB4_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(1));
        assertFalse(Arrays.equals(mTabs.toArray(), expectedTabModel.toArray()));

        reset(mTabGroupModelFilterObserver);

        // Undo the grouped action.
        mTabGroupModelFilter.undoGroupedTab(mTab1, POSITION1, TAB1_ROOT_ID, TAB1_TAB_GROUP_ID);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab1.getRootId(), equalTo(TAB1_ROOT_ID));
        assertNull(mTab1.getTabGroupId());
        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(0));
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab1, POSITION3);
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab1), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void undoGroupedTab_Backward_UpdateTabModel() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        Token tabGroupId = new Token(91234L, 84567L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(2));
        mTabGroupModelFilter.createSingleTabGroup(mTab1, true);
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));

        assertThat(mTab1.getTabGroupId(), equalTo(tabGroupId));

        // Simulate we just grouped mTab4 with mTab1
        mTab4.setRootId(TAB1_ROOT_ID);
        mTab4.setTabGroupId(tabGroupId);
        mTabModel.moveTab(mTab4.getId(), POSITION1 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab4.getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(0));
        assertFalse(Arrays.equals(mTabs.toArray(), expectedTabModel.toArray()));

        reset(mTabGroupModelFilterObserver);

        // Undo the grouped action.
        mTabGroupModelFilter.undoGroupedTab(mTab4, POSITION4, TAB4_ROOT_ID, TAB4_TAB_GROUP_ID);

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab4.getRootId(), equalTo(TAB4_ROOT_ID));
        assertNull(mTab4.getTabGroupId());
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(2));
        assertThat(mTabGroupModelFilter.getTabGroupCount(), equalTo(3));
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab4, POSITION1);
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab4), anyInt());
    }

    @Test
    public void undoGroupedTab_MultipleGroupUndoNoMovement() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        // Simulate we just grouped mTab4 and the group (mTab5, mTab6) with the group (mTab2,
        // mTab3).
        mTab4.setRootId(TAB2_ROOT_ID);
        mTab5.setRootId(TAB2_ROOT_ID);
        mTab6.setRootId(TAB2_ROOT_ID);
        mTab4.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTab5.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTab6.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab4.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab5.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab4.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(1));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Undo the grouped action in reverse order so indexes are correct.
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION6, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab6.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab5, POSITION5, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab5, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, mTab6.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab4, POSITION4, TAB4_ROOT_ID, TAB4_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab4, POSITION2);
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab4), anyInt());

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab4.getRootId(), equalTo(TAB4_ROOT_ID));
        assertThat(mTab5.getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTab4.getTabGroupId(), equalTo(TAB4_TAB_GROUP_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab4), equalTo(2));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(3));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(3));
    }

    @Test
    public void undoGroupedTabGroup_ToTab() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));

        // Simulate we just grouped the group (mTab2, mTab3) with mTab1.
        mTab1.setRootId(TAB1_ROOT_ID);
        mTab2.setRootId(TAB1_ROOT_ID);
        mTab3.setRootId(TAB1_ROOT_ID);
        mTab1.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTab2.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTab3.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab1.getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTab2.getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTab1.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(0));
        assertThat(mTabGroupModelFilter.indexOf(mTab2), equalTo(0));
        assertThat(mTabGroupModelFilter.indexOf(mTab3), equalTo(0));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Undo the grouped action in reverse order so indexes are correct.
        mTabGroupModelFilter.undoGroupedTab(mTab3, POSITION2, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab3, mTab3.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab2, POSITION2, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab2, POSITION1);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab2, mTab3.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab1, POSITION1, TAB1_ROOT_ID, null);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab1, POSITION1);
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab1), anyInt());

        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
        assertThat(mTab1.getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTab2.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ROOT_ID));
        assertNull(mTab1.getTabGroupId());
        assertThat(mTab2.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab3.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab1), equalTo(0));
        assertThat(mTabGroupModelFilter.indexOf(mTab2), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab3), equalTo(1));
    }

    @Test
    public void undoGroupedTab_MultipleGroupUndoWithMovement() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab6, mTab5, mTab4));

        // Simulate we just grouped the group (mTab5, mTab6) with the group (mTab2, mTab3).
        mTab5.setRootId(TAB2_ROOT_ID);
        mTab6.setRootId(TAB2_ROOT_ID);
        mTab5.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTab6.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTabModel.moveTab(mTab5.getId(), POSITION3 + 1);
        mTabModel.moveTab(mTab6.getId(), POSITION3 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab5.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(1));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Undo the grouped action in reverse order so indexes are correct.
        expectedTabModel = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION6, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab6.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab5, POSITION5, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab5, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, mTab6.getId());

        assertThat(mTab5.getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(3));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(3));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test
    public void undoGroupedTab_MultipleGroupUndoWithMovement_MergeListIncludesAllTabs() {
        List<Tab> expectedTabModel =
                new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab6, mTab5, mTab4));

        // Simulate we just grouped the group (mTab5, mTab6) with the group (mTab2, mTab3).
        mTab5.setRootId(TAB2_ROOT_ID);
        mTab6.setRootId(TAB2_ROOT_ID);
        mTab5.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTab6.setTabGroupId(TAB2_TAB_GROUP_ID);
        mTabModel.moveTab(mTab5.getId(), POSITION3 + 1);
        mTabModel.moveTab(mTab6.getId(), POSITION3 + 1);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab5.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB2_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(1));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(1));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());

        // Undo the grouped action in reverse order so indexes are correct.
        expectedTabModel = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6));
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION6, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab6, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab6, mTab6.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab5, POSITION5, TAB5_ROOT_ID, TAB5_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab5, POSITION2);
        verify(mTabGroupModelFilterObserver).didMergeTabToGroup(mTab5, mTab6.getId());
        mTabGroupModelFilter.undoGroupedTab(mTab3, POSITION3, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver, never()).didMoveTabOutOfGroup(eq(mTab3), anyInt());
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab3), anyInt());
        mTabGroupModelFilter.undoGroupedTab(mTab2, POSITION2, TAB2_ROOT_ID, TAB2_TAB_GROUP_ID);
        verify(mTabGroupModelFilterObserver, never()).didMoveTabOutOfGroup(eq(mTab2), anyInt());
        verify(mTabGroupModelFilterObserver, never()).didMergeTabToGroup(eq(mTab2), anyInt());

        assertThat(mTab5.getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB5_ROOT_ID));
        assertThat(mTab5.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTab6.getTabGroupId(), equalTo(TAB5_TAB_GROUP_ID));
        assertThat(mTabGroupModelFilter.indexOf(mTab5), equalTo(3));
        assertThat(mTabGroupModelFilter.indexOf(mTab6), equalTo(3));
        assertArrayEquals(mTabs.toArray(), expectedTabModel.toArray());
    }

    @Test(expected = AssertionError.class)
    public void undoGroupedTab_AssertTest() {
        // Simulate mTab6 is not in TabModel.
        doReturn(5).when(mTabModel).getCount();

        // Undo the grouped action.
        mTabGroupModelFilter.undoGroupedTab(mTab6, POSITION1, TAB1_ROOT_ID, TAB1_TAB_GROUP_ID);
    }

    @Test
    public void moveTab_Incognito() {
        setupTabGroupModelFilter(false, true);
        assertFalse(mTabGroupModelFilter.isTabModelRestored());

        mTabGroupModelFilter.markTabStateInitialized();
        assertTrue(mTabGroupModelFilter.isTabModelRestored());

        // Simulate that tab3 is going to be moved out of group.
        mTab3.setRootId(TAB3_ID);
        mTab3.setTabGroupId(null);

        mTabModelObserverCaptor.getValue().didMoveTab(mTab3, POSITION3, POSITION6);

        // Verify that the signal is not ignored.
        verify(mTabGroupModelFilterObserver).didMoveTabOutOfGroup(mTab3, POSITION2);
    }

    @Test
    public void resetFilterStateTest() {
        assertThat(mTab3.getRootId(), equalTo(TAB2_ROOT_ID));
        mTab3.setRootId(TAB1_ROOT_ID);
        mTabGroupModelFilter.resetFilterState();
        assertThat(mTab3.getRootId(), equalTo(TAB1_ROOT_ID));
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
        assertEquals(
                "Should have 1 related tab.",
                1,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB1_ROOT_ID));
        assertEquals(
                "Should have 2 related tabs.",
                2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB2_ROOT_ID));
        assertEquals(
                "Should have 2 related tabs.",
                2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB3_ROOT_ID));
        assertEquals(
                "Should have 1 related tab.",
                1,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB4_ROOT_ID));
        assertEquals(
                "Should have 2 related tabs.",
                2,
                mTabGroupModelFilter.getRelatedTabCountForRootId(TAB5_ROOT_ID));
        assertEquals(
                "Should have 2 related tabs.",
                2,
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
        // Override the setup behaviour for color SharedPreferences since after #didCreateNewGroup
        // is emitted, a color will have been set.
        when(mSharedPreferencesColor.getInt(anyString(), anyInt())).thenReturn(COLOR_ID);

        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab5, mTab6, mTab2, mTab3));
        List<Tab> expectedSourceTabs = mTabGroupModelFilter.getRelatedTabList(mTab2.getId());
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();
        List<Token> originalTabGroupIds = new ArrayList<>();

        List<Tab> expectedNotifiedTabs = new ArrayList();
        expectedNotifiedTabs.add(mTab5);
        expectedNotifiedTabs.addAll(expectedSourceTabs);
        originalIndexes.add(
                TabModelUtils.getTabIndexById(mTabGroupModelFilter.getTabModel(), mTab5.getId()));
        originalRootIds.add(mTab5.getRootId());
        originalTabGroupIds.add(mTab5.getTabGroupId());
        for (Tab tab : expectedSourceTabs) {
            // Use tab2 initial index for both related tabs index as the logic moves tab2 after
            // saving its index but before retrieving index for related tab3.
            originalIndexes.add(
                    TabModelUtils.getTabIndexById(
                            mTabGroupModelFilter.getTabModel(), mTab2.getId()));
            originalRootIds.add(tab.getRootId());
            originalTabGroupIds.add(tab.getTabGroupId());
        }

        mTabGroupModelFilter.mergeTabsToGroup(mTab2.getId(), mTab5.getId(), false);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab2, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(
                        expectedNotifiedTabs,
                        originalIndexes,
                        originalRootIds,
                        originalTabGroupIds,
                        TAB_TITLE,
                        COLOR_ID,
                        /* destinationGroupTitleCollapsed= */ true);
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(mTab2.getId(), TAB2_TAB_GROUP_ID, DidRemoveTabGroupReason.MERGE);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());

        assertEquals(mTab5.getTabGroupId(), TAB5_TAB_GROUP_ID);
        assertEquals(mTab6.getTabGroupId(), TAB5_TAB_GROUP_ID);
        assertEquals(mTab2.getTabGroupId(), TAB5_TAB_GROUP_ID);
        assertEquals(mTab3.getTabGroupId(), TAB5_TAB_GROUP_ID);
    }

    @Test
    public void mergeGroupToGroupNonAdjacent_doNotNotifyFilterObserver() {
        SharedPreferences.Editor titleEditor = Mockito.mock(SharedPreferences.Editor.class);
        when(mSharedPreferencesTitle.edit()).thenReturn(titleEditor);
        when(titleEditor.remove(anyString())).thenReturn(titleEditor);

        mTabGroupModelFilter.mergeTabsToGroup(mTab2.getId(), mTab5.getId(), true);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab2, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateGroup(
                        anyList(),
                        anyList(),
                        anyList(),
                        anyList(),
                        anyString(),
                        anyInt(),
                        anyBoolean());
        verify(titleEditor, times(2)).remove(String.valueOf(TAB2_ROOT_ID));
    }

    @Test
    public void mergeGroupToTabAdjacent_notifyFilterObserver() {
        // Override the setup behaviour for color SharedPreferences since after #didCreateNewGroup
        // is emitted, a color will have been set.
        when(mSharedPreferencesColor.getInt(anyString(), anyInt())).thenReturn(COLOR_ID);

        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab2, mTab3));
        List<Tab> expectedSourceTabs = mTabGroupModelFilter.getRelatedTabList(mTab3.getId());
        List<Integer> originalIndexes = new ArrayList<>();
        List<Integer> originalRootIds = new ArrayList<>();
        List<Token> originalTabGroupIds = new ArrayList<>();

        List<Tab> expectedNotifiedTabs = new ArrayList();
        expectedNotifiedTabs.add(mTab4);
        expectedNotifiedTabs.addAll(expectedSourceTabs);
        originalIndexes.add(
                TabModelUtils.getTabIndexById(mTabGroupModelFilter.getTabModel(), mTab4.getId()));
        originalRootIds.add(mTab4.getRootId());
        originalTabGroupIds.add(mTab4.getTabGroupId());
        for (Tab tab : expectedSourceTabs) {
            originalIndexes.add(
                    TabModelUtils.getTabIndexById(
                            mTabGroupModelFilter.getTabModel(), mTab2.getId()));
            originalRootIds.add(tab.getRootId());
            originalTabGroupIds.add(tab.getTabGroupId());
        }

        mTabGroupModelFilter.mergeTabsToGroup(mTab3.getId(), mTab4.getId(), false);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateNewGroup(mTab4, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver)
                .didCreateGroup(
                        expectedNotifiedTabs,
                        originalIndexes,
                        originalRootIds,
                        originalTabGroupIds,
                        TAB_TITLE,
                        COLOR_ID,
                        /* destinationGroupTitleCollapsed= */ true);
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab2.getId()).toArray(),
                expectedGroup.toArray());

        assertEquals(mTab4.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertEquals(mTab2.getTabGroupId(), TAB2_TAB_GROUP_ID);
        assertEquals(mTab3.getTabGroupId(), TAB2_TAB_GROUP_ID);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeTabToTab_notifyFilterObserver() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        // Override the setup behaviour for color SharedPreferences since after #didCreateNewGroup
        // is emitted, a color will have been set.
        when(mSharedPreferencesColor.getInt(anyString(), anyInt())).thenReturn(COLOR_ID);

        List<Tab> expectedGroup = new ArrayList<>(Arrays.asList(mTab4, mTab1));
        Token tabGroupId = new Token(33L, 82L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId(), false);
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab4, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateGroup(
                        anyList(),
                        anyList(),
                        anyList(),
                        anyList(),
                        anyString(),
                        anyInt(),
                        anyBoolean());
        assertArrayEquals(
                mTabGroupModelFilter.getRelatedTabList(mTab1.getId()).toArray(),
                expectedGroup.toArray());

        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab4.getTabGroupId(), tabGroupId);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID)
    public void mergeTabToTab_doNotNotifyFilterObserver() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(false);

        Token tabGroupId = new Token(33L, 28L);
        when(mTokenJniMock.createRandom()).thenReturn(tabGroupId);

        mTabGroupModelFilter.mergeTabsToGroup(mTab1.getId(), mTab4.getId(), true);
        verify(mTabGroupModelFilterObserver).didCreateNewGroup(mTab4, mTabGroupModelFilter);
        verify(mTabGroupModelFilterObserver, never())
                .didCreateGroup(any(), any(), any(), any(), any(), anyInt(), anyBoolean());

        assertEquals(mTab1.getTabGroupId(), tabGroupId);
        assertEquals(mTab4.getTabGroupId(), tabGroupId);
    }

    @Test
    public void testTabGroupIds() {
        assertEquals(mTab1.getRootId(), TAB1_ROOT_ID);
        assertEquals(mTab1.getTabGroupId(), TAB1_TAB_GROUP_ID);

        assertEquals(mTab2.getRootId(), TAB2_ROOT_ID);
        assertEquals(mTab2.getTabGroupId(), TAB2_TAB_GROUP_ID);

        assertEquals(mTab3.getRootId(), TAB3_ROOT_ID);
        assertEquals(mTab3.getTabGroupId(), TAB3_TAB_GROUP_ID);

        assertEquals(mTab4.getRootId(), TAB4_ROOT_ID);
        assertEquals(mTab4.getTabGroupId(), TAB4_TAB_GROUP_ID);

        assertEquals(mTab5.getRootId(), TAB5_ROOT_ID);
        assertEquals(mTab5.getTabGroupId(), TAB5_TAB_GROUP_ID);

        assertEquals(mTab6.getRootId(), TAB6_ROOT_ID);
        assertEquals(mTab6.getTabGroupId(), TAB6_TAB_GROUP_ID);
    }

    @Test
    public void testAssignTabGroupIds() {
        mTab1.setTabGroupId(null);
        mTab2.setTabGroupId(null);
        mTab3.setTabGroupId(null);
        mTab4.setTabGroupId(null);
        mTab5.setTabGroupId(null);
        mTab6.setTabGroupId(null);

        Token tabGroupIdTab2 = new Token(1L, 2L);
        Token tabGroupIdTab5 = new Token(5L, 6L);
        Token tabGroupIdUnused = new Token(3L, 3L);
        when(mTokenJniMock.createRandom())
                .thenReturn(tabGroupIdTab2)
                .thenReturn(tabGroupIdTab5)
                .thenReturn(tabGroupIdUnused);

        mTabGroupModelFilter.addTabGroupIdsForAllTabGroups();

        assertEquals(mTab1.getRootId(), TAB1_ROOT_ID);
        assertNull(mTab1.getTabGroupId());

        assertEquals(mTab2.getRootId(), TAB2_ROOT_ID);
        assertEquals(mTab2.getTabGroupId(), tabGroupIdTab2);

        assertEquals(mTab3.getRootId(), TAB3_ROOT_ID);
        assertEquals(mTab3.getTabGroupId(), tabGroupIdTab2);

        assertEquals(mTab4.getRootId(), TAB4_ROOT_ID);
        assertNull(mTab4.getTabGroupId());

        assertEquals(mTab5.getRootId(), TAB5_ROOT_ID);
        assertEquals(mTab5.getTabGroupId(), tabGroupIdTab5);

        assertEquals(mTab6.getRootId(), TAB6_ROOT_ID);
        assertEquals(mTab6.getTabGroupId(), tabGroupIdTab5);
    }

    @Test
    public void testRelatedTabsExistForRootId() {
        assertThat(mTab1.getRootId(), equalTo(TAB1_ROOT_ID));
        assertThat(mTab3.getRootId(), equalTo(TAB2_ROOT_ID));
        assertThat(mTab6.getRootId(), equalTo(TAB5_ROOT_ID));

        mTabGroupModelFilter.removeTab(mTab1);
        mTabGroupModelFilter.removeTab(mTab3);
        mTabGroupModelFilter.removeTab(mTab5);

        assertFalse(mTabGroupModelFilter.tabGroupExistsForRootId(mTab1.getRootId()));
        assertTrue(mTabGroupModelFilter.tabGroupExistsForRootId(mTab3.getRootId()));
        assertTrue(mTabGroupModelFilter.tabGroupExistsForRootId(mTab5.getRootId()));
    }

    @Test
    public void testGetAllTabGroupRootIds() {
        // With the given setup, mTab2 and mTab3 are in a group and mTab5 and mTab6 are in another
        // group.
        Set<Integer> rootIds = new ArraySet<>();
        rootIds.add(mTab2.getRootId());
        rootIds.add(mTab5.getRootId());

        assertEquals(rootIds, mTabGroupModelFilter.getAllTabGroupRootIds());
    }

    @Test
    public void testGetAllTabGroupIds() {
        // With the given setup, mTab2 and mTab3 are in a group and mTab5 and mTab6 are in another
        // group.
        Set<Token> tabGroupIds = new ArraySet<>();
        tabGroupIds.add(mTab2.getTabGroupId());
        tabGroupIds.add(mTab5.getTabGroupId());

        assertEquals(tabGroupIds, mTabGroupModelFilter.getAllTabGroupIds());
    }

    @Test
    public void testGetLazyAllTabRootIdsInComprehensiveModel() {
        // With the given setup, mTab2 and mTab3 are in a group and mTab5 and mTab6 are in another
        // group. Tabs 1 and 4 are also unique.
        Set<Integer> rootIds = new ArraySet<>();
        rootIds.add(mTab1.getRootId());
        rootIds.add(mTab2.getRootId());
        rootIds.add(mTab4.getRootId());
        rootIds.add(mTab5.getRootId());

        assertEquals(
                rootIds,
                mTabGroupModelFilter
                        .getLazyAllRootIdsInComprehensiveModel(new ArrayList<Tab>())
                        .get());
    }

    @Test
    public void testGetLazyAllTabGroupIdsInComprehensiveModel() {
        // With the given setup, mTab2 and mTab3 are in a group and mTab5 and mTab6 are in another
        // group.
        Set<Token> tabGroupIds = new ArraySet<>();
        tabGroupIds.add(mTab2.getTabGroupId());
        tabGroupIds.add(mTab5.getTabGroupId());

        assertEquals(
                tabGroupIds,
                mTabGroupModelFilter
                        .getLazyAllTabGroupIdsInComprehensiveModel(new ArrayList<Tab>())
                        .get());
    }

    @Test
    public void testGetLazyAllTabGroupIdsInComprehensiveModel_ExcludePartial() {
        // With the given setup, mTab2 and mTab3 are in a group and mTab5 and mTab6 are in another
        // group.
        Set<Token> tabGroupIds = new ArraySet<>();
        tabGroupIds.add(mTab2.getTabGroupId());
        tabGroupIds.add(mTab5.getTabGroupId());

        assertEquals(
                tabGroupIds,
                mTabGroupModelFilter
                        .getLazyAllTabGroupIdsInComprehensiveModel(List.of(mTab2, mTab5))
                        .get());
    }

    @Test
    public void testGetLazyAllTabGroupIdsInComprehensiveModel_ExcludeFull() {
        // With the given setup, mTab2 and mTab3 are in a group and mTab5 and mTab6 are in another
        // group.
        Set<Token> tabGroupIds = new ArraySet<>();
        tabGroupIds.add(mTab5.getTabGroupId());

        assertEquals(
                tabGroupIds,
                mTabGroupModelFilter
                        .getLazyAllTabGroupIdsInComprehensiveModel(List.of(mTab2, mTab3))
                        .get());
    }

    @Test
    public void testSetTabGroupTitle() {
        mTabGroupModelFilter.setTabGroupTitle(TAB2_ROOT_ID, "Foo");
        verify(mTabGroupModelFilterObserver).didChangeTabGroupTitle(TAB2_ROOT_ID, "Foo");
    }

    @Test
    public void testDeleteTabGroupTitle() {
        mTabGroupModelFilter.deleteTabGroupTitle(TAB2_ROOT_ID);
        verify(mTabGroupModelFilterObserver)
                .didChangeTabGroupTitle(TAB2_ROOT_ID, /* newTitle= */ null);
    }

    @Test
    public void testGetOrCreateTabGroupColor() {
        assertEquals(
                TabGroupColorId.GREY,
                mTabGroupModelFilter.getTabGroupColorWithFallback(TAB1_ROOT_ID));

        when(mSharedPreferencesColor.getInt(eq(String.valueOf(TAB2_ROOT_ID)), anyInt()))
                .thenReturn(TabGroupColorId.BLUE);
        assertEquals(
                TabGroupColorId.BLUE,
                mTabGroupModelFilter.getTabGroupColorWithFallback(TAB2_ROOT_ID));
    }

    @Test
    public void testSetTabGroupColor() {
        mTabGroupModelFilter.setTabGroupColor(TAB2_ROOT_ID, TabGroupColorId.GREY);
        verify(mTabGroupModelFilterObserver)
                .didChangeTabGroupColor(TAB2_ROOT_ID, TabGroupColorId.GREY);
    }

    @Test
    public void testSetTabGroupCollapsed() {
        mTabGroupModelFilter.setTabGroupCollapsed(TAB2_ROOT_ID, /* isCollapsed= */ true);
        verify(mTabGroupModelFilterObserver)
                .didChangeTabGroupCollapsed(TAB2_ROOT_ID, /* isCollapsed= */ true);
    }

    @Test
    public void testDeleteTabGroupCollapsed() {
        mTabGroupModelFilter.deleteTabGroupCollapsed(TAB2_ROOT_ID);
        verify(mTabGroupModelFilterObserver)
                .didChangeTabGroupCollapsed(TAB2_ROOT_ID, /* isCollapsed= */ false);
    }

    @Test
    public void testSetTabGroupSyncId() {
        String prefKey = String.valueOf(TAB2_ROOT_ID);
        mTabGroupModelFilter.setTabGroupSyncId(TAB2_ROOT_ID, "Foo");
        verify(mEditor).putString(eq(prefKey), eq("Foo"));
        mTabGroupModelFilter.getTabGroupSyncId(TAB2_ROOT_ID);
        verify(mSharedPreferencesSyncId).getString(eq(prefKey), eq(null));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseGroup_Hiding_Undone() {
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        List<Tab> groupWithTab2AndTab3 = List.of(mTab2, mTab3);
        // allowUndo will always be true for pending tab closure, but use false just to verify it is
        // forwarded correctly.
        var params =
                TabClosureParams.closeTabs(groupWithTab2AndTab3)
                        .allowUndo(false)
                        .hideTabGroups(true)
                        .build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);

        mTabGroupModelFilter.tabClosureUndone(mTab2);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        mTabGroupModelFilter.tabClosureUndone(mTab3);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseGroup_Hiding_Committed() {
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        List<Tab> groupWithTab2AndTab3 = List.of(mTab2, mTab3);
        var params = TabClosureParams.closeTabs(groupWithTab2AndTab3).hideTabGroups(true).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);

        mTabs.remove(mTab2);
        mTabs.remove(mTab3);
        mTabGroupModelFilter.onFinishingMultipleTabClosure(
                groupWithTab2AndTab3, /* canRestore= */ true);
        // The root ID might have mutated so just assert on the last two.
        verify(mTabGroupModelFilterObserver)
                .didRemoveTabGroup(
                        anyInt(), eq(TAB2_TAB_GROUP_ID), eq(DidRemoveTabGroupReason.CLOSE));
        verify(mTabGroupModelFilterObserver)
                .committedTabGroupClosure(TAB2_TAB_GROUP_ID, /* wasHiding= */ true);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseMultipleTabs_Hiding_GroupInParts() {
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        List<Tab> listWithTab2AndTab4 = List.of(mTab2, mTab4);
        var params = TabClosureParams.closeTabs(listWithTab2AndTab4).hideTabGroups(true).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);

        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab4, /* didCloseAlone= */ false);
        mTabs.remove(mTab2);
        mTabs.remove(mTab4);
        mTabGroupModelFilter.onFinishingMultipleTabClosure(
                listWithTab2AndTab4, /* canRestore= */ true);
        verify(mTabGroupModelFilterObserver, never()).committedTabGroupClosure(any(), anyBoolean());

        // Close the remainder of the group separately.
        List<Tab> groupWithTab3 = List.of(mTab3);
        params = TabClosureParams.closeTabs(groupWithTab3).hideTabGroups(true).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);
        mTabs.remove(mTab3);

        mTabGroupModelFilter.onFinishingMultipleTabClosure(groupWithTab3, /* canRestore= */ true);
        verify(mTabGroupModelFilterObserver)
                .committedTabGroupClosure(TAB2_TAB_GROUP_ID, /* wasHiding= */ true);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseGroup_Deleted_Committed() {
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        List<Tab> groupWithTab2AndTab3 = List.of(mTab2, mTab3);
        var params = TabClosureParams.closeTabs(groupWithTab2AndTab3).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);

        mTabs.remove(mTab2);
        mTabs.remove(mTab3);
        mTabGroupModelFilter.onFinishingMultipleTabClosure(
                groupWithTab2AndTab3, /* canRestore= */ true);
        verify(mTabGroupModelFilterObserver)
                .committedTabGroupClosure(TAB2_TAB_GROUP_ID, /* wasHiding= */ false);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseAllTabs_Hiding_Undone() {
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));

        var params = TabClosureParams.closeAllTabs().hideTabGroups(true).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab1, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab4, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab5, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab6, /* didCloseAlone= */ false);

        mTabs.remove(mTab2);
        mTabs.remove(mTab3);
        mTabs.remove(mTab4);
        mTabs.remove(mTab5);
        mTabs.remove(mTab6);

        mTabGroupModelFilter.tabClosureUndone(mTab1);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        mTabs.add(mTab2);
        mTabGroupModelFilter.tabClosureUndone(mTab2);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        mTabs.add(mTab3);
        mTabGroupModelFilter.tabClosureUndone(mTab3);
        mTabs.add(mTab4);
        mTabGroupModelFilter.tabClosureUndone(mTab4);
        mTabs.add(mTab5);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));
        mTabGroupModelFilter.tabClosureUndone(mTab5);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));
        mTabs.remove(mTab6);
        mTabGroupModelFilter.tabClosureUndone(mTab6);
    }

    /**
     * The partial tab-by-tab commit tested here is impossible with how {@link
     * PendingTabClosureManager} works, but for testing it is useful to verify the behavior is
     * correct.
     */
    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseAllTabs_Hiding_PartialCommit() {
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));

        var params = TabClosureParams.closeAllTabs().hideTabGroups(true).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab1, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab4, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab5, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab6, /* didCloseAlone= */ false);

        mTabs.remove(mTab2);
        mTabs.remove(mTab3);
        mTabs.remove(mTab4);
        mTabs.remove(mTab5);
        mTabs.remove(mTab6);

        mTabGroupModelFilter.tabClosureUndone(mTab1);
        assertTrue(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        mTabs.add(mTab2);
        mTabGroupModelFilter.tabClosureUndone(mTab2);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        mTabs.add(mTab3);
        mTabGroupModelFilter.tabClosureUndone(mTab3);

        mTabGroupModelFilter.onFinishingMultipleTabClosure(
                List.of(mTab4, mTab5, mTab6), /* canRestore= */ true);
        verify(mTabGroupModelFilterObserver, never())
                .committedTabGroupClosure(eq(TAB2_TAB_GROUP_ID), anyBoolean());
        verify(mTabGroupModelFilterObserver)
                .committedTabGroupClosure(TAB5_TAB_GROUP_ID, /* wasHiding= */ true);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_SYNC_ANDROID})
    public void testCloseAllTabs_Deleted() {
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));

        var params = TabClosureParams.closeAllTabs().hideTabGroups(false).build();
        mTabGroupModelFilter.closeTabs(params);
        verify(mTabModel).closeTabs(params);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));

        mTabGroupModelFilter.willCloseTab(mTab1, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab2, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab3, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab4, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab5, /* didCloseAlone= */ false);
        mTabGroupModelFilter.willCloseTab(mTab6, /* didCloseAlone= */ false);

        mTabs.remove(mTab1);
        mTabs.remove(mTab2);
        mTabs.remove(mTab3);
        mTabs.remove(mTab4);
        mTabs.remove(mTab5);
        mTabs.remove(mTab6);
        mTabGroupModelFilter.onFinishingMultipleTabClosure(
                List.of(mTab1, mTab2, mTab3, mTab4, mTab5, mTab6), /* canRestore= */ true);
        verify(mTabGroupModelFilterObserver)
                .committedTabGroupClosure(TAB2_TAB_GROUP_ID, /* wasHiding= */ false);
        verify(mTabGroupModelFilterObserver)
                .committedTabGroupClosure(TAB5_TAB_GROUP_ID, /* wasHiding= */ false);
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB2_TAB_GROUP_ID));
        assertFalse(mTabGroupModelFilter.isTabGroupHiding(TAB5_TAB_GROUP_ID));
    }

    @Test
    public void testWillMergingCreateNewGroup_NewGroup() {
        // Mock a merge between mTab1 and mTab4, neither of which are in a group.
        List<Tab> tabsToMerge = List.of(mTab1, mTab4);
        assertTrue(mTabGroupModelFilter.willMergingCreateNewGroup(tabsToMerge));
    }

    @Test
    public void testWillMergingCreateNewGroup_ExistingGroup() {
        // Mock a merge between mTab1, mTab2 and mTab3, of which the latter 2 are in a group.
        List<Tab> tabsToMerge = List.of(mTab1, mTab2, mTab3);
        assertFalse(mTabGroupModelFilter.willMergingCreateNewGroup(tabsToMerge));
    }
}
