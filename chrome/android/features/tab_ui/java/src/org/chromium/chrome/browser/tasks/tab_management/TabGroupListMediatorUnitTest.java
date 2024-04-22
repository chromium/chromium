// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_BOTTOM_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_LEFT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.ASYNC_FAVICON_TOP_RIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.PLUS_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.TITLE_DATA;

import android.graphics.drawable.Drawable;

import androidx.core.util.Pair;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.function.BiConsumer;

/** Tests for {@link TabGroupListMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
public class TabGroupListMediatorUnitTest {
    private static final String SYNC_GROUP_ID1 = "remote one";
    private static final String SYNC_GROUP_ID2 = "remote two";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private BiConsumer<GURL, Callback<Drawable>> mFaviconResolver;
    @Mock private Callback<Drawable> mFaviconCallback1;
    @Mock private Callback<Drawable> mFaviconCallback2;
    @Mock private Callback<Drawable> mFaviconCallback3;
    @Mock private Callback<Drawable> mFaviconCallback4;

    @Captor private ArgumentCaptor<TabGroupSyncService.Observer> mSyncObserverCaptor;

    private ModelList mModelList;

    @Before
    public void setUp() {
        mModelList = new ModelList();
    }

    @Test
    @SmallTest
    public void testNoTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);
        assertEquals(0, mModelList.size());
    }

    @Test
    @SmallTest
    public void testOneGroup() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);
        assertEquals(1, mModelList.size());

        PropertyModel model = mModelList.get(0).model;
        assertEquals(new Pair<>("Title", 1), model.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, model.get(COLOR_INDEX));
    }

    @Test
    @SmallTest
    public void testTwoGroups() {
        SavedTabGroup group1 = new SavedTabGroup();
        group1.syncId = SYNC_GROUP_ID1;
        group1.title = "Foo";
        group1.color = TabGroupColorId.BLUE;
        group1.savedTabs = Arrays.asList(new SavedTabGroupTab(), new SavedTabGroupTab());

        SavedTabGroup group2 = new SavedTabGroup();
        group2.syncId = SYNC_GROUP_ID1;
        group2.title = "Bar";
        group2.color = TabGroupColorId.RED;
        group2.savedTabs =
                Arrays.asList(
                        new SavedTabGroupTab(), new SavedTabGroupTab(), new SavedTabGroupTab());

        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2)).thenReturn(group2);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);
        assertEquals(2, mModelList.size());

        PropertyModel model1 = mModelList.get(0).model;
        assertEquals(new Pair<>("Foo", 2), model1.get(TITLE_DATA));
        assertEquals(TabGroupColorId.BLUE, model1.get(COLOR_INDEX));

        PropertyModel model2 = mModelList.get(1).model;
        assertEquals(new Pair<>("Bar", 3), model2.get(TITLE_DATA));
        assertEquals(TabGroupColorId.RED, model2.get(COLOR_INDEX));
    }

    @Test
    @SmallTest
    public void testObservation() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);
        assertEquals(1, mModelList.size());

        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        verify(mTabGroupSyncService).addObserver(mSyncObserverCaptor.capture());
        mSyncObserverCaptor.getValue().onTabGroupRemoved(SYNC_GROUP_ID1);
        ShadowLooper.idleMainLooper();

        assertEquals(0, mModelList.size());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testNoParity() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(new SavedTabGroupTab());
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);
        assertEquals(1, mModelList.size());
        // 0 is the default value.
        assertEquals(0, mModelList.get(0).model.get(COLOR_INDEX));
    }

    @Test
    @SmallTest
    public void testFavicons_one() {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.url = JUnitTestGURLs.URL_1;
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(tab);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);

        assertEquals(1, mModelList.size());
        PropertyModel propertyModel = mModelList.get(0).model;
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        assertNull(propertyModel.get(ASYNC_FAVICON_TOP_RIGHT));
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT));
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
    }

    @Test
    @SmallTest
    public void testFavicons_two() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(tab1, tab2);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);

        assertEquals(1, mModelList.size());
        PropertyModel propertyModel = mModelList.get(0).model;
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT));
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
    }

    @Test
    @SmallTest
    public void testFavicons_three() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroupTab tab3 = new SavedTabGroupTab();
        tab3.url = JUnitTestGURLs.URL_3;
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(tab1, tab2, tab3);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);

        assertEquals(1, mModelList.size());
        PropertyModel propertyModel = mModelList.get(0).model;
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT).accept(mFaviconCallback3);
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_3), eq(mFaviconCallback3));
    }

    @Test
    @SmallTest
    public void testFavicons_four() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroupTab tab3 = new SavedTabGroupTab();
        tab3.url = JUnitTestGURLs.URL_3;
        SavedTabGroupTab tab4 = new SavedTabGroupTab();
        tab4.url = JUnitTestGURLs.BLUE_1;
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(tab1, tab2, tab3, tab4);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);

        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);

        assertEquals(1, mModelList.size());
        PropertyModel propertyModel = mModelList.get(0).model;
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT).accept(mFaviconCallback3);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT).accept(mFaviconCallback4);
        assertNull(propertyModel.get(PLUS_COUNT));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_3), eq(mFaviconCallback3));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.BLUE_1), eq(mFaviconCallback4));
    }

    @Test
    @SmallTest
    public void testFavicons_five() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;
        SavedTabGroupTab tab3 = new SavedTabGroupTab();
        tab3.url = JUnitTestGURLs.URL_3;
        SavedTabGroupTab tab4 = new SavedTabGroupTab();
        tab4.url = JUnitTestGURLs.BLUE_1;
        SavedTabGroupTab tab5 = new SavedTabGroupTab();
        tab5.url = JUnitTestGURLs.BLUE_2;
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = Arrays.asList(tab1, tab2, tab3, tab4, tab5);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(group);
        new TabGroupListMediator(
                mModelList, mTabGroupModelFilter, mFaviconResolver, mTabGroupSyncService);

        assertEquals(1, mModelList.size());
        PropertyModel propertyModel = mModelList.get(0).model;
        propertyModel.get(ASYNC_FAVICON_TOP_LEFT).accept(mFaviconCallback1);
        propertyModel.get(ASYNC_FAVICON_TOP_RIGHT).accept(mFaviconCallback2);
        propertyModel.get(ASYNC_FAVICON_BOTTOM_LEFT).accept(mFaviconCallback3);
        assertNull(propertyModel.get(ASYNC_FAVICON_BOTTOM_RIGHT));
        assertEquals(2, propertyModel.get(PLUS_COUNT).intValue());
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_1), eq(mFaviconCallback1));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_2), eq(mFaviconCallback2));
        verify(mFaviconResolver).accept(eq(JUnitTestGURLs.URL_3), eq(mFaviconCallback3));
    }
}
