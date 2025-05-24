// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabmodel.TabGroupUtils.areAnyTabsPartOfSharedGroup;

import android.text.TextUtils;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Tests for {@link TabGroupUtils}. */
@SuppressWarnings("ResultOfMethodCallIgnored")
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUtilsUnitTest {
    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final Token TAB_GROUP_ID1 = new Token(2L, 2L);
    private static final Token TAB_GROUP_ID2 = new Token(4L, 4L);
    private static final String TAB_GROUP_TITLE = "Regrouped tabs";
    private static final ArrayList<Map.Entry<Integer, String>> TAB_IDS_TO_URLS =
            new ArrayList<>(
                    List.of(
                            Map.entry(TAB1_ID, "https://www.amazon.com/"),
                            Map.entry(TAB2_ID, "https://www.youtube.com/"),
                            Map.entry(TAB3_ID, "https://www.facebook.com/")));

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock TabModel mTabModel;
    @Mock TabModelSelector mTabModelSelector;
    @Mock TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabGroupSyncService mTabGroupSyncService;
    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock TabCreator mTabCreator;
    @Mock Profile mProfile;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;
    private SavedTabGroup mSavedTabGroup1;
    private SavedTabGroup mSavedTabGroup2;

    @Before
    public void setUp() {
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_TITLE, GURL.emptyGURL());
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_TITLE, GURL.emptyGURL());
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_TITLE, GURL.emptyGURL());

        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getCurrentTabGroupModelFilter();
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);

        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        mSavedTabGroup1 = new SavedTabGroup();
        mSavedTabGroup2 = new SavedTabGroup();
    }

    @Test
    public void testGetSelectedTabInGroupForTab() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);

        assertThat(
                TabGroupUtils.getSelectedTabInGroupForTab(mTabGroupModelFilter, mTab1),
                equalTo(mTab1));
        assertThat(
                TabGroupUtils.getSelectedTabInGroupForTab(mTabGroupModelFilter, mTab2),
                equalTo(mTab1));
    }

    @Test
    public void testGetFirstTabModelIndexForList() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));

        assertThat(TabGroupUtils.getFirstTabModelIndexForList(mTabModel, tabs), equalTo(POSITION1));
    }

    @Test
    public void testGetLastTabModelIndexForList() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));

        assertThat(TabGroupUtils.getLastTabModelIndexForList(mTabModel, tabs), equalTo(POSITION3));
    }

    @Test
    public void testOpenUrlInGroup() {
        when(mTabGroupModelFilter.getRelatedTabList(eq(TAB1_ID))).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);

        @TabLaunchType int launchType = TabLaunchType.FROM_TAB_GROUP_UI;
        String url = JUnitTestGURLs.URL_1.getSpec();
        TabGroupUtils.openUrlInGroup(mTabGroupModelFilter, url, TAB1_ID, launchType);
        ArgumentMatcher<LoadUrlParams> matcher = params -> TextUtils.equals(params.getUrl(), url);
        verify(mTabCreator).createNewTab(argThat(matcher), eq(launchType), eq(mTab1));
    }

    @Test
    public void testAreAnyTabsPartOfSharedGroup_oneSharedGroup() {
        mSavedTabGroup1.collaborationId = "collaborationId";
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID1);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(TAB_GROUP_ID1)))
                .thenReturn(mSavedTabGroup1);

        assertTrue(areAnyTabsPartOfSharedGroup(mTabModel, List.of(mTab1, mTab2), null));
    }

    @Test
    public void testAreAnyTabsPartOfSharedGroup_multipleSharedGroups() {
        mSavedTabGroup1.collaborationId = "collaborationId1";
        mSavedTabGroup2.collaborationId = "collaborationId2";
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID1);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID2);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(TAB_GROUP_ID1)))
                .thenReturn(mSavedTabGroup1);
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(TAB_GROUP_ID2)))
                .thenReturn(mSavedTabGroup2);

        assertTrue(areAnyTabsPartOfSharedGroup(mTabModel, List.of(mTab1, mTab2), null));
    }

    @Test
    public void testAreAnyTabsPartOfSharedGroup_oneSharedGroupWithDestId() {
        mSavedTabGroup1.collaborationId = "collaborationId";
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID1);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabGroupSyncService.getGroup(new LocalTabGroupId(TAB_GROUP_ID1)))
                .thenReturn(mSavedTabGroup1);

        assertFalse(areAnyTabsPartOfSharedGroup(mTabModel, List.of(mTab1, mTab2), TAB_GROUP_ID1));
    }

    @Test
    public void testRegroupTabs() {
        verifyRegroupTabs(/* shouldApplyCollapse= */ true);
    }

    @Test
    public void testRegroupTabs_CollapseStateNotApplied() {
        verifyRegroupTabs(/* shouldApplyCollapse= */ false);
    }

    @Test
    public void testFindSingleTabGroupIfPresent_oneTabNotInGroup() {
        when(mTab1.getTabGroupId()).thenReturn(null);
        assertNull(TabGroupUtils.findSingleTabGroupIfPresent(List.of(mTab1)));
    }

    @Test
    public void testFindSingleTabGroupIfPresent_oneTabInGroup() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID1);
        assertEquals(TAB_GROUP_ID1, TabGroupUtils.findSingleTabGroupIfPresent(List.of(mTab1)));
    }

    @Test
    public void testFindSingleTabGroupIfPresent_sameGroup() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID1);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID1);

        assertEquals(
                TAB_GROUP_ID1, TabGroupUtils.findSingleTabGroupIfPresent(List.of(mTab1, mTab2)));
    }

    @Test
    public void testFindSingleTabGroupIfPresent_notInSameGroup() {
        when(mTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID1);
        when(mTab2.getTabGroupId()).thenReturn(TAB_GROUP_ID2);

        assertNull(TabGroupUtils.findSingleTabGroupIfPresent(List.of(mTab1, mTab2)));
    }

    private void verifyRegroupTabs(boolean shouldApplyCollapse) {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));
        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        /* rootId= */ TAB1_ID,
                        /* selectedTabId= */ TAB1_ID,
                        /* sourceWindowId= */ 1,
                        TAB_GROUP_ID1,
                        TAB_IDS_TO_URLS,
                        /* tabGroupColor= */ 0,
                        TAB_GROUP_TITLE,
                        /* mhtmlTabTitle= */ null,
                        /* tabGroupCollapsed= */ true,
                        /* isGroupShared= */ false,
                        /* isIncognito= */ false);
        TabGroupUtils.regroupTabs(
                mTabGroupModelFilter, tabs, tabGroupMetadata, shouldApplyCollapse);

        verify(mTabGroupModelFilter).createTabGroupForTabGroupSync(any(), eq(TAB_GROUP_ID1));
        verify(mTabGroupModelFilter).setTabGroupColor(eq(TAB1_ID), eq(0));
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(TAB_GROUP_TITLE));
        if (shouldApplyCollapse) {
            verify(mTabGroupModelFilter).setTabGroupCollapsed(eq(TAB1_ID), eq(true), eq(false));
        } else {
            verify(mTabGroupModelFilter, never())
                    .setTabGroupCollapsed(anyInt(), anyBoolean(), anyBoolean());
        }
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(tab.getRootId()).thenReturn(rootId);
        }
    }
}
