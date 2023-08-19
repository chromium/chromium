// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import static android.content.Context.ACCESSIBILITY_SERVICE;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.geq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Handler;
import android.text.SpannableString;
import android.text.style.StyleSpan;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.history_clusters.HistoryCluster.MatchPosition;
import org.chromium.chrome.browser.history_clusters.HistoryClusterView.ClusterViewAccessibilityState;
import org.chromium.chrome.browser.history_clusters.HistoryClustersItemProperties.ItemType;
import org.chromium.chrome.browser.history_clusters.HistoryClustersMetricsLogger.VisitAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.components.browser_ui.widget.MoreProgressButton.State;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.ShadowGURL;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for HistoryClustersMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowAppCompatResources.class, ShadowGURL.class})
@SuppressWarnings("DoNotMock") // Mocks GURL
public class HistoryClustersMediatorTest {
    private static final String ITEM_URL_SPEC = "https://www.wombats.com/";
    private static final String INCOGNITO_EXTRA = "history_clusters.incognito";
    private static final String NEW_TAB_EXTRA = "history_clusters.new_tab";
    private static final String TAB_GROUP_EXTRA = "history_clusters.tab_group";
    private static final String ADDTIONAL_URLS_EXTRA = "history_clusters.addtional_urls";

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
    private GURL mGurl4;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTab2;
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
    @Mock
    private TabCreator mTabCreator;
    @Mock
    private TabCreator mIncognitoTabCreator;
    @Mock
    private HistoryClustersMetricsLogger mMetricsLogger;
    @Mock
    private AccessibilityManager mAccessibilityManager;
    @Mock
    private Configuration mConfiguration;
    @Mock
    private Callback<String> mAnnounceCallback;

    private ClusterVisit mVisit1;
    private ClusterVisit mVisit2;
    private ClusterVisit mVisit3;
    private ClusterVisit mVisit4;
    private ClusterVisit mVisit5;
    private ClusterVisit mVisit6;
    private HistoryCluster mCluster1;
    private HistoryCluster mCluster2;
    private HistoryCluster mCluster3;
    private HistoryCluster mClusterSingle;
    private HistoryClustersResult mHistoryClustersResultWithQuery;
    private HistoryClustersResult mHistoryClustersFollowupResultWithQuery;
    private HistoryClustersResult mHistoryClustersResultEmptyQuery;
    private ModelList mModelList;
    private PropertyModel mToolbarModel;
    private Intent mIntent = new Intent();
    private HistoryClustersMediator mMediator;
    private boolean mIsSeparateActivity;
    private HistoryClustersDelegate mHistoryClustersDelegate;
    private SelectionDelegate<ClusterVisit> mSelectionDelegate = new SelectionDelegate<>();
    private final ObservableSupplierImpl<Boolean> mShouldShowPrivacyDisclaimerSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean> mShouldShowClearBrowsingDataSupplier =
            new ObservableSupplierImpl<>();
    private List<ClusterVisit> mVisitsForRemoval = new ArrayList<>();
    private Handler mHandler;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(mContext);
        doReturn(mAccessibilityManager).when(mContext).getSystemService(ACCESSIBILITY_SERVICE);
        doReturn(mResources).when(mContext).getResources();
        doReturn(ITEM_URL_SPEC).when(mMockGurl).getSpec();
        doReturn(mLayoutManager).when(mRecyclerView).getLayoutManager();
        mConfiguration.keyboard = Configuration.KEYBOARD_NOKEYS;
        doReturn(mConfiguration).when(mResources).getConfiguration();
        mModelList = new ModelList();
        mToolbarModel = new PropertyModel(HistoryClustersToolbarProperties.ALL_KEYS);
        mHandler = new Handler();

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
            public <SerializableList extends List<String> & Serializable> Intent getOpenUrlIntent(
                    GURL gurl, boolean inIncognito, boolean createNewTab, boolean inTabGroup,
                    @Nullable SerializableList additionalUrls) {
                mIntent = new Intent();
                mIntent.setData(Uri.parse(gurl.getSpec()));
                mIntent.putExtra(INCOGNITO_EXTRA, inIncognito);
                mIntent.putExtra(NEW_TAB_EXTRA, createNewTab);
                mIntent.putExtra(TAB_GROUP_EXTRA, inTabGroup);
                mIntent.putExtra(ADDTIONAL_URLS_EXTRA, additionalUrls);
                return mIntent;
            }

            @Override
            public ViewGroup getToggleView(ViewGroup parent) {
                return null;
            }

            @Override
            public TabCreator getTabCreator(boolean isIncognito) {
                return isIncognito ? mIncognitoTabCreator : mTabCreator;
            }

            @Nullable
            @Override
            public ViewGroup getPrivacyDisclaimerView(ViewGroup parent) {
                return null;
            }

            @Nullable
            @Override
            public ObservableSupplier<Boolean> shouldShowPrivacyDisclaimerSupplier() {
                return mShouldShowPrivacyDisclaimerSupplier;
            }

            @Override
            public void toggleInfoHeaderVisibility() {}

            @Nullable
            @Override
            public ViewGroup getClearBrowsingDataView(ViewGroup parent) {
                return null;
            }

            @Nullable
            @Override
            public ObservableSupplier<Boolean> shouldShowClearBrowsingDataSupplier() {
                return mShouldShowClearBrowsingDataSupplier;
            }

            @Override
            public void markVisitForRemoval(ClusterVisit clusterVisit) {
                mVisitsForRemoval.add(clusterVisit);
            }
        };

        mShouldShowPrivacyDisclaimerSupplier.set(true);
        mShouldShowClearBrowsingDataSupplier.set(true);
        doReturn("http://spec1.com").when(mGurl1).getSpec();
        doReturn("http://spec2.com").when(mGurl2).getSpec();
        doReturn("http://spec3.com").when(mGurl3).getSpec();
        doReturn("http://spec3.com").when(mGurl4).getSpec();

        mMediator = new HistoryClustersMediator(mBridge, mLargeIconBridge, mContext, mResources,
                mModelList, mToolbarModel, mHistoryClustersDelegate, mClock, mTemplateUrlService,
                mSelectionDelegate, mMetricsLogger, mAnnounceCallback, mHandler);
        mVisit1 = new ClusterVisit(1.0F, mGurl1, "Title 1", "url1.com/", new ArrayList<>(),
                new ArrayList<>(), mGurl1, 123L, new ArrayList<>());
        mVisit2 = new ClusterVisit(1.0F, mGurl2, "Title 2", "url2.com/", new ArrayList<>(),
                new ArrayList<>(), mGurl2, 123L, new ArrayList<>());
        mVisit3 = new ClusterVisit(1.0F, mGurl3, "Title 3", "url3.com/", new ArrayList<>(),
                new ArrayList<>(), mGurl3, 123L, new ArrayList<>());
        mVisit4 = new ClusterVisit(1.0F, mGurl3, "Title 4", "url3.com/foo", new ArrayList<>(),
                new ArrayList<>(), mGurl3, 123L, new ArrayList<>());
        mVisit5 = new ClusterVisit(1.0F, mGurl3, "Title 5", "url5.com/", new ArrayList<>(),
                new ArrayList<>(), mGurl4, 123L, new ArrayList<>());
        mVisit6 = new ClusterVisit(1.0F, mGurl4, "Title 6", "url6.com/", new ArrayList<>(),
                new ArrayList<>(), mGurl4, 123L, new ArrayList<>());
        mCluster1 = new HistoryCluster(Arrays.asList(mVisit1, mVisit2), "\"label1\"", "label1",
                new ArrayList<>(), 456L, Arrays.asList("search 1", "search 2"));
        mCluster2 = new HistoryCluster(Arrays.asList(mVisit3, mVisit4), "hostname.com",
                "hostname.com", new ArrayList<>(), 123L, Collections.emptyList());
        mCluster3 = new HistoryCluster(Arrays.asList(mVisit5, mVisit6), "\"label3\"", "label3",
                new ArrayList<>(), 789L, Collections.EMPTY_LIST);
        mClusterSingle = new HistoryCluster(Arrays.asList(mVisit1), "\"label1\"", "label1",
                new ArrayList<>(), 789L, Collections.EMPTY_LIST);
        mHistoryClustersResultWithQuery =
                new HistoryClustersResult(Arrays.asList(mCluster1, mCluster2),
                        new LinkedHashMap<>(ImmutableMap.of("label", 1)), "query", true, false);
        mHistoryClustersFollowupResultWithQuery = new HistoryClustersResult(
                Arrays.asList(mCluster3),
                new LinkedHashMap<>(ImmutableMap.of("label", 1, "hostname.com", 1, "label3", 1)),
                "query", false, true);
        mHistoryClustersResultEmptyQuery =
                new HistoryClustersResult(Arrays.asList(mCluster1, mCluster2),
                        new LinkedHashMap<>(ImmutableMap.of("label", 1, "hostname.com", 1)), "",
                        false, false);
    }

    @After
    public void tearDown() {
        AccessibilityState.setIsTouchExplorationEnabledForTesting(false);
        AccessibilityState.setIsPerformGesturesEnabledForTesting(false);
    }

    @Test
    public void testCreateDestroy() {
        mMediator.destroy();
    }

    @Test
    public void testQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        assertEquals(1, mModelList.size());
        ListItem spinnerItem = mModelList.get(0);
        assertEquals(spinnerItem.type, ItemType.MORE_PROGRESS);
        assertEquals(spinnerItem.model.get(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE),
                State.LOADING);

        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        assertEquals(7, mModelList.size());
        ListItem clusterItem = mModelList.get(0);
        assertEquals(clusterItem.type, ItemType.CLUSTER);
        PropertyModel clusterModel = clusterItem.model;
        assertTrue(clusterModel.getAllSetProperties().containsAll(ImmutableList.of(
                HistoryClustersItemProperties.ACCESSIBILITY_STATE,
                HistoryClustersItemProperties.CLICK_HANDLER, HistoryClustersItemProperties.LABEL,
                HistoryClustersItemProperties.END_BUTTON_DRAWABLE)));
        assertEquals(ClusterViewAccessibilityState.COLLAPSIBLE,
                clusterModel.get(HistoryClustersItemProperties.ACCESSIBILITY_STATE));
        assertEquals(shadowOf(clusterModel.get(HistoryClustersItemProperties.END_BUTTON_DRAWABLE))
                             .getCreatedFromResId(),
                R.drawable.ic_expand_less_black_24dp);
        verify(mMetricsLogger).incrementQueryCount();

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
    public void testScrollToLoadDisabled() {
        mConfiguration.keyboard = Configuration.KEYBOARD_12KEY;
        AccessibilityState.setIsTouchExplorationEnabledForTesting(true);
        AccessibilityState.setIsPerformGesturesEnabledForTesting(true);
        mMediator = new HistoryClustersMediator(mBridge, mLargeIconBridge, mContext, mResources,
                mModelList, mToolbarModel, mHistoryClustersDelegate, mClock, mTemplateUrlService,
                mSelectionDelegate, mMetricsLogger, mAnnounceCallback, mHandler);

        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");
        Promise<HistoryClustersResult> secondPromise = new Promise();
        doReturn(secondPromise).when(mBridge).loadMoreClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));

        assertEquals(1, mModelList.size());
        ListItem spinnerItem = mModelList.get(0);
        assertEquals(spinnerItem.type, ItemType.MORE_PROGRESS);
        assertEquals(spinnerItem.model.get(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE),
                State.LOADING);

        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        spinnerItem = mModelList.get(mModelList.size() - 1);
        assertEquals(spinnerItem.type, ItemType.MORE_PROGRESS);
        assertEquals(spinnerItem.model.get(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE),
                State.BUTTON);

        mMediator.onScrolled(mRecyclerView, 1, 1);
        verify(mBridge, Mockito.never()).loadMoreClusters("query");

        spinnerItem.model.get(HistoryClustersItemProperties.CLICK_HANDLER).onClick(null);
        ShadowLooper.idleMainLooper();

        verify(mBridge).loadMoreClusters("query");
        spinnerItem = mModelList.get(mModelList.size() - 1);
        assertEquals(spinnerItem.type, ItemType.MORE_PROGRESS);
        assertEquals(spinnerItem.model.get(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE),
                State.LOADING);

        fulfillPromise(secondPromise, mHistoryClustersFollowupResultWithQuery);
        // There should no longer be a spinner or "load more" button once all possible results for
        // the current query have been loaded.
        for (int i = 0; i < mModelList.size(); i++) {
            ListItem item = mModelList.get(i);
            assertNotEquals(item.type, ItemType.MORE_PROGRESS);
        }
    }

    @Test
    public void testEmptyQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.setQueryState(QueryState.forQueryless());
        fulfillPromise(promise, mHistoryClustersResultEmptyQuery);

        // Two clusters + the header views (privacy disclaimer, clear browsing data, toggle).
        assertEquals(mModelList.size(), mHistoryClustersResultEmptyQuery.getClusters().size() + 3);
        assertThat(mModelList,
                hasExactItemTypes(ItemType.PRIVACY_DISCLAIMER, ItemType.CLEAR_BROWSING_DATA,
                        ItemType.TOGGLE, ItemType.CLUSTER, ItemType.CLUSTER));

        ListItem item = mModelList.get(3);
        PropertyModel model = item.model;
        assertTrue(model.getAllSetProperties().containsAll(
                Arrays.asList(HistoryClustersItemProperties.CLICK_HANDLER,
                        HistoryClustersItemProperties.TITLE, HistoryClustersItemProperties.LABEL)));
        assertFalse(mToolbarModel.get(HistoryClustersToolbarProperties.QUERY_STATE).isSearching());

        promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.setQueryState(QueryState.forQuery("", ""));
        fulfillPromise(promise, mHistoryClustersResultEmptyQuery);

        // The contents of the model list should be the same for an empty query in queryfull state
        // vs the queryless state, except that the queryfull state shouldn't have headers.
        assertEquals(mModelList.size(), mHistoryClustersResultEmptyQuery.getClusters().size());
        assertThat(mModelList, hasExactItemTypes(ItemType.CLUSTER, ItemType.CLUSTER));

        assertTrue(mModelList.get(0).model.getAllSetProperties().containsAll(
                Arrays.asList(HistoryClustersItemProperties.CLICK_HANDLER,
                        HistoryClustersItemProperties.TITLE, HistoryClustersItemProperties.LABEL)));
        assertTrue(mToolbarModel.get(HistoryClustersToolbarProperties.QUERY_STATE).isSearching());
    }

    @Test
    public void testHeaderVisibility() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.setQueryState(QueryState.forQueryless());
        fulfillPromise(promise, HistoryClustersResult.emptyResult());

        assertThat(mModelList,
                hasExactItemTypes(ItemType.PRIVACY_DISCLAIMER, ItemType.CLEAR_BROWSING_DATA,
                        ItemType.TOGGLE, ItemType.EMPTY_TEXT));

        mShouldShowPrivacyDisclaimerSupplier.set(false);
        assertThat(mModelList,
                hasExactItemTypes(
                        ItemType.CLEAR_BROWSING_DATA, ItemType.TOGGLE, ItemType.EMPTY_TEXT));

        mShouldShowClearBrowsingDataSupplier.set(false);
        assertThat(mModelList, hasExactItemTypes(ItemType.TOGGLE, ItemType.EMPTY_TEXT));

        mShouldShowClearBrowsingDataSupplier.set(true);
        assertThat(mModelList,
                hasExactItemTypes(
                        ItemType.CLEAR_BROWSING_DATA, ItemType.TOGGLE, ItemType.EMPTY_TEXT));

        mShouldShowPrivacyDisclaimerSupplier.set(true);
        assertThat(mModelList,
                hasExactItemTypes(ItemType.PRIVACY_DISCLAIMER, ItemType.CLEAR_BROWSING_DATA,
                        ItemType.TOGGLE, ItemType.EMPTY_TEXT));

        promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.setQueryState(QueryState.forQueryless());
        mMediator.startQuery("");
        fulfillPromise(promise, mHistoryClustersResultEmptyQuery);

        assertThat(mModelList,
                hasExactItemTypes(ItemType.PRIVACY_DISCLAIMER, ItemType.CLEAR_BROWSING_DATA,
                        ItemType.TOGGLE, ItemType.CLUSTER, ItemType.CLUSTER));
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
        // Add a placeholder entry to mModelList so we can check it was cleared.
        mModelList.add(new ListItem(42, new PropertyModel()));
        mMediator.onSearchTextChanged("p");
        mMediator.onSearchTextChanged("pa");
        mMediator.onSearchTextChanged("pan");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertEquals(mModelList.size(), 1);
        ListItem spinnerItem = mModelList.get(0);
        assertEquals(spinnerItem.type, ItemType.MORE_PROGRESS);
        assertEquals(spinnerItem.model.get(HistoryClustersItemProperties.PROGRESS_BUTTON_STATE),
                State.LOADING);
        verify(mBridge, never()).queryClusters("p");
        verify(mBridge, never()).queryClusters("pa");
        verify(mBridge).queryClusters("pan");

        doReturn(new Promise<>()).when(mBridge).queryClusters("");

        mMediator.onEndSearch();

        verify(mBridge).queryClusters("");
    }

    @Test
    public void testSetQueryState() {
        doReturn(new Promise<>()).when(mBridge).queryClusters("pandas");
        mMediator.setQueryState(QueryState.forQuery("pandas", "empty string"));
        assertEquals(mToolbarModel.get(HistoryClustersToolbarProperties.QUERY_STATE).getQuery(),
                "pandas");
    }

    @Test
    public void testNavigate() {
        mMediator.navigateToUrlInCurrentTab(mMockGurl, false);

        verify(mTab).loadUrl(argThat(hasSameUrl(ITEM_URL_SPEC)));
    }

    @Test
    public void testNavigateSeparateActivity() {
        mIsSeparateActivity = true;
        mMediator.navigateToUrlInCurrentTab(mMockGurl, false);
        verify(mContext).startActivity(mIntent);
    }

    @Test
    public void testItemClicked() {
        mIsSeparateActivity = true;
        mMediator.onClusterVisitClicked(null, mVisit1);

        verify(mContext).startActivity(mIntent);
        assertEquals(mIntent.getDataString(), mVisit1.getNormalizedUrl().getSpec());
        verify(mMetricsLogger)
                .recordVisitAction(HistoryClustersMetricsLogger.VisitAction.CLICKED, mVisit1);
    }

    @Test
    public void testRelatedSearchesClick() {
        doReturn(true).when(mTemplateUrlService).isLoaded();
        doReturn(JUnitTestGURLs.GOOGLE_URL_DOGS)
                .when(mTemplateUrlService)
                .getUrlForSearchQuery("dogs");
        mMediator.onRelatedSearchesChipClicked("dogs", 2);
        verify(mMetricsLogger).recordRelatedSearchesClick(2);
        verify(mTab).loadUrl(argThat(hasSameUrl(JUnitTestGURLs.GOOGLE_URL_DOGS)));
    }

    @Test
    public void testToggleClusterVisibility() {
        PropertyModel clusterModel = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel clusterModel2 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel visitModel1 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel visitModel2 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel visitModel3 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        PropertyModel visitModel4 = new PropertyModel(HistoryClustersItemProperties.ALL_KEYS);
        List<ListItem> visitItemsToHide = Arrays.asList(new ListItem(ItemType.VISIT, visitModel1),
                new ListItem(ItemType.VISIT, visitModel2));
        List<ListItem> visitItemsToHide2 = Arrays.asList(new ListItem(ItemType.VISIT, visitModel3),
                new ListItem(ItemType.VISIT, visitModel4));
        ListItem clusterItem1 = new ListItem(ItemType.CLUSTER, clusterModel);
        ListItem clusterItem2 = new ListItem(ItemType.CLUSTER, clusterModel2);
        mModelList.add(clusterItem1);
        mModelList.addAll(visitItemsToHide);
        mModelList.add(clusterItem2);
        mModelList.addAll(visitItemsToHide2);

        mMediator.hideClusterContents(clusterItem1, visitItemsToHide);
        assertEquals(mModelList.indexOf(visitItemsToHide.get(0)), -1);
        assertEquals(mModelList.indexOf(visitItemsToHide.get(1)), -1);
        assertEquals(4, mModelList.size());
        assertEquals(ClusterViewAccessibilityState.EXPANDABLE,
                clusterModel.get(HistoryClustersItemProperties.ACCESSIBILITY_STATE));

        mMediator.hideClusterContents(clusterItem2, visitItemsToHide2);
        assertEquals(mModelList.indexOf(visitItemsToHide2.get(0)), -1);
        assertEquals(mModelList.indexOf(visitItemsToHide2.get(1)), -1);
        assertEquals(2, mModelList.size());
        assertEquals(ClusterViewAccessibilityState.EXPANDABLE,
                clusterModel2.get(HistoryClustersItemProperties.ACCESSIBILITY_STATE));

        mMediator.showClusterContents(clusterItem2, visitItemsToHide2);
        assertEquals(mModelList.indexOf(visitItemsToHide2.get(0)), 2);
        assertEquals(mModelList.indexOf(visitItemsToHide2.get(1)), 3);
        assertEquals(4, mModelList.size());
        assertEquals(ClusterViewAccessibilityState.COLLAPSIBLE,
                clusterModel2.get(HistoryClustersItemProperties.ACCESSIBILITY_STATE));

        mMediator.showClusterContents(clusterItem1, visitItemsToHide);
        assertEquals(mModelList.indexOf(visitItemsToHide.get(0)), 1);
        assertEquals(mModelList.indexOf(visitItemsToHide.get(1)), 2);
        assertEquals(6, mModelList.size());
        assertEquals(ClusterViewAccessibilityState.COLLAPSIBLE,
                clusterModel.get(HistoryClustersItemProperties.ACCESSIBILITY_STATE));
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

        mMediator.setQueryState(QueryState.forQuery("query", ""));
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

    @Test
    public void testDeleteItems() {
        Promise<HistoryClustersResult> promise = new Promise();
        doReturn(promise).when(mBridge).queryClusters("query");
        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);
        int initialSize = mModelList.size();
        doReturn("multiple")
                .when(mResources)
                .getString(eq(R.string.multiple_history_items_deleted), anyInt());
        Mockito.doAnswer(invocation -> "single " + invocation.getArgument(1).toString())
                .when(mResources)
                .getString(eq(R.string.delete_message), anyString());

        mMediator.deleteVisits(Arrays.asList(mVisit1, mVisit2));
        assertThat(mVisitsForRemoval, Matchers.containsInAnyOrder(mVisit1, mVisit2));
        verify(mMetricsLogger).recordVisitAction(VisitAction.DELETED, mVisit1);
        verify(mMetricsLogger).recordVisitAction(VisitAction.DELETED, mVisit2);
        verify(mAnnounceCallback).onResult("multiple");
        // Deleting all of the visits in a cluster should also delete the ModelList entry for the
        // cluster itself.
        assertEquals(initialSize - 4, mModelList.size());

        ListItem clusterItem = mModelList.get(0);
        assertEquals(clusterItem.type, ItemType.CLUSTER);

        ListItem visitItem = mModelList.get(1);
        assertEquals(visitItem.type, ItemType.VISIT);
        PropertyModel visitModel = visitItem.model;
        assertEquals(mMediator.applyBolding(mVisit3.getTitle(), mVisit3.getTitleMatchPositions()),
                visitModel.get(HistoryClustersItemProperties.TITLE));
        assertEquals(
                mMediator.applyBolding(mVisit3.getUrlForDisplay(), mVisit3.getUrlMatchPositions()),
                visitModel.get(HistoryClustersItemProperties.URL));

        mMediator.deleteVisits(Arrays.asList(mVisit3));
        verify(mAnnounceCallback).onResult("single " + mVisit3.getTitle());

        mMediator.deleteVisits(Arrays.asList(mVisit4));
        // Deleting the final visit should result in an entirely empty list.
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testOpenInNewTab() {
        mIsSeparateActivity = true;
        mMediator.openVisitsInNewTabs(Arrays.asList(mVisit1, mVisit2), false, false);
        verify(mContext).startActivity(mIntent);
        assertEquals(true, mIntent.getBooleanExtra(NEW_TAB_EXTRA, false));
        assertEquals(false, mIntent.getBooleanExtra(INCOGNITO_EXTRA, true));
        assertEquals(false, mIntent.getBooleanExtra(TAB_GROUP_EXTRA, true));
        assertEquals(mGurl2.getSpec(),
                ((List<String>) mIntent.getSerializableExtra(ADDTIONAL_URLS_EXTRA)).get(0));

        mMediator.openVisitsInNewTabs(Arrays.asList(mVisit1, mVisit2), true, false);
        assertEquals(true, mIntent.getBooleanExtra(INCOGNITO_EXTRA, true));

        mIsSeparateActivity = false;
        doReturn(mTab2).when(mTabCreator).createNewTab(any(), anyInt(), any());
        mMediator.openVisitsInNewTabs(Arrays.asList(mVisit1, mVisit2), false, false);
        verify(mTabCreator)
                .createNewTab(argThat(hasSameUrl(mGurl1.getSpec())),
                        eq(TabLaunchType.FROM_CHROME_UI), eq(null));
        verify(mTabCreator)
                .createNewTab(argThat(hasSameUrl(mGurl2.getSpec())),
                        eq(TabLaunchType.FROM_CHROME_UI), eq(mTab2));

        doReturn(mTab2).when(mIncognitoTabCreator).createNewTab(any(), anyInt(), any());
        mMediator.openVisitsInNewTabs(Arrays.asList(mVisit1, mVisit2), true, false);
        verify(mIncognitoTabCreator)
                .createNewTab(argThat(hasSameUrl(mGurl1.getSpec())),
                        eq(TabLaunchType.FROM_CHROME_UI), eq(null));
        verify(mIncognitoTabCreator)
                .createNewTab(argThat(hasSameUrl(mGurl2.getSpec())),
                        eq(TabLaunchType.FROM_CHROME_UI), eq(mTab2));
    }

    @Test
    public void testOpenInGroup() {
        mIsSeparateActivity = true;
        mMediator.openVisitsInNewTabs(Arrays.asList(mVisit1, mVisit2), false, true);
        verify(mContext).startActivity(mIntent);
        assertEquals(true, mIntent.getBooleanExtra(NEW_TAB_EXTRA, false));
        assertEquals(false, mIntent.getBooleanExtra(INCOGNITO_EXTRA, true));
        assertEquals(true, mIntent.getBooleanExtra(TAB_GROUP_EXTRA, true));
        assertEquals(mGurl2.getSpec(),
                ((List<String>) mIntent.getSerializableExtra(ADDTIONAL_URLS_EXTRA)).get(0));

        mIsSeparateActivity = false;
        doReturn(mTab2).when(mTabCreator).createNewTab(any(), anyInt(), any());
        mMediator.openVisitsInNewTabs(Arrays.asList(mVisit1, mVisit2), false, true);
        verify(mTabCreator)
                .createNewTab(argThat(hasSameUrl(mGurl1.getSpec())),
                        eq(TabLaunchType.FROM_CHROME_UI), eq(null));
        verify(mTabCreator)
                .createNewTab(argThat(hasSameUrl(mGurl2.getSpec())),
                        eq(TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP), eq(mTab2));
    }

    @Test
    public void testHideAfterDelete() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        mMediator.deleteVisits(Arrays.asList(mVisit1));
        assertEquals(ItemType.CLUSTER, mModelList.get(0).type);
        PropertyModel clusterModel = mModelList.get(0).model;
        clusterModel.get(HistoryClustersItemProperties.CLICK_HANDLER).onClick(null);
    }

    @Test
    public void testClusterStartIconVisibility() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.setQueryState(QueryState.forQueryless());
        fulfillPromise(promise, mHistoryClustersResultEmptyQuery);

        assertEquals(mModelList.size(), mHistoryClustersResultEmptyQuery.getClusters().size() + 3);
        ListItem item = mModelList.get(3);
        PropertyModel clusterModel = item.model;
        assertEquals(View.VISIBLE,
                clusterModel.get(HistoryClustersItemProperties.START_ICON_VISIBILITY));

        promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");
        mMediator.onSearchTextChanged("query");
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        item = mModelList.get(0);
        assertEquals(ItemType.CLUSTER, item.type);
        assertEquals(
                View.GONE, item.model.get(HistoryClustersItemProperties.START_ICON_VISIBILITY));
    }

    @Test
    public void testDividers() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("");

        mMediator.setQueryState(QueryState.forQueryless());
        fulfillPromise(promise, mHistoryClustersResultEmptyQuery);

        assertEquals(ItemType.CLUSTER, mModelList.get(3).type);
        PropertyModel clusterModel = mModelList.get(3).model;
        assertTrue(clusterModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertFalse(clusterModel.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));

        promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        assertEquals(ItemType.CLUSTER, mModelList.get(0).type);
        assertEquals(ItemType.VISIT, mModelList.get(1).type);
        assertEquals(ItemType.VISIT, mModelList.get(2).type);
        assertEquals(ItemType.RELATED_SEARCHES, mModelList.get(3).type);

        clusterModel = mModelList.get(0).model;
        assertFalse(clusterModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        PropertyModel visitModel = mModelList.get(1).model;
        assertFalse(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        visitModel = mModelList.get(2).model;
        assertFalse(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        PropertyModel relatedSearchesModel = mModelList.get(3).model;
        assertTrue(relatedSearchesModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertTrue(relatedSearchesModel.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));

        // Hide the first cluster.
        clusterModel.get(HistoryClustersItemProperties.CLICK_HANDLER).onClick(null);

        assertTrue(clusterModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertEquals(ItemType.CLUSTER, mModelList.get(1).type);
        assertEquals(ItemType.VISIT, mModelList.get(2).type);

        // The last cluster shouldn't have a divider, even if the cluster above it is collapsed.
        clusterModel = mModelList.get(1).model;
        assertFalse(clusterModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        visitModel = mModelList.get(2).model;
        assertFalse(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));

        // Show the first cluster again.
        clusterModel = mModelList.get(0).model;
        clusterModel.get(HistoryClustersItemProperties.CLICK_HANDLER).onClick(null);
        assertFalse(clusterModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertEquals(ItemType.RELATED_SEARCHES, mModelList.get(3).type);

        relatedSearchesModel = mModelList.get(3).model;
        assertTrue(relatedSearchesModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertTrue(relatedSearchesModel.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));
    }

    @Test
    public void testDividers_continuedQuery() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        // The last cluster shouldn't have a divider.
        PropertyModel clusterModel = mModelList.get(5).model;
        assertFalse(clusterModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        PropertyModel visitModel = mModelList.get(6).model;
        assertFalse(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));

        Promise<HistoryClustersResult> secondPromise = new Promise();
        doReturn(secondPromise).when(mBridge).loadMoreClusters("query");
        mMediator.onScrolled(mRecyclerView, 1, 1);
        ShadowLooper.idleMainLooper();
        fulfillPromise(secondPromise, mHistoryClustersFollowupResultWithQuery);

        // The previously last cluster should now have a divider.
        assertTrue(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertTrue(visitModel.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));
    }

    @Test
    public void testDividers_deletedLastItem() {
        Promise promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        Promise<HistoryClustersResult> secondPromise = new Promise();
        doReturn(secondPromise).when(mBridge).loadMoreClusters("query");
        mMediator.onScrolled(mRecyclerView, 1, 1);
        ShadowLooper.idleMainLooper();
        fulfillPromise(secondPromise, mHistoryClustersFollowupResultWithQuery);

        PropertyModel visitModel = mModelList.get(5).model;
        assertFalse(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        visitModel = mModelList.get(6).model;
        assertTrue(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertTrue(visitModel.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));

        mMediator.deleteVisits(
                Arrays.asList(visitModel.get(HistoryClustersItemProperties.CLUSTER_VISIT)));
        visitModel = mModelList.get(5).model;
        assertTrue(visitModel.get(HistoryClustersItemProperties.DIVIDER_VISIBLE));
        assertTrue(visitModel.get(HistoryClustersItemProperties.DIVIDER_IS_THICK));
    }

    @Test
    public void testHideDeleteButtonWhenSelectionToggled() {
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        assertEquals(mModelList.get(1).type, ItemType.VISIT);
        assertEquals(mModelList.get(2).type, ItemType.VISIT);
        assertTrue(mModelList.get(1).model.get(HistoryClustersItemProperties.END_BUTTON_VISIBLE));
        assertTrue(mModelList.get(2).model.get(HistoryClustersItemProperties.END_BUTTON_VISIBLE));

        mSelectionDelegate.toggleSelectionForItem(mVisit1);

        assertEquals(mModelList.get(1).type, ItemType.VISIT);
        assertEquals(mModelList.get(2).type, ItemType.VISIT);
        assertFalse(mModelList.get(1).model.get(HistoryClustersItemProperties.END_BUTTON_VISIBLE));
        assertFalse(mModelList.get(2).model.get(HistoryClustersItemProperties.END_BUTTON_VISIBLE));
    }

    @Test
    public void testSingleVisitCluster() {
        HistoryClustersResult singletonVisitResult = new HistoryClustersResult(
                Arrays.asList(mClusterSingle), new LinkedHashMap<>(), "query", false, false);
        Promise<HistoryClustersResult> promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, singletonVisitResult);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testHistoryDeletedExternally() {
        mMediator.onHistoryDeletedExternally();

        Promise promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        verify(mBridge, times(1)).queryClusters("query");
        fulfillPromise(promise, mHistoryClustersResultWithQuery);

        mSelectionDelegate.toggleSelectionForItem(mVisit1);
        mSelectionDelegate.toggleSelectionForItem(mVisit2);

        mMediator.onHistoryDeletedExternally();

        assertTrue(mSelectionDelegate.getSelectedItems().isEmpty());
        assertThat(mModelList, hasExactItemTypes(ItemType.MORE_PROGRESS));
        verify(mBridge, times(2)).queryClusters("query");
    }

    @Test
    public void testContinueQueryAfterDestroy() {
        Promise promise = new Promise<>();
        doReturn(promise).when(mBridge).queryClusters("query");

        doReturn(3).when(mLayoutManager).findLastVisibleItemPosition();

        mMediator.setQueryState(QueryState.forQuery("query", ""));
        fulfillPromise(promise, mHistoryClustersResultWithQuery);
        mMediator.onScrolled(mRecyclerView, 1, 1);
        mMediator.destroy();

        ShadowLooper.idleMainLooper();
    }

    private <T> void fulfillPromise(Promise<T> promise, T result) {
        promise.fulfill(result);
        ShadowLooper.idleMainLooper();
    }

    static ArgumentMatcher<LoadUrlParams> hasSameUrl(String url) {
        return argument -> argument.getUrl().equals(url);
    }

    static Matcher<ModelList> hasExactItemTypes(@ItemType int... itemTypes) {
        return new BaseMatcher<ModelList>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("Has item types: " + Arrays.toString(itemTypes));
            }

            @Override
            public boolean matches(Object object) {
                ModelList modelList = (ModelList) object;
                if (itemTypes.length != modelList.size()) {
                    return false;
                }
                int i = 0;
                for (ListItem listItem : modelList) {
                    if (listItem.type != itemTypes[i++]) {
                        return false;
                    }
                }
                return true;
            }
        };
    }
}
