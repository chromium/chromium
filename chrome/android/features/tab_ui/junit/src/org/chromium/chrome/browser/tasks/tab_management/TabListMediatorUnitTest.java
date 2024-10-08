// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.core.view.accessibility.AccessibilityNodeInfoCompat.ACTION_CLICK;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.hasItems;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
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

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.ARCHIVED_TABS_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.FOR_TESTING;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.IPH;
import static org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType.PRICE_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Pair;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import com.google.protobuf.ByteString;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.PersistedTabDataConfiguration;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManagerThumbnailProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupTitleUtils;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager.ConfirmationResult;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.commerce.PriceTracking.BuyableProduct;
import org.chromium.components.commerce.PriceTracking.PriceTrackingData;
import org.chromium.components.commerce.PriceTracking.ProductPrice;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.stream.Collectors;

/** Tests for {@link TabListMediator}. */
@SuppressWarnings({
    "ArraysAsListWithZeroOrOneArgument",
    "ResultOfMethodCallIgnored",
    "ConstantConditions"
})
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
@LooperMode(LooperMode.Mode.LEGACY)
@DisableFeatures({ChromeFeatureList.DATA_SHARING})
public class TabListMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mMocker = new JniMocker();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final String TAB5_TITLE = "Tab5";
    private static final String TAB6_TITLE = "Tab6";
    private static final String TAB7_TITLE = "Tab7";
    private static final String NEW_TITLE = "New title";
    private static final String CUSTOMIZED_DIALOG_TITLE1 = "Cool Tabs";
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final GURL TAB1_URL = JUnitTestGURLs.URL_1;
    private static final GURL TAB2_URL = JUnitTestGURLs.URL_2;
    private static final GURL TAB3_URL = JUnitTestGURLs.URL_3;
    private static final GURL TAB4_URL = JUnitTestGURLs.RED_1;
    private static final GURL TAB5_URL = JUnitTestGURLs.RED_2;
    private static final GURL TAB6_URL = JUnitTestGURLs.RED_3;
    private static final GURL TAB7_URL = JUnitTestGURLs.URL_1;
    private static final String NEW_URL = JUnitTestGURLs.EXAMPLE_URL.getSpec();
    private static final int COLOR_2 = 1;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 290;
    private static final int TAB5_ID = 147;
    private static final int TAB6_ID = 258;
    private static final int TAB7_ID = 369;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final String COLLABORATION_ID1 = "A";
    private static final String GROUP_TITLE = "My Group";
    private static final Token TAB_GROUP_ID = new Token(829L, 283L);

    public static final PropertyKey[] TAB_GRID_SELECTABLE_KEYS =
            new PropertyKey[] {
                TabProperties.TAB_ACTION_BUTTON_DATA,
                TabProperties.TAB_CLICK_LISTENER,
                TabProperties.TAB_LONG_CLICK_LISTENER,
                TabProperties.IS_SELECTED,
            };

    public static final PropertyKey[] TAB_GRID_CLOSABLE_KEYS =
            new PropertyKey[] {
                TabProperties.TAB_ACTION_BUTTON_DATA,
                TabProperties.TAB_CLICK_LISTENER,
                TabProperties.TAB_LONG_CLICK_LISTENER,
                TabProperties.CONTENT_DESCRIPTION_STRING,
                TabProperties.ACTION_BUTTON_DESCRIPTION_STRING,
                TabProperties.IS_SELECTED,
            };

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
                    .setValue(
                            ByteString.copyFrom(
                                    PRICE_TRACKING_BUYABLE_PRODUCT_INITIAL.toByteArray()))
                    .build();
    private static final Any ANY_EMPTY = Any.newBuilder().build();

    @IntDef({
        TabListMediatorType.TAB_SWITCHER,
        TabListMediatorType.TAB_STRIP,
        TabListMediatorType.TAB_GRID_DIALOG
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMediatorType {
        int TAB_SWITCHER = 0;
        int TAB_STRIP = 1;
        int TAB_GRID_DIALOG = 2;
        int NUM_ENTRIES = 3;
    }

    @Mock TabContentManager mTabContentManager;
    @Mock TabModel mTabModel;
    @Mock TabModel mIncognitoTabModel;
    @Mock TabListFaviconProvider mTabListFaviconProvider;
    @Mock TabListFaviconProvider.TabFaviconFetcher mTabFaviconFetcher;
    @Mock RecyclerView mRecyclerView;
    @Mock TabListRecyclerView mTabListRecyclerView;
    @Mock RecyclerView.Adapter mAdapter;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock TabListMediator.GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    @Mock TabFavicon mFavicon;
    @Mock Bitmap mFaviconBitmap;
    @Mock Activity mActivity;
    @Mock TabListMediator.TabActionListener mOpenGroupActionListener;
    @Mock GridLayoutManager mGridLayoutManager;
    @Mock GridLayoutManager.SpanSizeLookup mSpanSizeLookup;
    @Mock Profile mProfile;
    @Mock Tracker mTracker;
    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock OptimizationGuideBridge mOptimizationGuideBridge;
    @Mock TabListMediator.TabGridAccessibilityHelper mTabGridAccessibilityHelper;
    @Mock TemplateUrlService mTemplateUrlService;
    @Mock PriceWelcomeMessageController mPriceWelcomeMessageController;
    @Mock ShoppingPersistedTabData mShoppingPersistedTabData;
    @Mock SelectionDelegate<Integer> mSelectionDelegate;
    @Mock ModalDialogManager mModalDialogManager;
    @Mock ActionConfirmationManager mActionConfirmationManager;
    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock IdentityServicesProvider mIdentityServicesProvider;
    @Mock IdentityManager mIdentityManager;
    @Mock TabGroupSyncService mTabGroupSyncService;
    @Mock DataSharingService mDataSharingService;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor ArgumentCaptor<Callback<TabFavicon>> mCallbackCaptor;
    @Captor ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;
    @Captor ArgumentCaptor<Callback<Integer>> mConfirmationResultCallbackCaptor;

    @Captor
    ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    @Captor ArgumentCaptor<RecyclerView.OnScrollListener> mOnScrollListenerCaptor;

    private final ObservableSupplierImpl<TabModelFilter> mCurrentTabModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private Tab mTab1;
    private Tab mTab2;
    private TabListMediator mMediator;
    private TabListModel mModel;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder1;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder2;
    private RecyclerView.ViewHolder mFakeViewHolder1;
    private RecyclerView.ViewHolder mFakeViewHolder2;
    private View mItemView1 = mock(View.class);
    private View mItemView2 = mock(View.class);
    private PriceDrop mPriceDrop;
    private PriceTabData mPriceTabData;
    private String mTab1Domain;
    private String mTab2Domain;
    private String mTab3Domain;
    private String mNewDomain;
    private GURL mFaviconUrl;
    private Resources mResources;

    @Before
    public void setUp() {
        mMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mMocker.mock(
                OptimizationGuideBridgeFactoryJni.TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        mMocker.mock(TabGroupSyncFeaturesJni.TEST_HOOKS, mTabGroupSyncFeaturesJniMock);
        doReturn(mOptimizationGuideBridge)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfile);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);

        mResources = spy(RuntimeEnvironment.application.getResources());
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket))
                .thenReturn(1);

        mTab1Domain = TAB1_URL.getHost().replace("www.", "");
        mTab2Domain = TAB2_URL.getHost().replace("www.", "");
        mTab3Domain = TAB3_URL.getHost().replace("www.", "");
        mNewDomain = new GURL(NEW_URL).getHost().replace("www.", "");
        mFaviconUrl = JUnitTestGURLs.RED_1;

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE, TAB1_URL);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE, TAB2_URL);
        mViewHolder1 = prepareViewHolder(TAB1_ID, POSITION1);
        mViewHolder2 = prepareViewHolder(TAB2_ID, POSITION2);
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, POSITION1);
        mFakeViewHolder2 = prepareFakeViewHolder(mItemView2, POSITION2);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        doNothing().when(mTabContentManager).getTabThumbnailWithCallback(anyInt(), any(), any());
        // Mock that tab restoring stage is over.
        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();
        doReturn(true).when(mIncognitoTabGroupModelFilter).isTabModelRestored();
        doReturn(mProfile).when(mTabModel).getProfile();

        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mIncognitoTabModel).when(mIncognitoTabGroupModelFilter).getTabModel();
        mCurrentTabModelFilterSupplier.set(mTabGroupModelFilter);
        doNothing().when(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION1).when(mTabModel).index();
        doReturn(mTab1).when(mIncognitoTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mIncognitoTabModel).getTabAt(POSITION2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doReturn(2).when(mIncognitoTabModel).getCount();
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForUrlAsync(any(GURL.class), anyBoolean(), mCallbackCaptor.capture());
        doReturn(mFavicon)
                .when(mTabListFaviconProvider)
                .getFaviconFromBitmap(any(Bitmap.class), any(GURL.class));
        doNothing().when(mTabFaviconFetcher).fetch(mCallbackCaptor.capture());
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getDefaultFaviconFetcher(anyBoolean());
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getFaviconForUrlFetcher(any(GURL.class), anyBoolean());
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getFaviconFromBitmapFetcher(any(Bitmap.class), any(GURL.class));
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getComposedFaviconImageFetcher(any(), anyBoolean());
        doReturn(2).when(mTabGroupModelFilter).getCount();
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
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);

        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        doAnswer(
                        invocation -> {
                            int position = invocation.getArgument(0);
                            int itemType = mModel.get(position).type;
                            if (itemType == UiType.MESSAGE || itemType == UiType.LARGE_MESSAGE) {
                                return mGridLayoutManager.getSpanCount();
                            }
                            return 1;
                        })
                .when(mSpanSizeLookup)
                .getSpanSize(anyInt());

        doAnswer(
                        invocation -> {
                            int rootId = invocation.getArgument(0);
                            String title = invocation.getArgument(1);
                            when(mTabGroupModelFilter.getTabGroupTitle(rootId)).thenReturn(title);
                            return null;
                        })
                .when(mTabGroupModelFilter)
                .setTabGroupTitle(anyInt(), anyString());
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
    }

    private static SharedPreferences getGroupTitleSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void resetWithNullTabs() {
        mMediator.resetWithListOfTabs(null, false);

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());
    }

    @Test
    public void updatesTitle_WithoutStoredTitle_Tab() {
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        when(mTab1.getTitle()).thenReturn(NEW_TITLE);
        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(NEW_TITLE));
    }

    @Test
    public void updatesTitle_WithoutStoredTitle_TabGroup() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, false);

        String defaultTitle = TabGroupTitleUtils.getDefaultTitle(mActivity, tabs.size());
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(defaultTitle));
    }

    @Test
    public void updatesTitle_WithStoredTitle_TabGroup() {
        // Mock that tab1 and new tab are in the same group with root ID as TAB1_ID.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_Tab() {
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);

        // Ignored as the tab is not in a group.
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_Empty() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), "");
        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupTitle(mTab1.getRootId(), "");
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo("2 tabs"));
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Tab() {
        var oldFaviconFetcher = mModel.get(0).model.get(TabProperties.FAVICON_FETCHER);
        mTabGroupModelFilter.setTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);

        // Ignored as the tab is not in a group.
        assertEquals(oldFaviconFetcher, mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNull(mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Group_Grid() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        var provider = mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Group_List() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);

        var oldFaviconFetcher = mModel.get(0).model.get(TabProperties.FAVICON_FETCHER);
        var oldProvider = mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertEquals(TabGroupColorId.GREY, oldProvider.getTabGroupColorIdForTesting());
        mTabGroupModelFilter.setTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);

        assertEquals(oldFaviconFetcher, mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        var newProvider = mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertEquals(oldProvider, newProvider);
        assertEquals(TabGroupColorId.BLUE, newProvider.getTabGroupColorIdForTesting());
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Group_RejectFromOldRootId() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        var provider = mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());

        // Updates for the old root ID of one of the grouped tabs should be rejected.
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(TAB3_ID, TabGroupColorId.GREY);
        provider = mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Reset() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);

        PropertyModel model = mModel.get(0).model;
        var provider = spy(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mMediator.resetWithListOfTabs(null, false);
        verify(provider).destroy();
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Remove() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getRootId(), TabGroupColorId.BLUE);

        PropertyModel model = mModel.get(0).model;
        var provider = spy(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mMediator.removeAt(0);
        verify(provider).destroy();
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Ungroup() {
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), false);

        PropertyModel model = mModel.get(0).model;
        var provider = mock(TabGroupColorViewProvider.class);
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertNull(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        verify(provider).destroy();
    }

    @Test
    public void updatesFaviconFetcher_SingleTab_Gts() {
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        TabListFaviconProvider.TabFavicon[] favicon = new TabListFaviconProvider.TabFavicon[1];
        mModel.get(0)
                .model
                .get(TabProperties.FAVICON_FETCHER)
                .fetch(
                        tabFavicon -> {
                            favicon[0] = tabFavicon;
                        });
        mCallbackCaptor.getValue().onResult(mFavicon);
        assertEquals(favicon[0], mFavicon);
    }

    @Test
    public void updatesFaviconFetcher_SingleTabGroup_Gts() {
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        createTabGroup(Arrays.asList(mTab1), TAB1_ID, TAB_GROUP_ID);

        var oldThumbnailFetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotEquals(
                oldThumbnailFetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_SingleTab_NonGts() {
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        TabListFaviconProvider.TabFavicon[] favicon = new TabListFaviconProvider.TabFavicon[1];
        mModel.get(0)
                .model
                .get(TabProperties.FAVICON_FETCHER)
                .fetch(
                        tabFavicon -> {
                            favicon[0] = tabFavicon;
                        });
        mCallbackCaptor.getValue().onResult(mFavicon);
        assertEquals(favicon[0], mFavicon);
    }

    @Test
    public void updatesFaviconFetcher_TabGroup_Gts() {
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        // Assert that tab1 is in a group.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        createTabGroup(Arrays.asList(mTab1, newTab), TAB1_ID, TAB_GROUP_ID);

        var oldThumbnailFetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotEquals(
                oldThumbnailFetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_TabGroup_ListGts() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        // Assert that tab1 is in a group.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        createTabGroup(Arrays.asList(mTab1, newTab), TAB1_ID, TAB_GROUP_ID);
        List<GURL> urls = Arrays.asList(mTab1.getUrl(), newTab.getUrl());

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        verify(mTabListFaviconProvider).getComposedFaviconImageFetcher(eq(urls), anyBoolean());
        assertNull(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_TabGroup_ListGts_SingleTab() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        // Assert that tab1 is in a group.
        createTabGroup(Arrays.asList(mTab1), TAB1_ID, TAB_GROUP_ID);
        List<GURL> urls = Arrays.asList(mTab1.getUrl());

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNull(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        // Don't use the composed fetcher if there is only a single tab.
        verify(mTabListFaviconProvider, never())
                .getComposedFaviconImageFetcher(eq(urls), anyBoolean());
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNull(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpSameDocument() {
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(navigationHandle.isSameDocument()).thenReturn(true);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpSameUrl() {
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB1_URL);
        when(navigationHandle.isSameDocument()).thenReturn(false);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpNtpUrl() {
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        GURL ntpUrl = JUnitTestGURLs.NTP_URL;
        doReturn("")
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(ntpUrl.getSpec()), anyBoolean());

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(navigationHandle.isSameDocument()).thenReturn(false);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, ntpUrl);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
        verify(newTab).addObserver(mTabObserverCaptor.getValue());

        mModel.get(2).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(2).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(newTab, navigationHandle);
        assertNull(mModel.get(2).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation() {
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);

        when(navigationHandle.isSameDocument()).thenReturn(false);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpTabGroup() {
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);

        NavigationHandle navigationHandle = mock(NavigationHandle.class);

        when(navigationHandle.isSameDocument()).thenReturn(false);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void sendsSelectSignalCorrectly() {
        mModel.get(1)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView2, mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mGridCardOnClickListenerProvider)
                .onTabSelecting(mModel.get(1).model.get(TabProperties.TAB_ID), true);
    }

    @Test
    public void sendsOpenGroupSignalCorrectly_SingleTabGroup() {
        List<Tab> tabs = Arrays.asList(mTab1);
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1, mTab2), false);
        mModel.get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, mModel.get(0).model.get(TabProperties.TAB_ID));

        verify(mOpenGroupActionListener).run(mItemView1, TAB1_ID);
    }

    @Test
    public void sendsOpenGroupSignalCorrectly_TabGroup() {
        List<Tab> tabs = Arrays.asList(mTab1, mTab2);
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1, mTab2), false);
        mModel.get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, mModel.get(0).model.get(TabProperties.TAB_ID));

        verify(mOpenGroupActionListener).run(mItemView1, TAB1_ID);
    }

    @Test
    public void sendsCloseSignalCorrectly_ImmediateContinue() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        mModel.get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mActionConfirmationManager)
                .processCloseTabAttempt(any(), mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.IMMEDIATE_CONTINUE);

        TabClosureParams params = TabClosureParams.closeTab(mTab2).build();
        verify(mTabModel).closeTabs(params);
        assertTrue(mModel.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void sendsCloseSignalCorrectly_ConfirmationPositive() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        mModel.get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mActionConfirmationManager)
                .processCloseTabAttempt(any(), mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_POSITIVE);

        TabClosureParams params = TabClosureParams.closeTab(mTab2).allowUndo(false).build();
        verify(mTabModel).closeTabs(params);
        assertTrue(mModel.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void sendsCloseSignalCorrectly_ConfirmationNegative() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        mModel.get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mActionConfirmationManager)
                .processCloseTabAttempt(any(), mConfirmationResultCallbackCaptor.capture());
        mConfirmationResultCallbackCaptor
                .getValue()
                .onResult(ConfirmationResult.CONFIRMATION_NEGATIVE);

        verify(mTabModel, never()).closeTabs(any());
        assertFalse(mModel.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void sendsCloseSignalCorrectly_ActionOnAllRelatedTabs() {
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mModel.get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mActionConfirmationManager, never()).processCloseTabAttempt(any(), any());
        verify(mTabModel).closeTabs(argThat(params -> params.tabs.get(0) == mTab2));
    }

    @Test
    public void sendsCloseSignalCorrectly_Incognito() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        when(mTabGroupModelFilter.isIncognito()).thenReturn(true);
        mModel.get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mActionConfirmationManager, never()).processCloseTabAttempt(any(), any());
        verify(mTabModel).closeTabs(argThat(params -> params.tabs.get(0) == mTab2));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithGroup() {
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabGroupModelFilter).moveRelatedTabs(eq(TAB1_ID), eq(2));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithinGroup() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        getItemTouchHelperCallback().onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    public void sendsMergeTabSignalCorrectly() {
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mFakeViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mFakeViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    // Regression test for https://crbug.com/1372487
    @Test
    public void handlesGroupMergeCorrectly_InOrder() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.getTabAt(3)).thenReturn(tab4);
        View itemView3 = mock(View.class);
        View itemView4 = mock(View.class);
        RecyclerView.ViewHolder fakeViewHolder3 = prepareFakeViewHolder(itemView3, 2);
        RecyclerView.ViewHolder fakeViewHolder4 = prepareFakeViewHolder(itemView4, 3);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3, tab4));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(4));

        // Merge 2 to 1.
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mFakeViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        itemTouchHelperCallback.onSelectedChanged(
                mFakeViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID))
                .thenReturn(Arrays.asList(mTab1, mTab2));
        when(mTabModel.indexOf(mTab1)).thenReturn(POSITION1);
        when(mTabModel.indexOf(mTab2)).thenReturn(POSITION2);
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB1_ID);

        assertThat(mModel.size(), equalTo(3));
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, 0);
        fakeViewHolder3 = prepareFakeViewHolder(itemView3, 1);
        fakeViewHolder4 = prepareFakeViewHolder(itemView4, 2);

        // Merge 4 to 3.
        when(mTabGroupModelFilter.getTabAt(1)).thenReturn(tab3);
        when(mTabGroupModelFilter.getTabAt(2)).thenReturn(tab4);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, fakeViewHolder3);

        itemTouchHelperCallback.onSelectedChanged(
                fakeViewHolder4, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB4_ID), eq(TAB3_ID));
        verify(mGridLayoutManager).removeView(itemView4);
        verify(mTracker, times(2)).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabGroupModelFilter.getRelatedTabList(TAB4_ID)).thenReturn(Arrays.asList(tab3, tab4));
        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID)).thenReturn(Arrays.asList(tab3, tab4));
        when(mTabModel.indexOf(tab3)).thenReturn(2);
        when(mTabModel.indexOf(tab4)).thenReturn(3);
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(tab4, TAB3_ID);

        assertThat(mModel.size(), equalTo(2));
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, 0);
        fakeViewHolder3 = prepareFakeViewHolder(itemView3, 1);

        // Merge 3 to 1.
        when(mTabGroupModelFilter.getTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getTabAt(1)).thenReturn(tab3);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(0);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(1);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mFakeViewHolder1);

        itemTouchHelperCallback.onSelectedChanged(
                fakeViewHolder3, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB3_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(itemView3);
        verify(mTracker, times(3)).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID))
                .thenReturn(Arrays.asList(mTab1, mTab2, tab3, tab4));
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(tab3, TAB1_ID);

        assertThat(mModel.size(), equalTo(1));
    }

    @Test
    public void sendsUngroupSignalCorrectly() {
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(false);
        itemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mFakeViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(1).when(mAdapter).getItemCount();

        // Simulate the ungroup action.
        itemTouchHelperCallback.onSelectedChanged(
                mFakeViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).moveTabOutOfGroupInDirection(TAB1_ID, /* trailing= */ true);
        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void tabClosure() {
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);

        verify(mTab2).removeObserver(any());
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabRemoval() {
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().tabRemoved(mTab2);

        verify(mTab2).removeObserver(any());
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabClosure_IgnoresUpdatesForTabsOutsideOfModel() {
        mTabModelObserverCaptor
                .getValue()
                .willCloseTab(prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL), true);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Restore_SyncingTabListModelWithTabModel() {
        // Mock that tab1 and tab2 are in the same group, and they are being restored. The
        // TabListModel has been cleaned out before the restoring happens. This case could happen
        // within a incognito tab group when user switches between light/dark mode.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        mModel.clear();

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab2,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        assertThat(mModel.size(), equalTo(0));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab1,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        assertThat(mModel.size(), equalTo(1));
    }

    @Test
    public void tabAddition_Gts() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Gts_delayAdd() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        // Add tab marked as delayed
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        true);

        // Verify tab did not get added and delayed tab is captured.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mMediator.getTabToAddDelayedForTesting(), equalTo(newTab));

        // Select delayed tab
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabSelectionType.FROM_USER, mTab1.getId());
        // Assert old tab is still marked as selected
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));

        when(mTabModel.getTabAt(2)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(3);

        // Hide GTS to complete tab addition and selection
        mMediator.postHiding();
        // Assert tab added and selected. Assert old tab is de-selected.
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNull(mMediator.getTabToAddDelayedForTesting());
        verify(mTab1).removeObserver(mTabObserverCaptor.getValue());
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        verify(newTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mTabGroupModelFilter).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mTabGroupModelFilter)
                .removeTabGroupObserver(mTabGroupModelFilterObserverCaptor.getValue());
    }

    @Test
    public void tabAddition_Gts_delayAdd_WithUnexpectedUpdate() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(Arrays.asList(mTab1)).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(Arrays.asList(mTab2)).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(3).when(mTabGroupModelFilter).getCount();
        assertEquals(mModel.size(), 2);

        // Add tab marked as delayed.
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        true);

        // Verify tab did not get added and delayed tab is captured.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mMediator.getTabToAddDelayedForTesting(), equalTo(newTab));

        // Select delayed tab.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabSelectionType.FROM_USER, mTab2.getId());
        // Assert old tab is still marked as selected.
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Remove the first two tabs.
        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, false);
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);
        doReturn(newTab).when(mTabGroupModelFilter).getTabAt(0);
        when(mTabModel.getTabAt(0)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabGroupModelFilter.getTabAt(0)).thenReturn(newTab);
        when(mTabGroupModelFilter.getTabAt(1)).thenReturn(null);
        when(mTabGroupModelFilter.getTabAt(2)).thenReturn(null);
        when(mTabGroupModelFilter.getCount()).thenReturn(1);

        // Hide GTS to complete tab addition and selection.
        mMediator.postHiding();
        // Assert tab added and selected. Assert old tab is de-selected.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNull(mMediator.getTabToAddDelayedForTesting());
        verify(mTab1).removeObserver(mTabObserverCaptor.getValue());
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        verify(newTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mTabGroupModelFilter).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mTabGroupModelFilter)
                .removeTabGroupObserver(mTabGroupModelFilterObserverCaptor.getValue());
    }

    @Test
    public void tabAddition_Gts_Skip() {
        // Add a new tab to the group with mTab2.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(2).when(mTabGroupModelFilter).getCount();
        doReturn(Arrays.asList(mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Gts_Middle() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(newTab).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_End() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Middle() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, newTab, mTab2))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Skip() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        // newTab is of another group.
        doReturn(Arrays.asList(mTab1, mTab2))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabSelection() {
        PropertyModel model0 = mModel.get(0).model;
        PropertyModel model1 = mModel.get(1).model;
        ThumbnailFetcher tab1Fetcher = model0.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = model1.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotNull(tab1Fetcher);
        assertNotNull(tab2Fetcher);
        tab1Fetcher = mock(ThumbnailFetcher.class);
        model0.set(TabProperties.THUMBNAIL_FETCHER, tab1Fetcher);
        tab2Fetcher = mock(ThumbnailFetcher.class);
        model1.set(TabProperties.THUMBNAIL_FETCHER, tab2Fetcher);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertEquals(2, mModel.size());
        assertFalse(model0.get(TabProperties.IS_SELECTED));
        assertNotEquals(model0.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        verify(tab1Fetcher).cancel();
        assertTrue(model1.get(TabProperties.IS_SELECTED));
        assertNotEquals(model1.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);
        verify(tab2Fetcher).cancel();
    }

    @Test
    public void tabSelection_Group() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, newTab));
        createTabGroup(tabs, TAB2_ID, TAB_GROUP_ID);

        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Select tab 3 although the represenative tab 2 should update.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);
    }

    // Regression test for: crbug.com/349773923.
    @Test
    public void tabSelection_LeaveGroupClears() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, newTab));
        createTabGroup(tabs, TAB2_ID, TAB_GROUP_ID);

        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Select tab 3 although the represenative tab 2 should update.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);

        tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        tab2Fetcher = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Select tab 1 again and the other group should unselect.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab1, TabLaunchType.FROM_CHROME_UI, TAB3_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);
    }

    @Test
    public void tabSelection_updatePreviousSelectedTabThumbnailFetcher() {
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        null,
                        mGridCardOnClickListenerProvider,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabActionState.CLOSABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
        // mTabModelObserverCaptor captures on every resetWithListOfTabs call.
        verify(mTabGroupModelFilter, times(2)).addObserver(mTabModelObserverCaptor.capture());

        ThumbnailFetcher tab1Fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(tab2Fetcher, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabClosureUndone() {
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabClosureUndone_SingleTabGroup() {
        assertThat(mModel.size(), equalTo(2));

        createTabGroup(Arrays.asList(mTab2), TAB2_ID, TAB_GROUP_ID);

        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB2_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void testCloseTabInGroup_withArchivedTabsMessagePresent() {
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        when(mTabGroupModelFilter.tabGroupExistsForRootId(anyInt())).thenReturn(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        assertThat(mModel.size(), equalTo(2));

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(ARCHIVED_TABS_MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.CUSTOM_MESSAGE, model);
        assertThat(mModel.size(), equalTo(3));

        // This crashed previously when it tried to update the message instead of the tab group
        // (crbug.com/347970497).
        mTabModelObserverCaptor.getValue().willCloseTab(newTab, true);
        verify(model, times(0)).set(eq(TabProperties.TAB_ID), anyInt());
    }

    @Test
    public void tabMergeIntoGroup() {
        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);

        // Assume that reset in TabGroupModelFilter is finished.
        createTabGroup(Arrays.asList(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(POSITION2));
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotNull(mModel.get(1).model.get(TabProperties.FAVICON_FETCHER));

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab1, TAB2_ID);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo("2 tabs"));
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void tabMergeIntoGroup_Parity() {
        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);

        // Assume that reset in TabGroupModelFilter is finished.
        createTabGroup(Arrays.asList(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(POSITION2));
        var oldFetcher = mModel.get(0).model.get(TabProperties.FAVICON_FETCHER);
        assertNotNull(oldFetcher);
        assertNotNull(mModel.get(1).model.get(TabProperties.FAVICON_FETCHER));

        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab1, TAB2_ID);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE1));
        var newFetcher = mModel.get(0).model.get(TabProperties.FAVICON_FETCHER);
        assertNull(newFetcher);

        assertNotNull(mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
    }

    @Test
    public void tabMergeIntoGroup_Dialog() {
        createTabGroup(List.of(mTab1), TAB1_ID, TAB_GROUP_ID);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1), false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(List.of(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);

        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB1_ID)).thenReturn(TAB1_ID);
        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        verify(mTabGridDialogHandler).updateDialogContent(TAB1_ID);
    }

    @Test
    public void tabMergeIntoGroup_Dialog_NoOp() {
        createTabGroup(List.of(mTab1), TAB1_ID, TAB_GROUP_ID);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1), false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(List.of(mTab2), TAB2_ID, new Token(7, 9));

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB2_ID);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        verify(mTabGridDialogHandler, never()).updateDialogContent(TAB1_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Gts_Moved_Tab_Selected() {
        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2));
        mMediator.resetWithListOfTabs(tabs, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(mTab1);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_Origin_Tab_Selected() {
        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_LastTab() {
        // Assume that tab1 is a single tab group that became a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // These properties should get reset.
        mModel.get(0).model.set(TabProperties.TITLE, CUSTOMIZED_DIALOG_TITLE1);
        ThumbnailFetcher fetcher = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Ungroup the single tab.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertNotEquals(fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_TabAdditionWithSameId() {
        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);

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
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

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
        assertThat(
                mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(
                mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
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
        assertThat(
                mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(
                mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
    }

    @Test
    public void testShoppingFetcherUngroupedThenGrouped() {
        prepareForPriceDrop();
        resetWithRegularTabs(false);

        assertThat(mModel.size(), equalTo(2));
        assertThat(
                mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(
                mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        resetWithRegularTabs(true);
        assertThat(mModel.size(), equalTo(2));
        assertNull(mModel.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModel.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
    }

    /** Set flags and initialize for verifying price drop behavior */
    private void prepareForPriceDrop() {
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PersistedTabDataConfiguration.setUseTestConfig(true);
        initAndAssertAllProperties();
    }

    /**
     * Reset mediator with non-incognito tabs which are optionally grouped
     *
     * @param isGrouped true if the tabs should be grouped
     */
    private void resetWithRegularTabs(boolean isGrouped) {
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(1);
        doReturn(2).when(mTabGroupModelFilter).getCount();
        if (isGrouped) {
            doReturn(Arrays.asList(mTab1, mTab2))
                    .when(mTabGroupModelFilter)
                    .getRelatedTabList(eq(TAB1_ID));
            doReturn(Arrays.asList(mTab1, mTab2))
                    .when(mTabGroupModelFilter)
                    .getRelatedTabList(eq(TAB2_ID));
            doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(mTab1);
            doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(mTab2);
        } else {
            doReturn(Arrays.asList(mTab1))
                    .when(mTabGroupModelFilter)
                    .getRelatedTabList(eq(TAB1_ID));
            doReturn(Arrays.asList(mTab2))
                    .when(mTabGroupModelFilter)
                    .getRelatedTabList(eq(TAB2_ID));
            doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(mTab1);
            doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(mTab2);
        }
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(false).when(mTab1).isIncognito();
        doReturn(false).when(mTab2).isIncognito();
        mMediator.resetWithListOfTabs(tabs, false);
    }

    @Test
    public void tabMoveOutOfGroup_Dialog() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler).updateDialogContent(TAB2_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Dialog_LastTab() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Ungroup the single tab.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        verify(mTabGridDialogHandler).updateDialogContent(Tab.INVALID_TAB_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Strip() {
        setUpTabListMediator(TabListMediatorType.TAB_STRIP, TabListMode.GRID);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler, never()).updateDialogContent(anyInt());
    }

    @Test
    public void tabMovementWithGroup_Forward() {
        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithGroup_Backward() {
        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabGridDialog_Forward() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabGridDialog_Backward() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_Forward() {
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
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB3_ID).when(tab3).getRootId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(2));

        // Select tab3 so the group doesn't have the selected tab.
        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(tab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

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

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        // mTab1 is still first in group after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        // TODO(crbug.com/40242432): Make this an assertion and don't update.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab2, tab3. Update is precautionary.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_Backward() {
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
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB3_ID).when(tab3).getRootId();

        // Select tab3 so the group doesn't have the selected tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(2));

        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(tab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

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

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        // mTab1 is first in group after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        // TODO(crbug.com/40242432): Make this an assertion and don't update.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab2, tab3. Update is precautionary.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_SelectedNotMoved() {
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
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB1_ID).when(tab3).getRootId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(1));

        // Assume that moveTab in TabModel is finished.

        // mTab1 selected.
        doReturn(POSITION1).when(mTabModel).index();
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab1, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

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

        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(mTab2, POSITION2, 2);

        // mTab1 still selected after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        // TODO(crbug.com/40242432): Make this an assertion.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab3, tab2.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_SelectedMoved() {
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
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB1_ID).when(tab3).getRootId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(1));

        // Assume that moveTab in TabModel is finished.

        // mTab1 selected (at new position).
        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab1, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

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

        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(mTab1, 2, POSITION1);

        // mTab1 still selected after the move (last selected), but the thumbnail updated.
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        // TODO(crbug.com/40242432): Make this an assertion.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab3, tab2.
        assertNotEquals(tab1Fetcher, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void undoGrouped_One_Adjacent_Tab() {
        // Assume there are 3 tabs in TabModel, mTab2 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping mTab2 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoForwardGrouped_One_Tab() {
        // Assume there are 3 tabs in TabModel, tab3 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping tab3 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(tab3);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(tab3, POSITION1);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoBackwardGrouped_One_Tab() {
        // Assume there are 3 tabs in TabModel, mTab1 just grouped with mTab2;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping mTab1 from mTab2.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);
        doReturn(2).when(mTabGroupModelFilter).indexOf(tab3);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(mTab1);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoForwardGrouped_BetweenGroups() {
        // Assume there are 3 tabs in TabModel, tab3, tab4, just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        doReturn(4).when(mTabModel).getCount();
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false);
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
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(tab3);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab4);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(tab3, POSITION1);
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
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab3);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab4);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(tab4).when(mTabModel).getTabAt(3);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);
        doReturn(TAB3_ID).when(tab4).getRootId();
        doReturn(2).when(mTabGroupModelFilter).getRelatedTabCountForRootId(TAB3_ID);
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(tab4, POSITION1);
        assertThat(mModel.size(), equalTo(2));

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(tab4, TAB4_ID);

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

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(configuration);

        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT);
    }

    @Test
    public void updateSpanCount_Landscape_SingleWindow() {
        initAndAssertAllProperties();
        // Mock that we are switching to landscape mode.
        Configuration configuration = new Configuration();
        configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        configuration.screenWidthDp = TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP - 1;

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(configuration);

        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void updateSpanCount_onTablet_multipleScreenWidths() {
        initAndAssertAllProperties(3);
        // Mock tablet
        when(mResources.getInteger(R.integer.min_screen_width_bucket))
                .thenReturn(TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP + 1);
        Configuration portraitConfiguration = new Configuration();
        portraitConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;

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
    public void getLatestTitle_NotGts() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        // Even if we have a stored title, we only show it in tab switcher.
        assertThat(
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true), equalTo(TAB1_TITLE));
    }

    @Test
    public void getLatestTitle_SingleTabGroupSupported_Gts() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        // We never show stored title for single tab.
        assertThat(
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void getLatestTitle_SingleTabGroupNotSupported_Gts() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabs, TAB1_ID, null);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);

        // We never show stored title for single tab.
        assertThat(
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true), equalTo(TAB1_TITLE));
    }

    @Test
    public void getLatestTitle_Stored_Gts() {
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        assertThat(
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void getLatestTitle_Default_Gts() {
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        assertThat(
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true), equalTo("2 tabs"));
    }

    @Test
    public void getLatestTitle_NoDefault_Gts() {
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        assertThat(mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ false), equalTo(""));
    }

    @Test
    public void updateTabGroupTitle_Gts() {
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand tab group with 2 tabs, color Grey.";
        assertThat(mModel.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Mock that tab1 and newTab are in the same group and group root id is TAB1_ID.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);

        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);

        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    public void updateTabGroupTitle_SingleTab_Gts() {
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand tab group with 1 tab, color Grey.";
        assertThat(mModel.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(Arrays.asList(mTab1), TAB1_ID, TAB_GROUP_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);

        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);

        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    public void tabGroupTitleEditor_storeTitle() {
        TabGroupTitleEditor tabGroupTitleEditor = mMediator.getTabGroupTitleEditor();
        tabGroupTitleEditor.storeTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
        verify(mTabGroupModelFilter).setTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);
    }

    @Test
    public void tabGroupTitleEditor_deleteTitle() {
        TabGroupTitleEditor tabGroupTitleEditor = mMediator.getTabGroupTitleEditor();
        tabGroupTitleEditor.deleteTabGroupTitle(mTab1.getRootId());
        verify(mTabGroupModelFilter).deleteTabGroupTitle(mTab1.getRootId());
    }

    @Test
    public void addSpecialItem() {
        mMediator.resetWithListOfTabs(null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.MESSAGE, model);

        assertTrue(mModel.size() > 0);
        assertEquals(TabProperties.UiType.MESSAGE, mModel.get(0).type);
    }

    @Test
    public void addSpecialItem_notPersistOnReset() {
        mMediator.resetWithListOfTabs(null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.MESSAGE, model);
        assertEquals(TabProperties.UiType.MESSAGE, mModel.get(0).type);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabs, /* quickMode= */ false);
        assertThat(mModel.size(), equalTo(2));
        assertNotEquals(TabProperties.UiType.MESSAGE, mModel.get(0).type);
        assertNotEquals(TabProperties.UiType.MESSAGE, mModel.get(1).type);

        mMediator.addSpecialItemToModel(1, TabProperties.UiType.MESSAGE, model);
        assertThat(mModel.size(), equalTo(3));
        assertEquals(TabProperties.UiType.MESSAGE, mModel.get(1).type);
    }

    @Test
    public void addSpecialItem_withoutTabListModelProperties() {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        mMediator.resetWithListOfTabs(null, false);

        try {
            mMediator.addSpecialItemToModel(0, TabProperties.UiType.MESSAGE, new PropertyModel());
        } catch (AssertionError e) {
            return;
        }
        fail("PropertyModel#validateKey() assert should have failed.");
    }

    @Test
    public void removeSpecialItem_Message() {
        mMediator.resetWithListOfTabs(null, false);

        PropertyModel model = mock(PropertyModel.class);
        int expectedMessageType = FOR_TESTING;
        int wrongMessageType = PRICE_MESSAGE;
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
        mMediator.resetWithListOfTabs(null, false);

        PropertyModel model = mock(PropertyModel.class);
        int expectedMessageType = PRICE_MESSAGE;
        int wrongMessageType = IPH;
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
    public void removeSpecialItem_Message_CustomMessage() {
        mMediator.resetWithListOfTabs(null, false);

        PropertyModel model = mock(PropertyModel.class);
        int expectedMessageType = ARCHIVED_TABS_MESSAGE;
        int wrongMessageType = IPH;
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.CUSTOM_MESSAGE, model);
        assertEquals(1, mModel.size());

        mMediator.removeSpecialItemFromModel(TabProperties.UiType.MESSAGE, wrongMessageType);
        assertEquals(1, mModel.size());

        mMediator.removeSpecialItemFromModel(
                TabProperties.UiType.CUSTOM_MESSAGE, expectedMessageType);
        assertEquals(0, mModel.size());
    }

    @Test
    public void testUrlUpdated_forSingleTab_Gts() {
        assertNotEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        doReturn(new GURL(NEW_URL)).when(mTab1).getUrl();

        var oldFetcher = mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        assertNotEquals(
                oldFetcher, mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testUrlUpdated_forGroup_Gts() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(
                mTab1Domain + ", " + mTab2Domain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        // Update URL_DOMAIN for mTab1.
        doReturn(new GURL(NEW_URL)).when(mTab1).getUrl();
        var oldFetcher = mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(
                mNewDomain + ", " + mTab2Domain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        var newFetcher = mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);

        // Update URL_DOMAIN for mTab2.
        doReturn(new GURL(NEW_URL)).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(
                mNewDomain + ", " + mNewDomain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        var newestFetcher = mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(newFetcher, newestFetcher);
    }

    @Test
    public void testUrlUpdated_forGroup_Dialog() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        verify(mTab2, times(1)).addObserver(mTabObserverCaptor.getValue());

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(mTab1Domain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());
        var oldFetcher = mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Update URL_DOMAIN for mTab1.
        doReturn(new GURL(NEW_URL)).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        var newFetcher = mModel.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);

        oldFetcher = mModel.get(POSITION2).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Update URL_DOMAIN for mTab2.
        doReturn(new GURL(NEW_URL)).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(mNewDomain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mNewDomain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));

        newFetcher = mModel.get(POSITION2).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);
    }

    @Test
    public void testUrlUpdated_forUngroup() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilterObserverCaptor.getValue().didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(
                mTab1Domain + ", " + mTab2Domain,
                mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        // Assume that TabGroupModelFilter is already updated.
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB2_ID).when(mTab2).getRootId();
        doReturn(1).when(mTabGroupModelFilter).getRelatedTabCountForRootId(TAB1_ID);
        doReturn(1).when(mTabGroupModelFilter).getRelatedTabCountForRootId(TAB2_ID);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);
        assertEquals(mTab1Domain, mModel.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModel.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo() {
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
    public void testPerformAccessibilityAction() {
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
    public void testPerformAccessibilityAction_defaultAccessibilityAction() {
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
    public void testPerformAccessibilityAction_InvalidIndex() {
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

        assertThat(mModel.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void testTabObserverReattachToUndoClosedTab() {
        initAndAssertAllProperties();
        // Called twice in test set up due to reset with list & adding tab to model.
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());

        assertThat(mModel.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);
        assertThat(mModel.size(), equalTo(1));
        verify(mTab2).removeObserver(any());

        // Assume that TabModelFilter is already updated to reflect closed tab is undone.
        doReturn(2).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);
        assertThat(mModel.size(), equalTo(2));
        // Verify the count increased when we added the tab to the model.
        verify(mTab2, times(3)).addObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testUnchangeCheckIgnoreNonTabs() {
        initAndAssertAllProperties();
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        boolean showQuickly = mMediator.resetWithListOfTabs(tabs, /* quickMode= */ false);
        assertThat(showQuickly, equalTo(true));

        // Create a PropertyModel that is not a tab and add it to the existing TabListModel.
        PropertyModel propertyModel = mock(PropertyModel.class);
        when(propertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(mModel.size(), TabProperties.UiType.MESSAGE, propertyModel);
        assertThat(mModel.size(), equalTo(tabs.size() + 1));

        // TabListModel unchange check should ignore the non-Tab item.
        showQuickly = mMediator.resetWithListOfTabs(tabs, /* quickMode= */ false);
        assertThat(showQuickly, equalTo(true));
    }

    // TODO(crbug.com/40168614): the assertThat in fetch callback is never reached.
    @Test
    public void testPriceTrackingProperty() {
        setPriceTrackingEnabledForTesting(true);
        for (boolean signedInAndSyncEnabled : new boolean[] {false, true}) {
            for (boolean priceTrackingEnabled : new boolean[] {false, true}) {
                for (boolean incognito : new boolean[] {false, true}) {
                    TabListMediator mMediatorSpy = spy(mMediator);
                    doReturn(false).when(mMediatorSpy).isTabInTabGroup(any());
                    PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(
                            signedInAndSyncEnabled);
                    PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                            PriceTrackingUtilities.TRACK_PRICES_ON_TABS, priceTrackingEnabled);
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
                        long timestamp = System.currentTimeMillis();
                        Tab tab = mTabModel.getTabAt(i);
                        doReturn(timestamp).when(tab).getTimestampMillis();
                    }

                    tabs.add(mTabModel.getTabAt(0));
                    tabs.add(mTabModel.getTabAt(1));

                    mMediatorSpy.resetWithListOfTabs(tabs, /* quickMode= */ false);
                    if (signedInAndSyncEnabled && priceTrackingEnabled && !incognito) {
                        mModel.get(0)
                                .model
                                .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                                .fetch(
                                        (shoppingPersistedTabData) -> {
                                            assertThat(
                                                    shoppingPersistedTabData.getPriceMicros(),
                                                    equalTo(123456789012345L));
                                        });
                        mModel.get(1)
                                .model
                                .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                                .fetch(
                                        (shoppingPersistedTabData) -> {
                                            assertThat(
                                                    shoppingPersistedTabData.getPriceMicros(),
                                                    equalTo(
                                                            ShoppingPersistedTabData
                                                                    .NO_PRICE_KNOWN));
                                        });
                    } else {
                        assertNull(
                                mModel.get(0)
                                        .model
                                        .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
                        assertNull(
                                mModel.get(1)
                                        .model
                                        .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
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

        doAnswer(
                        invocation -> {
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
    public void testIndexOfNthTabCardOrInvalid() {
        initAndAssertAllProperties();
        addSpecialItem(1, TabProperties.UiType.LARGE_MESSAGE, PRICE_MESSAGE);

        assertThat(mModel.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mMediator.getIndexOfNthTabCard(-1), equalTo(TabModel.INVALID_TAB_INDEX));
        assertThat(mMediator.getIndexOfNthTabCard(0), equalTo(0));
        assertThat(mMediator.getIndexOfNthTabCard(1), equalTo(2));
        assertThat(mMediator.getIndexOfNthTabCard(2), equalTo(TabModel.INVALID_TAB_INDEX));
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
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        setPriceTrackingEnabledForTesting(true);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        null,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
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
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        setPriceTrackingEnabledForTesting(true);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        null,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
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
                new ShoppingPersistedTabDataFetcher(mTab1, () -> mPriceWelcomeMessageController);
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(1)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_MessageDisabled() {
        prepareTestMaybeShowPriceWelcomeMessage();
        ShoppingPersistedTabDataFetcher fetcher =
                new ShoppingPersistedTabDataFetcher(mTab1, () -> mPriceWelcomeMessageController);

        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, false);
        assertThat(
                PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile), equalTo(false));
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_SupplierIsNull() {
        prepareTestMaybeShowPriceWelcomeMessage();

        new ShoppingPersistedTabDataFetcher(mTab1, null)
                .maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_SupplierContainsNull() {
        prepareTestMaybeShowPriceWelcomeMessage();

        Supplier<PriceWelcomeMessageController> supplier = () -> null;
        new ShoppingPersistedTabDataFetcher(mTab1, supplier)
                .maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_NoPriceDrop() {
        prepareTestMaybeShowPriceWelcomeMessage();
        ShoppingPersistedTabDataFetcher fetcher =
                new ShoppingPersistedTabDataFetcher(mTab1, () -> mPriceWelcomeMessageController);

        fetcher.maybeShowPriceWelcomeMessage(null);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);

        doReturn(null).when(mShoppingPersistedTabData).getPriceDrop();
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0)).showPriceWelcomeMessage(mPriceTabData);
    }

    @Test
    public void testDidCreateNewGroup() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        when(mTabModel.isIncognito()).thenReturn(false);
        // Mock that we have a stored color stored with reference to root ID of tab1.
        when(mTabGroupModelFilter.getTabGroupColor(mTab1.getRootId())).thenReturn(COLOR_2);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(mTab1.getRootId()))
                .thenReturn(COLOR_2);

        assertNull(mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        // Test a group of three.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didCreateNewGroup(mTab1, mTabGroupModelFilter);

        assertNotNull(mModel.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_Grid() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);

        when(mTabModel.isIncognito()).thenReturn(false);
        // Mock that we have a stored color stored with reference to root ID of tab1.
        when(mTabGroupModelFilter.getTabGroupColor(mTab1.getRootId())).thenReturn(COLOR_2);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(mTab1.getRootId()))
                .thenReturn(COLOR_2);

        // Test a group of three.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_List() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);

        // Test a group of three.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(List.of(mTab1), false);

        List<GURL> urls = new ArrayList<>(Arrays.asList(TAB1_URL, TAB2_URL, TAB3_URL));
        verify(mTabListFaviconProvider).getComposedFaviconImageFetcher(eq(urls), anyBoolean());
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        // Try it from the TabObserver.
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);
        verify(mTabListFaviconProvider, times(2))
                .getComposedFaviconImageFetcher(eq(urls), anyBoolean());
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));

        // Test a group of five.
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        Tab tab4 = prepareTab(0, "tab 4", TAB2_URL);
        Tab tab5 = prepareTab(1, "tab 5", JUnitTestGURLs.EXAMPLE_URL);
        tabs.addAll(Arrays.asList(tab4, tab5));
        createTabGroup(tabs, TAB2_ID, TAB_GROUP_ID);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);
        urls = new ArrayList<>(Arrays.asList(TAB1_URL, TAB2_URL, TAB3_URL, TAB2_URL));
        verify(mTabListFaviconProvider).getComposedFaviconImageFetcher(eq(urls), anyBoolean());
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_StaleIndex_SelectAnotherTabWithinGroup() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mModel.get(1).model.set(TabProperties.FAVICON_FETCHER, null);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        List<GURL> group1Urls = new ArrayList<>(Arrays.asList(mTab2.getUrl(), tab3.getUrl()));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);
        assertEquals(1, mModel.indexFromId(TAB2_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);
        verify(mTabListFaviconProvider)
                .getComposedFaviconImageFetcher(eq(group1Urls), anyBoolean());

        // Simulate selecting another Tab within TabGroup.
        mModel.get(1).model.set(TabProperties.TAB_ID, TAB3_ID);

        assertNotEquals(1, mModel.indexFromId(TAB2_ID));
        assertNotNull(mModel.get(1).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_StaleIndex_CloseTab() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mModel.get(1).model.set(TabProperties.FAVICON_FETCHER, null);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        List<GURL> group1Urls = new ArrayList<>(Arrays.asList(mTab2.getUrl(), tab3.getUrl()));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);
        assertEquals(1, mModel.indexFromId(TAB2_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);
        verify(mTabListFaviconProvider)
                .getComposedFaviconImageFetcher(eq(group1Urls), anyBoolean());

        // Simulate closing mTab1 at index 0.
        mModel.removeAt(0);

        assertEquals(0, mModel.indexFromId(TAB2_ID));
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_StaleIndex_Reset() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.LIST);
        mModel.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mModel.get(1).model.set(TabProperties.FAVICON_FETCHER, null);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        List<GURL> group1Urls = new ArrayList<>(Arrays.asList(mTab2.getUrl(), tab3.getUrl()));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);
        assertEquals(1, mModel.indexFromId(TAB2_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab2, mFaviconBitmap, mFaviconUrl);
        verify(mTabListFaviconProvider)
                .getComposedFaviconImageFetcher(eq(group1Urls), anyBoolean());

        // Simulate TabListMediator reset with null.
        mModel.set(new ArrayList<>());
    }

    @Test(expected = AssertionError.class)
    public void testGetDomainOnDestroyedTab() {
        Tab tab = new MockTab(TAB1_ID, mProfile);
        tab.destroy();
        TabListMediator.getDomain(tab);
    }

    @Test
    public void testTabDescriptionStringSetup() {
        // Setup the string template.
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand tab group with 2 tabs, color Grey.";

        // Setup a tab group with {tab2, tab3}.
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);

        // Reset with show quickly.
        assertThat(mMediator.resetWithListOfTabs(tabs, false), equalTo(true));
        assertThat(
                mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));

        // Reset without show quickly.
        mModel.clear();
        assertThat(mMediator.resetWithListOfTabs(tabs, false), equalTo(false));
        assertThat(
                mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format(
                        "Expand %s tab group with 2 tabs, color Grey.", CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB2_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab2, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testTabGroupShareExpandDescriptionString() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        // Setup a tab group with {tab2, tab3}.
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);
        setupSyncedGroup(/* isShared= */ true);

        final @TabGroupColorId int defaultColor = TabGroupColorId.GREY;
        final @StringRes int colorDesc =
                ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(defaultColor);
        String emptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_expand_shared_tab_group_with_color,
                        group1.size(),
                        group1.size(),
                        mResources.getString(colorDesc));

        // Check that a base group with no title has the correct content description.
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(
                mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(emptyTitleTargetString));

        String nonEmptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_expand_shared_tab_group_with_group_name_with_color,
                        group1.size(),
                        CUSTOMIZED_DIALOG_TITLE1,
                        group1.size(),
                        mResources.getString(colorDesc));
        // Check that a customized title provides a different content description.
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB2_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab2, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModel.get(POSITION2).model.get(TabProperties.CONTENT_DESCRIPTION_STRING),
                equalTo(nonEmptyTitleTargetString));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_GROUP_PANE_ANDROID})
    public void testCloseButtonDescriptionStringSetup_TabSwitcher() {
        setUpCloseButtonDescriptionString(false);
        String targetString = "Close Tab1 tab";

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        setUpCloseButtonDescriptionString(true);
        targetString = "Close tab group with 2 tabs, color Grey.";

        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format("Close %s group with 2 tabs, color Grey.", CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB1_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_GROUP_PANE_ANDROID})
    public void testCloseButtonDescriptionStringSetup_SingleTabGroup_TabSwitcher() {
        // Create tab group.
        List<Tab> tabs = Arrays.asList(mTab1);
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        setUpCloseButtonDescriptionString(true);
        String targetString = "Close tab group with 1 tab, color Grey.";

        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format("Close %s group with 1 tab, color Grey.", CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB1_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_GROUP_PANE_ANDROID})
    public void testCloseButtonDescriptionStringWithColorSetup_TabSwitcher() {
        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        String targetString = "Close tab group with 2 tabs, color Grey.";

        mMediator.resetWithListOfTabs(group1, false);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format("Close %s group with 2 tabs, color Grey.", CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB1_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
    })
    public void testActionButtonDescriptionStringGroupOverflowMenu_TabSwitcher() {
        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        final @TabGroupColorId int defaultColor = TabGroupColorId.GREY;
        final @StringRes int colorDesc =
                ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(defaultColor);
        String targetString =
                String.format(
                        "Open the tab group action menu for tab group 2 tabs, color %s.",
                        mResources.getString(colorDesc));

        mMediator.resetWithListOfTabs(group1, false);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format(
                        "Open the tab group action menu for tab group %s, color %s.",
                        CUSTOMIZED_DIALOG_TITLE1, mResources.getString(colorDesc));
        mMediator.getTabGroupTitleEditor().storeTabGroupTitle(TAB1_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModel.get(POSITION1).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(targetString));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PANE_ANDROID, ChromeFeatureList.DATA_SHARING})
    public void testActionButtonDescriptionStringGroupOverflowMenu_TabSwitcherSharedGroup() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);
        setupSyncedGroup(/* isShared= */ true);

        String defaultTitle = TabGroupTitleUtils.getDefaultTitle(mActivity, group1.size());
        final @TabGroupColorId int defaultColor = TabGroupColorId.GREY;
        final @StringRes int colorDesc =
                ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(defaultColor);
        String emptyTitleTargetString =
                mResources.getString(
                        R.string
                                .accessibility_open_shared_tab_group_overflow_menu_with_group_name_with_color,
                        defaultTitle,
                        mResources.getString(colorDesc));

        // Check that a base group with no title has the correct content description.
        mMediator.resetWithListOfTabs(group1, false);
        assertThat(
                mModel.get(POSITION2).model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING),
                equalTo(emptyTitleTargetString));
    }

    @Test
    public void testRecordPriceAnnotationsEnabledMetrics() {
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        String histogramName = "Commerce.PriceDrop.AnnotationsEnabled";

        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        long presetTime = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);
        preferencesManager.writeLong(
                ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                presetTime);
        mMediator.recordPriceAnnotationsEnabledMetrics();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(histogramName), equalTo(1));
        long updatedTime =
                preferencesManager.readLong(
                        ChromePreferenceKeys.PRICE_TRACKING_ANNOTATIONS_ENABLED_METRICS_TIMESTAMP,
                        presetTime);
        assertNotEquals(presetTime, updatedTime);

        // This metrics should only be recorded once within one day.
        mMediator.recordPriceAnnotationsEnabledMetrics();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(histogramName), equalTo(1));
        assertEquals(
                updatedTime,
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

        mMediator.resetWithListOfTabs(tabs, /* quickMode= */ false);

        prepareRecyclerViewForScroll();
        mMediator.registerOnScrolledListener(mRecyclerView);
        verify(mRecyclerView).addOnScrollListener(mOnScrollListenerCaptor.capture());
        mOnScrollListenerCaptor
                .getValue()
                .onScrolled(mRecyclerView, /* dx= */ mTabModel.getCount(), /* dy= */ 0);
        assertEquals(2, mMediator.getViewedTabIdsForTesting().size());
    }

    @Test
    public void testSelectableUpdates_withoutRelated() {
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> {
                            return mSelectionDelegate;
                        },
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.SELECTABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(false);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        ThumbnailFetcher fetcher2 = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModel.get(1).model.get(TabProperties.TAB_CLICK_LISTENER).run(mItemView2, TAB2_ID);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertEquals(fetcher2, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSelectableUpdates_withRelated() {
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> {
                            return mSelectionDelegate;
                        },
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.SELECTABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(false);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        ThumbnailFetcher fetcher2 = mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModel.get(1).model.get(TabProperties.TAB_CLICK_LISTENER).run(mItemView2, TAB2_ID);
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(fetcher2, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSelectableUpdates_onReset() {
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> {
                            return mSelectionDelegate;
                        },
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.SELECTABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID))
                .thenReturn(Arrays.asList(mTab2, tab4));
        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID)).thenReturn(Arrays.asList(tab3));
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(tab4)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(tab3)).thenReturn(false);
        List<Tab> tabs = Arrays.asList(mTab1, mTab2, tab3);
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(false);
        mMediator.resetWithListOfTabs(tabs, false);
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
        mMediator.resetWithListOfTabs(tabs, true);

        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertEquals(fetcher1, mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertNotEquals(fetcher2, mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertEquals(fetcher3, mModel.get(2).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testChangingTabModelFilters() {
        mCurrentTabModelFilterSupplier.set(mIncognitoTabGroupModelFilter);

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());

        // Not added until the next resetWithListOfTabs call.
        verify(mIncognitoTabGroupModelFilter, never()).addObserver(any());
        verify(mIncognitoTabGroupModelFilter, never()).addTabGroupObserver(any());
    }

    @Test
    public void testSpecialItemExist() {
        mMediator.resetWithListOfTabs(null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(FOR_TESTING);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.LARGE_MESSAGE, model);

        assertTrue(mModel.size() > 0);
        assertTrue(mMediator.specialItemExistsInModel(FOR_TESTING));
        assertFalse(mMediator.specialItemExistsInModel(PRICE_MESSAGE));
        assertTrue(mMediator.specialItemExistsInModel(MessageService.MessageType.ALL));
    }

    @Test
    public void testIsLastItemMessage() {
        initAndAssertAllProperties();

        assertEquals(2, mModel.size());
        assertFalse(mMediator.isLastItemMessage());

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(FOR_TESTING);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.LARGE_MESSAGE, model);

        assertEquals(3, mModel.size());
        assertFalse(mMediator.isLastItemMessage());

        mMediator.addSpecialItemToModel(mModel.size(), TabProperties.UiType.LARGE_MESSAGE, model);

        assertEquals(4, mModel.size());
        assertTrue(mMediator.isLastItemMessage());

        mMediator.removeSpecialItemFromModel(
                TabProperties.UiType.LARGE_MESSAGE, MessageService.MessageType.ALL);

        assertEquals(2, mModel.size());
        assertFalse(mMediator.isLastItemMessage());
    }

    @Test
    public void tabClosure_updatesTabGroup_inTabSwitcher() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), true);
        ThumbnailFetcher fetcherBefore = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModel.size());

        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabGroupModelFilter).tabGroupExistsForRootId(TAB1_ID);
        doReturn(false).when(mTab1).isClosing();

        mTabModelObserverCaptor.getValue().willCloseTab(tab3, true);

        assertEquals(2, mModel.size());

        ThumbnailFetcher fetcherAfter = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, not(fetcherAfter));
    }

    @Test
    public void tabClosure_doesNotUpdateTabGroup_inTabSwitcher_WhenClosing() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), true);
        ThumbnailFetcher fetcherBefore = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModel.size());

        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabGroupModelFilter).tabGroupExistsForRootId(TAB1_ID);
        doReturn(true).when(mTab1).isClosing();

        mTabModelObserverCaptor.getValue().willCloseTab(tab3, true);

        assertEquals(2, mModel.size());

        ThumbnailFetcher fetcherAfter = mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, equalTo(fetcherAfter));
    }

    @Test
    public void tabClosure_ignoresUpdateForTabGroup_outsideTabSwitcher() {
        initAndAssertAllProperties();
        TabListMediator.TabActionListener actionListenerBeforeUpdate =
                mModel.get(0).model.get(TabProperties.TAB_CLICK_LISTENER);

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        assertEquals(2, mModel.size());

        mMediator.setActionOnAllRelatedTabsForTesting(false);
        doReturn(true).when(mTabGroupModelFilter).tabGroupExistsForRootId(TAB1_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);

        assertEquals(1, mModel.size());

        TabListMediator.TabActionListener actionListenerAfterUpdate =
                mModel.get(0).model.get(TabProperties.TAB_CLICK_LISTENER);
        // The selection listener should remain unchanged, since the property model of the tab group
        // should not get updated when the closure is triggered from outside the tab switcher.
        assertThat(actionListenerBeforeUpdate, equalTo(actionListenerAfterUpdate));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PANE_ANDROID, ChromeFeatureList.DATA_SHARING})
    public void testIsTabGroup_TabSwitcher() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);

        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, false);

        assertEquals(
                TabActionButtonType.OVERFLOW,
                mModel.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
    }

    @Test
    public void testOnMenuItemClickedCallback_CloseGroupInTabSwitcher() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModel.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(TAB1_ID)).thenReturn(tabs);
        mMediator.onMenuItemClicked(R.id.close_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mTabGroupModelFilter)
                .closeTabs(TabClosureParams.closeTabs(tabs).hideTabGroups(true).build());
    }

    @Test
    public void testOnMenuItemClickedCallback_UngroupInTabSwitcher_IncognitoNoShow() {
        mCurrentTabModelFilterSupplier.set(mIncognitoTabGroupModelFilter);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mIncognitoTabModel.getCount(); i++) {
            tabs.add(mIncognitoTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModel.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mIncognitoTabGroupModelFilter.getRelatedTabListForRootId(TAB1_ID)).thenReturn(tabs);
        mMediator.onMenuItemClicked(R.id.ungroup_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mIncognitoTabGroupModelFilter)
                .moveTabOutOfGroupInDirection(TAB1_ID, /* trailing= */ true);
    }

    @Test
    public void testOnMenuItemClickedCallback_DeleteGroupInTabSwitcher_IncognitoNoShow() {
        mCurrentTabModelFilterSupplier.set(mIncognitoTabGroupModelFilter);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        when(mIncognitoTabGroupModelFilter.isIncognitoBranded()).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mIncognitoTabModel.getCount(); i++) {
            tabs.add(mIncognitoTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModel.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mIncognitoTabGroupModelFilter.getRelatedTabListForRootId(TAB1_ID)).thenReturn(tabs);
        mMediator.onMenuItemClicked(R.id.delete_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mIncognitoTabGroupModelFilter).closeTabs(TabClosureParams.closeTabs(tabs).build());
    }

    @Test
    public void testOnMenuItemClickedCallback_CloseGroupInTabSwitcher_SingleTabGroup() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModel.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getRelatedTabListForRootId(TAB1_ID)).thenReturn(tabs);
        mMediator.onMenuItemClicked(R.id.close_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mTabGroupModelFilter)
                .closeTabs(TabClosureParams.closeTabs(tabs).hideTabGroups(true).build());
    }

    @Test
    public void testQuickDeleteAnimationTabFiltering() {
        // Add five more tabs.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        Tab tab5 = prepareTab(TAB5_ID, TAB5_TITLE, TAB5_URL);
        Tab tab6 = prepareTab(TAB6_ID, TAB6_TITLE, TAB6_URL);
        Tab tab7 = prepareTab(TAB7_ID, TAB7_TITLE, TAB7_URL);
        when(mTabModel.getTabAt(4)).thenReturn(tab7);

        // Mock that tab3 and tab4 are in the same group and group root id is TAB3_ID.
        List<Tab> groupTabs1 = new ArrayList<>(Arrays.asList(tab3, tab4));
        createTabGroup(groupTabs1, TAB3_ID, TAB_GROUP_ID, 2);
        when(mTabGroupModelFilter.getTabAt(2)).thenReturn(tab3);

        // Mock that tab5 and tab6 are in the same group and group root id is TAB5_ID.
        List<Tab> groupTabs2 = new ArrayList<>(Arrays.asList(tab5, tab6));
        createTabGroup(groupTabs2, TAB5_ID, TAB_GROUP_ID, 3);
        when(mTabGroupModelFilter.getTabAt(3)).thenReturn(tab5);

        Rect tab1Rect = new Rect();
        tab1Rect.bottom = 1;
        when(mTabListRecyclerView.getRectOfCurrentThumbnail(0, TAB1_ID)).thenReturn(tab1Rect);

        Rect tab2Rect = new Rect();
        tab2Rect.bottom = 1;
        when(mTabListRecyclerView.getRectOfCurrentThumbnail(1, TAB2_ID)).thenReturn(tab2Rect);

        Rect tab3Rect = new Rect();
        tab3Rect.bottom = 2;
        when(mTabListRecyclerView.getRectOfCurrentThumbnail(2, TAB3_ID)).thenReturn(tab3Rect);

        Rect tab5Rect = new Rect();
        tab5Rect.bottom = 2;
        when(mTabListRecyclerView.getRectOfCurrentThumbnail(3, TAB5_ID)).thenReturn(tab5Rect);

        // Mock tab7 is outside the screen view.
        when(mTabListRecyclerView.getRectOfCurrentThumbnail(4, TAB7_ID)).thenReturn(null);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3, tab5, tab7));
        mMediator.resetWithListOfTabs(tabs, false);
        assertThat(mModel.size(), equalTo(5));

        TreeMap<Integer, List<Integer>> resultMap = new TreeMap<>();

        List<Tab> tabsToFade = new ArrayList<>(Arrays.asList(mTab1, tab4, tab5, tab6, tab7));

        mMediator.getOrderOfTabsForQuickDeleteAnimation(
                mTabListRecyclerView, tabsToFade, resultMap);

        assertThat(resultMap.keySet(), contains(1, 2));

        // Tab 1 and group tab 5 & 6 should be filtered for animation.
        assertThat(resultMap.get(1), contains(0));
        assertThat(resultMap.get(2), contains(3));
    }

    @Test
    public void setTabActionState_UnbindsPropertiesCorrectly() {
        when(mSelectionDelegate.isItemSelected(TAB1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(TAB2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(TAB3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> {
                            return mSelectionDelegate;
                        },
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        // Unique sets of keys for each of SELECTABLE/CLOSABLE.
        ArrayList<PropertyKey> uniqueClosableKeys =
                new ArrayList<>(Arrays.asList(TAB_GRID_CLOSABLE_KEYS));
        uniqueClosableKeys.removeAll(Arrays.asList(TAB_GRID_SELECTABLE_KEYS));

        // The test starts in the CLOSABLE state.
        PropertyModel model = mModel.get(0).model;
        // Intitially, the CLOSABLE properties should be set and the SELECTABLE properties should
        // be unset.
        Collection<PropertyKey> setProps = model.getAllSetProperties();
        assertEquals(TabActionState.CLOSABLE, model.get(TabProperties.TAB_ACTION_STATE));
        assertThat(setProps, hasItems(TAB_GRID_CLOSABLE_KEYS));

        // After the TabActionState is changed to SELECTABLE, the CLOSABLE state properties should
        // still be present but unbound.
        mMediator.setTabActionState(TabActionState.SELECTABLE);
        setProps = model.getAllSetProperties();
        assertEquals(TabActionState.SELECTABLE, model.get(TabProperties.TAB_ACTION_STATE));
        assertThat(setProps, hasItems(TAB_GRID_CLOSABLE_KEYS));
        assertThat(setProps, hasItems(TAB_GRID_SELECTABLE_KEYS));
        assertAllUnset(model, uniqueClosableKeys);

        // Switching back to CLOSABLE will unbind the SELECTABLE properties, but they will still be
        // present.
        mMediator.setTabActionState(TabActionState.CLOSABLE);
        setProps = model.getAllSetProperties();
        assertEquals(TabActionState.CLOSABLE, model.get(TabProperties.TAB_ACTION_STATE));
        assertThat(setProps, hasItems(TAB_GRID_CLOSABLE_KEYS));
        assertThat(setProps, hasItems(TAB_GRID_SELECTABLE_KEYS));
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidNotClose() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, false);

        mModel.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        mMediator.getMaybeUnsetShrinkCloseAnimationCallback(TAB1_ID).onResult(false);
        assertFalse(mModel.get(0).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_NoModels() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, false);

        mModel.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);

        var callback = mMediator.getMaybeUnsetShrinkCloseAnimationCallback(TAB1_ID);

        mMediator.resetWithListOfTabs(null, false);

        callback.onResult(true);

        assertEquals(0, mModel.size());
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_Tab1Closed() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, false);

        mModel.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        var callback = mMediator.getMaybeUnsetShrinkCloseAnimationCallback(TAB1_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, /* didCloseAlone= */ false);

        callback.onResult(true);
        assertFalse(mModel.get(0).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_TabsClosed() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, false);

        mModel.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        var callback = mMediator.getMaybeUnsetShrinkCloseAnimationCallback(TAB1_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, /* didCloseAlone= */ false);
        mTabModelObserverCaptor.getValue().willCloseTab(newTab, /* didCloseAlone= */ false);

        callback.onResult(true);

        assertEquals(0, mModel.size());
    }

    @Test
    public void testUpdateTabStripNotificationBubble_hasUpdate() {
        // Setup the test such that the tab list is strip mode, with a tab group of 2 tabs.
        setUpTabListMediator(TabListMediatorType.TAB_STRIP, TabListMode.STRIP);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, false);

        assertFalse(mModel.get(POSITION1).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));
        assertFalse(mModel.get(POSITION2).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));

        // Only pass in updates for mTab1 and leaving mTab2 untouched.
        Set<Integer> tabIdsToBeUpdated = new HashSet<>();
        tabIdsToBeUpdated.add(mTab1.getId());
        mMediator.updateTabStripNotificationBubble(tabIdsToBeUpdated, true);

        assertTrue(mModel.get(POSITION1).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));
        assertFalse(mModel.get(POSITION2).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));
    }

    @Test
    public void testUpdateTabCardLabels() {
        TabCardLabelData tabCardLabelData = mock(TabCardLabelData.class);
        Map<Integer, TabCardLabelData> dataMap = new HashMap<>();
        dataMap.put(TAB1_ID, tabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        assertEquals(
                tabCardLabelData,
                mModel.get(POSITION1).model.get(TabProperties.TAB_CARD_LABEL_DATA));
        assertNull(mModel.get(POSITION2).model.get(TabProperties.TAB_CARD_LABEL_DATA));

        dataMap.replace(TAB1_ID, null);
        dataMap.put(TAB2_ID, tabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        assertNull(mModel.get(POSITION1).model.get(TabProperties.TAB_CARD_LABEL_DATA));
        assertEquals(
                tabCardLabelData,
                mModel.get(POSITION2).model.get(TabProperties.TAB_CARD_LABEL_DATA));
    }

    private void setUpCloseButtonDescriptionString(boolean isGroup) {
        if (isGroup) {
            doAnswer(
                            invocation -> {
                                String title = invocation.getArgument(1);
                                String num = invocation.getArgument(2);
                                return String.format("Close %s group with %s tabs", title, num);
                            })
                    .when(mActivity)
                    .getString(anyInt(), anyString(), anyString());

            doAnswer(
                            invocation -> {
                                String num = invocation.getArgument(1);
                                return String.format("Close tab group with %s tabs", num);
                            })
                    .when(mActivity)
                    .getString(anyInt(), anyString());
        } else {
            doAnswer(
                            invocation -> {
                                String title = invocation.getArgument(1);
                                return String.format("Close %s tab", title);
                            })
                    .when(mActivity)
                    .getString(anyInt(), anyString());
        }
    }

    private void setUpTabGroupCardDescriptionString() {
        doAnswer(
                        invocation -> {
                            String title = invocation.getArgument(1);
                            String num = invocation.getArgument(2);
                            return String.format("Expand %s tab group with %s tabs.", title, num);
                        })
                .when(mActivity)
                .getString(anyInt(), anyString(), anyString());

        doAnswer(
                        invocation -> {
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

        int tabGroupModelFilterObserverCount =
                mTabGroupModelFilterObserverCaptor.getAllValues().size();
        int tabModelObserverCount = mTabModelObserverCaptor.getAllValues().size();

        mediator.resetWithListOfTabs(tabs, false);

        assertEquals(mTabModelObserverCaptor.getAllValues().size(), tabModelObserverCount + 1);
        assertEquals(
                mTabGroupModelFilterObserverCaptor.getAllValues().size(),
                tabGroupModelFilterObserverCount + 1);

        for (Callback<TabFavicon> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(mFavicon);
        }

        assertThat(mModel.size(), equalTo(mTabModel.getCount()));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        if (!mTabGroupModelFilter.isTabInTabGroup(mTab1)) {
            assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        }
        if (!mTabGroupModelFilter.isTabInTabGroup(mTab2)) {
            assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        }

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotNull(mModel.get(1).model.get(TabProperties.FAVICON_FETCHER));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));

        if (mMediator.getTabListModeForTesting() == TabListMode.GRID) {
            assertThat(
                    mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER),
                    instanceOf(ThumbnailFetcher.class));
            assertThat(
                    mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER),
                    instanceOf(ThumbnailFetcher.class));
        } else {
            assertNull(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
            assertNull(mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
        }

        if (mModel.get(0).model.get(TabProperties.TAB_LONG_CLICK_LISTENER) != null) return;

        assertThat(
                mModel.get(0).model.get(TabProperties.TAB_CLICK_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(
                mModel.get(1).model.get(TabProperties.TAB_CLICK_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));

        assertThat(
                mModel.get(0).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).tabActionListener,
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(
                mModel.get(1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).tabActionListener,
                instanceOf(TabListMediator.TabActionListener.class));
    }

    private Tab prepareTab(int id, String title, GURL url) {
        Tab tab = TabUiUnitTestUtils.prepareTab(id, title, url);
        when(tab.getView()).thenReturn(mock(View.class));
        doReturn(true).when(tab).isIncognito();
        when(tab.getTitle()).thenReturn(title);
        int count = mTabModel.getCount();
        doReturn(tab).when(mTabModel).getTabAt(count);
        doReturn(count).when(mTabModel).getCount();
        when(mTabModel.getTabById(id)).thenReturn(tab);
        when(mIncognitoTabModel.getTabById(id)).thenReturn(tab);
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(id)).thenReturn(1);
        doReturn(mProfile).when(tab).getProfile();
        return tab;
    }

    private SimpleRecyclerViewAdapter.ViewHolder prepareViewHolder(int id, int position) {
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                mock(SimpleRecyclerViewAdapter.ViewHolder.class);
        viewHolder.model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, id)
                        .with(CARD_TYPE, TAB)
                        .build();
        doReturn(position).when(viewHolder).getAdapterPosition();
        return viewHolder;
    }

    private RecyclerView.ViewHolder prepareFakeViewHolder(View itemView, int index) {
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(itemView) {};
        when(mRecyclerView.findViewHolderForAdapterPosition(index)).thenReturn(viewHolder);
        return viewHolder;
    }

    private TabGridItemTouchHelperCallback getItemTouchHelperCallback() {
        return (TabGridItemTouchHelperCallback) mMediator.getItemTouchHelperCallback(0f, 0f, 0f);
    }

    private void setUpTabListMediator(@TabListMediatorType int type, @TabListMode int mode) {
        setUpTabListMediator(type, mode, TabActionState.CLOSABLE);
    }

    private void setUpTabListMediator(
            @TabListMediatorType int type,
            @TabListMode int mode,
            @TabActionState int tabActionState) {
        if (mMediator != null) {
            mMediator.resetWithListOfTabs(null, false);
            mMediator.destroy();
            mMediator = null;
        }
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        doNothing().when(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());

        TabListMediator.TabGridDialogHandler handler =
                type == TabListMediatorType.TAB_GRID_DIALOG ? mTabGridDialogHandler : null;
        boolean actionOnRelatedTabs = type == TabListMediatorType.TAB_SWITCHER;
        ThumbnailProvider thumbnailProvider =
                mode == TabListMode.GRID ? getTabThumbnailCallback() : null;

        mMediator =
                new TabListMediator(
                        mActivity,
                        mModel,
                        mode,
                        mModalDialogManager,
                        mCurrentTabModelFilterSupplier,
                        thumbnailProvider,
                        mTabListFaviconProvider,
                        actionOnRelatedTabs,
                        null,
                        mGridCardOnClickListenerProvider,
                        handler,
                        null,
                        getClass().getSimpleName(),
                        tabActionState,
                        mActionConfirmationManager,
                        /* onTabGroupCreation= */ null);
        TrackerFactory.setTrackerForTests(mTracker);
        mMediator.registerOrientationListener(mGridLayoutManager);

        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
    }

    private void createTabGroup(List<Tab> tabs, int rootId, @Nullable Token tabGroupId) {
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)).thenReturn(tabs.size());
        List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabIds(tab.getId())).thenReturn(tabIds);
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
            when(tab.getRootId()).thenReturn(rootId);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
        }
    }

    private void createTabGroup(List<Tab> tabs, int rootId, @Nullable Token tabGroupId, int index) {
        when(mTabGroupModelFilter.getRelatedTabCountForRootId(rootId)).thenReturn(tabs.size());
        List<Integer> tabIds = tabs.stream().map(Tab::getId).collect(Collectors.toList());
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabIds(tab.getId())).thenReturn(tabIds);
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
            when(tab.getRootId()).thenReturn(rootId);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
            when(mTabGroupModelFilter.indexOf(tab)).thenReturn(index);
        }
    }

    private void mockOptimizationGuideResponse(
            @OptimizationGuideDecision int decision, Map<GURL, Any> responses) {
        for (Map.Entry<GURL, Any> responseEntry : responses.entrySet()) {
            doAnswer(
                            new Answer<Void>() {
                                @Override
                                public Void answer(InvocationOnMock invocation) {
                                    OptimizationGuideCallback callback =
                                            (OptimizationGuideCallback)
                                                    invocation.getArguments()[2];
                                    callback.onOptimizationGuideDecision(
                                            decision, responseEntry.getValue());
                                    return null;
                                }
                            })
                    .when(mOptimizationGuideBridge)
                    .canApplyOptimization(
                            eq(responseEntry.getKey()),
                            any(HintsProto.OptimizationType.class),
                            any(OptimizationGuideCallback.class));
        }
    }

    private void initWithThreeTabs() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, false);
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

    private ThumbnailProvider getTabThumbnailCallback() {
        return new TabContentManagerThumbnailProvider(mTabContentManager);
    }

    private static void setPriceTrackingEnabledForTesting(boolean value) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingFeatures.PRICE_DROP_IPH_ENABLED_PARAM,
                String.valueOf(value));
        FeatureList.mergeTestValues(testValues, /* replace= */ true);

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(value);
    }

    private void assertAllUnset(PropertyModel model, List<PropertyKey> keys) {
        for (PropertyKey key : keys) {
            assertUnset(model, key);
        }
    }

    /** Asserts that the given key is non-null (aka "set") in the given model. */
    private void assertSet(PropertyModel model, PropertyKey propertyKey) {
        if (propertyKey instanceof ReadableObjectPropertyKey) {
            ReadableObjectPropertyKey objectKey = (ReadableObjectPropertyKey) propertyKey;
            assertNotNull(
                    "Expected property to be set, property=" + objectKey, model.get(objectKey));
        } else {
            assert false : "Unsupported key type passed to function, add it to #assertSet";
        }
    }

    /** Asserts that the given key is null (aka "unset") in the given model. */
    private void assertUnset(PropertyModel model, PropertyKey propertyKey) {
        if (propertyKey instanceof ReadableObjectPropertyKey) {
            ReadableObjectPropertyKey objectKey = (ReadableObjectPropertyKey) propertyKey;
            assertNull(
                    "Expected property to be unset, property=" + objectKey, model.get(objectKey));
        } else {
            assert false : "Unsupported key type passed to function, add it to #assertUnset";
        }
    }

    private void setupSyncedGroup(boolean isShared) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = isShared ? COLLABORATION_ID1 : null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
    }
}
