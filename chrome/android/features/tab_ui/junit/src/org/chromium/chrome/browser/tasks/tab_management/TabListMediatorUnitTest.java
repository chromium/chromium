// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_CLICK;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.contains;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.GRID_TAB_SWITCHER_FOR_TABLETS;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.START_SURFACE_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.FOR_TESTING;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.PRICE_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.TAB_SUGGESTION;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.OTHERS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.util.Pair;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import com.google.protobuf.ByteString;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.PersistedTabDataConfiguration;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ThumbnailFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.commerce.PriceTracking.BuyableProduct;
import org.chromium.components.commerce.PriceTracking.PriceTrackingData;
import org.chromium.components.commerce.PriceTracking.ProductPrice;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
/**
 * Tests for {@link TabListMediator}.
 */
@SuppressWarnings(
        {"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored", "ConstantConditions"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        instrumentedPackages =
                {
                        "androidx.recyclerview.widget.RecyclerView" // required to mock final
                })
@LooperMode(LooperMode.Mode.LEGACY)
// clang-format off
@Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
@Features.DisableFeatures({
    TAB_GROUPS_ANDROID, TAB_GROUPS_CONTINUATION_ANDROID, START_SURFACE_ANDROID})
public class TabListMediatorUnitTest {
    // clang-format on
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final String NEW_TITLE = "New title";
    private static final String CUSTOMIZED_DIALOG_TITLE1 = "Cool Tabs";
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final GURL TAB1_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
    private static final GURL TAB2_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
    private static final GURL TAB3_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_3);
    private static final GURL TAB4_URL = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);
    private static final String NEW_URL = JUnitTestGURLs.EXAMPLE_URL;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 290;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    private static final BuyableProduct BUYABLE_PRODUCT_PROTO_INITIAL =
            BuyableProduct.newBuilder()
                    .setCurrentPrice(createProductPrice(123456789012345L, "USD"))
                    .build();
    private static ProductPrice createProductPrice(long amountMicros, String currencyCode) {
        return ProductPrice.newBuilder()
                .setCurrencyCode(currencyCode)
                .setAmountMicros(amountMicros)
                .build();
    }
    private static final PriceTrackingData PRICE_TRACKING_BUYABLE_PRODUCT_INITIAL =
            PriceTrackingData.newBuilder().setBuyableProduct(BUYABLE_PRODUCT_PROTO_INITIAL).build();
    private static final Any ANY_BUYABLE_PRODUCT_INITIAL =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(
                            PRICE_TRACKING_BUYABLE_PRODUCT_INITIAL.toByteArray()))
                    .build();
    private static final Any ANY_EMPTY = Any.newBuilder().build();

    @IntDef({TabListMediatorType.TAB_SWITCHER, TabListMediatorType.TAB_STRIP,
            TabListMediatorType.TAB_GRID_DIALOG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMediatorType {
        int TAB_SWITCHER = 0;
        int TAB_STRIP = 1;
        int TAB_GRID_DIALOG = 2;
        int NUM_ENTRIES = 3;
    }

    @Mock
    TabContentManager mTabContentManager;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModel mTabModel;
    @Mock
    TabListFaviconProvider mTabListFaviconProvider;
    @Mock
    RecyclerView mRecyclerView;
    @Mock
    TabListRecyclerView mTabListRecyclerView;
    @Mock
    RecyclerView.Adapter mAdapter;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    EmptyTabModelFilter mEmptyTabModelFilter;
    @Mock
    TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock
    TabListMediator.GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    @Mock
    TabFavicon mFavicon;
    @Mock
    Bitmap mFaviconBitmap;
    @Mock
    Activity mActivity;
    @Mock
    TabListMediator.TabActionListener mOpenGroupActionListener;
    @Mock
    GridLayoutManager mGridLayoutManager;
    @Mock
    GridLayoutManager.SpanSizeLookup mSpanSizeLookup;
    @Mock
    Profile mProfile;
    @Mock
    Tracker mTracker;
    @Mock
    PseudoTab.TitleProvider mTitleProvider;
    @Mock
    UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock
    OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;
    @Mock
    TabListMediator.TabGridAccessibilityHelper mTabGridAccessibilityHelper;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    TabSwitcherMediator.PriceWelcomeMessageController mPriceWelcomeMessageController;
    @Mock
    ShoppingPersistedTabData mShoppingPersistedTabData;
    @Mock
    SelectionDelegate<Integer> mSelectionDelegate;

    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    ArgumentCaptor<Callback<TabFavicon>> mCallbackCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverCaptor;
    @Captor
    ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;
    @Captor
    ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver> mTemplateUrlServiceObserver;
    @Captor
    ArgumentCaptor<RecyclerView.OnScrollListener> mOnScrollListenerCaptor;
    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;
    @Mock
    private Resources mResources;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabListMediator mMediator;
    private TabListModel mModel;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder1;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder2;
    private RecyclerView.ViewHolder mDummyViewHolder1;
    private RecyclerView.ViewHolder mDummyViewHolder2;
    private View mItemView1 = mock(View.class);
    private View mItemView2 = mock(View.class);
    private TabModelObserver mMediatorTabModelObserver;
    private TabGroupModelFilter.Observer mMediatorTabGroupModelFilterObserver;
    private PriceDrop mPriceDrop;
    private PriceTabData mPriceTabData;
    private String mTab1Domain;
    private String mTab2Domain;
    private String mTab3Domain;
    private String mNewDomain;
    private GURL mFaviconUrl;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        // Ensure native pointer is initialized
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();

        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(1);

        mTab1Domain = TAB1_URL.getHost().replace("www.", "");
        mTab2Domain = TAB2_URL.getHost().replace("www.", "");
        mTab3Domain = TAB3_URL.getHost().replace("www.", "");
        mNewDomain = JUnitTestGURLs.getGURL(NEW_URL).getHost().replace("www.", "");
        mFaviconUrl = JUnitTestGURLs.getGURL(JUnitTestGURLs.RED_1);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE, TAB1_URL);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE, TAB2_URL);
        mViewHolder1 = prepareViewHolder(TAB1_ID, POSITION1);
        mViewHolder2 = prepareViewHolder(TAB2_ID, POSITION2);
        mDummyViewHolder1 = prepareDummyViewHolder(mItemView1, POSITION1);
        mDummyViewHolder2 = prepareDummyViewHolder(mItemView2, POSITION2);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(), any(), any(), anyBoolean(), anyBoolean());
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(mTab1).when(mTabModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(POSITION2);
        doReturn(tabs1).when(mTabModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForUrlAsync(anyObject(), anyBoolean(), mCallbackCaptor.capture());
        doReturn(mFavicon)
                .when(mTabListFaviconProvider)
                .getFaviconFromBitmap(any(Bitmap.class), any(GURL.class));
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(mOpenGroupActionListener)
                .when(mGridCardOnClickListenerProvider)
                .openTabGridDialog(any(Tab.class));
        doNothing().when(mActivity).registerComponentCallbacks(mComponentCallbacksCaptor.capture());
        doReturn(mGridLayoutManager).when(mRecyclerView).getLayoutManager();
        doReturn(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT)
                .when(mGridLayoutManager)
                .getSpanCount();
        doReturn(mSpanSizeLookup).when(mGridLayoutManager).getSpanSizeLookup();
        doReturn(mTab1Domain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(TAB1_URL.getSpec()), anyBoolean());
        doReturn(mTab2Domain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(TAB2_URL.getSpec()), anyBoolean());
        doReturn(mTab3Domain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(TAB3_URL.getSpec()), anyBoolean());
        doNothing().when(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        doReturn(true).when(mTabListFaviconProvider).isInitialized();

        mModel = new TabListModel();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, false, null,
                mGridCardOnClickListenerProvider, null, null, getClass().getSimpleName(),
                UiType.CLOSABLE, null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        TrackerFactory.setTrackerForTests(mTracker);

        // TabModelObserver is registered when native is ready.
        assertThat(mTabModelObserverCaptor.getAllValues().isEmpty(), equalTo(true));
        mMediator.initWithNative(mProfile);
        assertThat(mTabModelObserverCaptor.getAllValues().isEmpty(), equalTo(false));

        doAnswer(invocation -> {
            int position = invocation.getArgument(0);
            int itemType = mModel.get(position).type;
            if (itemType == UiType.MESSAGE || itemType == UiType.LARGE_MESSAGE) {
                return mGridLayoutManager.getSpanCount();
            }
            return 1;
        })
                .when(mSpanSizeLookup)
                .getSpanSize(anyInt());
    }

    @After
    public void tearDown() {
        PseudoTab.clearForTesting();
        TabAttributeCache.clearAllForTesting();
        getGroupTitleSharedPreferences().edit().clear();
    }

    private static SharedPreferences getGroupTitleSharedPreferences() {
        return ContextUtils.getApplicationContext().getSharedPreferences(
                TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void updatesTitle_WithoutStoredTitle() {
        initAndAssertAllProperties();

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        when(mTitleProvider.getTitle(mActivity, PseudoTab.fromTab(mTab1))).thenReturn(NEW_TITLE);
        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(NEW_TITLE));
    }

    @Test
    @Features.EnableFeatures(TAB_GROUPS_CONTINUATION_ANDROID)
    public void updatesTitle_WithStoredTitle_TabGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Mock that tab1 and new tab are in the same group with root ID as TAB1_ID.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        getGroupTitleSharedPreferences()
                .edit()
                .putString(String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()),
                        CUSTOMIZED_DIALOG_TITLE1)
                .apply();
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesFavicon_SingleTab_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_SingleTab_NonGTS() {
        initAndAssertAllProperties();

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_TabGroup_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
        // Assert that tab1 is in a group.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(Arrays.asList(mTab1, newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updateFavicon_StaleIndex() {
        initAndAssertAllProperties();
        mModel.get(0).model.set(TabProperties.FAVICON, null);
        mModel.get(1).model.set(TabProperties.FAVICON, null);

        mMediator.updateFaviconForTab(PseudoTab.fromTab(mTab2), null, null);
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        // Before executing callback, there is a deletion in tab list model which makes the index
        // stale.
        mModel.removeAt(0);
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(0));

        // Start to execute callback.
        mCallbackCaptor.getValue().onResult(mFavicon);

        assertThat(mModel.get(0).model.get(TabProperties.FAVICON), equalTo(mFavicon));
    }

    @Test
    public void updatesFavicon_Navigation_NoOpSameDocument() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(navigationHandle.isSameDocument()).thenReturn(true);

        mTabObserverCaptor.getValue().onDidStartNavigationInPrimaryMainFrame(
                mTab1, navigationHandle);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_Navigation_NoOpSameUrl() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB1_URL);
        when(navigationHandle.isSameDocument()).thenReturn(false);

        mTabObserverCaptor.getValue().onDidStartNavigationInPrimaryMainFrame(
                mTab1, navigationHandle);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_Navigation_NoOpNtpUrl() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(navigationHandle.isSameDocument()).thenReturn(false);

        TabImpl newTab =
                prepareTab(TAB3_ID, TAB3_TITLE, JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL));
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));

        mModel.get(2).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(2).model.get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onDidStartNavigationInPrimaryMainFrame(
                newTab, navigationHandle);
        assertNull(mModel.get(2).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_Navigation() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);

        when(navigationHandle.isSameDocument()).thenReturn(false);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor.getValue().onDidStartNavigationInPrimaryMainFrame(
                mTab1, navigationHandle);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void sendsSelectSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .model.get(TabProperties.TAB_SELECTED_LISTENER)
                .run(mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mGridCardOnClickListenerProvider)
                .onTabSelecting(mModel.get(1).model.get(TabProperties.TAB_ID), true);
    }

    @Test
    public void sendsCloseSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .model.get(TabProperties.TAB_CLOSED_LISTENER)
                .run(mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mTabModel).closeTab(eq(mTab2), eq(null), eq(false), eq(false), eq(true));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithoutGroup() {
        initAndAssertAllProperties();
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabGroupModelFilter).moveRelatedTabs(eq(TAB1_ID), eq(2));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithinGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        getItemTouchHelperCallback().onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    public void sendsMergeTabSignalCorrectly() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    // Regression test for https://crbug.com/1372487
    @Test
    public void handlesGroupMergeCorrectly_InOrder() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.getTabAt(3)).thenReturn(tab4);
        SimpleRecyclerViewAdapter.ViewHolder viewHolder3 = prepareViewHolder(TAB3_ID, POSITION1);
        SimpleRecyclerViewAdapter.ViewHolder viewHolder4 = prepareViewHolder(TAB4_ID, POSITION2);
        View itemView3 = mock(View.class);
        View itemView4 = mock(View.class);
        RecyclerView.ViewHolder dummyViewHolder3 = prepareDummyViewHolder(itemView3, 2);
        RecyclerView.ViewHolder dummyViewHolder4 = prepareDummyViewHolder(itemView4, 3);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3, tab4));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(4));

        // Merge 2 to 1.
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID))
                .thenReturn(Arrays.asList(mTab1, mTab2));
        when(mTabModel.indexOf(mTab1)).thenReturn(POSITION1);
        when(mTabModel.indexOf(mTab2)).thenReturn(POSITION2);
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB1_ID);

        assertThat(mModel.size(), equalTo(3));
        mDummyViewHolder1 = prepareDummyViewHolder(mItemView1, 0);
        dummyViewHolder3 = prepareDummyViewHolder(itemView3, 1);
        dummyViewHolder4 = prepareDummyViewHolder(itemView4, 2);

        // Merge 4 to 3.
        when(mTabGroupModelFilter.getTabAt(1)).thenReturn(tab3);
        when(mTabGroupModelFilter.getTabAt(2)).thenReturn(tab4);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, dummyViewHolder3);

        itemTouchHelperCallback.onSelectedChanged(
                dummyViewHolder4, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB4_ID), eq(TAB3_ID));
        verify(mGridLayoutManager).removeView(itemView4);
        verify(mTracker, times(2)).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(Arrays.asList(tab3, tab4));
        when(mTabModel.indexOf(tab3)).thenReturn(2);
        when(mTabModel.indexOf(tab4)).thenReturn(3);
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(tab4, TAB3_ID);

        assertThat(mModel.size(), equalTo(2));
        mDummyViewHolder1 = prepareDummyViewHolder(mItemView1, 0);
        dummyViewHolder3 = prepareDummyViewHolder(itemView3, 1);

        // Merge 3 to 1.
        when(mTabGroupModelFilter.getTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getTabAt(1)).thenReturn(tab3);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(0);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(1);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        itemTouchHelperCallback.onSelectedChanged(
                dummyViewHolder3, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB3_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(itemView3);
        verify(mTracker, times(3)).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID))
                .thenReturn(Arrays.asList(mTab1, mTab2, tab3, tab4));
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(tab3, TAB1_ID);

        assertThat(mModel.size(), equalTo(1));
    }

    @Test
    public void neverSendsMergeTabSignal_Without_Group() {
        initAndAssertAllProperties();

        mMediator.setActionOnAllRelatedTabsForTesting(true);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager, never()).removeView(any(View.class));
    }

    @Test
    public void sendsUngroupSignalCorrectly() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(false);
        itemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(1).when(mAdapter).getItemCount();

        // Simulate the ungroup action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).moveTabOutOfGroup(eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void tabClosure() {
        initAndAssertAllProperties();

        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabClosure_IgnoresUpdatesForTabsOutsideOfModel() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().willCloseTab(
                prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL), false, true);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_RestoreNotComplete() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND, false);

        // When tab restoring stage is not yet finished, this tab info should not be added to
        // property model.
        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Restore() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        // Mock that tab restoring stage is over.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        TabListMediator.TabActionListener actionListenerBeforeUpdate =
                mModel.get(1).model.get(TabProperties.TAB_SELECTED_LISTENER);

        // Mock that newTab was in the same group with tab, and now it is restored.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = Arrays.asList(mTab2, newTab);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(1).when(mTabModelFilter).indexOf(newTab);
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(eq(TAB2_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND, false);

        TabListMediator.TabActionListener actionListenerAfterUpdate =
                mModel.get(1).model.get(TabProperties.TAB_SELECTED_LISTENER);
        // The selection listener should be updated which indicates that corresponding property
        // model is updated.
        assertThat(actionListenerBeforeUpdate, not(actionListenerAfterUpdate));
        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Restore_SyncingTabListModelWithTabModel() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        // Mock that tab restoring stage is over.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        // Mock that tab1 and tab2 are in the same group, and they are being restored. The
        // TabListModel has been cleaned out before the restoring happens. This case could happen
        // within a incognito tab group when user switches between light/dark mode.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        mModel.clear();

        mMediatorTabModelObserver.didAddTab(
                mTab2, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND, false);
        assertThat(mModel.size(), equalTo(0));

        mMediatorTabModelObserver.didAddTab(
                mTab1, TabLaunchType.FROM_RESTORE, TabCreationState.LIVE_IN_FOREGROUND, false);
        assertThat(mModel.size(), equalTo(1));
    }

    @Test
    public void tabAddition_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_TAB_SWITCHER_UI,
                TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_GTS_delayAdd() {
        mMediator.setComponentNameForTesting(TabSwitcherCoordinator.COMPONENT_NAME);
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(TAB1_ID);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        TabModelObserver tabModelObserver = mTabModelObserverCaptor.getValue();
        // Add tab marked as delayed
        tabModelObserver.didAddTab(newTab, TabLaunchType.FROM_TAB_SWITCHER_UI,
                TabCreationState.LIVE_IN_FOREGROUND, true);

        // Verify tab did not get added and delayed tab is captured.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mMediator.getTabToAddDelayedForTesting(), equalTo(newTab));

        // Select delayed tab
        tabModelObserver.didSelectTab(newTab, TabSelectionType.FROM_USER, mTab1.getId());
        // Assert old tab is still marked as selected
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Hide GTS to complete tab addition and selection
        mMediator.postHiding();
        // Assert tab added and selected. Assert old tab is de-selected.
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNull(mMediator.getTabToAddDelayedForTesting());
    }

    @Test
    public void tabAddition_GTS_Skip() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        // Add a new tab to the group with mTab2.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab2, newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_TAB_SWITCHER_UI,
                TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_GTS_Middle() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(newTab).when(mTabModelFilter).getTabAt(1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_End() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Middle() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, newTab, mTab2))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Skip() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        // newTab is of another group.
        doReturn(Arrays.asList(mTab1, mTab2)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    @Features.DisableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void tabAddition_Redundant() {
        initAndAssertAllProperties();
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        assertThat(mModel.size(), equalTo(2));
        doReturn(Arrays.asList(mTab1, mTab2)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));

        // Try to do a redundant addition by adding the PropertyModel of an existing tab to the
        // TabListModel.
        mTabModelObserverCaptor.getValue().didAddTab(
                mTab1, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND, false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabSelection() {
        initAndAssertAllProperties();

        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), equalTo(tab1Fetcher));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(tab2Fetcher, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabSelection_updatePreviousSelectedTabThumbnailFetcher() {
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, true, null,
                mGridCardOnClickListenerProvider, null, null, getClass().getSimpleName(),
                UiType.CLOSABLE, null);
        mMediator.initWithNative(mProfile);
        // mTabModelObserverCaptor captures on every initWithNative calls. There is one
        // initWithNative call in the setup already.
        verify(mTabModelFilterProvider, times(2))
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        initAndAssertAllProperties();

        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(tab2Fetcher, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabClosureUndone() {
        initAndAssertAllProperties();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB1_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(newTab);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabMergeIntoGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);

        // Assume that reset in TabGroupModelFilter is finished.
        doReturn(new ArrayList<>(Arrays.asList(mTab1, mTab2)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(POSITION2));
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
        assertNotNull(mModel.get(1).model.get(TabProperties.FAVICON));

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab1, TAB2_ID);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void tabMoveOutOfGroup_GTS_Moved_Tab_Selected() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(false).when(mTabGroupModelFilter).hasOtherRelatedTabs(mTab1);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabMoveOutOfGroup_GTS_Origin_Tab_Selected() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    public void tabMoveOutOfGroup_GTS_LastTab() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Ungroup the single tab.
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void tabMoveOutOfGroup_GTS_TabAdditionWithSameId() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        // The ungroup will add tab1 to the TabListModel at index 0. Note that before this addition,
        // there is the PropertyModel represents the group with the same id at the same index. The
        // addition should still take effect in this case.
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    public void testShoppingFetcherActiveForForUngroupedTabs() {
        prepareForPriceDrop();
        resetWithRegularTabs(false);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
    }

    @Test
    public void testShoppingFetcherInactiveForForGroupedTabs() {
        prepareForPriceDrop();
        resetWithRegularTabs(true);

        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
    }

    @Test
    public void testShoppingFetcherGroupedThenUngrouped() {
        prepareForPriceDrop();
        resetWithRegularTabs(true);

        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        resetWithRegularTabs(false);
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
    }

    @Test
    public void testShoppingFetcherUngroupedThenGrouped() {
        prepareForPriceDrop();
        resetWithRegularTabs(false);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        resetWithRegularTabs(true);
        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
    }

    /**
     * Set flags and initialize for verifying price drop behavior
     */
    private void prepareForPriceDrop() {
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PersistedTabDataConfiguration.setUseTestConfig(true);
        initAndAssertAllProperties();
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
    }

    @Test
    public void testCouponFetcherActiveForForUngroupedTabs() {
        prepareForCoupon();
        resetWithRegularTabs(false);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.CouponPersistedTabDataFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.CouponPersistedTabDataFetcher.class));
    }

    @Test
    public void testCouponFetcherInactiveForForGroupedTabs() {
        prepareForCoupon();
        resetWithRegularTabs(true);

        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER));
    }

    @Test
    public void testCouponFetcherGroupedThenUngrouped() {
        prepareForCoupon();
        resetWithRegularTabs(true);
        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER));
        resetWithRegularTabs(false);
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.CouponPersistedTabDataFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.CouponPersistedTabDataFetcher.class));
    }

    @Test
    public void testCouponFetcherUngroupedThenGrouped() {
        prepareForCoupon();
        resetWithRegularTabs(false);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.CouponPersistedTabDataFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.CouponPersistedTabDataFetcher.class));
        resetWithRegularTabs(true);
        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.COUPON_PERSISTED_TAB_DATA_FETCHER));
    }

    /**
     * Set flags and initialize for verifying coupon annotations behavior
     */
    private void prepareForCoupon() {
        ChromeFeatureList.sCommerceCoupons.setForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PersistedTabDataConfiguration.setUseTestConfig(true);
        initAndAssertAllProperties();
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
    }

    /**
     * Reset mediator with non-incognito tabs which are optionally grouped
     * @param isGrouped true if the tabs should be grouped
     */
    private void resetWithRegularTabs(boolean isGrouped) {
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        if (isGrouped) {
            doReturn(Arrays.asList(mTab1, mTab2))
                    .when(mTabModelFilter)
                    .getRelatedTabList(eq(TAB1_ID));
            doReturn(Arrays.asList(mTab1, mTab2))
                    .when(mTabModelFilter)
                    .getRelatedTabList(eq(TAB2_ID));
        } else {
            doReturn(Arrays.asList(mTab1)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));
            doReturn(Arrays.asList(mTab2)).when(mTabModelFilter).getRelatedTabList(eq(TAB2_ID));
        }
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(false).when(mTab1).isIncognito();
        doReturn(false).when(mTab2).isIncognito();
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
    }

    @Test
    public void tabMoveOutOfGroup_Dialog() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler).updateDialogContent(TAB2_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Dialog_LastTab() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Ungroup the single tab.
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        verify(mTabGridDialogHandler).updateDialogContent(Tab.INVALID_TAB_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Strip() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_STRIP, TabListMode.GRID);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler, never()).updateDialogContent(anyInt());
    }

    @Test
    public void tabMovementWithoutGroup_Forward() {
        initAndAssertAllProperties();

        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithoutGroup_Backward() {
        initAndAssertAllProperties();

        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithGroup_Forward() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithGroup_Backward() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabGridDialong_Forward() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        doReturn(TAB1_ID).when(criticalPersistedTabData1).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabGridDialog_Backward() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        doReturn(TAB1_ID).when(criticalPersistedTabData1).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_Forward() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs with groups (mTab1, mTab2) and tab3.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        List<Integer> groupTabIds = new ArrayList<>(Arrays.asList(TAB1_ID, TAB2_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        CriticalPersistedTabData criticalPersistedTabData3 = CriticalPersistedTabData.from(tab3);
        doReturn(TAB1_ID).when(criticalPersistedTabData1).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData2).getRootId();
        doReturn(TAB3_ID).when(criticalPersistedTabData3).getRootId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(2));

        // Select tab3 so the group doesn't have the selected tab.
        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor.getValue().didSelectTab(
                tab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Assume that moveTab in TabModel is finished (swap mTab1 and mTab2).
        group = new ArrayList<>(Arrays.asList(mTab2, mTab1));
        groupTabIds = new ArrayList<>(Arrays.asList(TAB2_ID, TAB1_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        // mTab1 is first in group before the move.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        // mTab1 is still first in group after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        // TODO(crbug/1374361): Make this an assertion and don't update.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab2, tab3. Update is precautionary.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_Backward() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs with groups (mTab1, mTab2) and tab3.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        List<Integer> groupTabIds = new ArrayList<>(Arrays.asList(TAB1_ID, TAB2_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        CriticalPersistedTabData criticalPersistedTabData3 = CriticalPersistedTabData.from(tab3);
        doReturn(TAB1_ID).when(criticalPersistedTabData1).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData2).getRootId();
        doReturn(TAB3_ID).when(criticalPersistedTabData3).getRootId();

        // Select tab3 so the group doesn't have the selected tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(2));

        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor.getValue().didSelectTab(
                tab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Assume that moveTab in TabModel is finished (swap mTab1 and mTab2).
        group = new ArrayList<>(Arrays.asList(mTab2, mTab1));
        groupTabIds = new ArrayList<>(Arrays.asList(TAB2_ID, TAB1_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        // mTab1 is first in group before the move.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        // mTab1 is first in group after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        // TODO(crbug/1374361): Make this an assertion and don't update.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab2, tab3. Update is precautionary.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_SelectedNotMoved() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs grouped together.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        List<Integer> groupTabIds = new ArrayList<>(Arrays.asList(TAB1_ID, TAB2_ID, TAB3_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB3_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        CriticalPersistedTabData criticalPersistedTabData3 = CriticalPersistedTabData.from(tab3);
        doReturn(TAB1_ID).when(criticalPersistedTabData1).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData2).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData3).getRootId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(1));

        // Assume that moveTab in TabModel is finished.

        // mTab1 selected.
        doReturn(POSITION1).when(mTabModel).index();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        // Swap mTab2 and tab3.
        doReturn(mTab2).when(mTabModel).getTabAt(2);
        doReturn(tab3).when(mTabModel).getTabAt(POSITION2);
        group = new ArrayList<>(Arrays.asList(mTab1, tab3, mTab2));
        groupTabIds = new ArrayList<>(Arrays.asList(TAB1_ID, TAB3_ID, TAB2_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB3_ID);

        // mTab1 selected before update.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab2, POSITION2, 2);

        // mTab1 still selected after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        // TODO(crbug/1374361): Make this an assertion.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab3, tab2.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_SelectedMoved() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs grouped together.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        List<Integer> groupTabIds = new ArrayList<>(Arrays.asList(TAB1_ID, TAB2_ID, TAB3_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB3_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        CriticalPersistedTabData criticalPersistedTabData3 = CriticalPersistedTabData.from(tab3);
        doReturn(TAB1_ID).when(criticalPersistedTabData1).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData2).getRootId();
        doReturn(TAB1_ID).when(criticalPersistedTabData3).getRootId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(1));

        // Assume that moveTab in TabModel is finished.

        // mTab1 selected (at new position).
        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab1, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        // Swap mTab1 and mTab3.
        doReturn(mTab1).when(mTabModel).getTabAt(2);
        doReturn(tab3).when(mTabModel).getTabAt(POSITION1);
        group = new ArrayList<>(Arrays.asList(tab3, mTab2, mTab1));
        groupTabIds = new ArrayList<>(Arrays.asList(TAB3_ID, TAB2_ID, TAB1_ID));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB1_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB2_ID);
        doReturn(groupTabIds).when(mTabGroupModelFilter).getRelatedTabIds(TAB3_ID);

        // mTab1 selected before update.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab1, 2, POSITION1);

        // mTab1 still selected after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        // TODO(crbug/1374361): Make this an assertion.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab3, tab2.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void undoGrouped_One_Adjacent_Tab() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume there are 3 tabs in TabModel, mTab2 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping mTab2 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoForwardGrouped_One_Tab() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume there are 3 tabs in TabModel, tab3 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping tab3 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(false).when(mTabGroupModelFilter).hasOtherRelatedTabs(tab3);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(tab3, POSITION1);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoBackwardGrouped_One_Tab() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume there are 3 tabs in TabModel, mTab1 just grouped with mTab2;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping mTab1 from mTab2.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(false).when(mTabGroupModelFilter).hasOtherRelatedTabs(mTab1);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoForwardGrouped_BetweenGroups() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume there are 3 tabs in TabModel, tab3, tab4, just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(1));

        // Assume undo grouping tab3 with mTab1.
        doReturn(2).when(mTabGroupModelFilter).getCount();

        // Undo tab 3.
        List<Tab> relatedTabs = Arrays.asList(tab3);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(tab4).when(mTabModel).getTabAt(2);
        doReturn(tab3).when(mTabModel).getTabAt(3);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(tab4);
        doReturn(0).when(mTabModel).indexOf(mTab1);
        doReturn(1).when(mTabModel).indexOf(mTab2);
        doReturn(2).when(mTabModel).indexOf(tab4);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(3).when(mTabModel).indexOf(tab3);
        doReturn(false).when(mTabGroupModelFilter).hasOtherRelatedTabs(tab3);
        doReturn(true).when(mTabGroupModelFilter).hasOtherRelatedTabs(tab4);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(tab3, POSITION1);
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(-1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB4_ID), equalTo(-1));

        // Undo tab 4
        relatedTabs = Arrays.asList(tab3, tab4);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(tab4);
        doReturn(2).when(mTabModel).indexOf(tab3);
        doReturn(3).when(mTabModel).indexOf(tab4);
        doReturn(true).when(mTabGroupModelFilter).hasOtherRelatedTabs(tab3);
        doReturn(true).when(mTabGroupModelFilter).hasOtherRelatedTabs(tab4);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(tab4).when(mTabModel).getTabAt(3);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(tab4, POSITION1);
        assertThat(mModel.size(), equalTo(2));

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(tab4, TAB4_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(-1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB4_ID), equalTo(-1));
    }

    @Test
    public void updateSpanCount_Portrait_SingleWindow() {
        initAndAssertAllProperties();
        // Mock that we are switching to portrait mode.
        Configuration configuration = new Configuration();
        configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
        configuration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP - 1;
        // Mock that we are in single window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(configuration);

        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT);
    }

    @Test
    public void updateSpanCount_Landscape_SingleWindow() {
        initAndAssertAllProperties();
        // Mock that we are switching to landscape mode.
        Configuration configuration = new Configuration();
        configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        configuration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP - 1;
        // Mock that we are in single window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(configuration);

        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM);
    }

    @Test
    public void updateSpanCount_MultiWindow() {
        initAndAssertAllProperties();
        Configuration portraitConfiguration = new Configuration();
        portraitConfiguration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP - 1;
        portraitConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        Configuration landscapeConfiguration = new Configuration();
        landscapeConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        landscapeConfiguration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP - 1;
        // Mock that we are in multi window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(landscapeConfiguration);
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(portraitConfiguration);
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(landscapeConfiguration);

        // The span count is fixed to 2 for multi window mode regardless of the orientation change.
        verify(mGridLayoutManager, times(3))
                .setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT);
    }

    @Test
    @Features.EnableFeatures(GRID_TAB_SWITCHER_FOR_TABLETS)
    public void updateSpanCount_onTablet_multipleScreenWidths() {
        initAndAssertAllProperties(3);
        // Mock tablet
        when(mResources.getInteger(R.integer.min_screen_width_bucket))
                .thenReturn(TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP + 1);
        Configuration portraitConfiguration = new Configuration();
        portraitConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;

        // Mock that we are in single window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);

        // Compact width
        portraitConfiguration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP - 1;
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(portraitConfiguration);
        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT);

        // Medium width
        portraitConfiguration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP - 1;
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(portraitConfiguration);
        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM);

        // Large width
        portraitConfiguration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP + 1;
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(portraitConfiguration);
        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_LARGE);
    }

    @Test
    public void resetWithListOfTabs_MruOrder() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        assertThat(tabs.size(), equalTo(2));

        long timestamp1 = 1;
        long timestamp2 = 2;
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        doReturn(timestamp1).when(criticalPersistedTabData1).getTimestampMillis();
        doReturn(timestamp2).when(criticalPersistedTabData2).getTimestampMillis();
        mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/true);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mMediator.indexOfTab(TAB1_ID), equalTo(1));
        assertThat(mMediator.indexOfTab(TAB2_ID), equalTo(0));

        doReturn(timestamp2).when(criticalPersistedTabData1).getTimestampMillis();
        doReturn(timestamp1).when(criticalPersistedTabData2).getTimestampMillis();
        mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/true);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mMediator.indexOfTab(TAB1_ID), equalTo(0));
        assertThat(mMediator.indexOfTab(TAB2_ID), equalTo(1));
    }

    @Test
    public void groupAndUngroupSelectedTabsInMRUOrder() {
        StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER.setForTesting(true);
        // A new mediator with mode equals TabListMode.CAROUSEL will be created.
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.CAROUSEL);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        assertThat(tabs.size(), equalTo(2));
        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        tabs.add(tab3);
        int position3 = 2;
        doReturn(tab3).when(mTabModel).getTabAt(position3);
        doReturn(position3).when(mTabModel).indexOf(tab3);
        doReturn(3).when(mTabModel).getCount();

        long timestamp1 = 1;
        long timestamp2 = 2;
        long timestamp3 = 3;
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        CriticalPersistedTabData criticalPersistedTabData3 = CriticalPersistedTabData.from(tab3);
        doReturn(timestamp1).when(criticalPersistedTabData1).getTimestampMillis();
        doReturn(timestamp2).when(criticalPersistedTabData2).getTimestampMillis();
        doReturn(timestamp3).when(criticalPersistedTabData3).getTimestampMillis();

        mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/true);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mMediator.indexOfTab(TAB1_ID), equalTo(2));
        assertThat(mMediator.indexOfTab(TAB2_ID), equalTo(1));
        assertThat(mMediator.indexOfTab(TAB3_ID), equalTo(0));

        // Group Tab3 to Tab1, and create a new group with id equals to TAB1_ID. Tab3 is the current
        // Tab. Assume that TabGroupModelFilter is already updated.
        doReturn(tab3).when(mTabModelSelector).getCurrentTab();
        doReturn(TAB3_ID).when(mTabModelSelector).getCurrentTabId();
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group, TAB1_ID);

        // In TabModel, it becomes {Tab1, Tab3}, Tab2 from Tab1, Tab2, Tab3.
        doReturn(POSITION2).when(mTabModel).indexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(tab3).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(position3);
        Pair<Integer, Integer> positions = mModel.getIndexesForMergeToGroup(mTabModel, group);
        // On the mModel (MRU order), moving Tab3 to Tab1 is to move scrIndex 0 to desIndex 2.
        assertEquals(2, positions.first.intValue()); // desIndex
        assertEquals(0, positions.second.intValue()); // scrIndex

        // The TabGroupModelFilter.getTabAt() returns the last selected Tab for each group, which
        // could be only has a single Tab.
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(tab3, TAB3_ID);
        assertEquals(2, mModel.size());
        // Verifies that the new group {Tab1, Tab3} is moved to the location of Tab3 and updated
        // with Tab3's id.
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Move Tab3 out of the group. In TabModel, it becomes: Tab1, Tab3, Tab2 from {Tab1, Tab3},
        // Tab2.
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(position3);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(tab3, POSITION1);
        assertEquals(mModel.size(), 3);
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER.setForTesting(false);
    }

    @Test
    public void mergeNoneSelectedToGroupBackwardsAndUngroup() {
        StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER.setForTesting(true);
        // A new mediator with mode equals TabListMode.CAROUSEL will be created.
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.CAROUSEL);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        assertThat(tabs.size(), equalTo(2));
        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        int tab4Id = 0;
        TabImpl tab4 = prepareTab(tab4Id, "tab 4", TAB2_URL);
        tabs.add(tab3);
        tabs.add(tab4);

        int position3 = 2;
        int position4 = 3;
        doReturn(tab3).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab4).when(mTabModel).getTabAt(position4);
        doReturn(position3).when(mTabModel).indexOf(tab3);
        doReturn(position4).when(mTabModel).indexOf(tab4);
        doReturn(4).when(mTabModel).getCount();

        long timestamp1 = 1;
        long timestamp2 = 2;
        long timestamp3 = 3;
        long timestamp4 = 4;
        CriticalPersistedTabData criticalPersistedTabData1 = CriticalPersistedTabData.from(mTab1);
        CriticalPersistedTabData criticalPersistedTabData2 = CriticalPersistedTabData.from(mTab2);
        CriticalPersistedTabData criticalPersistedTabData3 = CriticalPersistedTabData.from(tab3);
        CriticalPersistedTabData criticalPersistedTabData4 = CriticalPersistedTabData.from(tab4);
        doReturn(timestamp1).when(criticalPersistedTabData1).getTimestampMillis();
        doReturn(timestamp2).when(criticalPersistedTabData2).getTimestampMillis();
        doReturn(timestamp3).when(criticalPersistedTabData3).getTimestampMillis();
        doReturn(timestamp4).when(criticalPersistedTabData4).getTimestampMillis();

        mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/true);

        assertThat(mModel.size(), equalTo(4));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(tab4Id));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(3).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mMediator.indexOfTab(TAB1_ID), equalTo(3));
        assertThat(mMediator.indexOfTab(TAB2_ID), equalTo(2));
        assertThat(mMediator.indexOfTab(TAB3_ID), equalTo(1));
        assertThat(mMediator.indexOfTab(tab4Id), equalTo(0));

        // Group Tab1 to Tab3, and create a new group {Tab3, Tab1} with id equals to TAB3_ID. Tab4
        // is the current Tab. Assume that TabGroupModelFilter is already updated.
        doReturn(tab4).when(mTabModelSelector).getCurrentTab();
        doReturn(tab4Id).when(mTabModelSelector).getCurrentTabId();
        List<Tab> group = new ArrayList<>(Arrays.asList(tab3, mTab1));
        createTabGroup(group, TAB3_ID);

        // In TabModel, it becomes Tab2, {Tab3, Tab1}, Tab4 from Tab1, Tab2, Tab3, Tab4.
        doReturn(POSITION1).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION2).when(mTabModel).indexOf(tab3);
        doReturn(position3).when(mTabModel).indexOf(mTab1);
        doReturn(position4).when(mTabModel).indexOf(tab4);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(tab3).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab1).when(mTabModel).getTabAt(position3);
        doReturn(tab4).when(mTabModel).getTabAt(position4);

        // The TabGroupModelFilter.getTabAt() returns the last selected Tab for each group, which
        // could be only has a single Tab.
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(position3).when(mTabGroupModelFilter).indexOf(tab4);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab4).when(mTabGroupModelFilter).getTabAt(position3);

        Pair<Integer, Integer> positions = mModel.getIndexesForMergeToGroup(mTabModel, group);
        // On the mModel (MRU order), moving Tab1 to Tab3 is to move srcIndex 3 to desIndex 1.
        assertEquals(1, positions.first.intValue()); // desIndex
        assertEquals(3, positions.second.intValue()); // srcIndex

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab1, TAB3_ID);
        assertEquals(3, mModel.size());
        // Verifies that the new group {Tab3, Tab1} is moved to the location of Tab3 and updated
        // with Tab3's id.
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(tab4Id));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Move Tab3 out of the group. In TabModel, it becomes: Tab2, Tab1, Tab3, Tab4 from Tab2,
        // {Tab3, Tab1}, Tab4.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(position3);
        doReturn(tab4).when(mTabGroupModelFilter).getTabAt(position4);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(tab3, POSITION2);
        assertEquals(mModel.size(), 4);
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(tab4Id));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(3).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        StartSurfaceConfiguration.SHOW_TABS_IN_MRU_ORDER.setForTesting(false);
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void getLatestTitle_NotGTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        getGroupTitleSharedPreferences()
                .edit()
                .putString(String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()),
                        CUSTOMIZED_DIALOG_TITLE1)
                .apply();
        assertThat(mMediator.getTabGroupTitleEditor().getTabGroupTitle(
                           CriticalPersistedTabData.from(mTab1).getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        // Even if we have a stored title, we only show it in tab switcher.
        assertThat(mMediator.getLatestTitleForTab(PseudoTab.fromTab(mTab1)), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void getLatestTitle_SingleTab_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        getGroupTitleSharedPreferences()
                .edit()
                .putString(String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()),
                        CUSTOMIZED_DIALOG_TITLE1)
                .apply();
        assertThat(mMediator.getTabGroupTitleEditor().getTabGroupTitle(
                           CriticalPersistedTabData.from(mTab1).getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabs, TAB1_ID);

        // We never show stored title for single tab.
        assertThat(mMediator.getLatestTitleForTab(PseudoTab.fromTab(mTab1)), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void getLatestTitle_Stored_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        getGroupTitleSharedPreferences()
                .edit()
                .putString(String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()),
                        CUSTOMIZED_DIALOG_TITLE1)
                .apply();
        assertThat(mMediator.getTabGroupTitleEditor().getTabGroupTitle(
                           CriticalPersistedTabData.from(mTab1).getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        assertThat(mMediator.getLatestTitleForTab(PseudoTab.fromTab(mTab1)),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void updateTabGroupTitle_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand tab group with 2 tabs.";
        assertThat(mModel.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Mock that tab1 and newTab are in the same group and group root id is TAB1_ID.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);

        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);

        assertThat(mModel.get(POSITION1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(mModel.get(POSITION1).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void tabGroupTitleEditor_storeTitle() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        TabGroupTitleEditor tabGroupTitleEditor = mMediator.getTabGroupTitleEditor();

        assertNull(getGroupTitleSharedPreferences().getString(
                String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()), null));

        tabGroupTitleEditor.storeTabGroupTitle(
                CriticalPersistedTabData.from(mTab1).getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertEquals(CUSTOMIZED_DIALOG_TITLE1,
                getGroupTitleSharedPreferences().getString(
                        String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()), null));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void tabGroupTitleEditor_deleteTitle() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        TabGroupTitleEditor tabGroupTitleEditor = mMediator.getTabGroupTitleEditor();

        getGroupTitleSharedPreferences()
                .edit()
                .putString(String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()),
                        CUSTOMIZED_DIALOG_TITLE1)
                .apply();
        assertEquals(CUSTOMIZED_DIALOG_TITLE1,
                getGroupTitleSharedPreferences().getString(
                        String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()), null));

        tabGroupTitleEditor.deleteTabGroupTitle(CriticalPersistedTabData.from(mTab1).getRootId());
        assertNull(getGroupTitleSharedPreferences().getString(
                String.valueOf(CriticalPersistedTabData.from(mTab1).getRootId()), null));
    }

    @Test
    public void addSpecialItem() {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.DIVIDER, model);

        assertTrue(mModel.size() > 0);
        assertEquals(TabProperties.UiType.DIVIDER, mModel.get(0).type);
    }

    @Test
    public void addSpecialItem_notPersistOnReset() {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.DIVIDER, model);
        assertEquals(TabProperties.UiType.DIVIDER, mModel.get(0).type);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/false);
        assertThat(mModel.size(), equalTo(2));
        assertNotEquals(TabProperties.UiType.DIVIDER, mModel.get(0).type);
        assertNotEquals(TabProperties.UiType.DIVIDER, mModel.get(1).type);

        mMediator.addSpecialItemToModel(1, TabProperties.UiType.DIVIDER, model);
        assertThat(mModel.size(), equalTo(3));
        assertEquals(TabProperties.UiType.DIVIDER, mModel.get(1).type);
    }

    @Test
    public void addSpecialItem_withoutTabListModelProperties() {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        try {
            mMediator.addSpecialItemToModel(0, TabProperties.UiType.DIVIDER, new PropertyModel());
        } catch (AssertionError e) {
            return;
        }
        fail("PropertyModel#validateKey() assert should have failed.");
    }

    @Test
    public void removeSpecialItem_Message() {
        PropertyModel model = mock(PropertyModel.class);
        int expectedMessageType = FOR_TESTING;
        int wrongMessageType = TAB_SUGGESTION;
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.MESSAGE, model);
        assertEquals(1, mModel.size());

        mMediator.removeSpecialItemFromModel(TabProperties.UiType.MESSAGE, wrongMessageType);
        assertEquals(1, mModel.size());

        mMediator.removeSpecialItemFromModel(TabProperties.UiType.MESSAGE, expectedMessageType);
        assertEquals(0, mModel.size());
    }

    @Test
    public void removeSpecialItem_Message_PriceMessage() {
        PropertyModel model = mock(PropertyModel.class);
        int expectedMessageType = PRICE_MESSAGE;
        int wrongMessageType = TAB_SUGGESTION;
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.LARGE_MESSAGE, model);
        assertEquals(1, mModel.size());

        mMediator.removeSpecialItemFromModel(TabProperties.UiType.MESSAGE, wrongMessageType);
        assertEquals(1, mModel.size());

        mMediator.removeSpecialItemFromModel(
                TabProperties.UiType.LARGE_MESSAGE, expectedMessageType);
        assertEquals(0, mModel.size());
    }

    @Test
    @Features.DisableFeatures({TAB_GROUPS_ANDROID})
    public void testUrlUpdated_forSingleTab_GTS_GroupNotEnabled() {
        initAndAssertAllProperties();
        assertNotEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(JUnitTestGURLs.getGURL(NEW_URL)).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        // TabProperties.URL_DOMAIN is empty string if TabGroupsAndroidContinuationEnabled is false.
        assertEquals("", mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals("", mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUrlUpdated_forSingleTab_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertNotEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        doReturn(JUnitTestGURLs.getGURL(NEW_URL)).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUrlUpdated_forGroup_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(mTab1Domain + ", " + mTab2Domain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        // Update URL_DOMAIN for mTab1.
        doReturn(JUnitTestGURLs.getGURL(NEW_URL)).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain + ", " + mTab2Domain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        // Update URL_DOMAIN for mTab2.
        doReturn(JUnitTestGURLs.getGURL(NEW_URL)).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(mNewDomain + ", " + mNewDomain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUrlUpdated_forGroup_Dialog() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(mTab1Domain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        // Update URL_DOMAIN for mTab1.
        doReturn(JUnitTestGURLs.getGURL(NEW_URL)).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));

        // Update URL_DOMAIN for mTab2.
        doReturn(JUnitTestGURLs.getGURL(NEW_URL)).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mNewDomain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUrlUpdated_forUnGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(mTab1Domain + ", " + mTab2Domain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        // Assume that TabGroupModelFilter is already updated.
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION1);
        assertEquals(mTab1Domain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testOnInitializeAccessibilityNodeInfo() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Setup related mocks and initialize needed components.
        AccessibilityNodeInfo accessibilityNodeInfo = mock(AccessibilityNodeInfo.class);
        AccessibilityAction action1 = new AccessibilityAction(R.id.move_tab_left, "left");
        AccessibilityAction action2 = new AccessibilityAction(R.id.move_tab_right, "right");
        AccessibilityAction action3 = new AccessibilityAction(R.id.move_tab_up, "up");
        doReturn(new ArrayList<>(Arrays.asList(action1, action2, action3)))
                .when(mTabGridAccessibilityHelper)
                .getPotentialActionsForView(mItemView1);
        InOrder accessibilityNodeInfoInOrder = Mockito.inOrder(accessibilityNodeInfo);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.onInitializeAccessibilityNodeInfo(mItemView1, accessibilityNodeInfo);

        accessibilityNodeInfoInOrder.verify(accessibilityNodeInfo).addAction(eq(action1));
        accessibilityNodeInfoInOrder.verify(accessibilityNodeInfo).addAction(eq(action2));
        accessibilityNodeInfoInOrder.verify(accessibilityNodeInfo).addAction(eq(action3));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testPerformAccessibilityAction() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Setup related mocks and initialize needed components.
        Bundle args = mock(Bundle.class);
        int action = R.id.move_tab_left;
        // Mock that the action indicates that tab2 will move left and thus tab2 and tab1 should
        // switch positions.
        doReturn(new Pair<>(1, 0))
                .when(mTabGridAccessibilityHelper)
                .getPositionsOfReorderAction(mItemView1, action);
        doReturn(true).when(mTabGridAccessibilityHelper).isReorderAction(action);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.performAccessibilityAction(mItemView1, action, args);

        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testPerformAccessibilityAction_defaultAccessibilityAction() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Setup related mocks and initialize needed components.
        Bundle args = mock(Bundle.class);
        int action = ACTION_CLICK;
        // Mock that the action indicates that tab2 will move to position 2 which is invalid.
        doReturn(false).when(mTabGridAccessibilityHelper).isReorderAction(action);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.performAccessibilityAction(mItemView1, action, args);
        verify(mTabGridAccessibilityHelper, never())
                .getPositionsOfReorderAction(mItemView1, action);
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testPerformAccessibilityAction_InvalidIndex() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Setup related mocks and initialize needed components.
        Bundle args = mock(Bundle.class);
        int action = R.id.move_tab_left;
        // Mock that the action indicates that tab2 will move to position 2 which is invalid.
        doReturn(new Pair<>(1, 2))
                .when(mTabGridAccessibilityHelper)
                .getPositionsOfReorderAction(mItemView1, action);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.performAccessibilityAction(mItemView1, action, args);

        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    public void testTabObserverRemovedFromClosedTab() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        assertThat(mModel.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void testTabObserverReattachToUndoClosedTab() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        assertThat(mModel.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false, true);
        assertThat(mModel.size(), equalTo(1));

        // Assume that TabModelFilter is already updated to reflect closed tab is undone.
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(mTab1).when(mTabModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(POSITION2);
        when(mTabModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);
        assertThat(mModel.size(), equalTo(2));
        // First time is when mTab2 initially added to mModel; second time is when mTab2 added back
        // to mModel because of undo action.
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testUnchangeCheckIgnoreNonTabs() {
        initAndAssertAllProperties();
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        boolean showQuickly = mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/false);
        assertThat(showQuickly, equalTo(true));

        // Create a PropertyModel that is not a tab and add it to the existing TabListModel.
        PropertyModel propertyModel = mock(PropertyModel.class);
        when(propertyModel.get(CARD_TYPE)).thenReturn(OTHERS);
        mMediator.addSpecialItemToModel(mModel.size(), TabProperties.UiType.MESSAGE, propertyModel);
        assertThat(mModel.size(), equalTo(tabs.size() + 1));

        // TabListModel unchange check should ignore the non-Tab item.
        showQuickly = mMediator.resetWithListOfTabs(
                PseudoTab.getListOfPseudoTab(tabs), /*quickMode =*/false, /*mruMode =*/false);
        assertThat(showQuickly, equalTo(true));
    }

    // TODO(crbug.com/1177036): the assertThat in fetch callback is never reached.
    @Test
    public void testPriceTrackingProperty() {
        setPriceTrackingEnabledForTesting(true);
        for (boolean signedInAndSyncEnabled : new boolean[] {false, true}) {
            for (boolean priceTrackingEnabled : new boolean[] {false, true}) {
                for (boolean incognito : new boolean[] {false, true}) {
                    TabListMediator mMediatorSpy = spy(mMediator);
                    doReturn(true).when(mMediatorSpy).isUngroupedTab(anyInt());
                    PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(
                            signedInAndSyncEnabled);
                    PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                            PriceTrackingUtilities.TRACK_PRICES_ON_TABS, priceTrackingEnabled);
                    Profile.setLastUsedProfileForTesting(mProfile);
                    Map<GURL, Any> responses = new HashMap<>();
                    responses.put(TAB1_URL, ANY_BUYABLE_PRODUCT_INITIAL);
                    responses.put(TAB2_URL, ANY_EMPTY);
                    mockOptimizationGuideResponse(OptimizationGuideDecision.TRUE, responses);
                    PersistedTabDataConfiguration.setUseTestConfig(true);
                    initAndAssertAllProperties(mMediatorSpy);
                    List<Tab> tabs = new ArrayList<>();
                    doReturn(incognito).when(mTab1).isIncognito();
                    doReturn(incognito).when(mTab2).isIncognito();

                    for (int i = 0; i < 2; i++) {
                        CriticalPersistedTabData criticalPersistedTabData =
                                mock(CriticalPersistedTabData.class);
                        doReturn(System.currentTimeMillis())
                                .when(criticalPersistedTabData)
                                .getTimestampMillis();
                        mTabModel.getTabAt(i).getUserDataHost().setUserData(
                                CriticalPersistedTabData.class, criticalPersistedTabData);
                    }

                    tabs.add(mTabModel.getTabAt(0));
                    tabs.add(mTabModel.getTabAt(1));

                    mMediatorSpy.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs),
                            /*quickMode =*/false, /*mruMode =*/false);
                    if (signedInAndSyncEnabled && priceTrackingEnabled && !incognito) {
                        mModel.get(0)
                                .model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                                .fetch((shoppingPersistedTabData) -> {
                                    assertThat(shoppingPersistedTabData.getPriceMicros(),
                                            equalTo(123456789012345L));
                                });
                        mModel.get(1)
                                .model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                                .fetch((shoppingPersistedTabData) -> {
                                    assertThat(shoppingPersistedTabData.getPriceMicros(),
                                            equalTo(ShoppingPersistedTabData.NO_PRICE_KNOWN));
                                });
                    } else {
                        assertNull(mModel.get(0).model.get(
                                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
                        assertNull(mModel.get(1).model.get(
                                TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
                    }
                }
            }
        }
        // Set incognito status back to how it was
        doReturn(true).when(mTab1).isIncognito();
        doReturn(true).when(mTab2).isIncognito();
    }

    @Test
    public void testGetPriceWelcomeMessageInsertionIndex() {
        initWithThreeTabs();

        doReturn(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT)
                .when(mGridLayoutManager)
                .getSpanCount();
        assertThat(mMediator.getPriceWelcomeMessageInsertionIndex(), equalTo(2));

        doReturn(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM)
                .when(mGridLayoutManager)
                .getSpanCount();
        assertThat(mMediator.getPriceWelcomeMessageInsertionIndex(), equalTo(3));
    }

    @Test
    public void testUpdateLayout_PriceMessage() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.LARGE_MESSAGE, PRICE_MESSAGE);
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));

        doAnswer(invocation -> {
            int position = invocation.getArgument(0);
            int itemType = mModel.get(position).type;
            if (itemType == TabProperties.UiType.LARGE_MESSAGE) {
                return mGridLayoutManager.getSpanCount();
            }
            return 1;
        })
                .when(mSpanSizeLookup)
                .getSpanSize(anyInt());
        mMediator.updateLayout();
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        mMediator.updateLayout();
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
    }

    @Test
    public void testUpdateLayout_Divider() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.DIVIDER, 0);
        assertThat(mModel.get(1).type, equalTo(TabProperties.UiType.DIVIDER));

        doAnswer(invocation -> {
            int position = invocation.getArgument(0);
            int itemType = mModel.get(position).type;
            if (itemType == TabProperties.UiType.DIVIDER) {
                return mGridLayoutManager.getSpanCount();
            }
            return 1;
        })
                .when(mSpanSizeLookup)
                .getSpanSize(anyInt());
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        mMediator.updateLayout();
        assertThat(mModel.get(1).type, equalTo(TabProperties.UiType.DIVIDER));
    }

    @Test
    public void testIndexOfNthTabCard() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.LARGE_MESSAGE, PRICE_MESSAGE);

        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModel.indexOfNthTabCard(-1), equalTo(TabModel.INVALID_TAB_INDEX));
        assertThat(mModel.indexOfNthTabCard(0), equalTo(0));
        assertThat(mModel.indexOfNthTabCard(1), equalTo(2));
        assertThat(mModel.indexOfNthTabCard(2), equalTo(3));
    }

    @Test
    public void testGetTabCardCountsBefore() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.LARGE_MESSAGE, PRICE_MESSAGE);

        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModel.getTabCardCountsBefore(-1), equalTo(TabModel.INVALID_TAB_INDEX));
        assertThat(mModel.getTabCardCountsBefore(0), equalTo(0));
        assertThat(mModel.getTabCardCountsBefore(1), equalTo(1));
        assertThat(mModel.getTabCardCountsBefore(2), equalTo(1));
        assertThat(mModel.getTabCardCountsBefore(3), equalTo(2));
    }

    @Test
    public void testGetTabIndexBefore() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.LARGE_MESSAGE, PRICE_MESSAGE);
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModel.getTabIndexBefore(2), equalTo(0));
        assertThat(mModel.getTabIndexBefore(0), equalTo(TabModel.INVALID_TAB_INDEX));
    }

    @Test
    public void testGetTabIndexAfter() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.LARGE_MESSAGE, PRICE_MESSAGE);
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModel.getTabIndexAfter(0), equalTo(2));
        assertThat(mModel.getTabIndexAfter(2), equalTo(TabModel.INVALID_TAB_INDEX));
    }

    @Test
    public void testListObserver_OnItemRangeInserted() {
        setPriceTrackingEnabledForTesting(true);
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, true, null,
                null, null, null, getClass().getSimpleName(), TabProperties.UiType.CLOSABLE, null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(PRICE_MESSAGE);
        mMediator.addSpecialItemToModel(1, TabProperties.UiType.LARGE_MESSAGE, model);
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
    }

    @Test
    public void testListObserver_OnItemRangeRemoved() {
        setPriceTrackingEnabledForTesting(true);
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, true, null,
                null, null, null, getClass().getSimpleName(), TabProperties.UiType.CLOSABLE, null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initWithThreeTabs();

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(PRICE_MESSAGE);
        mMediator.addSpecialItemToModel(2, TabProperties.UiType.LARGE_MESSAGE, model);
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
        mModel.removeAt(0);
        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage() {
        prepareTestMaybeShowPriceWelcomeMessage();
        ShoppingPersistedTabDataFetcher fetcher =
                new ShoppingPersistedTabDataFetcher(mTab1, mPriceWelcomeMessageController);
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(1)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_MessageDisabled() {
        prepareTestMaybeShowPriceWelcomeMessage();
        ShoppingPersistedTabDataFetcher fetcher =
                new ShoppingPersistedTabDataFetcher(mTab1, mPriceWelcomeMessageController);

        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, false);
        assertThat(PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(), equalTo(false));
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_NullParameter() {
        prepareTestMaybeShowPriceWelcomeMessage();

        new ShoppingPersistedTabDataFetcher(mTab1, null)
                .maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_NoPriceDrop() {
        prepareTestMaybeShowPriceWelcomeMessage();
        ShoppingPersistedTabDataFetcher fetcher =
                new ShoppingPersistedTabDataFetcher(mTab1, mPriceWelcomeMessageController);

        fetcher.maybeShowPriceWelcomeMessage(null);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);

        doReturn(null).when(mShoppingPersistedTabData).getPriceDrop();
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUpdateFaviconForGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mModel.get(0).model.set(TabProperties.FAVICON, null);
        doNothing()
                .when(mTabListFaviconProvider)
                .getComposedFaviconImageAsync(any(), anyBoolean(), mCallbackCaptor.capture());

        // Test a group of three.
        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        createTabGroup(tabs, TAB1_ID);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);
        List<GURL> urls = new ArrayList<>(Arrays.asList(TAB1_URL, TAB2_URL, TAB3_URL));
        verify(mTabListFaviconProvider).getComposedFaviconImageAsync(eq(urls), anyBoolean(), any());
        mCallbackCaptor.getValue().onResult(mFavicon);
        assertThat(mModel.get(0).model.get(TabProperties.FAVICON), equalTo(mFavicon));

        // Test a group of five.
        mModel.get(1).model.set(TabProperties.FAVICON, null);
        TabImpl tab4 = prepareTab(0, "tab 4", TAB2_URL);
        TabImpl tab5 = prepareTab(1, "tab 5", JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL));
        tabs.addAll(Arrays.asList(tab4, tab5));
        createTabGroup(tabs, TAB2_ID);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);
        urls = new ArrayList<>(Arrays.asList(TAB2_URL, TAB1_URL, TAB3_URL, TAB2_URL));

        verify(mTabListFaviconProvider).getComposedFaviconImageAsync(eq(urls), anyBoolean(), any());
        mCallbackCaptor.getValue().onResult(mFavicon);
        assertThat(mModel.get(1).model.get(TabProperties.FAVICON), equalTo(mFavicon));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUpdateFaviconForGroup_StaleIndex_SelectAnotherTabWithinGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mModel.get(0).model.set(TabProperties.FAVICON, null);
        mModel.get(1).model.set(TabProperties.FAVICON, null);
        doNothing()
                .when(mTabListFaviconProvider)
                .getComposedFaviconImageAsync(any(), anyBoolean(), mCallbackCaptor.capture());

        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID);
        assertEquals(1, mModel.indexFromId(TAB2_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);

        // Simulate selecting another Tab within TabGroup before callback in
        // getComposedFaviconImageAsync triggers
        mModel.get(1).model.set(TabProperties.TAB_ID, TAB3_ID);
        mCallbackCaptor.getValue().onResult(mFavicon);

        assertNotEquals(1, mModel.indexFromId(TAB2_ID));
        assertNull(mModel.get(1).model.get(TabProperties.FAVICON));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUpdateFaviconForGroup_StaleIndex_CloseTab() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mModel.get(0).model.set(TabProperties.FAVICON, null);
        mModel.get(1).model.set(TabProperties.FAVICON, null);
        doNothing()
                .when(mTabListFaviconProvider)
                .getComposedFaviconImageAsync(any(), anyBoolean(), mCallbackCaptor.capture());

        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID);
        assertEquals(1, mModel.indexFromId(TAB2_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);

        // Simulate closing mTab1 at index 0 before callback in getComposedFaviconImageAsync
        // triggers.
        mModel.removeAt(0);
        mCallbackCaptor.getValue().onResult(mFavicon);

        assertEquals(0, mModel.indexFromId(TAB2_ID));
        assertEquals(mFavicon, mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testUpdateFaviconForGroup_StaleIndex_Reset() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mModel.get(0).model.set(TabProperties.FAVICON, null);
        mModel.get(1).model.set(TabProperties.FAVICON, null);
        doNothing()
                .when(mTabListFaviconProvider)
                .getComposedFaviconImageAsync(any(), anyBoolean(), mCallbackCaptor.capture());

        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID);
        assertEquals(1, mModel.indexFromId(TAB2_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);

        // Simulate TabListMediator reset with null before callback in getComposedFaviconImageAsync
        // triggers.
        mModel.set(new ArrayList<>());
        mCallbackCaptor.getValue().onResult(mFavicon);
    }

    @Test(expected = AssertionError.class)
    public void testGetDomainOnDestroyedTab() {
        Tab tab = new MockTab(TAB1_ID, false);
        tab.destroy();
        TabListMediator.getDomain(tab);
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testTabDescriptionStringSetup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        // Setup the string template.
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand tab group with 2 tabs.";

        // Setup a tab group with {tab2, tab3}.
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID);

        // Reset with show quickly.
        assertThat(mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false),
                equalTo(true));
        assertThat(mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));

        // Reset without show quickly.
        mModel.clear();
        assertThat(mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false),
                equalTo(false));
        assertThat(mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString = String.format("Expand %s tab group with 2 tabs.", CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB2_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab2, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_CONTINUATION_ANDROID})
    public void testCloseButtonDescriptionStringSetup_TabSwitcher() {
        TabUiFeatureUtilities.ENABLE_LAUNCH_POLISH.setForTesting(true);
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        setUpCloseButtonDescriptionString(false);
        String targetString = "Close Tab1 tab";

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.get(POSITION1).model.get(TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Create tab group.
        TabImpl tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group1, TAB1_ID);
        setUpCloseButtonDescriptionString(true);
        targetString = "Close tab group with 2 tabs";

        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.get(POSITION1).model.get(TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString = String.format("Close %s group with 2 tabs", CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB1_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModel.get(POSITION1).model.get(TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));
        TabUiFeatureUtilities.ENABLE_LAUNCH_POLISH.setForTesting(false);
    }

    @Test
    public void testRecordPriceAnnotationsEnabledMetrics() {
        UmaRecorderHolder.resetForTesting();
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        String histogramName = "Commerce.PriceDrop.AnnotationsEnabled";

        SharedPreferencesManager preferencesManager = SharedPreferencesManager.getInstance();
        long presetTime = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);
        preferencesManager.writeLong(
                ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                presetTime);
        mMediator.recordPriceAnnotationsEnabledMetrics();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(histogramName), equalTo(1));
        long updatedTime = preferencesManager.readLong(
                ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                presetTime);
        assertNotEquals(presetTime, updatedTime);

        // This metrics should only be recorded once within one day.
        mMediator.recordPriceAnnotationsEnabledMetrics();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(histogramName), equalTo(1));
        assertEquals(updatedTime,
                preferencesManager.readLong(
                        ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                        -1));
    }

    @Test
    public void testPriceDropSeen() throws TimeoutException {
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.TRACK_PRICES_ON_TABS, true);

        doReturn(false).when(mTab1).isIncognito();
        doReturn(false).when(mTab2).isIncognito();

        List<Tab> tabs = new ArrayList<>();
        tabs.add(mTabModel.getTabAt(0));
        tabs.add(mTabModel.getTabAt(1));

        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs),
                /*quickMode =*/false, /*mruMode =*/false);

        prepareRecyclerViewForScroll();
        mMediator.registerOnScrolledListener(mRecyclerView);
        verify(mRecyclerView).addOnScrollListener(mOnScrollListenerCaptor.capture());
        mOnScrollListenerCaptor.getValue().onScrolled(
                mRecyclerView, /*dx =*/mTabModel.getCount(), /*dy =*/0);
        assertEquals(2, mMediator.getViewedTabIdsForTesting().size());
    }

    @Test
    public void testSelectableUpdates_withoutRelated() {
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, true,
                ()
                        -> { return mSelectionDelegate; },
                null, null, null, getClass().getSimpleName(), TabProperties.UiType.SELECTABLE,
                null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(false);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabModelSelector.getTabById(TAB2_ID)).thenReturn(mTab2);
        when(mTabGroupModelFilter.hasOtherRelatedTabs(mTab2)).thenReturn(false);
        ThumbnailFetcher fetcher2 = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModel.get(1).model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(TAB2_ID);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertEquals(fetcher2, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSelectableUpdates_withRelated() {
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, true,
                ()
                        -> { return mSelectionDelegate; },
                null, null, null, getClass().getSimpleName(), TabProperties.UiType.SELECTABLE,
                null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(false);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabModelSelector.getTabById(TAB2_ID)).thenReturn(mTab2);
        when(mTabGroupModelFilter.hasOtherRelatedTabs(mTab2)).thenReturn(true);
        ThumbnailFetcher fetcher2 = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModel.get(1).model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(TAB2_ID);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(fetcher2, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSelectableUpdates_onReset() {
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        mMediator = new TabListMediator(mActivity, mModel, TabListMode.GRID, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider, true,
                ()
                        -> { return mSelectionDelegate; },
                null, null, null, getClass().getSimpleName(), TabProperties.UiType.SELECTABLE,
                null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID))
                .thenReturn(Arrays.asList(mTab2, tab4));
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(false);
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        ThumbnailFetcher fetcher1 = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher fetcher2 = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher fetcher3 = mModel.get(2).model.get(TabProperties.THUMBNAIL_FETCHER);
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), true, false);

        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertEquals(fetcher1, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertNotEquals(fetcher2, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertEquals(fetcher3, mModel.get(2).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    private void setUpCloseButtonDescriptionString(boolean isGroup) {
        if (isGroup) {
            doAnswer(invocation -> {
                String title = invocation.getArgument(1);
                String num = invocation.getArgument(2);
                return String.format("Close %s group with %s tabs", title, num);
            })
                    .when(mActivity)
                    .getString(anyInt(), anyString(), anyString());

            doAnswer(invocation -> {
                String num = invocation.getArgument(1);
                return String.format("Close tab group with %s tabs", num);
            })
                    .when(mActivity)
                    .getString(anyInt(), anyString());
        } else {
            doAnswer(invocation -> {
                String title = invocation.getArgument(1);
                return String.format("Close %s tab", title);
            })
                    .when(mActivity)
                    .getString(anyInt(), anyString());
        }
    }

    private void setUpTabGroupCardDescriptionString() {
        doAnswer(invocation -> {
            String title = invocation.getArgument(1);
            String num = invocation.getArgument(2);
            return String.format("Expand %s tab group with %s tabs.", title, num);
        })
                .when(mActivity)
                .getString(anyInt(), anyString(), anyString());

        doAnswer(invocation -> {
            String num = invocation.getArgument(1);
            return String.format("Expand tab group with %s tabs.", num);
        })
                .when(mActivity)
                .getString(anyInt(), anyString());
    }

    // initAndAssertAllProperties called with regular mMediator
    private void initAndAssertAllProperties() {
        initAndAssertAllProperties(mMediator);
    }

    // initAndAssertAllProperties called with regular mMediator
    private void initAndAssertAllProperties(int extraTabCount) {
        int index = mTabModel.getCount();
        int totalCount = mTabModel.getCount() + extraTabCount;
        while (index < totalCount) {
            Tab tab = prepareTab(index, TAB1_TITLE, TAB1_URL);
            doReturn(tab).when(mTabModel).getTabAt(index);
            doReturn(index).when(mTabModel).indexOf(tab);
            index++;
        }
        doReturn(totalCount).when(mTabModel).getCount();
        initAndAssertAllProperties(mMediator);
    }

    // initAndAssertAllProperties called with custom mMediator (e.g. if spy needs to be used)
    private void initAndAssertAllProperties(TabListMediator mediator) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        mediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        for (Callback<TabFavicon> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(mFavicon);
        }

        assertThat(mModel.size(), equalTo(mTabModel.getCount()));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        assertThat(mModel.get(0).model.get(TabProperties.FAVICON), instanceOf(TabFavicon.class));
        assertThat(mModel.get(1).model.get(TabProperties.FAVICON), instanceOf(TabFavicon.class));

        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));

        assertThat(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));

        if (mModel.get(0).model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER) != null) return;

        assertThat(mModel.get(0).model.get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
    }

    private TabImpl prepareTab(int id, String title, GURL url) {
        TabImpl tab = TabUiUnitTestUtils.prepareTab(id, title, url);
        when(tab.getView()).thenReturn(mock(View.class));
        doReturn(true).when(tab).isIncognito();
        when(mTitleProvider.getTitle(mActivity, PseudoTab.fromTab(tab))).thenReturn(title);
        doReturn(tab).when(mTabModelSelector).getTabById(id);
        return tab;
    }

    private SimpleRecyclerViewAdapter.ViewHolder prepareViewHolder(int id, int position) {
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                mock(SimpleRecyclerViewAdapter.ViewHolder.class);
        viewHolder.model = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                                   .with(TabProperties.TAB_ID, id)
                                   .with(CARD_TYPE, TAB)
                                   .build();
        doReturn(position).when(viewHolder).getAdapterPosition();
        return viewHolder;
    }

    private RecyclerView.ViewHolder prepareDummyViewHolder(View itemView, int index) {
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(itemView) {};
        when(mRecyclerView.findViewHolderForAdapterPosition(index)).thenReturn(viewHolder);
        return viewHolder;
    }

    private TabGridItemTouchHelperCallback getItemTouchHelperCallback() {
        return (TabGridItemTouchHelperCallback) mMediator.getItemTouchHelperCallback(
                0f, 0f, 0f, mProfile);
    }

    private void setUpForTabGroupOperation(@TabListMediatorType int type, @TabListMode int mode) {
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());

        TabListMediator.TabGridDialogHandler handler =
                type == TabListMediatorType.TAB_GRID_DIALOG ? mTabGridDialogHandler : null;
        boolean actionOnRelatedTabs = type == TabListMediatorType.TAB_SWITCHER;
        int uiType = 0;
        if (type == TabListMediatorType.TAB_SWITCHER
                || type == TabListMediatorType.TAB_GRID_DIALOG) {
            uiType = TabProperties.UiType.CLOSABLE;
        } else if (type == TabListMediatorType.TAB_STRIP) {
            uiType = TabProperties.UiType.STRIP;
        }
        // TODO(crbug.com/1058196): avoid re-instanciate TabListMediator by using annotation.
        ChromeFeatureList.sTabGroupsAndroid.setForTesting(true);

        mMediator = new TabListMediator(mActivity, mModel, mode, mTabModelSelector,
                getTabThumbnailCallback(), mTitleProvider, mTabListFaviconProvider,
                actionOnRelatedTabs, null, null, handler, null, getClass().getSimpleName(), uiType,
                null);
        mMediator.registerOrientationListener(mGridLayoutManager);

        // TabGroupModelFilterObserver is registered when native is ready.
        assertThat(mTabGroupModelFilterObserverCaptor.getAllValues().isEmpty(), equalTo(true));
        mMediator.initWithNative(mProfile);
        assertThat(mTabGroupModelFilterObserverCaptor.getAllValues().isEmpty(), equalTo(false));

        // There are two TabModelObserver and two TabGroupModelFilter.Observer added when
        // initializing TabListMediator, one set from TabListMediator and the other from
        // TabGroupTitleEditor. Here we only test the ones from TabListMediator.
        mMediatorTabModelObserver = mTabModelObserverCaptor.getAllValues().get(1);
        mMediatorTabGroupModelFilterObserver =
                mTabGroupModelFilterObserverCaptor.getAllValues().get(0);

        initAndAssertAllProperties();
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            CriticalPersistedTabData criticalPersistedTabData = CriticalPersistedTabData.from(tab);
            doReturn(rootId).when(criticalPersistedTabData).getRootId();
        }
    }

    private void mockEndpointResponse(Map<String, String> responses) {
        for (Map.Entry<String, String> entry : responses.entrySet()) {
            doAnswer(new Answer<Void>() {
                @Override
                public Void answer(InvocationOnMock invocation) {
                    Callback callback = (Callback) invocation.getArguments()[8];
                    callback.onResult(new EndpointResponse(entry.getValue()));
                    return null;
                }
            })
                    .when(mEndpointFetcherJniMock)
                    .nativeFetchOAuth(any(Profile.class), anyString(), contains(entry.getKey()),
                            anyString(), anyString(), any(String[].class), anyString(), anyLong(),
                            anyInt(), any(Callback.class));
        }
    }

    private void mockOptimizationGuideResponse(
            @OptimizationGuideDecision int decision, Map<GURL, Any> responses) {
        for (Map.Entry<GURL, Any> responseEntry : responses.entrySet()) {
            doAnswer(new Answer<Void>() {
                @Override
                public Void answer(InvocationOnMock invocation) {
                    OptimizationGuideCallback callback =
                            (OptimizationGuideCallback) invocation.getArguments()[3];
                    callback.onOptimizationGuideDecision(decision, responseEntry.getValue());
                    return null;
                }
            })
                    .when(mOptimizationGuideBridgeJniMock)
                    .canApplyOptimization(anyLong(), eq(responseEntry.getKey()), anyInt(),
                            any(OptimizationGuideCallback.class));
        }
    }

    private void initWithThreeTabs() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(PseudoTab.getListOfPseudoTab(tabs), false, false);
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    private void addSpecialItem(int index, @UiType int uiType, int itemIdentifier) {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        if (uiType == TabProperties.UiType.MESSAGE
                || uiType == TabProperties.UiType.LARGE_MESSAGE) {
            when(model.get(MESSAGE_TYPE)).thenReturn(itemIdentifier);
        }
        // Avoid auto-updating the layout when inserting the special card.
        doReturn(1).when(mSpanSizeLookup).getSpanSize(anyInt());
        mMediator.addSpecialItemToModel(index, uiType, model);
    }

    private void prepareTestMaybeShowPriceWelcomeMessage() {
        initAndAssertAllProperties();
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        mPriceDrop = new PriceDrop("1", "2");
        mPriceTabData = new PriceTabData(TAB1_ID, mPriceDrop);
        doReturn(mPriceDrop).when(mShoppingPersistedTabData).getPriceDrop();
    }

    private void prepareRecyclerViewForScroll() {
        View seenView = mock(View.class);
        for (int i = 0; i < mTabModel.getCount(); i++) {
            when(mRecyclerView.getChildAt(i)).thenReturn(seenView);
        }

        doReturn(true).when(mGridLayoutManager).isViewPartiallyVisible(seenView, false, true);
        doReturn(mTabModel.getCount()).when(mRecyclerView).getChildCount();
    }

    private TabListMediator.ThumbnailProvider getTabThumbnailCallback() {
        return (tabId, thumbnailSize, callback, forceUpdate, writeToCache, isSelected) -> {
            mTabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, callback, forceUpdate, writeToCache);
        };
    }

    private static void setPriceTrackingEnabledForTesting(boolean value) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        testValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingFeatures.PRICE_TRACKING_PARAM, String.valueOf(value));
        testValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingFeatures.PRICE_DROP_IPH_ENABLED_PARAM, String.valueOf(value));
        FeatureList.setTestValues(testValues);
    }
}
