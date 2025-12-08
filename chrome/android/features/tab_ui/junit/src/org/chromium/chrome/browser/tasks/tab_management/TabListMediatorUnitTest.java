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
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.ARCHIVED_TABS_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.FOR_TESTING;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.IPH;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.PRICE_MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.isMessageCard;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;
import android.util.Size;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

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
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
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
import org.chromium.chrome.browser.tab.Tab.MediaState;
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
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconMetadata;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUiUnitTestUtils;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarExplicitTrigger;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
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
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
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
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Tests for {@link TabListMediator}. */
@SuppressWarnings({
    "ArraysAsListWithZeroOrOneArgument",
    "ResultOfMethodCallIgnored",
    "ConstantConditions",
    "DirectInvocationOnMock"
})
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
@LooperMode(LooperMode.Mode.LEGACY)
@DisableFeatures({
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY,
})
public class TabListMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String TAB4_TITLE = "Tab4";
    private static final String TAB5_TITLE = "Tab5";
    private static final String TAB6_TITLE = "Tab6";
    private static final String TAB7_TITLE = "Tab7";
    private static final String NEW_TITLE = "New title";
    private static final String CUSTOMIZED_DIALOG_TITLE1 = "Cool Tabs";
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
    private static final String SYNC_GROUP_ID1 = "sync_group_id1";
    private static final String SYNC_GROUP_ID2 = "sync_group_id2";
    private static final @TabGroupColorId int SYNC_GROUP_COLOR1 = TabGroupColorId.BLUE;
    private static final @TabGroupColorId int SYNC_GROUP_COLOR2 = TabGroupColorId.RED;
    private static final TabListEditorItemSelectionId ITEM1_ID =
            TabListEditorItemSelectionId.createTabId(TAB1_ID);
    private static final TabListEditorItemSelectionId ITEM2_ID =
            TabListEditorItemSelectionId.createTabId(TAB2_ID);
    private static final TabListEditorItemSelectionId ITEM3_ID =
            TabListEditorItemSelectionId.createTabId(TAB3_ID);
    private static final TabListEditorItemSelectionId ITEM4_ID =
            TabListEditorItemSelectionId.createTabGroupSyncId(SYNC_GROUP_ID1);

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
                TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER,
                TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER,
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
    @Spy TabModel mTabModel;
    @Spy TabModel mIncognitoTabModel;
    @Mock TabListFaviconProvider mTabListFaviconProvider;
    @Mock TabListFaviconProvider.TabFaviconFetcher mTabFaviconFetcher;
    @Mock RecyclerView mRecyclerView;
    @Mock TabListRecyclerView mTabListRecyclerView;
    @Mock RecyclerView.Adapter mAdapter;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabGroupModelFilter mIncognitoTabGroupModelFilter;
    @Mock TabUngrouper mTabUngrouper;
    @Mock TabUngrouper mIncognitoTabUngrouper;
    @Mock TabRemover mTabRemover;
    @Mock TabRemover mIncognitoTabRemover;
    @Mock TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock TabListMediator.GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    @Mock TabFavicon mFavicon;
    @Mock Bitmap mFaviconBitmap;
    @Mock Activity mActivity;
    @Mock TabActionListener mOpenGroupActionListener;
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
    @Mock SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    @Mock ModalDialogManager mModalDialogManager;
    @Mock DataSharingTabManager mDataSharingTabManager;
    @Mock TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock IdentityServicesProvider mIdentityServicesProvider;
    @Mock IdentityManager mIdentityManager;
    @Mock TabGroupSyncService mTabGroupSyncService;
    @Mock DataSharingService mDataSharingService;
    @Mock CollaborationService mCollaborationService;
    @Mock ServiceStatus mServiceStatus;
    @Mock UndoBarExplicitTrigger mUndoBarExplicitTrigger;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor ArgumentCaptor<Callback<TabFavicon>> mCallbackCaptor;
    @Captor ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;
    @Captor ArgumentCaptor<Callback<Integer>> mActionConfirmationResultCallbackCaptor;
    @Captor ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;

    @Captor
    ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    @Captor ArgumentCaptor<RecyclerView.OnScrollListener> mOnScrollListenerCaptor;

    private final ObservableSupplierImpl<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private Tab mTab1;
    private Tab mTab2;
    private TabListMediator mMediator;
    private TabListModel mModelList;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder1;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder2;
    private RecyclerView.ViewHolder mFakeViewHolder1;
    private RecyclerView.ViewHolder mFakeViewHolder2;
    private final View mItemView1 = mock(View.class);
    private final View mItemView2 = mock(View.class);
    private PriceDrop mPriceDrop;
    private PriceTabData mPriceTabData;
    private String mTab1Domain;
    private String mTab2Domain;
    private String mTab3Domain;
    private String mNewDomain;
    private GURL mFaviconUrl;
    private Resources mResources;
    private Context mContext;
    private SavedTabGroup mSavedTabGroup1;
    private SavedTabGroup mSavedTabGroup2;

    @Before
    public void setUp() {
        UrlUtilitiesJni.setInstanceForTesting(mUrlUtilitiesJniMock);
        OptimizationGuideBridgeFactoryJni.setInstanceForTesting(
                mOptimizationGuideBridgeFactoryJniMock);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(mOptimizationGuideBridge)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfile);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);

        mResources = spy(RuntimeEnvironment.application.getResources());
        mContext = ApplicationProvider.getApplicationContext();
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getInteger(R.integer.min_screen_width_bucket)).thenReturn(1);

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
        when(mItemView1.isAttachedToWindow()).thenReturn(true);
        when(mItemView2.isAttachedToWindow()).thenReturn(true);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));
        mSavedTabGroup1 = prepareSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE, SYNC_GROUP_COLOR1, 1);
        mSavedTabGroup2 = prepareSavedTabGroup(SYNC_GROUP_ID2, "", SYNC_GROUP_COLOR2, 2);

        doNothing().when(mTabContentManager).getTabThumbnailWithCallback(anyInt(), any(), any());
        // Mock that tab restoring stage is over.
        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();
        doReturn(true).when(mIncognitoTabGroupModelFilter).isTabModelRestored();
        doReturn(mProfile).when(mTabModel).getProfile();

        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mIncognitoTabGroupModelFilter.getTabUngrouper()).thenReturn(mIncognitoTabUngrouper);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mIncognitoTabModel.getTabRemover()).thenReturn(mIncognitoTabRemover);
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(mIncognitoTabModel).when(mIncognitoTabGroupModelFilter).getTabModel();
        mCurrentTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
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
        when(mTabModel.iterator()).thenAnswer(invocation -> List.of(mTab1, mTab2).iterator());
        doReturn(2).when(mTabModel).getCount();
        when(mIncognitoTabModel.iterator())
                .thenAnswer(invocation -> List.of(mTab1, mTab2).iterator());
        doReturn(2).when(mIncognitoTabModel).getCount();
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForTabAsync(any(TabFaviconMetadata.class), mCallbackCaptor.capture());
        doReturn(mFavicon)
                .when(mTabListFaviconProvider)
                .getFaviconFromBitmap(any(Bitmap.class), any(GURL.class));
        doNothing().when(mTabFaviconFetcher).fetch(mCallbackCaptor.capture());
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getDefaultFaviconFetcher(anyBoolean());
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getFaviconForTabFetcher(any(Tab.class));
        doReturn(mTabFaviconFetcher)
                .when(mTabListFaviconProvider)
                .getFaviconFromBitmapFetcher(any(Bitmap.class), any(GURL.class));
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(mOpenGroupActionListener)
                .when(mGridCardOnClickListenerProvider)
                .openTabGridDialog(any(Tab.class));
        doReturn(mOpenGroupActionListener)
                .when(mGridCardOnClickListenerProvider)
                .openTabGridDialog(anyString());
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
        doReturn(mSavedTabGroup1).when(mTabGroupSyncService).getGroup(SYNC_GROUP_ID1);
        doReturn(mSavedTabGroup2).when(mTabGroupSyncService).getGroup(SYNC_GROUP_ID2);

        mModelList = new TabListModel();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        doAnswer(
                        invocation -> {
                            int position = invocation.getArgument(0);
                            @UiType int itemType = mModelList.get(position).type;
                            if (isMessageCard(itemType)) {
                                return mGridLayoutManager.getSpanCount();
                            }
                            return 1;
                        })
                .when(mSpanSizeLookup)
                .getSpanSize(anyInt());

        doAnswer(
                        invocation -> {
                            Token tabGroupId = invocation.getArgument(0);
                            String title = invocation.getArgument(1);
                            when(mTabGroupModelFilter.getTabGroupTitle(tabGroupId))
                                    .thenReturn(title);
                            return null;
                        })
                .when(mTabGroupModelFilter)
                .setTabGroupTitle(any(), anyString());
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void resetWithNullTabs() {
        mMediator.resetWithListOfTabs(null, null, false);

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());
    }

    @Test
    public void updatesTitle_WithoutStoredTitle_Tab() {
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        when(mTab1.getTitle()).thenReturn(NEW_TITLE);
        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(NEW_TITLE));
    }

    @Test
    public void updatesTitle_WithoutStoredTitle_TabGroup() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        String defaultTitle = TabGroupTitleUtils.getDefaultTitle(mActivity, tabs.size());
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(defaultTitle));
    }

    @Test
    public void updatesTitle_WithStoredTitle_TabGroup() {
        // Mock that tab1 and new tab are in the same group with root ID as TAB1_ID.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);

        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_Tab() {
        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);

        // Ignored as the tab is not in a group.
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_Empty() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, "");
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), "");
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo("2 tabs"));
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Tab() {
        var oldFaviconFetcher = mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER);
        mTabGroupModelFilter.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getTabGroupId(), TabGroupColorId.BLUE);

        // Ignored as the tab is not in a group.
        assertEquals(oldFaviconFetcher, mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNull(mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Group_Grid() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getTabGroupId(), TabGroupColorId.BLUE);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        var provider = mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Reset() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getTabGroupId(), TabGroupColorId.BLUE);

        PropertyModel model = mModelList.get(0).model;
        var provider = spy(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mMediator.resetWithListOfTabs(null, null, false);
        verify(provider).destroy();
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Remove() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupColor(mTab1.getTabGroupId(), TabGroupColorId.BLUE);

        PropertyModel model = mModelList.get(0).model;
        var provider = spy(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mModelList.removeAt(0);
        verify(provider).destroy();
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Ungroup() {
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        PropertyModel model = mModelList.get(0).model;
        var provider = mock(TabGroupColorViewProvider.class);
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertNull(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        verify(provider).destroy();
    }

    @Test
    public void updatesFaviconFetcher_SingleTab_Gts() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        TabListFaviconProvider.TabFavicon[] favicon = new TabListFaviconProvider.TabFavicon[1];
        mModelList
                .get(0)
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
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        createTabGroup(Arrays.asList(mTab1), TAB1_ID, TAB_GROUP_ID);

        var oldThumbnailFetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotEquals(
                oldThumbnailFetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_SingleTab_NonGts() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        TabListFaviconProvider.TabFavicon[] favicon = new TabListFaviconProvider.TabFavicon[1];
        mModelList
                .get(0)
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
        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        // Assert that tab1 is in a tab group.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        createTabGroup(Arrays.asList(mTab1, newTab), TAB1_ID, TAB_GROUP_ID);

        var oldThumbnailFetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotEquals(
                oldThumbnailFetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpSameDocument() {
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(navigationHandle.isSameDocument()).thenReturn(true);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpSameUrl() {
        doReturn(mFavicon).when(mTabListFaviconProvider).getDefaultFavicon(anyBoolean());

        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        when(navigationHandle.getUrl()).thenReturn(TAB1_URL);
        when(navigationHandle.isSameDocument()).thenReturn(false);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
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
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
        verify(newTab).addObserver(mTabObserverCaptor.getValue());

        mModelList.get(2).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(2).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(newTab, navigationHandle);
        assertNull(mModelList.get(2).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        NavigationHandle navigationHandle = mock(NavigationHandle.class);

        when(navigationHandle.isSameDocument()).thenReturn(false);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpTabGroup() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);

        NavigationHandle navigationHandle = mock(NavigationHandle.class);

        when(navigationHandle.isSameDocument()).thenReturn(false);
        when(navigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, navigationHandle);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void sendsSelectSignalCorrectly() {
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView2,
                        mModelList.get(1).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        verify(mGridCardOnClickListenerProvider)
                .onTabSelecting(mModelList.get(1).model.get(TabProperties.TAB_ID), true);
    }

    @Test
    public void sendsOpenGroupSignalCorrectly_SingleTabGroup() {
        List<Tab> tabs = Arrays.asList(mTab1);
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1, mTab2), null, false);
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        verify(mOpenGroupActionListener).run(mItemView1, TAB1_ID, /* triggeringMotion= */ null);
    }

    @Test
    public void sendsOpenGroupSignalCorrectly_TabGroup() {
        List<Tab> tabs = Arrays.asList(mTab1, mTab2);
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1, mTab2), null, false);
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        verify(mOpenGroupActionListener).run(mItemView1, TAB1_ID, /* triggeringMotion= */ null);
    }

    @Test
    public void sendsCloseSignalCorrectly() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView2,
                        mModelList.get(1).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        TabClosureParams params = TabClosureParams.closeTab(mTab2).allowUndo(true).build();
        verify(mTabRemover)
                .closeTabs(
                        eq(params),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertTrue(mModelList.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));

        when(mTabGroupModelFilter.getRelatedTabList(anyInt())).thenReturn(new ArrayList<>());
        TabModelActionListener listener = mTabModelActionListenerCaptor.getValue();
        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_POSITIVE);
        assertTrue(mModelList.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));

        listener.onConfirmationDialogResult(
                DialogType.SYNC, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        assertFalse(mModelList.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void sendsCloseSignalCorrectly_TriggeringMotionFromMouse_DisallowUndo() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView2,
                        mModelList.get(1).model.get(TabProperties.TAB_ID),
                        MotionEventTestUtils.createMouseMotionInfo(
                                /* downTime= */ SystemClock.uptimeMillis(),
                                /* eventTime= */ SystemClock.uptimeMillis() + 200,
                                MotionEvent.ACTION_UP));

        verify(mTabRemover)
                .closeTabs(
                        eq(TabClosureParams.closeTab(mTab2).allowUndo(false).build()),
                        /* allowDialog= */ eq(true),
                        /* listener= */ any());
    }

    @Test
    public void sendsCloseSignalCorrectly_ActionOnAllRelatedTabs() {
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView2,
                        mModelList.get(1).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs.get(0) == mTab2),
                        /* allowDialog= */ eq(true),
                        any());
    }

    @Test
    public void sendsCloseSignalCorrectly_Incognito() {
        mMediator.setActionOnAllRelatedTabsForTesting(false);
        when(mTabModel.isIncognito()).thenReturn(true);
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView2,
                        mModelList.get(1).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs.get(0) == mTab2),
                        /* allowDialog= */ eq(true),
                        any());
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithGroup() {
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabGroupModelFilter).moveRelatedTabs(eq(TAB1_ID), eq(1));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithinGroup() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        getItemTouchHelperCallback().onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(1));
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
        when(itemView3.isAttachedToWindow()).thenReturn(true);
        when(itemView4.isAttachedToWindow()).thenReturn(true);

        RecyclerView.ViewHolder fakeViewHolder3 = prepareFakeViewHolder(itemView3, 2);
        RecyclerView.ViewHolder fakeViewHolder4 = prepareFakeViewHolder(itemView4, 3);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3, tab4));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(4));

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
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(3));
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, 0);
        fakeViewHolder3 = prepareFakeViewHolder(itemView3, 1);
        fakeViewHolder4 = prepareFakeViewHolder(itemView4, 2);

        // Merge 4 to 3.
        when(mTabGroupModelFilter.getRepresentativeTabAt(1)).thenReturn(tab3);
        when(mTabGroupModelFilter.getRepresentativeTabAt(2)).thenReturn(tab4);
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
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(tab4, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(2));
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, 0);
        fakeViewHolder3 = prepareFakeViewHolder(itemView3, 1);

        // Merge 3 to 1.
        when(mTabGroupModelFilter.getRepresentativeTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getRepresentativeTabAt(1)).thenReturn(tab3);
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
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(tab3, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(1));
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

        verify(mTabUngrouper)
                .ungroupTabs(List.of(mTab1), /* trailing= */ true, /* allowDialog= */ true);
        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void tabClosure() {
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);

        verify(mTab2).removeObserver(any());
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabRemoval() {
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().tabRemoved(mTab2);

        verify(mTab2).removeObserver(any());
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabClosure_IgnoresUpdatesForTabsOutsideOfModel() {
        mTabModelObserverCaptor
                .getValue()
                .didRemoveTabForClosure(prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL));

        assertThat(mModelList.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Restore_SyncingTabListModelWithTabModel() {
        // Mock that tab1 and tab2 are in the same group, and they are being restored. The
        // TabListModel has been cleaned out before the restoring happens. This case could happen
        // within a incognito tab group when user switches between light/dark mode.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(1).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        mModelList.clear();

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab2,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        assertThat(mModelList.size(), equalTo(0));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab1,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        assertThat(mModelList.size(), equalTo(1));
    }

    @Test
    public void tabAddition_Gts() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_TabGridDialog_delayAdd() {
        mMediator.setComponentNameForTesting(TabGridDialogCoordinator.COMPONENT_NAME_PREFIX);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModelList.size(), equalTo(2));

        // Add tab marked as delayed.
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_GROUP_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        true);

        // Verify tab did not get added and delayed tab is captured.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mMediator.getTabToAddDelayedForTesting(), equalTo(newTab));

        // Select delayed tab.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabSelectionType.FROM_USER, mTab1.getId());
        // Assert old tab is still marked as selected.
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));

        when(mTabModel.iterator())
                .thenAnswer(invocation -> List.of(mTab1, mTab2, newTab).iterator());
        when(mTabModel.getTabAt(2)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(3);

        // Hide dialog to complete and ensure the delayed tab is not added.
        resetWithNullTabs();
        mMediator.postHiding();
        // Assert tab was not added.
        assertThat(mModelList.size(), equalTo(0));
    }

    @Test
    public void tabAddition_Gts_delayAdd() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModelList.size(), equalTo(2));

        // Add tab marked as delayed
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        true);

        // Verify tab did not get added and delayed tab is captured.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mMediator.getTabToAddDelayedForTesting(), equalTo(newTab));

        // Select delayed tab
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabSelectionType.FROM_USER, mTab1.getId());
        // Assert old tab is still marked as selected
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));

        when(mTabModel.iterator())
                .thenAnswer(invocation -> List.of(mTab1, mTab2, newTab).iterator());
        when(mTabModel.getTabAt(2)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(3);

        // Hide GTS to complete tab addition and selection
        mMediator.postHiding();
        // Assert tab added and selected. Assert old tab is de-selected.
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(true));
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
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(Arrays.asList(mTab1)).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(Arrays.asList(mTab2)).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        assertEquals(2, mModelList.size());

        // Add tab marked as delayed.
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        true);

        // Verify tab did not get added and delayed tab is captured.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mMediator.getTabToAddDelayedForTesting(), equalTo(newTab));

        // Select delayed tab.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabSelectionType.FROM_USER, mTab2.getId());
        // Assert old tab is still marked as selected.
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Remove the first two tabs.
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        when(mTabModel.getTabAt(0)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.iterator()).thenAnswer(invocation -> List.of(newTab).iterator());
        when(mTabGroupModelFilter.getRepresentativeTabAt(0)).thenReturn(newTab);
        when(mTabGroupModelFilter.getRepresentativeTabAt(1)).thenReturn(null);
        when(mTabGroupModelFilter.getRepresentativeTabAt(2)).thenReturn(null);
        when(mTabGroupModelFilter.getIndividualTabAndGroupCount()).thenReturn(1);

        // Hide GTS to complete tab addition and selection.
        mMediator.postHiding();
        // Assert tab added and selected. Assert old tab is de-selected.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
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
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(Arrays.asList(mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB3_ID));
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_TAB_SWITCHER_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Gts_Middle() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_End() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.iterator())
                .thenAnswer(invocation -> List.of(mTab1, mTab2, newTab).iterator());
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Middle() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.iterator())
                .thenAnswer(invocation -> List.of(mTab1, newTab, mTab2).iterator());
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, newTab, mTab2))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
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
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(2));
    }

    @Test
    public void tabSelection() {
        PropertyModel model0 = mModelList.get(0).model;
        PropertyModel model1 = mModelList.get(1).model;
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

        assertEquals(2, mModelList.size());
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

        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Select tab 3 although the represenative tab 2 should update.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);
    }

    // Regression test for: crbug.com/349773923.
    @Test
    public void tabSelection_LeaveGroupClears() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, newTab));
        createTabGroup(tabs, TAB2_ID, TAB_GROUP_ID);

        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Select tab 3 although the represenative tab 2 should update.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(newTab, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);

        tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        tab2Fetcher = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Select tab 1 again and the other group should unselect.
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab1, TabLaunchType.FROM_CHROME_UI, TAB3_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER), tab1Fetcher);
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER), tab2Fetcher);
    }

    @Test
    public void tabSelection_updatePreviousSelectedTabThumbnailFetcher() {
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        mGridCardOnClickListenerProvider,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
        // mTabModelObserverCaptor captures on every resetWithListOfTabs call.
        verify(mTabGroupModelFilter, times(2)).addObserver(mTabModelObserverCaptor.capture());

        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertNotEquals(tab1Fetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(tab2Fetcher, mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabClosureUndone() {
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabClosureUndone_SingleTabGroup() {
        assertThat(mModelList.size(), equalTo(2));

        createTabGroup(Arrays.asList(mTab2), TAB2_ID, TAB_GROUP_ID);

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(
                mModelList.get(1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void testCloseTabInGroup_withArchivedTabsMessagePresent() {
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        when(mTabGroupModelFilter.tabGroupExists(any())).thenReturn(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        assertThat(mModelList.size(), equalTo(2));

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(ARCHIVED_TABS_MESSAGE);
        mMediator.addSpecialItemToModel(0, UiType.ARCHIVED_TABS_MESSAGE, model);
        assertThat(mModelList.size(), equalTo(3));

        // This crashed previously when it tried to update the message instead of the tab group
        // (crbug.com/347970497).
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(newTab);
        verify(model, times(0)).set(eq(TabProperties.TAB_ID), anyInt());
    }

    @Test
    public void tabMergeIntoGroup() {
        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);

        // Assume that reset in TabGroupModelFilter is finished.
        createTabGroup(Arrays.asList(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(POSITION2));
        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotNull(mModelList.get(1).model.get(TabProperties.FAVICON_FETCHER));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab1, /* isDestinationTab= */ true);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo("2 tabs"));
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void tabMergeIntoGroup_Parity() {
        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);

        // Assume that reset in TabGroupModelFilter is finished.
        createTabGroup(Arrays.asList(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(POSITION2));
        var oldFetcher = mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER);
        assertNotNull(oldFetcher);
        assertNotNull(mModelList.get(1).model.get(TabProperties.FAVICON_FETCHER));

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab1, /* isDestinationTab= */ true);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        var newFetcher = mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER);
        assertNull(newFetcher);

        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
    }

    @Test
    public void tabMergeIntoGroup_Dialog() {
        createTabGroup(List.of(mTab1), TAB1_ID, TAB_GROUP_ID);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1), null, false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(List.of(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);

        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        verify(mTabGridDialogHandler).updateDialogContent(TAB1_ID);
    }

    @Test
    public void tabMergeIntoGroup_Dialog_NoOp() {
        createTabGroup(List.of(mTab1), TAB1_ID, TAB_GROUP_ID);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        mMediator.resetWithListOfTabs(Arrays.asList(mTab1), null, false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(List.of(mTab2), TAB2_ID, new Token(7, 9));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        verify(mTabGridDialogHandler, never()).updateDialogContent(TAB1_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Gts_Moved_Tab_Selected_GetsFavicon() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        // Assume that TabGroupModelFilter is already updated.
        when(mTabModel.index()).thenReturn(POSITION2);
        when(mTabModel.getTabAt(POSITION2)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getRepresentativeTabAt(POSITION1)).thenReturn(mTab2);
        when(mTabGroupModelFilter.representativeIndexOf(mTab2)).thenReturn(POSITION1);
        when(mTabGroupModelFilter.getRepresentativeTabAt(POSITION2)).thenReturn(mTab1);
        when(mTabGroupModelFilter.representativeIndexOf(mTab1)).thenReturn(POSITION2);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabGroupModelFilter.getIndividualTabAndGroupCount()).thenReturn(2);
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTab2.getRootId()).thenReturn(TAB2_ID);
        when(mTabGroupModelFilter.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab2));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo("1 tab"));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotNull(mModelList.get(1).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_Moved_Tab_Selected() {
        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2));
        mMediator.resetWithListOfTabs(tabs, null, false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(mTab1);
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_Origin_Tab_Selected() {
        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_LastTab() {
        // Assume that tab1 is a single tab group that became a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);
        doReturn(1).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // These properties should get reset.
        mModelList.get(0).model.set(TabProperties.TITLE, CUSTOMIZED_DIALOG_TITLE1);
        ThumbnailFetcher fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Ungroup the single tab.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertNotEquals(fetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMoveOutOfGroup_Gts_TabAdditionWithSameId() {
        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();

        // The ungroup will add tab1 to the TabListModel at index 0. Note that before this addition,
        // there is the PropertyModel represents the group with the same id at the same index. The
        // addition should still take effect in this case.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    public void testShoppingFetcherActiveForForUngroupedTabs() {
        prepareForPriceDrop();
        resetWithRegularTabs(false);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(
                mModelList.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(
                mModelList.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
    }

    @Test
    public void testShoppingFetcherInactiveForForGroupedTabs() {
        prepareForPriceDrop();
        resetWithRegularTabs(true);

        assertThat(mModelList.size(), equalTo(2));
        assertNull(mModelList.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModelList.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
    }

    @Test
    public void testShoppingFetcherGroupedThenUngrouped() {
        prepareForPriceDrop();
        resetWithRegularTabs(true);

        assertThat(mModelList.size(), equalTo(2));
        assertNull(mModelList.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModelList.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        resetWithRegularTabs(false);
        assertThat(mModelList.size(), equalTo(2));
        assertThat(
                mModelList.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(
                mModelList.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
    }

    @Test
    public void testShoppingFetcherUngroupedThenGrouped() {
        prepareForPriceDrop();
        resetWithRegularTabs(false);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(
                mModelList.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        assertThat(
                mModelList.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER),
                instanceOf(TabListMediator.ShoppingPersistedTabDataFetcher.class));
        resetWithRegularTabs(true);
        assertThat(mModelList.size(), equalTo(2));
        assertNull(mModelList.get(0).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
        assertNull(mModelList.get(1).model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
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
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
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
        mMediator.resetWithListOfTabs(tabs, null, false);
    }

    @Test
    public void tabMoveOutOfGroup_Dialog() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler).updateDialogContent(TAB2_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Dialog_LastTab() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);
        doReturn(1).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Ungroup the single tab.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        verify(mTabGridDialogHandler).updateDialogContent(Tab.INVALID_TAB_ID);
    }

    @Test
    public void tabMoveOutOfGroup_Strip() {
        setUpTabListMediator(TabListMediatorType.TAB_STRIP, TabListMode.GRID);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler, never()).updateDialogContent(anyInt());
    }

    @Test
    public void tabMoveOutOfGroup_Strip_Undo() {
        setUpTabListMediator(TabListMediatorType.TAB_STRIP, TabListMode.GRID);

        // Setup the same as tabMoveOutOfGroup_Strip.
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler, never()).updateDialogContent(anyInt());

        // Pretend we grouped tab 1 with tab 2, but the reset already occurred so we are just
        // showing tab 1. Now we need to make sure that removing tab 1 from the group (which is
        // already showing) no-ops.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler, never()).updateDialogContent(anyInt());
    }

    @Test
    public void tabMovementWithGroup_Forward() {
        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithGroup_Backward() {
        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabGridDialog_Forward() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB_GROUP_ID).when(mTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mTab2).getTabGroupId();

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabGridDialog_Backward() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB_GROUP_ID).when(mTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mTab2).getTabGroupId();

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_Forward() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs with groups (mTab1, mTab2) and tab3.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB3_ID).when(tab3).getRootId();
        doReturn(TAB_GROUP_ID).when(mTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mTab2).getTabGroupId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Select tab3 so the group doesn't have the selected tab.
        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(tab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Assume that moveTab in TabModel is finished (swap mTab1 and mTab2).
        group = new ArrayList<>(Arrays.asList(mTab2, mTab1));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        // mTab1 is first in group before the move.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        // mTab1 is still first in group after the move (last selected), but the thumbnail updated.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        // TODO(crbug.com/40242432): Make this an assertion and don't update.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab2, tab3. Update is precautionary.
        assertNotEquals(tab1Fetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_Backward() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs with groups (mTab1, mTab2) and tab3.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB3_ID).when(tab3).getRootId();
        doReturn(TAB_GROUP_ID).when(mTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mTab2).getTabGroupId();

        // Select tab3 so the group doesn't have the selected tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        doReturn(2).when(mTabModel).index();
        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(tab3, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));

        // Assume that moveTab in TabModel is finished (swap mTab1 and mTab2).
        group = new ArrayList<>(Arrays.asList(mTab2, mTab1));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);

        // mTab1 is first in group before the move.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        // mTab1 is first in group after the move (last selected), but the thumbnail updated.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        // TODO(crbug.com/40242432): Make this an assertion and don't update.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab2, tab3. Update is precautionary.
        assertNotEquals(tab1Fetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_SelectedNotMoved() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs grouped together.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB1_ID).when(tab3).getRootId();
        doReturn(TAB_GROUP_ID).when(mTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mTab2).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(tab3).getTabGroupId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(1));

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
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

        // mTab1 selected before update.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(mTab2, POSITION2, 2);

        // mTab1 still selected after the move (last selected), but the thumbnail updated.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        // TODO(crbug.com/40242432): Make this an assertion.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab3, tab2.
        assertNotEquals(tab1Fetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void tabMovementWithinGroup_TabSwitcher_SelectedMoved() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);

        // Setup three tabs grouped together.
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();
        doReturn(TAB1_ID).when(tab3).getRootId();
        doReturn(TAB_GROUP_ID).when(mTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mTab2).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(tab3).getTabGroupId();

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(1));

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
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(group).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

        // mTab1 selected before update.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        ThumbnailFetcher tab1Fetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveWithinGroup(mTab1, 2, POSITION1);

        // mTab1 still selected after the move (last selected), but the thumbnail updated.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        // TODO(crbug.com/40242432): Make this an assertion.
        // Thumbnail order was: tab1, tab2, tab3. Now: tab1, tab3, tab2.
        assertNotEquals(tab1Fetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void undoGrouped_One_Adjacent_Tab() {
        // Assume there are 3 tabs in TabModel, mTab2 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Assume undo grouping mTab2 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(2);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoForwardGrouped_One_Tab() {
        // Assume there are 3 tabs in TabModel, tab3 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Assume undo grouping tab3 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(tab3);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(tab3, POSITION1);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoBackwardGrouped_One_Tab() {
        // Assume there are 3 tabs in TabModel, mTab1 just grouped with mTab2;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Assume undo grouping mTab1 from mTab2.
        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(2);
        doReturn(2).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(mTab1);

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(2));
    }

    @Test
    public void undoForwardGrouped_BetweenGroups() {
        // Assume there are 3 tabs in TabModel, tab3, tab4, just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabModel.iterator())
                .thenAnswer(invocation -> List.of(mTab1, mTab2, tab3, tab4).iterator());
        doReturn(4).when(mTabModel).getCount();
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(1));

        // Assume undo grouping tab3 with mTab1.
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();

        // Undo tab 3.
        List<Tab> relatedTabs = Arrays.asList(tab3);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(tab4).when(mTabModel).getTabAt(2);
        doReturn(tab3).when(mTabModel).getTabAt(3);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(tab4);
        doReturn(0).when(mTabModel).indexOf(mTab1);
        doReturn(1).when(mTabModel).indexOf(mTab2);
        doReturn(2).when(mTabModel).indexOf(tab4);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(tab3);
        doReturn(3).when(mTabModel).indexOf(tab3);
        doReturn(false).when(mTabGroupModelFilter).isTabInTabGroup(tab3);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab4);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(tab3, POSITION1);
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(-1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB4_ID), equalTo(-1));

        // Undo tab 4
        relatedTabs = Arrays.asList(tab3, tab4);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(tab4);
        doReturn(2).when(mTabModel).indexOf(tab3);
        doReturn(3).when(mTabModel).indexOf(tab4);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab3);
        doReturn(true).when(mTabGroupModelFilter).isTabInTabGroup(tab4);
        doReturn(tab3).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(tab3).when(mTabModel).getTabAt(2);
        doReturn(tab4).when(mTabModel).getTabAt(3);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        doReturn(relatedTabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);
        when(tab4.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabGroupModelFilter.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(2);
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(tab4, POSITION1);
        assertThat(mModelList.size(), equalTo(2));

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(tab4, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(-1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB4_ID), equalTo(-1));
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
    public void getLatestTitle_NoTitleUrlFallback() {
        assertEquals(TAB1_TITLE,
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true));

        when(mTab1.getTitle()).thenReturn("");
        assertEquals(TAB1_URL.getSpec(),
                mMediator.getLatestTitleForTab(mTab1, /* useDefault= */ true));
    }

    @Test
    public void getLatestTitle_NotGts() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        createTabGroup(Collections.singletonList(mTab1), TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabGroupModelFilter.getTabGroupTitle(mTab1.getTabGroupId()),
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
        createTabGroup(Collections.singletonList(mTab1), TAB1_ID, TAB_GROUP_ID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabGroupModelFilter.getTabGroupTitle(mTab1.getTabGroupId()),
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
        createTabGroup(Collections.singletonList(mTab1), TAB1_ID, TAB_GROUP_ID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabGroupModelFilter.getTabGroupTitle(mTab1.getTabGroupId()),
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
        createTabGroup(Collections.singletonList(mTab1), TAB1_ID, TAB_GROUP_ID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabGroupModelFilter.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabGroupModelFilter.getTabGroupTitle(mTab1.getTabGroupId()),
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
        String targetString = "Expand Cool Tabs tab group with 2 tabs, color Grey.";
        assertThat(mModelList.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Mock that tab1 and newTab are in the same group and group root id is TAB1_ID.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);

        assertThat(
                mModelList.get(POSITION1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(
                mModelList
                        .get(POSITION1)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));
    }

    @Test
    public void updateTabGroupTitle_SingleTab_Gts() {
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand Cool Tabs tab group with 1 tab, color Grey.";
        assertThat(mModelList.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(Arrays.asList(mTab1), TAB1_ID, TAB_GROUP_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);

        assertThat(
                mModelList.get(POSITION1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(
                mModelList
                        .get(POSITION1)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));
    }

    @Test
    public void tabGroupTitleEditor_storeTitle() {
        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        verify(mTabGroupModelFilter).setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
    }

    @Test
    public void tabGroupTitleEditor_deleteTitle() {
        mTabGroupModelFilter.deleteTabGroupTitle(TAB_GROUP_ID);
        verify(mTabGroupModelFilter).deleteTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void addSpecialItem() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, model);

        assertTrue(!mModelList.isEmpty());
        assertEquals(UiType.PRICE_MESSAGE, mModelList.get(0).type);
    }

    @Test
    public void addSpecialItem_notPersistOnReset() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, model);
        assertEquals(UiType.PRICE_MESSAGE, mModelList.get(0).type);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabs, null, /* quickMode= */ false);
        assertThat(mModelList.size(), equalTo(2));
        assertNotEquals(UiType.PRICE_MESSAGE, mModelList.get(0).type);
        assertNotEquals(UiType.PRICE_MESSAGE, mModelList.get(1).type);

        mMediator.addSpecialItemToModel(1, UiType.PRICE_MESSAGE, model);
        assertThat(mModelList.size(), equalTo(3));
        assertEquals(UiType.PRICE_MESSAGE, mModelList.get(1).type);
    }

    @Test
    public void addSpecialItem_withoutTabListModelProperties() {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        mMediator.resetWithListOfTabs(null, null, false);

        try {
            mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, new PropertyModel());
        } catch (AssertionError e) {
            return;
        }
        fail("PropertyModel#validateKey() assert should have failed.");
    }

    @Test
    public void removeSpecialItem_Message() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        @MessageType int expectedMessageType = IPH;
        @MessageType int wrongMessageType = PRICE_MESSAGE;
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        when(model.containsKeyEqualTo(MESSAGE_TYPE, IPH)).thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.IPH_MESSAGE, model);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, wrongMessageType);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, expectedMessageType);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void removeSpecialItem_Message_PriceMessage() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        @MessageType int expectedMessageType = PRICE_MESSAGE;
        @MessageType int wrongMessageType = IPH;
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        when(model.containsKeyEqualTo(MESSAGE_TYPE, PRICE_MESSAGE)).thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, model);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, wrongMessageType);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.PRICE_MESSAGE, expectedMessageType);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void removeSpecialItem_Message_CustomMessage() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        @MessageType int expectedMessageType = ARCHIVED_TABS_MESSAGE;
        @MessageType int wrongMessageType = IPH;
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        when(model.containsKeyEqualTo(MESSAGE_TYPE, ARCHIVED_TABS_MESSAGE)).thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.ARCHIVED_TABS_MESSAGE, model);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, wrongMessageType);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.ARCHIVED_TABS_MESSAGE, expectedMessageType);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testUrlUpdated_forSingleTab_Gts() {
        assertNotEquals(mNewDomain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        doReturn(new GURL(NEW_URL)).when(mTab1).getUrl();

        PropertyModel model1 = mModelList.get(POSITION1).model;
        var oldThumbnailFetcher = model1.get(TabProperties.THUMBNAIL_FETCHER);
        // Set to null to see if an update happens.
        model1.set(TabProperties.FAVICON_FETCHER, null);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, model1.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        assertNotEquals(oldThumbnailFetcher, model1.get(TabProperties.THUMBNAIL_FETCHER));
        assertNotNull(model1.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUrlUpdated_forGroup_Gts() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(
                mTab1Domain + ", " + mTab2Domain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        // Update URL_DOMAIN for mTab1.
        doReturn(new GURL(NEW_URL)).when(mTab1).getUrl();
        var oldFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(
                mNewDomain + ", " + mTab2Domain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        var newFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);

        // Update URL_DOMAIN for mTab2.
        doReturn(new GURL(NEW_URL)).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(
                mNewDomain + ", " + mNewDomain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        var newestFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(newFetcher, newestFetcher);
    }

    @Test
    public void testUrlUpdated_forGroup_Dialog() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab2);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        verify(mTab2, times(1)).addObserver(mTabObserverCaptor.getValue());

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(mTab1Domain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());

        doReturn(mNewDomain)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());
        var oldFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Update URL_DOMAIN for mTab1.
        doReturn(new GURL(NEW_URL)).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        var newFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);

        oldFetcher = mModelList.get(POSITION2).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Update URL_DOMAIN for mTab2.
        doReturn(new GURL(NEW_URL)).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(mNewDomain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mNewDomain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));

        newFetcher = mModelList.get(POSITION2).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);
    }

    @Test
    public void testUrlUpdated_forUngroup() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(
                mTab1Domain + ", " + mTab2Domain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        // Assume that TabGroupModelFilter is already updated.
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));
        when(mTabGroupModelFilter.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        when(mTab2.getTabGroupId()).thenReturn(null);
        doReturn(1).when(mTabGroupModelFilter).getTabCountForGroup(TAB_GROUP_ID);
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();

        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);
        assertEquals(mTab1Domain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
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
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

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

        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    public void testPerformAccessibilityAction_defaultAccessibilityAction() {
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

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
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

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

        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    public void testTabObserverRemovedFromClosedTab() {
        initAndAssertAllProperties();

        assertThat(mModelList.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void testTabObserverReattachToUndoClosedTab() {
        initAndAssertAllProperties();
        // Called twice in test set up due to reset with list & adding tab to model.
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());

        assertThat(mModelList.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);
        assertThat(mModelList.size(), equalTo(1));
        verify(mTab2).removeObserver(any());

        // Assume that TabGroupModelFilter is already updated to reflect closed tab is undone.
        doReturn(2).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);
        assertThat(mModelList.size(), equalTo(2));
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

        boolean showQuickly = mMediator.resetWithListOfTabs(tabs, null, /* quickMode= */ false);
        assertThat(showQuickly, equalTo(true));

        // Create a PropertyModel that is not a tab and add it to the existing TabListModel.
        PropertyModel propertyModel = mock(PropertyModel.class);
        when(propertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(mModelList.size(), UiType.IPH_MESSAGE, propertyModel);
        assertThat(mModelList.size(), equalTo(tabs.size() + 1));

        // TabListModel unchange check should ignore the non-Tab item.
        showQuickly = mMediator.resetWithListOfTabs(tabs, null, /* quickMode= */ false);
        assertThat(showQuickly, equalTo(true));
    }

    // TODO(crbug.com/40168614): the assertThat in fetch callback is never reached.
    @Test
    public void testPriceTrackingProperty() {
        setPriceTrackingEnabledForTesting(true);
        for (boolean signedInAndSyncEnabled : new boolean[] {false, true}) {
            for (boolean priceTrackingEnabled : new boolean[] {false, true}) {
                for (boolean incognito : new boolean[] {false, true}) {
                    TabListMediator mediatorSpy = spy(mMediator);
                    doReturn(false).when(mediatorSpy).isTabInTabGroup(any());
                    PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(
                            signedInAndSyncEnabled);
                    PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                            PriceTrackingUtilities.TRACK_PRICES_ON_TABS, priceTrackingEnabled);
                    Map<GURL, Any> responses = new HashMap<>();
                    responses.put(TAB1_URL, ANY_BUYABLE_PRODUCT_INITIAL);
                    responses.put(TAB2_URL, ANY_EMPTY);
                    mockOptimizationGuideResponse(OptimizationGuideDecision.TRUE, responses);
                    PersistedTabDataConfiguration.setUseTestConfig(true);
                    initAndAssertAllProperties(mediatorSpy);
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

                    mediatorSpy.resetWithListOfTabs(tabs, null, /* quickMode= */ false);
                    if (signedInAndSyncEnabled && priceTrackingEnabled && !incognito) {
                        mModelList
                                .get(0)
                                .model
                                .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                                .fetch(
                                        (shoppingPersistedTabData) -> {
                                            assertThat(
                                                    shoppingPersistedTabData.getPriceMicros(),
                                                    equalTo(123456789012345L));
                                        });
                        mModelList
                                .get(1)
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
                                mModelList
                                        .get(0)
                                        .model
                                        .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER));
                        assertNull(
                                mModelList
                                        .get(1)
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
        addSpecialItem(1, UiType.PRICE_MESSAGE, PRICE_MESSAGE);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));

        doAnswer(
                        invocation -> {
                            int position = invocation.getArgument(0);
                            int itemType = mModelList.get(position).type;
                            if (itemType == UiType.PRICE_MESSAGE) {
                                return mGridLayoutManager.getSpanCount();
                            }
                            return 1;
                        })
                .when(mSpanSizeLookup)
                .getSpanSize(anyInt());
        mMediator.updateLayout();
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        mMediator.updateLayout();
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
    }

    @Test
    public void testIndexOfNthTabCard() {
        initAndAssertAllProperties();
        addSpecialItem(1, UiType.PRICE_MESSAGE, PRICE_MESSAGE);

        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModelList.indexOfNthTabCard(-1), equalTo(TabModel.INVALID_TAB_INDEX));
        assertThat(mModelList.indexOfNthTabCard(0), equalTo(0));
        assertThat(mModelList.indexOfNthTabCard(1), equalTo(2));
        assertThat(mModelList.indexOfNthTabCard(2), equalTo(3));
    }

    @Test
    public void testIndexOfNthTabCardOrInvalid() {
        initAndAssertAllProperties();
        addSpecialItem(1, UiType.PRICE_MESSAGE, PRICE_MESSAGE);

        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModelList.indexOfNthTabCardOrInvalid(-1), equalTo(TabModel.INVALID_TAB_INDEX));
        assertThat(mModelList.indexOfNthTabCardOrInvalid(0), equalTo(0));
        assertThat(mModelList.indexOfNthTabCardOrInvalid(1), equalTo(2));
        assertThat(mModelList.indexOfNthTabCardOrInvalid(2), equalTo(TabModel.INVALID_TAB_INDEX));
    }

    @Test
    public void testGetTabCardCountsBefore() {
        initAndAssertAllProperties();
        addSpecialItem(1, UiType.PRICE_MESSAGE, PRICE_MESSAGE);

        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModelList.getTabCardCountsBefore(-1), equalTo(TabModel.INVALID_TAB_INDEX));
        assertThat(mModelList.getTabCardCountsBefore(0), equalTo(0));
        assertThat(mModelList.getTabCardCountsBefore(1), equalTo(1));
        assertThat(mModelList.getTabCardCountsBefore(2), equalTo(1));
        assertThat(mModelList.getTabCardCountsBefore(3), equalTo(2));
    }

    @Test
    public void testGetTabIndexBefore() {
        initAndAssertAllProperties();
        addSpecialItem(1, UiType.PRICE_MESSAGE, PRICE_MESSAGE);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModelList.getTabIndexBefore(2), equalTo(0));
        assertThat(mModelList.getTabIndexBefore(0), equalTo(TabModel.INVALID_TAB_INDEX));
    }

    @Test
    public void testGetTabIndexAfter() {
        initAndAssertAllProperties();
        addSpecialItem(1, UiType.PRICE_MESSAGE, PRICE_MESSAGE);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(1));
        assertThat(mModelList.getTabIndexAfter(0), equalTo(2));
        assertThat(mModelList.getTabIndexAfter(2), equalTo(TabModel.INVALID_TAB_INDEX));
    }

    @Test
    public void testListObserver_OnItemRangeInserted() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        setPriceTrackingEnabledForTesting(true);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(PRICE_MESSAGE);
        mMediator.addSpecialItemToModel(1, UiType.PRICE_MESSAGE, model);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
    }

    @Test
    public void testListObserver_OnItemRangeRemoved() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        setPriceTrackingEnabledForTesting(true);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initWithThreeTabs();

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(PRICE_MESSAGE);
        mMediator.addSpecialItemToModel(2, UiType.PRICE_MESSAGE, model);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
        mModelList.removeAt(0);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
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

        createTabGroup(Collections.singletonList(mTab1), TAB1_ID, TAB_GROUP_ID);
        when(mTabModel.isIncognito()).thenReturn(false);
        // Mock that we have a stored color stored with reference to root ID of tab1.
        when(mTabGroupModelFilter.getTabGroupColor(TAB_GROUP_ID)).thenReturn(COLOR_2);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(TAB_GROUP_ID)).thenReturn(COLOR_2);

        assertNull(mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        // Test a group of three.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didCreateNewGroup(mTab1, mTabGroupModelFilter);

        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_Grid() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);

        createTabGroup(Collections.singletonList(mTab1), TAB1_ID, TAB_GROUP_ID);
        when(mTabModel.isIncognito()).thenReturn(false);
        // Mock that we have a stored color stored with reference to root ID of tab1.
        when(mTabGroupModelFilter.getTabGroupColor(TAB_GROUP_ID)).thenReturn(COLOR_2);
        when(mTabGroupModelFilter.getTabGroupColorWithFallback(TAB_GROUP_ID)).thenReturn(COLOR_2);

        // Test a group of three.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
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
        assertThat(mMediator.resetWithListOfTabs(tabs, null, false), equalTo(true));
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));

        // Reset without show quickly.
        mModelList.clear();
        assertThat(mMediator.resetWithListOfTabs(tabs, null, false), equalTo(false));
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format(
                        "Expand %s tab group with 2 tabs, color Grey.", CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));
    }

    @Test
    public void testTabDescriptionString_Archived() {
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        ArchivedTabsDialogCoordinator.COMPONENT_NAME,
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        mUndoBarExplicitTrigger,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        mMediator.resetWithListOfTabs(tabs, null, false);

        String targetString = mResources.getString(R.string.accessibility_restore_tab, TAB3_TITLE);

        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));
    }

    @Test
    public void testTabDescriptionString_withTabGroupType_Archived() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<String> syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID1));
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        // Ensure the groups are archived.
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();
        mSavedTabGroup2.archivalTimeMs = System.currentTimeMillis();

        @StringRes
        int colorDesc1 =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        SYNC_GROUP_COLOR1);
        String nonEmptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_restore_tab_group_with_group_name_with_color,
                        mSavedTabGroup1.savedTabs.size(),
                        GROUP_TITLE,
                        mSavedTabGroup1.savedTabs.size(),
                        mResources.getString(colorDesc1));

        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertThat(
                mModelList
                        .get(0)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(nonEmptyTitleTargetString));

        @StringRes
        int colorDesc2 =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        SYNC_GROUP_COLOR2);
        String emptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_restore_tab_group_with_color,
                        mSavedTabGroup2.savedTabs.size(),
                        mSavedTabGroup2.savedTabs.size(),
                        mResources.getString(colorDesc2));

        syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID2));
        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertThat(
                mModelList
                        .get(0)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(emptyTitleTargetString));
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
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        defaultColor);
        String emptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_expand_shared_tab_group_with_color,
                        group1.size(),
                        group1.size(),
                        mResources.getString(colorDesc));

        // Check that a base group with no title has the correct content description.
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(emptyTitleTargetString));

        String nonEmptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_expand_shared_tab_group_with_group_name_with_color,
                        group1.size(),
                        CUSTOMIZED_DIALOG_TITLE1,
                        group1.size(),
                        mResources.getString(colorDesc));
        // Check that a customized title provides a different content description.
        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab2.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(nonEmptyTitleTargetString));
    }

    @Test
    public void testActionButtonDescriptionStringGroupOverflowMenu_TabSwitcher() {
        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        final @TabGroupColorId int defaultColor = TabGroupColorId.GREY;
        final @StringRes int colorDesc =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        defaultColor);
        String targetString =
                String.format(
                        "Open the tab group action menu for tab group 2 tabs, color %s.",
                        mResources.getString(colorDesc));

        mMediator.resetWithListOfTabs(group1, null, false);
        assertThat(
                mModelList
                        .get(POSITION1)
                        .model
                        .get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));

        // Set group name.
        targetString =
                String.format(
                        "Open the tab group action menu for tab group %s, color %s.",
                        CUSTOMIZED_DIALOG_TITLE1, mResources.getString(colorDesc));
        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didChangeTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mModelList
                        .get(POSITION1)
                        .model
                        .get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));
    }

    @Test
    public void testTabGroupActionButtonDescriptionString_WithTabGroupType_Archived() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<String> syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID1));
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        // Ensure the groups are archived.
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();
        mSavedTabGroup2.archivalTimeMs = System.currentTimeMillis();

        @StringRes
        int colorDesc1 =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        SYNC_GROUP_COLOR1);
        String nonEmptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_close_tab_group_button_with_group_name_with_color,
                        mSavedTabGroup1.savedTabs.size(),
                        GROUP_TITLE,
                        mSavedTabGroup1.savedTabs.size(),
                        mResources.getString(colorDesc1));

        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertThat(
                mModelList
                        .get(0)
                        .model
                        .get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(nonEmptyTitleTargetString));

        @StringRes
        int colorDesc2 =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        SYNC_GROUP_COLOR2);
        String emptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_close_tab_group_button_with_color,
                        mSavedTabGroup2.savedTabs.size(),
                        mSavedTabGroup2.savedTabs.size(),
                        mResources.getString(colorDesc2));

        syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID2));
        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertThat(
                mModelList
                        .get(0)
                        .model
                        .get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(emptyTitleTargetString));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
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
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        defaultColor);
        String emptyTitleTargetString =
                mResources.getString(
                        R.string
                                .accessibility_open_shared_tab_group_overflow_menu_with_group_name_with_color,
                        defaultTitle,
                        mResources.getString(colorDesc));

        // Check that a base group with no title has the correct content description.
        mMediator.resetWithListOfTabs(group1, null, false);
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
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
    public void testSelectableUpdates_withoutRelated() {
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.SELECTABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(false);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(false);
        ThumbnailFetcher fetcher2 = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView2, TAB2_ID, /* triggeringMotion= */ null);
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertEquals(fetcher2, mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSelectableUpdates_withRelated() {
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.SELECTABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(false);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabGroupModelFilter.isTabInTabGroup(mTab2)).thenReturn(true);
        ThumbnailFetcher fetcher2 = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView2, TAB2_ID, /* triggeringMotion= */ null);
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNotEquals(fetcher2, mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSelectableUpdates_onReset() {
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.SELECTABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
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
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(false);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        ThumbnailFetcher fetcher1 = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher fetcher2 = mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher fetcher3 = mModelList.get(2).model.get(TabProperties.THUMBNAIL_FETCHER);
        mMediator.resetWithListOfTabs(tabs, null, true);

        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertEquals(fetcher1, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertNotEquals(fetcher2, mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
        assertEquals(fetcher3, mModelList.get(2).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testChangingTabGroupModelFilters() {
        mCurrentTabGroupModelFilterSupplier.set(mIncognitoTabGroupModelFilter);

        verify(mTabGroupModelFilter).removeObserver(any());
        verify(mTabGroupModelFilter).removeTabGroupObserver(any());

        // Not added until the next resetWithListOfTabs call.
        verify(mIncognitoTabGroupModelFilter, never()).addObserver(any());
        verify(mIncognitoTabGroupModelFilter, never()).addTabGroupObserver(any());
    }

    @Test
    public void testSpecialItemExist() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(model.get(MESSAGE_TYPE)).thenReturn(FOR_TESTING);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, model);

        assertTrue(mModelList.size() > 0);
        assertTrue(mMediator.specialItemExistsInModel(FOR_TESTING));
        assertFalse(mMediator.specialItemExistsInModel(PRICE_MESSAGE));
        assertTrue(mMediator.specialItemExistsInModel(TabSwitcherMessageManager.MessageType.ALL));
    }

    @Test
    public void tabClosure_updatesTabGroup_inTabSwitcher() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);
        ThumbnailFetcher fetcherBefore =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModelList.size());

        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabGroupModelFilter).tabGroupExists(TAB_GROUP_ID);
        doReturn(false).when(mTab1).isClosing();

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);

        assertEquals(2, mModelList.size());

        ThumbnailFetcher fetcherAfter =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, not(fetcherAfter));
    }

    @Test
    public void tabClosure_doesNotUpdateTabGroup_inTabSwitcher_WhenClosing() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);
        ThumbnailFetcher fetcherBefore =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModelList.size());

        mMediator.setActionOnAllRelatedTabsForTesting(true);
        doReturn(true).when(mTabGroupModelFilter).tabGroupExists(TAB_GROUP_ID);
        doReturn(true).when(mTab1).isClosing();

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);

        assertEquals(2, mModelList.size());

        ThumbnailFetcher fetcherAfter =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, equalTo(fetcherAfter));
    }

    @Test
    public void tabClosure_ignoresUpdateForTabGroup_outsideTabSwitcher() {
        initAndAssertAllProperties();
        TabActionListener actionListenerBeforeUpdate =
                mModelList.get(0).model.get(TabProperties.TAB_CLICK_LISTENER);

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        assertEquals(2, mModelList.size());

        mMediator.setActionOnAllRelatedTabsForTesting(false);
        doReturn(true).when(mTabGroupModelFilter).tabGroupExists(TAB_GROUP_ID);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);

        assertEquals(1, mModelList.size());

        TabActionListener actionListenerAfterUpdate =
                mModelList.get(0).model.get(TabProperties.TAB_CLICK_LISTENER);
        // The selection listener should remain unchanged, since the property model of the tab group
        // should not get updated when the closure is triggered from outside the tab switcher.
        assertThat(actionListenerBeforeUpdate, equalTo(actionListenerAfterUpdate));
    }

    @Test
    public void tabClosure_resetTabsListForTabGroupUpdate_insideTabSwitcher() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);
        ThumbnailFetcher fetcherBefore =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModelList.size());
        assertEquals(mModelList.get(0).model.get(TabProperties.TAB_ID), mTab1.getId());

        mMediator.setActionOnAllRelatedTabsForTesting(true);

        mMediator.resetWithListOfTabs(List.of(tab3, mTab2), null, true);

        assertEquals(2, mModelList.size());

        ThumbnailFetcher fetcherAfter =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, not(fetcherAfter));

        assertEquals(mModelList.get(0).model.get(TabProperties.TAB_ID), tab3.getId());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
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
        mMediator.resetWithListOfTabs(tabs, null, false);

        assertEquals(
                TabActionButtonType.OVERFLOW,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_PINNED_TABS})
    public void testIsTabPinned_TabSwitcher() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);

        List<Tab> tabsInModel = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabsInModel.add(mTabModel.getTabAt(i));
        }
        Tab tabToTest = tabsInModel.get(POSITION1);

        // Scenario 1: Tab is UNPINNED
        // Mock the tab at POSITION1 as unpinned.
        doReturn(false).when(tabToTest).getIsPinned();

        mMediator.resetWithListOfTabs(tabsInModel, null, false);

        assertEquals(
                TabActionButtonType.CLOSE,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));

        // Scenario 2: Tab is PINNED
        // Mock the tab at POSITION1 as pinned.
        doReturn(true).when(tabToTest).getIsPinned();

        // Re-process the tabs. The mediator should pick up the changed pinned state.
        mMediator.resetWithListOfTabs(tabsInModel, null, false);

        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
        assertTrue(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testOnTabPinnedStateChanged() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);

        List<Tab> tabsInModel = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabsInModel.add(mTabModel.getTabAt(i));
        }
        Tab tabToTest = tabsInModel.get(POSITION1);

        // Set initial state to unpinned.
        doReturn(false).when(tabToTest).getIsPinned();
        mMediator.resetWithListOfTabs(tabsInModel, null, false);
        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));

        // Pin the tab and notify the observer.
        doReturn(true).when(tabToTest).getIsPinned();
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(tabToTest, true);
        assertTrue(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));
        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);

        // Unpin the tab and notify the observer.
        doReturn(false).when(tabToTest).getIsPinned();
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(tabToTest, false);
        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));
        assertEquals(
                TabActionButtonType.CLOSE,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testOnTabPinnedStateChanged_MovesTab() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(false).when(mTab1).getIsPinned();
        doReturn(false).when(mTab2).getIsPinned();
        doReturn(false).when(tab3).getIsPinned();

        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.iterator()).thenAnswer(invocation -> List.of(mTab1, mTab2, tab3).iterator());

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(tab3)).thenReturn(2);

        when(mTabGroupModelFilter.getIndividualTabAndGroupCount()).thenReturn(3);
        when(mTabGroupModelFilter.getRepresentativeTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getRepresentativeTabAt(1)).thenReturn(mTab2);
        when(mTabGroupModelFilter.getRepresentativeTabAt(2)).thenReturn(tab3);

        List<Tab> tabsInModel = new ArrayList<>(Arrays.asList(mTab1, mTab2, tab3));
        mMediator.resetWithListOfTabs(tabsInModel, null, false);
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertFalse(mModelList.get(1).model.get(TabProperties.IS_PINNED));

        // Pin mTab2. It should move to the front.
        doReturn(true).when(mTab2).getIsPinned();
        when(mTabModel.indexOf(mTab2)).thenReturn(0);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(tab3)).thenReturn(2);

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab2, true);

        // Verify mTab2 is now at the front and pinned.
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertTrue(mModelList.get(0).model.get(TabProperties.IS_PINNED));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(0).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);

        // Pin mTab1. It should not move.
        doReturn(true).when(mTab1).getIsPinned();
        when(mTabModel.indexOf(mTab2)).thenReturn(0);
        when(mTabModel.indexOf(mTab1)).thenReturn(1);
        when(mTabModel.indexOf(tab3)).thenReturn(2);

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, true);

        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertTrue(mModelList.get(1).model.get(TabProperties.IS_PINNED));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);

        // Unpin mTab2. It should return to its original position.
        doReturn(false).when(mTab2).getIsPinned();
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(tab3)).thenReturn(2);

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab2, false);

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertFalse(mModelList.get(1).model.get(TabProperties.IS_PINNED));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertNotEquals(
                TabActionButtonType.PIN,
                mModelList.get(1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_PINNED_TABS)
    public void testOnTabPinnedStateChanged_MovesTab_OutOfBounds() {
        mMediator.setComponentNameForTesting(TabSwitcherPaneCoordinator.COMPONENT_NAME);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        // Start with tab1 pinned, others not.
        doReturn(true).when(mTab1).getIsPinned();
        doReturn(false).when(mTab2).getIsPinned();
        doReturn(false).when(tab3).getIsPinned();

        // TabModel has all 3 tabs, with pinned tab first.
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.iterator()).thenAnswer(invocation -> List.of(mTab1, mTab2, tab3).iterator());

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(tab3)).thenReturn(2);

        // TabGroupModelFilter also represents all 3.
        when(mTabGroupModelFilter.getIndividualTabAndGroupCount()).thenReturn(3);
        when(mTabGroupModelFilter.getRepresentativeTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getRepresentativeTabAt(1)).thenReturn(mTab2);
        when(mTabGroupModelFilter.getRepresentativeTabAt(2)).thenReturn(tab3);
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabGroupModelFilter.getRelatedTabList(TAB3_ID)).thenReturn(List.of(tab3));

        // But TabListModel only has the first two.
        List<Tab> tabsInModel = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabsInModel, null, false);
        assertEquals(2, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertTrue(mModelList.get(0).model.get(TabProperties.IS_PINNED));

        // Now, unpin tab1. After this, its position in TabModel will be at the end of the
        // unpinned tabs. With tab2 and tab3 unpinned, and assuming stable sort, tab1 will go
        // after tab3. Let's say the new order is [tab2, tab3, tab1].
        doReturn(false).when(mTab1).getIsPinned();
        when(mTabModel.indexOf(mTab1)).thenReturn(2);
        when(mTabModel.indexOf(mTab2)).thenReturn(0);
        when(mTabModel.indexOf(tab3)).thenReturn(1);

        // Trigger the observer.
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, false);

        // `index` of tab1 in model is 0.
        // `indexOf` tab1 in TabModel is 2.
        // `indexOfNthTabCard(2)` on a model with 2 tabs returns 2.
        // This would call move(0, 2) on a list of size 2, which would crash.
        // With the fix, it should call move(0, 1). The list should become [tab2, tab1].
        assertEquals(2, mModelList.size());
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertFalse(mModelList.get(1).model.get(TabProperties.IS_PINNED));
    }

    @Test
    public void testOnMenuItemClickedCallback_CloseGroupInTabSwitcher_NullListViewTouchTracker() {
        testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
                R.id.close_tab_group,
                /* listViewTouchTracker= */ null,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ true);
    }

    @Test
    public void testOnMenuItemClickedCallback_CloseGroupInTabSwitcher_ClickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
                R.id.close_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ true);
    }

    @Test
    public void testOnMenuItemClickedCallback_CloseGroupInTabSwitcher_ClickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
                R.id.close_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ false,
                /* shouldHideTabGroups= */ true);
    }

    @Test
    public void testOnMenuItemClickedCallback_DeleteGroupInTabSwitcher_NullListViewTouchTracker() {
        testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
                R.id.delete_tab_group,
                /* listViewTouchTracker= */ null,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ false);
    }

    @Test
    public void testOnMenuItemClickedCallback_DeleteGroupInTabSwitcher_ClickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
                R.id.delete_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ false);
    }

    @Test
    public void testOnMenuItemClickedCallback_DeleteGroupInTabSwitcher_ClickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
                R.id.delete_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ false,
                /* shouldHideTabGroups= */ false);
    }

    private void testOnMenuItemClickedCallback_CloseOrDeleteGroupInTabSwitcher(
            @IdRes int menuId,
            @Nullable ListViewTouchTracker listViewTouchTracker,
            boolean shouldAllowUndo,
            boolean shouldHideTabGroups) {
        assertTrue(menuId == R.id.close_tab_group || menuId == R.id.delete_tab_group);

        // Create tab group
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        List<Tab> group = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);

        // Act
        mMediator.onMenuItemClicked(
                menuId, TAB_GROUP_ID, /* collaborationId= */ null, listViewTouchTracker);

        // Assert
        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(
                                                mTabGroupModelFilter, TAB_GROUP_ID)
                                        .allowUndo(shouldAllowUndo)
                                        .hideTabGroups(shouldHideTabGroups)
                                        .build()),
                        /* allowDialog= */ eq(true),
                        any());
    }

    @Test
    public void testOnMenuItemClickedCallback_UngroupInTabSwitcher_IncognitoNoShow() {
        mCurrentTabGroupModelFilterSupplier.set(mIncognitoTabGroupModelFilter);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mIncognitoTabModel.getCount(); i++) {
            tabs.add(mIncognitoTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mIncognitoTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mIncognitoTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID))
                .thenReturn(TAB1_ID);
        when(mIncognitoTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        mMediator.onMenuItemClicked(
                R.id.ungroup_tab,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mIncognitoTabUngrouper)
                .ungroupTabGroup(TAB_GROUP_ID, /* trailing= */ false, /* allowDialog= */ true);
    }

    @Test
    public void testOnMenuItemClickedCallback_DeleteGroupInTabSwitcher_Incognito() {
        mCurrentTabGroupModelFilterSupplier.set(mIncognitoTabGroupModelFilter);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mIncognitoTabModel.getCount(); i++) {
            tabs.add(mIncognitoTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mIncognitoTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mIncognitoTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID))
                .thenReturn(TAB1_ID);
        mMediator.onMenuItemClicked(
                R.id.delete_tab_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mIncognitoTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(
                                                mIncognitoTabGroupModelFilter, TAB_GROUP_ID)
                                        .allowUndo(true)
                                        .hideTabGroups(false)
                                        .build()),
                        /* allowDialog= */ eq(true),
                        any());
    }

    @Test
    public void testOnMenuItemClickedCallback_ShareGroupInTabSwitcher() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(group1, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        mMediator.onMenuItemClicked(
                R.id.share_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mDataSharingTabManager).createOrManageFlow(any(), anyInt(), any());
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
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        mMediator.onMenuItemClicked(
                R.id.close_tab_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(
                                                mTabGroupModelFilter, TAB_GROUP_ID)
                                        .allowUndo(true)
                                        .hideTabGroups(true)
                                        .build()),
                        /* allowDialog= */ eq(true),
                        any());
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
        when(mTabGroupModelFilter.getRepresentativeTabAt(2)).thenReturn(tab3);

        Token otherGroupId = new Token(74893L, 8490L);
        // Mock that tab5 and tab6 are in the same group and group root id is TAB5_ID.
        List<Tab> groupTabs2 = new ArrayList<>(Arrays.asList(tab5, tab6));
        createTabGroup(groupTabs2, TAB5_ID, otherGroupId, 3);
        when(mTabGroupModelFilter.getRepresentativeTabAt(3)).thenReturn(tab5);

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
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(5));

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
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        // Unique sets of keys for each of SELECTABLE/CLOSABLE.
        ArrayList<PropertyKey> uniqueClosableKeys =
                new ArrayList<>(Arrays.asList(TAB_GRID_CLOSABLE_KEYS));
        uniqueClosableKeys.removeAll(Arrays.asList(TAB_GRID_SELECTABLE_KEYS));

        // The test starts in the CLOSABLE state.
        PropertyModel model = mModelList.get(0).model;
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

        mMediator.resetWithListOfTabs(tabs, null, false);

        mModelList.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        mMediator.getOnMaybeTabClosedCallback(TAB1_ID).onResult(false);
        assertFalse(mModelList.get(0).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_NoModels() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        mModelList.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);

        var callback = mMediator.getOnMaybeTabClosedCallback(TAB1_ID);

        mMediator.resetWithListOfTabs(null, null, false);

        callback.onResult(true);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_Tab1Closed() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        mModelList.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        var callback = mMediator.getOnMaybeTabClosedCallback(TAB1_ID);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);

        callback.onResult(true);
        assertFalse(mModelList.get(0).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_TabsClosed() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        mModelList.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        var callback = mMediator.getOnMaybeTabClosedCallback(TAB1_ID);

        when(mTabGroupModelFilter.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(newTab);

        callback.onResult(true);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testUpdateTabStripNotificationBubble_hasUpdate() {
        // Setup the test such that the tab list is strip mode, with a tab group of 2 tabs.
        setUpTabListMediator(TabListMediatorType.TAB_STRIP, TabListMode.STRIP);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));
        assertFalse(mModelList.get(POSITION2).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));

        // Only pass in updates for mTab1 and leaving mTab2 untouched.
        Set<Integer> tabIdsToBeUpdated = new HashSet<>();
        tabIdsToBeUpdated.add(mTab1.getId());
        mMediator.updateTabStripNotificationBubble(tabIdsToBeUpdated, true);

        assertTrue(mModelList.get(POSITION1).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));
        assertFalse(mModelList.get(POSITION2).model.get(TabProperties.HAS_NOTIFICATION_BUBBLE));
    }

    @Test
    public void testUpdateTabCardLabels() {
        TabCardLabelData tabCardLabelData = mock(TabCardLabelData.class);
        Map<Integer, TabCardLabelData> dataMap = new HashMap<>();
        dataMap.put(TAB1_ID, tabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        assertEquals(
                tabCardLabelData,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_CARD_LABEL_DATA));
        assertNull(mModelList.get(POSITION2).model.get(TabProperties.TAB_CARD_LABEL_DATA));

        dataMap.replace(TAB1_ID, null);
        dataMap.put(TAB2_ID, tabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        assertNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_CARD_LABEL_DATA));
        assertEquals(
                tabCardLabelData,
                mModelList.get(POSITION2).model.get(TabProperties.TAB_CARD_LABEL_DATA));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testShareUpdateTabCardLabelsContentDescription() {
        when(mProfile.isOffTheRecord()).thenReturn(false);
        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        // Setup a tab group with {tab2, tab3}.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = new ArrayList<>(Arrays.asList(mTab2, tab3));
        createTabGroup(group1, TAB2_ID, TAB_GROUP_ID);
        setupSyncedGroup(/* isShared= */ true);

        TabCardLabelData tabCardLabelData =
                new TabCardLabelData(
                        TabCardLabelType.ACTIVITY_UPDATE,
                        (context) -> "Test label",
                        /* asyncImageFactory= */ null,
                        (context) -> "Alice changed");

        Map<Integer, TabCardLabelData> dataMap = new HashMap<>();
        dataMap.put(TAB2_ID, tabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        String targetString1 =
                "Expand shared tab group with 2 tabs, color Grey, with label Alice changed.";
        assertEquals(
                targetString1,
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext));

        mTabGroupModelFilter.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        String targetString2 =
                "Expand shared Cool Tabs tab group with 2 tabs, color Grey, with label Alice"
                        + " changed.";
        assertEquals(
                targetString2,
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext));

        dataMap.replace(TAB2_ID, null);
        mMediator.updateTabCardLabels(dataMap);
        String targetString3 = "Expand shared Cool Tabs tab group with 2 tabs, color Grey.";
        assertEquals(
                targetString3,
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext));
    }

    @Test
    public void testObserversRemovedAfterHiding() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        verify(mTabGroupModelFilter, times(1)).addObserver(mTabModelObserverCaptor.getValue());
        verify(mTabGroupModelFilter, times(1))
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.getValue());

        // Hide the GTS. The observers should be removed.
        mMediator.postHiding();
        verify(mTabGroupModelFilter).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mTabGroupModelFilter)
                .removeTabGroupObserver(mTabGroupModelFilterObserverCaptor.getValue());
    }

    @Test
    public void testMoveNonExistantTab() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume added a new tab to tab model after tab1 and move it to the end(index is 2).
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(0);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(1);
        doReturn(newTab).when(mTabGroupModelFilter).getRepresentativeTabAt(2);

        // The index was changed to 2 from 1 after moving the new tab from 1 to 2.
        doReturn(2).when(mTabGroupModelFilter).representativeIndexOf(newTab);

        doReturn(3).when(mTabGroupModelFilter).getIndividualTabAndGroupCount();
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabGroupModelFilter).getRelatedTabList(eq(TAB3_ID));

        // The tab list wasn't updated. The length is still 2.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        // Call didMoveTabGroup with the new tab. It should not crash and the tab list should not be
        // updated.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabGroup(newTab, 2, 1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testGetSpanCount_OnXrDevice() {
        DeviceInfo.setIsXrForTesting(true);
        // Perform action and validate for compact width.
        assertEquals(
                TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM,
                mMediator.getSpanCount(TabListCoordinator.MAX_SCREEN_WIDTH_COMPACT_DP - 1));
        // Perform action and validate for medium width.
        assertEquals(
                TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM,
                mMediator.getSpanCount(TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP - 1));
        // Perform action and validate for large width.
        assertEquals(
                TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM,
                mMediator.getSpanCount(TabListCoordinator.MAX_SCREEN_WIDTH_MEDIUM_DP + 1));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS)
    public void testAddSpecialItemToModelList_tabGroup() {
        mMediator.resetWithListOfTabs(null, null, false);

        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(TAB_GROUP);
        mMediator.addSpecialItemToModel(0, UiType.TAB_GROUP, model);

        assertTrue(mModelList.size() > 0);
        assertEquals(UiType.TAB_GROUP, mModelList.get(0).type);
    }

    @Test
    public void testResetWithListOfTabs_withTabGroupType() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<String> syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID1));
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        // Assert that group types come before tabs and all properties are correct.
        assertEquals(TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertEquals(SYNC_GROUP_ID1, mModelList.get(0).model.get(TabProperties.TAB_GROUP_SYNC_ID));
        assertEquals(GROUP_TITLE, mModelList.get(0).model.get(TabProperties.TITLE));
        var provider = mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());
    }

    @Test
    public void testBindTabGroupActionButtonData_withTabGroupType() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        List<String> syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID1));
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        // Ensure the group is archived.
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();

        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_GROUP_SYNC_ID),
                        /* triggeringMotion= */ null);

        // Assert that the tab group has been removed from the model list and archive status reset.
        assertEquals(TAB, mModelList.get(0).model.get(CARD_TYPE));
        verify(mTabGroupSyncService).updateArchivalStatus(eq(SYNC_GROUP_ID1), eq(false));
        verify(mUndoBarExplicitTrigger).triggerSnackbarForSavedTabGroup(eq(SYNC_GROUP_ID1));
    }

    @Test
    public void testSingleTabClosure_ArchivedTab_ExplicitTriggerSnackbar() {
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        false,
                        () -> mSelectionDelegate,
                        null,
                        null,
                        null,
                        ArchivedTabsDialogCoordinator.COMPONENT_NAME,
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        mUndoBarExplicitTrigger,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        initAndAssertAllProperties();

        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView2,
                        mModelList.get(1).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.tabs.get(0) == mTab2),
                        /* allowDialog= */ eq(true),
                        any());

        verify(mUndoBarExplicitTrigger).triggerSnackbarForTab(eq(mTab2));
    }

    @Test
    public void sendsOpenGroupSignalCorrectly_SavedTabGroupType() {
        List<Tab> tabs = Arrays.asList(mTab1);
        List<String> syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID1));
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        mMediator.resetWithListOfTabs(tabs, syncIds, false);
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_GROUP_SYNC_ID),
                        /* triggeringMotion= */ null);

        verify(mOpenGroupActionListener)
                .run(mItemView1, SYNC_GROUP_ID1, /* triggeringMotion= */ null);
    }

    @Test
    public void setTabActionState_bindsTabGroupTypePropertiesCorrectly() {
        // Start off with a closable type but an actionable selection delegate.
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        true,
                        () -> mSelectionDelegate,
                        mGridCardOnClickListenerProvider,
                        null,
                        null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        List<Tab> tabs = List.of(mTab1);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        // Assert that a tab group type is the first item in the list.
        mMediator.resetWithListOfTabs(tabs, syncIds, false);
        assertEquals(SYNC_GROUP_ID1, mModelList.get(0).model.get(TabProperties.TAB_GROUP_SYNC_ID));

        // Toggle the action state to selectable.
        mMediator.setTabActionState(TabActionState.SELECTABLE);
        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_CLICK_LISTENER));
        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_LONG_CLICK_LISTENER));

        // Verify the selection properties and click listener logic.
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_GROUP_SYNC_ID),
                        /* triggeringMotion= */ null);
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void removeListItem_TabGroup() {
        List<String> syncIds = new ArrayList<>(Arrays.asList(SYNC_GROUP_ID1));
        mMediator.setDefaultGridCardSize(new Size(100, 200));
        mMediator.resetWithListOfTabs(null, syncIds, false);

        assertEquals(1, mModelList.size());

        // Assert removing a tab type does nothing.
        mMediator.removeListItemFromModelList(UiType.TAB_GROUP, ITEM1_ID);
        assertEquals(1, mModelList.size());

        mMediator.removeListItemFromModelList(UiType.TAB_GROUP, ITEM4_ID);
        assertEquals(0, mModelList.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MEDIA_INDICATORS_ANDROID)
    public void testMediaState_TabAudible() {
        assertEquals(MediaState.NONE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab1, MediaState.AUDIBLE);
        assertEquals(
                MediaState.AUDIBLE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MEDIA_INDICATORS_ANDROID)
    public void testMediaState_TabMuted() {
        assertEquals(MediaState.NONE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab1, MediaState.AUDIBLE);
        assertEquals(
                MediaState.AUDIBLE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab1, MediaState.MUTED);
        assertEquals(MediaState.MUTED, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MEDIA_INDICATORS_ANDROID)
    public void testMediaState_TabNone() {
        updateTabMediaState(mTab1, MediaState.AUDIBLE);
        assertEquals(
                MediaState.AUDIBLE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab1, MediaState.NONE);
        assertEquals(MediaState.NONE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MEDIA_INDICATORS_ANDROID)
    public void testMediaState_TabRecording() {
        assertEquals(MediaState.NONE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab1, MediaState.RECORDING);
        assertEquals(
                MediaState.RECORDING, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MEDIA_INDICATORS_ANDROID)
    public void testMediaState_TabGroup() {
        when(mTab1.getMediaState()).thenReturn(MediaState.MUTED);
        when(mTab2.getMediaState()).thenReturn(MediaState.AUDIBLE);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // AUDIBLE has priority over MUTED.
        assertEquals(
                MediaState.AUDIBLE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab2, MediaState.MUTED);
        assertEquals(MediaState.MUTED, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        updateTabMediaState(mTab1, MediaState.AUDIBLE);
        assertEquals(
                MediaState.AUDIBLE, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        // MUTED has priority over NONE.
        updateTabMediaState(mTab1, MediaState.NONE);
        assertEquals(MediaState.MUTED, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));

        // RECORDING has priority over AUDIBLE.
        updateTabMediaState(mTab1, MediaState.RECORDING);
        updateTabMediaState(mTab2, MediaState.AUDIBLE);
        assertEquals(
                MediaState.RECORDING, mModelList.get(0).model.get(TabProperties.MEDIA_INDICATOR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.MEDIA_INDICATORS_ANDROID)
    public void testMediaState_TabGroup_ContentDescription() {
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);
        PropertyModel model = mModelList.get(0).model;

        Resources res = mContext.getResources();
        String playingAudio =
                res.getString(org.chromium.chrome.tab_ui.R.string.accessibility_tab_group_audible);
        String mutedAudio =
                res.getString(org.chromium.chrome.tab_ui.R.string.accessibility_tab_group_muted);
        String recording =
                res.getString(
                        org.chromium.chrome.tab_ui.R.string.accessibility_tab_group_recording);
        String sharing =
                res.getString(org.chromium.chrome.tab_ui.R.string.accessibility_tab_group_sharing);

        // Description without media state.
        final @TabGroupColorId int defaultColor = TabGroupColorId.GREY;
        final @StringRes int colorDescRes =
                TabGroupColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(
                        defaultColor);
        String baseDescription =
                res.getQuantityString(
                        org.chromium.chrome.tab_ui.R.plurals
                                .accessibility_expand_tab_group_with_color,
                        2,
                        2,
                        res.getString(colorDescRes));

        // MediaState AUDIBLE.
        updateTabMediaState(mTab1, MediaState.AUDIBLE);
        assertEquals(
                baseDescription + " " + playingAudio,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // MediaState MUTED.
        updateTabMediaState(mTab1, MediaState.MUTED);
        assertEquals(
                baseDescription + " " + mutedAudio,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // MediaState RECORDING.
        updateTabMediaState(mTab2, MediaState.RECORDING);
        assertEquals(
                baseDescription + " " + recording,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // MediaState SHARING.
        updateTabMediaState(mTab2, MediaState.SHARING);
        assertEquals(
                baseDescription + " " + sharing,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // MediaState NONE.
        updateTabMediaState(mTab1, MediaState.NONE);
        updateTabMediaState(mTab2, MediaState.NONE);
        assertEquals(
                baseDescription,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());
    }

    @Test
    public void testContextClickListener() {
        mMediator =
                new TabListMediator(
                        mActivity,
                        mModelList,
                        TabListMode.GRID,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        getTabThumbnailCallback(),
                        mTabListFaviconProvider,
                        /* actionOnRelatedTabs= */ true,
                        () -> mSelectionDelegate,
                        /* gridCardOnClickListenerProvider= */ null,
                        /* dialogHandler= */ null,
                        /* priceWelcomeMessageControllerSupplier= */ null,
                        getClass().getSimpleName(),
                        TabProperties.TabActionState.CLOSABLE,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        /* undoBarExplicitTrigger= */ null,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER));

        mMediator.setTabActionState(TabActionState.SELECTABLE);
        assertNull(mModelList.get(0).model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER));
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

        mediator.resetWithListOfTabs(tabs, null, false);

        assertEquals(mTabModelObserverCaptor.getAllValues().size(), tabModelObserverCount + 1);
        assertEquals(
                mTabGroupModelFilterObserverCaptor.getAllValues().size(),
                tabGroupModelFilterObserverCount + 1);

        for (Callback<TabFavicon> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(mFavicon);
        }

        assertThat(mModelList.size(), equalTo(mTabModel.getCount()));

        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        if (!mTabGroupModelFilter.isTabInTabGroup(mTab1)) {
            assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        }
        if (!mTabGroupModelFilter.isTabInTabGroup(mTab2)) {
            assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        }

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotNull(mModelList.get(1).model.get(TabProperties.FAVICON_FETCHER));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));

        if (mMediator.getTabListModeForTesting() == TabListMode.GRID) {
            assertThat(
                    mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER),
                    instanceOf(ThumbnailFetcher.class));
            assertThat(
                    mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER),
                    instanceOf(ThumbnailFetcher.class));
        } else {
            assertNull(mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
            assertNull(mModelList.get(1).model.get(TabProperties.THUMBNAIL_FETCHER));
        }

        if (mModelList.get(0).model.get(TabProperties.TAB_LONG_CLICK_LISTENER) != null) return;

        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_CLICK_LISTENER),
                instanceOf(TabActionListener.class));
        assertThat(
                mModelList.get(1).model.get(TabProperties.TAB_CLICK_LISTENER),
                instanceOf(TabActionListener.class));

        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).tabActionListener,
                instanceOf(TabActionListener.class));
        assertThat(
                mModelList.get(1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).tabActionListener,
                instanceOf(TabActionListener.class));
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
        doReturn(mProfile).when(tab).getProfile();
        return tab;
    }

    private SavedTabGroup prepareSavedTabGroup(
            String syncId, String title, @TabGroupColorId int colorId, int numTabs) {
        List<SavedTabGroupTab> savedTabs = new ArrayList<>();
        for (int i = 0; i < numTabs; i++) {
            savedTabs.add(new SavedTabGroupTab());
        }

        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = syncId;
        savedTabGroup.title = title;
        savedTabGroup.color = colorId;
        savedTabGroup.savedTabs = savedTabs;
        return savedTabGroup;
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
            mMediator.resetWithListOfTabs(null, null, false);
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
                        mModelList,
                        mode,
                        mModalDialogManager,
                        mCurrentTabGroupModelFilterSupplier,
                        thumbnailProvider,
                        mTabListFaviconProvider,
                        actionOnRelatedTabs,
                        () -> mSelectionDelegate,
                        mGridCardOnClickListenerProvider,
                        handler,
                        null,
                        getClass().getSimpleName(),
                        tabActionState,
                        mDataSharingTabManager,
                        /* onTabGroupCreation= */ null,
                        mUndoBarExplicitTrigger,
                        /* snackbarManager= */ null,
                        /* allowedSelectionCount= */ 0);
        TrackerFactory.setTrackerForTests(mTracker);
        mMediator.registerOrientationListener(mGridLayoutManager);

        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
    }

    private void createTabGroup(List<Tab> tabs, int rootId, Token tabGroupId) {
        createTabGroup(tabs, rootId, tabGroupId, /* index= */ null);
    }

    private void createTabGroup(
            List<Tab> tabs, int rootId, Token tabGroupId, @Nullable Integer index) {
        when(mTabGroupModelFilter.getTabCountForGroup(tabGroupId)).thenReturn(tabs.size());
        when(mTabGroupModelFilter.tabGroupExists(tabGroupId)).thenReturn(true);
        when(mTabGroupModelFilter.getTabsInGroup(tabGroupId)).thenReturn(tabs);
        int firstTabId = tabs.get(0).getId();
        when(mTabGroupModelFilter.getGroupLastShownTabId(tabGroupId)).thenReturn(firstTabId);
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
            when(tab.getRootId()).thenReturn(rootId);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
            if (index != null) {
                when(mTabGroupModelFilter.representativeIndexOf(tab)).thenReturn(index);
            }
        }
    }

    private void mockOptimizationGuideResponse(
            @OptimizationGuideDecision int decision, Map<GURL, Any> responses) {
        for (Map.Entry<GURL, Any> responseEntry : responses.entrySet()) {
            doAnswer(
                            new Answer<>() {
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
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    private void addSpecialItem(int index, @UiType int uiType, int itemIdentifier) {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        if (isMessageCard(uiType)) {
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

    private ThumbnailProvider getTabThumbnailCallback() {
        return new TabContentManagerThumbnailProvider(mTabContentManager);
    }

    private static void setPriceTrackingEnabledForTesting(boolean value) {
        FeatureOverrides.enable(ChromeFeatureList.PRICE_ANNOTATIONS);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(value);
    }

    private void assertAllUnset(PropertyModel model, List<PropertyKey> keys) {
        for (PropertyKey key : keys) {
            assertUnset(model, key);
        }
    }

    /** Asserts that the given key is null (aka "unset") in the given model. */
    private void assertUnset(PropertyModel model, PropertyKey propertyKey) {
        if (propertyKey instanceof ReadableObjectPropertyKey) {
            ReadableObjectPropertyKey objectKey = (ReadableObjectPropertyKey) propertyKey;
            assertNull(
                    "Expected property to be unset, property=" + objectKey, model.get(objectKey));
        } else {
            throw new AssertionError(
                    "Unsupported key type passed to function, add it to #assertUnset");
        }
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_COLLECTION_ANDROID)
    public void tabMergeIntoGroup_Gts_UpdatesCards() {
        // Setup with two tabs, but pretend tab 1's card is already gone.
        initAndAssertAllProperties();
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        mModelList.removeAt(1);

        // Mock that the tabs are now in a group together.
        createTabGroup(Arrays.asList(mTab1, mTab2), TAB1_ID, TAB_GROUP_ID);
        when(mTabGroupModelFilter.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB2_ID);

        // Simulate a merge of tab 2 into tab 1.
        mTabGroupModelFilterObserverCaptor
                .getValue()
                .didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        // Verify that the model now only contains one item for the group, and since tab 2 is now
        // the last shown tab for the group, it should be the one that is used for TAB_ID.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_COLLECTION_ANDROID)
    public void tabMoveOutOfGroup_Gts_UpdatesCards() {
        // Setup with a single group of two tabs.
        initAndAssertAllProperties();
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Pretend the tabs are already grouped.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID, TAB_GROUP_ID);

        // Ungroup tab 2 from a group that no longer contains any tabs.
        mTabGroupModelFilterObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        // Verify that the model now contains one item, and the empty card was removed.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    private void setupSyncedGroup(boolean isShared) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = isShared ? COLLABORATION_ID1 : null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
    }

    private void updateTabMediaState(Tab tab, @MediaState int mediaState) {
        when(tab.getMediaState()).thenReturn(mediaState);
        mTabObserverCaptor.getValue().onMediaStateChanged(tab, mediaState);
    }
}
