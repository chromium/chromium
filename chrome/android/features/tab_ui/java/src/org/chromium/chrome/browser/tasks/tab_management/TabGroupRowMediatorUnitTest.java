// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.CLUSTER_DATA;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupRowProperties.COLOR_INDEX;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupFaviconCluster.ClusterData;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link TabGroupRowMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID, ChromeFeatureList.DATA_SHARING})
public class TabGroupRowMediatorUnitTest {
    private static final String SYNC_GROUP_ID1 = "remote one";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FaviconResolver mFaviconResolver;

    private PropertyModel buildTestModel(List<SavedTabGroupTab> savedTabs) {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = SYNC_GROUP_ID1;
        group.title = "Title";
        group.color = TabGroupColorId.BLUE;
        group.savedTabs = savedTabs;

        return TabGroupRowMediator.buildModel(
                group, mFaviconResolver, /* openRunnable= */ null, /* deleteRunnable= */ null);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testNoParity() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList(new SavedTabGroupTab()));
        // 0 is the default value.
        assertEquals(0, propertyModel.get(COLOR_INDEX));
    }

    @Test
    @SmallTest
    public void testFavicons_zero() {
        PropertyModel propertyModel = buildTestModel(Arrays.asList());
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(0, clusterData.totalCount);
        assertEquals(0, clusterData.firstUrls.size());
    }

    @Test
    @SmallTest
    public void testFavicons_one() {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.url = JUnitTestGURLs.URL_1;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(1, clusterData.totalCount);
        assertEquals(1, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
    }

    @Test
    @SmallTest
    public void testFavicons_two() {
        SavedTabGroupTab tab1 = new SavedTabGroupTab();
        tab1.url = JUnitTestGURLs.URL_1;
        SavedTabGroupTab tab2 = new SavedTabGroupTab();
        tab2.url = JUnitTestGURLs.URL_2;

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(2, clusterData.totalCount);
        assertEquals(2, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
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

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2, tab3));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(3, clusterData.totalCount);
        assertEquals(3, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
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

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2, tab3, tab4));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(4, clusterData.totalCount);
        assertEquals(4, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
        assertEquals(JUnitTestGURLs.BLUE_1, clusterData.firstUrls.get(3));
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

        PropertyModel propertyModel = buildTestModel(Arrays.asList(tab1, tab2, tab3, tab4, tab5));
        ClusterData clusterData = propertyModel.get(CLUSTER_DATA);
        assertEquals(5, clusterData.totalCount);
        assertEquals(4, clusterData.firstUrls.size());
        assertEquals(JUnitTestGURLs.URL_1, clusterData.firstUrls.get(0));
        assertEquals(JUnitTestGURLs.URL_2, clusterData.firstUrls.get(1));
        assertEquals(JUnitTestGURLs.URL_3, clusterData.firstUrls.get(2));
        assertEquals(JUnitTestGURLs.BLUE_1, clusterData.firstUrls.get(3));
    }
}
