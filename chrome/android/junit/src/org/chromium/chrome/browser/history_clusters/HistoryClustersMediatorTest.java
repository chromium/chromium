// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;

/** Unit tests for HistoryClustersMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersMediatorTest {
    private static final String ITEM_URL_SPEC = "https://www.wombats.com/";

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
    private Tab mTab;
    @Mock
    private GURL mMockGurl;
    @Mock
    private Function<GURL, Intent> mUrlIntentCreator;

    private ClusterVisit mVisit1;
    private ClusterVisit mVisit2;
    private ClusterVisit mVisit3;
    private HistoryCluster mCluster1;
    private HistoryCluster mCluster2;
    private HistoryClustersResult mHistoryClustersResult;
    private ModelList mModelList;
    private PropertyModel mToolbarModel;
    private Intent mIntent = new Intent();
    private Supplier<Intent> mHistoryActivityIntentFactory = () -> mIntent;
    private Supplier<Tab> mTabSupplier = () -> mTab;
    private HistoryClustersMediator mMediator;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(mContext);
        doReturn(mResources).when(mContext).getResources();
        doReturn(ITEM_URL_SPEC).when(mMockGurl).getSpec();
        doReturn(mIntent).when(mUrlIntentCreator).apply(mMockGurl);
        mModelList = new ModelList();
        mToolbarModel = new PropertyModel(HistoryClustersToolbarProperties.ALL_KEYS);
        mMediator = new HistoryClustersMediator(mBridge, mLargeIconBridge, mContext, mResources,
                mModelList, mToolbarModel, mHistoryActivityIntentFactory, mTabSupplier, false,
                mUrlIntentCreator);
        mVisit1 = new ClusterVisit(1.0F, mGurl1, "Title 1");
        mVisit2 = new ClusterVisit(1.0F, mGurl2, "Title 1");
        mVisit3 = new ClusterVisit(1.0F, mGurl3, "Title 1");
        mCluster1 = new HistoryCluster(
                Arrays.asList("foo"), Arrays.asList(mVisit1, mVisit2), "label1", new ArrayList<>());
        mCluster2 = new HistoryCluster(
                Arrays.asList("bar", "baz"), Arrays.asList(mVisit3), "label2", new ArrayList<>());
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
        assertTrue(model.getAllSetProperties().containsAll(
                Arrays.asList(HistoryClustersItemProperties.CLICK_HANDLER,
                        HistoryClustersItemProperties.TITLE, HistoryClustersItemProperties.URL)));
    }

    @Test
    public void testOpenInFullPageTablet() {
        doReturn(2).when(mResources).getInteger(R.integer.min_screen_width_bucket);
        mMediator.openHistoryClustersUi("pandas");
        verify(mTab).loadUrl(argThat(hasSameUrl("chrome://history/journeys?q=pandas")));
    }

    @Test
    public void testOpenInFullPagePhone() {
        doReturn(1).when(mResources).getInteger(R.integer.min_screen_width_bucket);
        mMediator.openHistoryClustersUi("pandas");

        verify(mContext).startActivity(mIntent);
        assertTrue(IntentUtils.safeGetBooleanExtra(
                mIntent, HistoryClustersConstants.EXTRA_SHOW_HISTORY_CLUSTERS, false));
        assertEquals(IntentUtils.safeGetStringExtra(
                             mIntent, HistoryClustersConstants.EXTRA_HISTORY_CLUSTERS_QUERY),
                "pandas");
    }

    @Test
    public void testSearchTextChanged() {
        doReturn(new Promise<>()).when(mBridge).queryClusters("pan");
        // Add a dummy entry to mModelList so we can check it was cleared.
        mModelList.add(new ListItem(42, new PropertyModel()));
        mMediator.onSearchTextChanged("pan");

        assertEquals(mModelList.size(), 0);
        verify(mBridge).queryClusters("pan");

        doReturn(new Promise<>()).when(mBridge).queryClusters("");
        mModelList.add(new ListItem(42, new PropertyModel()));
        mMediator.onEndSearch();

        assertEquals(mModelList.size(), 0);
        verify(mBridge).queryClusters("");
    }

    @Test
    public void testStartSearch() {
        mMediator.startSearch("pandas");
        assertEquals(mToolbarModel.get(HistoryClustersToolbarProperties.QUERY_STATE).getQuery(),
                "pandas");
    }

    @Test
    public void testNavigate() {
        mMediator.navigateToItemUrl(mMockGurl);

        verify(mTab).loadUrl(argThat(hasSameUrl(ITEM_URL_SPEC)));
    }

    @Test
    public void testNavigateSeparateActivity() {
        HistoryClustersMediator standaloneMediator = new HistoryClustersMediator(mBridge,
                mLargeIconBridge, mContext, mResources, mModelList, mToolbarModel,
                mHistoryActivityIntentFactory, mTabSupplier, true, mUrlIntentCreator);
        standaloneMediator.navigateToItemUrl(mMockGurl);

        verify(mUrlIntentCreator).apply(mMockGurl);
        verify(mContext).startActivity(mIntent);
    }

    private <T> void fulfillPromise(Promise<T> promise, T result) {
        promise.fulfill(result);
        ShadowLooper.idleMainLooper();
    }

    private ArgumentMatcher<LoadUrlParams> hasSameUrl(String url) {
        return argument -> argument.getUrl().equals(url);
    }
}
