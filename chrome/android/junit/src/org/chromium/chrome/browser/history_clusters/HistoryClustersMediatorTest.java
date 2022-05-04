// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Promise;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Unit tests for HistoryClustersMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private HistoryClustersBridge mBridge;
    @Mock
    private Context mContext;
    @Mock
    private Resources mResources;
    @Mock
    private LargeIconBridge mLargeIconBridge;
    @Mock
    private GURL mGurl1;
    @Mock
    private GURL mGurl2;
    @Mock
    private GURL mGurl3;
    @Mock
    private BottomSheetController mBottomSheetController;

    private ClusterVisit mVisit1;
    private ClusterVisit mVisit2;
    private ClusterVisit mVisit3;
    private HistoryCluster mCluster1;
    private HistoryCluster mCluster2;
    private HistoryClustersResult mHistoryClustersResult;
    private ModelList mModelList;
    private HistoryClustersBottomSheetContent mBottomSheetContent =
            new HistoryClustersBottomSheetContent();
    private Intent mIntent = new Intent();
    private Supplier<Intent> mHistoryActivityIntentFactory = () -> mIntent;
    private HistoryClustersMediator mMediator;

    @Before
    public void setUp() {
        doReturn(mResources).when(mContext).getResources();
        mModelList = new ModelList();
        mBottomSheetContent = new HistoryClustersBottomSheetContent();
        mMediator = new HistoryClustersMediator(mBridge, mLargeIconBridge, mContext, mResources,
                mModelList, new PropertyModel(HistoryClustersBottomSheetToolbarProperties.ALL_KEYS),
                new PropertyModel(HistoryClustersToolbarProperties.ALL_KEYS),
                mBottomSheetController, mBottomSheetContent, mHistoryActivityIntentFactory);
        mVisit1 = new ClusterVisit(1.0F, mGurl1, "Title 1");
        mVisit2 = new ClusterVisit(1.0F, mGurl2, "Title 1");
        mVisit3 = new ClusterVisit(1.0F, mGurl3, "Title 1");
        mCluster1 = new HistoryCluster(Arrays.asList("foo"), Arrays.asList(mVisit1, mVisit2));
        mCluster2 = new HistoryCluster(Arrays.asList("bar", "baz"), Arrays.asList(mVisit3));
        mHistoryClustersResult = new HistoryClustersResult(
                Arrays.asList(mCluster1, mCluster2), "query", false, false);
    }

    @Test
    public void testCreateDestroy() {
        mMediator.destroy();
    }

    @Test
    public void testQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.query("query");
        assertEquals(mModelList.size(), 0);

        fulfillPromise(promise, mHistoryClustersResult);

        assertEquals(mModelList.size(), 3);
        ListItem item = mModelList.get(0);
        assertEquals(item.type, ItemType.VISIT);
        PropertyModel model = item.model;
        assertTrue(model.getAllSetProperties().containsAll(Arrays.asList(
                HistoryClustersItemProperties.TITLE, HistoryClustersItemProperties.URL)));
    }

    @Test
    public void testShowBottomSheet() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("foo");

        mMediator.showBottomSheet("foo");
        fulfillPromise(promise, mHistoryClustersResult);
        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, true);

        assertEquals(mModelList.size(), 3);
    }

    @Test
    public void testShowBottomSheet_emptyQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.showBottomSheet("");
        fulfillPromise(promise, mHistoryClustersResult);

        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, true);
        assertEquals(mModelList.size(), 3);
    }

    private <T> void fulfillPromise(Promise<T> promise, T result) {
        promise.fulfill(result);
        ShadowLooper.idleMainLooper();
    }
}
