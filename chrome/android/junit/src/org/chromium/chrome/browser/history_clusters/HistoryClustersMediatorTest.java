// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.geq;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

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
import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history_clusters.HistoryCluster.MatchPosition;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for HistoryClustersMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowAppCompatResources.class})
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
    private HistoryClustersMediator.Clock mClock;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private RecyclerView mRecyclerView;
    @Mock
    private LinearLayoutManager mLayoutManager;

    private ClusterVisit mVisit1;
    private ClusterVisit mVisit2;
    private ClusterVisit mVisit3;
    private ClusterVisit mVisit4;
    private HistoryCluster mCluster1;
    private HistoryCluster mCluster2;
    private HistoryCluster mCluster3;
    private HistoryClustersResult mHistoryClustersResultWithQuery;
    private HistoryClustersResult mHistoryClustersFollowupResultWithQuery;
    private HistoryClustersResult mHistoryClustersResultEmptyQuery;
    private ModelList mModelList;
    private PropertyModel mToolbarModel;
    private Intent mIntent = new Intent();
    private HistoryClustersMediator mMediator;
    private boolean mIsSeparateActivity;
    private HistoryClustersDelegate mHistoryClustersDelegate;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(mContext);
        doReturn(mResources).when(mContext).getResources();
        doReturn(ITEM_URL_SPEC).when(mMockGurl).getSpec();
        doReturn(mLayoutManager).when(mRecyclerView).getLayoutManager();
        mModelList = new ModelList();
        mToolbarModel = new PropertyModel(HistoryClustersToolbarProperties.ALL_KEYS);

        mHistoryClustersDelegate = new HistoryClustersDelegate() {
            @Override
            public boolean isSeparateActivity() {
                return mIsSeparateActivity;
            }

            @Override
            public Tab getTab() {
                return mTab;
            }

            @Override
            public Intent getHistoryActivityIntent() {
                return mIntent;
            }

            @Override
            public Intent getOpenUrlIntent(GURL gurl) {
                return mIntent;
            }

            @Override
            public ViewGroup getToggleView(ViewGroup parent) {
                return null;
            }
        };

        mMediator = new HistoryClustersMediator(mBridge, mLargeIconBridge, mContext, mResources,
                mModelList, mToolbarModel, mHistoryClustersDelegate, mClock, mTemplateUrlService);
        mVisit1 = new ClusterVisit(
                1.0F, mGurl1, "Title 1", "url1.com/", new ArrayList<>(), new ArrayList<>());
        mVisit2 = new ClusterVisit(
                1.0F, mGurl2, "Title 2", "url2.com/", new ArrayList<>(), new ArrayList<>());
        mVisit3 = new ClusterVisit(
                1.0F, mGurl3, "Title 3", "url3.com/", new ArrayList<>(), new ArrayList<>());
        mVisit4 = new ClusterVisit(
                1.0F, mGurl3, "Title 4", "url3.com/foo", new ArrayList<>(), new ArrayList<>());
        mCluster1 = new HistoryCluster(Arrays.asList("foo"), Arrays.asList(mVisit1, mVisit2),
                "label1", new ArrayList<>(), 456L, Arrays.asList("search 1", "search 2"));
        mCluster2 = new HistoryCluster(Arrays.asList("bar", "baz"), Arrays.asList(mVisit3),
                "label2", new ArrayList<>(), 123L, Collections.emptyList());
        mCluster3 = new HistoryCluster(Arrays.asList("key"), Arrays.asList(mVisit4), "label3",
                new ArrayList<>(), 789L, Collections.EMPTY_LIST);
        mHistoryClustersResultWithQuery = new HistoryClustersResult(
                Arrays.asList(mCluster1, mCluster2), "query", true, false);
        mHistoryClustersFollowupResultWithQuery =
                new HistoryClustersResult(Arrays.asList(mCluster3), "query", false, true);
        mHistoryClustersResultEmptyQuery =
                new HistoryClustersResult(Arrays.asList(mCluster1, mCluster2), "", false, false);
    }

    @Test
    public void testCreateDestroy() {
        mMediator.destroy();
    }

    @Test
    public void testQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.startQuery("query");
        assertEquals(mModelList.size(), 0);

        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        assertEquals(mModelList.size(), 6);
        ListItem clusterItem = mModelList.get(0);
        assertEquals(clusterItem.type, ItemType.CLUSTER);
        PropertyModel clusterModel = clusterItem.model;
        assertTrue(clusterModel.getAllSetProperties().containsAll(Arrays.asList(
                HistoryClustersItemProperties.CLICK_HANDLER, HistoryClustersItemProperties.LABEL,
                HistoryClustersItemProperties.END_BUTTON_DRAWABLE)));
        assertEquals(shadowOf(clusterModel.get(HistoryClustersItemProperties.END_BUTTON_DRAWABLE))
                             .getCreatedFromResId(),
                R.drawable.ic_expand_more_black_24dp);

        ListItem visitItem = mModelList.get(1);
        assertEquals(visitItem.type, ItemType.VISIT);
        PropertyModel visitModel = visitItem.model;
        assertTrue(visitModel.getAllSetProperties().containsAll(
                Arrays.asList(HistoryClustersItemProperties.CLICK_HANDLER,
                        HistoryClustersItemProperties.TITLE, HistoryClustersItemProperties.URL)));

        ListItem relatedSearchesItem = mModelList.get(3);
        assertEquals(relatedSearchesItem.type, ItemType.RELATED_SEARCHES);
        PropertyModel relatedSearchesModel = relatedSearchesItem.model;
        assertTrue(relatedSearchesModel.getAllSetProperties().containsAll(
                Arrays.asList(HistoryClustersItemProperties.CHIP_CLICK_HANDLER,
                        HistoryClustersItemProperties.RELATED_SEARCHES)));
    }
    @Test
    public void testEmptyQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.startQuery("");
        fulfillPromise(promise, mHistoryClustersResultEmptyQuery);

        // Two clusters + the toggle view
        assertEquals(mModelList.size(), mHistoryClustersResultEmptyQuery.getClusters().size() + 1);
        ListItem item = mModelList.get(0);
        assertEquals(item.type, ItemType.TOGGLE);

        item = mModelList.get(1);
        assertEquals(item.type, ItemType.CLUSTER);
        PropertyModel model = item.model;
        assertTrue(model.getAllSetProperties().containsAll(Arrays.asList(
                HistoryClustersItemProperties.CLICK_HANDLER, HistoryClustersItemProperties.TITLE)));
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
    public void testSetQueryState() {
        mMediator.setQueryState(QueryState.forQuery("pandas"));
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
        mIsSeparateActivity = true;
        mMediator.navigateToItemUrl(mMockGurl);
        verify(mContext).startActivity(mIntent);
    }

    @Test
    public void testToggleClusterVisibility() {
        PropertyModel clusterModel = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel visitModel1 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel visitModel2 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        List<ListItem> visitItems = Arrays.asList(new ListItem(ItemType.VISIT, visitModel1),
                new ListItem(ItemType.VISIT, visitModel2));

        mMediator.hideCluster(mCluster1, clusterModel, visitItems);

        assertEquals(visitModel1.get(HistoryClustersItemProperties.VISIBILITY), View.GONE);
        assertEquals(visitModel2.get(HistoryClustersItemProperties.VISIBILITY), View.GONE);

        mMediator.showCluster(mCluster1, clusterModel, visitItems);

        assertEquals(visitModel1.get(HistoryClustersItemProperties.VISIBILITY), View.VISIBLE);
        assertEquals(visitModel2.get(HistoryClustersItemProperties.VISIBILITY), View.VISIBLE);
    }

    @Test
    public void testGetTimeString() {
        String dayString = "1 day ago";
        String hourString = "1 hour ago";
        String minuteString = "1 minute ago";
        String justNowString = "Just now";

        doReturn(dayString)
                .when(mResources)
                .getQuantityString(eq(R.plurals.n_days_ago), geq(1), geq(1));
        doReturn(hourString)
                .when(mResources)
                .getQuantityString(eq(R.plurals.n_hours_ago), geq(1), geq(1));
        doReturn(minuteString)
                .when(mResources)
                .getQuantityString(eq(R.plurals.n_minutes_ago), geq(1), geq(1));
        doReturn(justNowString).when(mResources).getString(R.string.just_now);

        doReturn(TimeUnit.DAYS.toMillis(1)).when(mClock).currentTimeMillis();
        String timeString = mMediator.getTimeString(0L);
        assertEquals(timeString, dayString);

        doReturn(TimeUnit.HOURS.toMillis(1)).when(mClock).currentTimeMillis();
        timeString = mMediator.getTimeString(0L);
        assertEquals(timeString, hourString);

        doReturn(TimeUnit.MINUTES.toMillis(1)).when(mClock).currentTimeMillis();
        timeString = mMediator.getTimeString(0L);
        assertEquals(timeString, minuteString);

        doReturn(TimeUnit.SECONDS.toMillis(1)).when(mClock).currentTimeMillis();
        timeString = mMediator.getTimeString(0L);
        assertEquals(timeString, justNowString);
    }

    @Test
    public void testScrolling() {
        Promise<HistoryClustersResult> promise = new Promise();
        doReturn(promise).when(mBridge).queryClusters("query");
        Promise<HistoryClustersResult> secondPromise = new Promise();
        doReturn(secondPromise).when(mBridge).loadMoreClusters("query");
        doReturn(3).when(mLayoutManager).findLastVisibleItemPosition();

        mMediator.startQuery("query");
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        mMediator.onScrolled(mRecyclerView, 1, 1);
        ShadowLooper.idleMainLooper();

        verify(mBridge).loadMoreClusters("query");
        fulfillPromise(secondPromise, mHistoryClustersFollowupResultWithQuery);

        doReturn(4).when(mLayoutManager).findLastVisibleItemPosition();
        mMediator.onScrolled(mRecyclerView, 1, 1);
        // There should have been no further calls to loadMoreClusters since the current result
        // specifies that it has no more data.
        verify(mBridge).loadMoreClusters("query");
    }

    @Test
    public void testBolding() {
        String text = "this is fun";
        List<MatchPosition> matchPositions =
                Arrays.asList(new MatchPosition(0, 5), new MatchPosition(8, 10));
        SpannableString result = mMediator.applyBolding(text, matchPositions);

        StyleSpan[] styleSpans = result.getSpans(0, 5, StyleSpan.class);
        assertEquals(styleSpans.length, 1);
        assertEquals(styleSpans[0].getStyle(), Typeface.BOLD);

        styleSpans = result.getSpans(8, 10, StyleSpan.class);
        assertEquals(styleSpans.length, 1);
        assertEquals(styleSpans[0].getStyle(), Typeface.BOLD);
    }

    private <T> void fulfillPromise(Promise<T> promise, T result) {
        promise.fulfill(result);
        ShadowLooper.idleMainLooper();
    }

    private ArgumentMatcher<LoadUrlParams> hasSameUrl(String url) {
        return argument -> argument.getUrl().equals(url);
    }
}
