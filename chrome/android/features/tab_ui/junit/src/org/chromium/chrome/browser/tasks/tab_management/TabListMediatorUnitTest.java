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
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.UNSET_TAB_GROUP_TITLE;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.ARCHIVED_TAB_GROUP;
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
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;
import android.util.Size;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import com.google.protobuf.ByteString;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.Token;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.ActorOverlayState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.HandoffButtonState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.compositor.overlays.strip.TabUnderlineManager;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils;
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
import org.chromium.chrome.browser.tab_ui.TabListMode;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver.DidRemoveTabGroupReason;
import org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUiUnitTestUtils;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.OnLongPressTabItemEventListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabGridDialogHandler;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListItemOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.AnimationStatus;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarExplicitTrigger;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
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
import org.chromium.components.tabs.TabAlert;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
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
@SuppressWarnings({"ConstantConditions", "DirectInvocationOnMock"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        instrumentedPackages = {
            "androidx.recyclerview.widget.RecyclerView" // required to mock final
        })
@DisableFeatures({
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY,
    ChromeFeatureList.GLIC
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
        TabListMediatorType.TAB_GRID_DIALOG,
        TabListMediatorType.VERTICAL_TABS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMediatorType {
        int TAB_SWITCHER = 0;
        int TAB_STRIP = 1;
        int TAB_GRID_DIALOG = 2;
        int VERTICAL_TABS = 3;
    }

    @Mock TabContentManager mTabContentManager;
    @Spy TabModel mTabModel;
    @Spy TabModel mIncognitoTabModel;
    @Mock TabListFaviconProvider mTabListFaviconProvider;
    @Mock TabListFaviconProvider.TabFaviconFetcher mTabFaviconFetcher;
    @Mock RecyclerView mRecyclerView;
    @Mock TabListRecyclerView mTabListRecyclerView;
    @Mock RecyclerView.Adapter mAdapter;
    @Mock TabUngrouper mTabUngrouper;
    @Mock TabUngrouper mIncognitoTabUngrouper;
    @Mock TabRemover mTabRemover;
    @Mock TabRemover mIncognitoTabRemover;
    @Mock TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock TabListMediator.TabListItemOnClickListenerProvider mTabListItemOnClickListenerProvider;
    @Mock TabFavicon mFavicon;
    @Mock Bitmap mFaviconBitmap;
    @Mock Activity mActivity;
    @Mock TabActionListener mOpenGroupActionListener;
    @Mock GridLayoutManager mGridLayoutManager;
    @Mock GridLayoutManager.SpanSizeLookup mSpanSizeLookup;
    @Mock Profile mProfile;
    @Mock Tracker mTracker;
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
    @Mock ActorUiTabController mActorUiTabController;
    @Mock ActorOverlayState mActorOverlayState;
    @Mock HandoffButtonState mHandoffButtonState;
    @Mock UndoBarExplicitTrigger mUndoBarExplicitTrigger;
    @Mock SnackbarManager mSnackbarManager;
    @Mock MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock PropertyObservable.PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock MotionEvent mMotionEvent;
    @Mock View mItemView1;
    @Mock View mItemView2;
    @Mock View mItemView3;
    @Mock View mItemView4;
    @Mock View mTabView;
    @Mock TabGroupColorViewProvider mTabGroupColorViewProvider;
    @Mock NavigationHandle mNavigationHandle;
    @Mock PropertyModel mPropertyModel;
    @Mock ThumbnailFetcher mThumbnailFetcher1;
    @Mock ThumbnailFetcher mThumbnailFetcher2;
    @Mock AccessibilityNodeInfo mAccessibilityNodeInfo;
    @Mock Bundle mBundle;
    @Mock ListObserver<Void> mListObserver;
    @Mock TabCardLabelData mTabCardLabelData;
    @Mock SimpleRecyclerViewAdapter.ViewHolder mViewHolder1;
    @Mock SimpleRecyclerViewAdapter.ViewHolder mViewHolder2;
    @Mock TabUnderlineManager mTabUnderlineManager;

    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor ArgumentCaptor<Callback<TabFavicon>> mCallbackCaptor;
    @Captor ArgumentCaptor<TabGroupObserver> mTabGroupObserverCaptor;
    @Captor ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;
    @Captor ArgumentCaptor<TabModelActionListener> mTabModelActionListenerCaptor;

    @Captor
    ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    @Captor ArgumentCaptor<TabUnderlineManager.Observer> mTabUnderlineObserverCaptor;

    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();

    private Tab mTab1;
    private Tab mTab2;
    private TabListMediator mMediator;
    private TabListModel mModelList;
    private TabListConfig mTabListConfig;
    private RecyclerView.ViewHolder mFakeViewHolder1;
    private RecyclerView.ViewHolder mFakeViewHolder2;
    private PriceTabData mPriceTabData;
    private String mTab1Domain;
    private String mTab2Domain;
    private String mNewDomain;
    private GURL mFaviconUrl;
    private Resources mResources;
    private Context mContext;
    private SavedTabGroup mSavedTabGroup1;
    private SavedTabGroup mSavedTabGroup2;
    private @Nullable ThumbnailProvider mThumbnailProvider;

    private class MediatorBuilder {
        private @Nullable ThumbnailProvider mThumbnailProvider =
                TabListMediatorUnitTest.this.mThumbnailProvider;
        private @Nullable TabListItemOnClickListenerProvider mTabListItemOnClickListenerProvider =
                TabListMediatorUnitTest.this.mTabListItemOnClickListenerProvider;
        private @Nullable TabListConfig mTabListConfig =
                TabListMediatorUnitTest.this.mTabListConfig;
        private @Nullable TabGridDialogHandler mDialogHandler;
        private @TabComponentId int mComponentId = TabComponentId.GRID_TAB_SWITCHER;
        private @TabActionState int mTabActionState = TabActionState.CLOSABLE;
        private @Nullable UndoBarExplicitTrigger mUndoBarExplicitTrigger =
                TabListMediatorUnitTest.this.mUndoBarExplicitTrigger;
        private @Nullable SnackbarManager mSnackbarManager;
        private int mAllowedSelectionCount;

        public MediatorBuilder setThumbnailProvider(@Nullable ThumbnailProvider thumbnailProvider) {
            mThumbnailProvider = thumbnailProvider;
            return this;
        }

        public MediatorBuilder setTabListItemOnClickListenerProvider(
                @Nullable TabListItemOnClickListenerProvider provider) {
            mTabListItemOnClickListenerProvider = provider;
            return this;
        }

        public MediatorBuilder setTabListConfig(@Nullable TabListConfig tabListConfig) {
            mTabListConfig = tabListConfig;
            return this;
        }

        public MediatorBuilder setDialogHandler(@Nullable TabGridDialogHandler dialogHandler) {
            mDialogHandler = dialogHandler;
            return this;
        }

        public MediatorBuilder setComponentId(@TabComponentId int componentId) {
            mComponentId = componentId;
            return this;
        }

        public MediatorBuilder setTabActionState(@TabActionState int tabActionState) {
            mTabActionState = tabActionState;
            return this;
        }

        public MediatorBuilder setUndoBarExplicitTrigger(
                @Nullable UndoBarExplicitTrigger undoBarExplicitTrigger) {
            mUndoBarExplicitTrigger = undoBarExplicitTrigger;
            return this;
        }

        public MediatorBuilder setSnackbarManager(@Nullable SnackbarManager snackbarManager) {
            mSnackbarManager = snackbarManager;
            return this;
        }

        public MediatorBuilder setAllowedSelectionCount(int allowedSelectionCount) {
            mAllowedSelectionCount = allowedSelectionCount;
            return this;
        }

        public TabListMediator build() {
            return new TabListMediator(
                    mActivity,
                    mModelList,
                    mModalDialogManager,
                    mCurrentTabModelSupplier,
                    mThumbnailProvider,
                    mTabListFaviconProvider,
                    () -> mSelectionDelegate,
                    mTabListItemOnClickListenerProvider,
                    mTabListConfig,
                    mDialogHandler,
                    /* priceWelcomeMessageControllerSupplier= */ null,
                    mComponentId,
                    mTabActionState,
                    mDataSharingTabManager,
                    /* onTabGroupCreation= */ null,
                    mUndoBarExplicitTrigger,
                    mSnackbarManager,
                    mAllowedSelectionCount,
                    /* isSingleContextMode= */ false,
                    /* onDragStateChangedListener= */ CallbackUtils.emptyRunnable());
        }
    }

    @Before
    public void setUp() {
        OptimizationGuideBridgeFactoryJni.setInstanceForTesting(
                mOptimizationGuideBridgeFactoryJniMock);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        when(mOptimizationGuideBridgeFactoryJniMock.getForProfile(mProfile))
                .thenReturn(mOptimizationGuideBridge);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);

        mResources = spy(RuntimeEnvironment.application.getResources());
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mActivity.getTheme()).thenReturn(mContext.getTheme());
        when(mResources.getInteger(R.integer.min_screen_width_bucket)).thenReturn(1);

        mTab1Domain = TAB1_URL.getHost().replace("www.", "");
        mTab2Domain = TAB2_URL.getHost().replace("www.", "");
        //        mTab3Domain = TAB3_URL.getHost().replace("www.", "");
        mNewDomain = new GURL(NEW_URL).getHost().replace("www.", "");
        mFaviconUrl = JUnitTestGURLs.RED_1;

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE, TAB1_URL);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE, TAB2_URL);
        prepareViewHolder(mViewHolder1, TAB1_ID, POSITION1);
        prepareViewHolder(mViewHolder2, TAB2_ID, POSITION2);
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, POSITION1);
        mFakeViewHolder2 = prepareFakeViewHolder(mItemView2, POSITION2);
        when(mItemView1.isAttachedToWindow()).thenReturn(true);
        when(mItemView2.isAttachedToWindow()).thenReturn(true);
        List<Tab> tabs1 = List.of(mTab1);
        List<Tab> tabs2 = List.of(mTab2);
        mSavedTabGroup1 = prepareSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE, SYNC_GROUP_COLOR1, 1);
        mSavedTabGroup2 = prepareSavedTabGroup(SYNC_GROUP_ID2, "", SYNC_GROUP_COLOR2, 2);

        doNothing().when(mTabContentManager).getTabThumbnailWithCallback(anyInt(), any(), any());
        // Mock that tab restoring stage is over.
        when(mTabModel.isTabModelRestored()).thenReturn(true);
        when(mIncognitoTabModel.isTabModelRestored()).thenReturn(true);
        when(mTabModel.getProfile()).thenReturn(mProfile);

        when(mTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);
        when(mIncognitoTabModel.getTabUngrouper()).thenReturn(mIncognitoTabUngrouper);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mIncognitoTabModel.getTabRemover()).thenReturn(mIncognitoTabRemover);
        mCurrentTabModelSupplier.set(mTabModel);
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        when(mTabModel.getTabAt(POSITION1)).thenReturn(mTab1);
        when(mTabModel.getTabAt(POSITION2)).thenReturn(mTab2);
        when(mTabModel.indexOf(mTab1)).thenReturn(POSITION1);
        when(mTabModel.indexOf(mTab2)).thenReturn(POSITION2);
        when(mTabModel.index()).thenReturn(POSITION1);
        when(mIncognitoTabModel.getTabAt(POSITION1)).thenReturn(mTab1);
        when(mIncognitoTabModel.getTabAt(POSITION2)).thenReturn(mTab2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        when(mTabModel.index()).thenReturn(0);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2).iterator());
        when(mTabModel.getCount()).thenReturn(2);
        when(mIncognitoTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2).iterator());
        when(mIncognitoTabModel.getCount()).thenReturn(2);
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForTabAsync(any(TabFaviconMetadata.class), mCallbackCaptor.capture());
        when(mTabListFaviconProvider.getFaviconFromBitmap(any(Bitmap.class), any(GURL.class)))
                .thenReturn(mFavicon);
        doNothing().when(mTabFaviconFetcher).fetch(mCallbackCaptor.capture());
        when(mTabListFaviconProvider.getDefaultFaviconFetcher(anyBoolean()))
                .thenReturn(mTabFaviconFetcher);
        when(mTabListFaviconProvider.getFaviconForTabFetcher(any(Tab.class)))
                .thenReturn(mTabFaviconFetcher);
        when(mTabListFaviconProvider.getFaviconFromBitmapFetcher(
                        any(Bitmap.class), any(GURL.class)))
                .thenReturn(mTabFaviconFetcher);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(tabs1);
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(tabs2);
        mockRepresentativeTabs(mTab1, mTab2);
        when(mTabListItemOnClickListenerProvider.onTabGroupClicked(any(Tab.class)))
                .thenReturn(mOpenGroupActionListener);
        when(mTabListItemOnClickListenerProvider.onTabGroupClicked(anyString()))
                .thenReturn(mOpenGroupActionListener);
        doAnswer(
                        invocation -> {
                            Supplier<TabActionListener> defaultListenerSupplier =
                                    invocation.getArgument(2);
                            return new TabActionButtonData(
                                    TabActionButtonType.OVERFLOW, defaultListenerSupplier.get());
                        })
                .when(mTabListItemOnClickListenerProvider)
                .getTabGroupActionButtonData(any(), any(), any());
        doNothing().when(mActivity).registerComponentCallbacks(mComponentCallbacksCaptor.capture());
        when(mRecyclerView.getLayoutManager()).thenReturn(mGridLayoutManager);
        when(mGridLayoutManager.getSpanCount())
                .thenReturn(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT);
        when(mGridLayoutManager.getSpanSizeLookup()).thenReturn(mSpanSizeLookup);
        doNothing().when(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        when(mTabListFaviconProvider.isInitialized()).thenReturn(true);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1)).thenReturn(mSavedTabGroup1);
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID2)).thenReturn(mSavedTabGroup2);
        when(mTabModel.getTabGroupTitle(any(Token.class))).thenReturn(UNSET_TAB_GROUP_TITLE);
        when(mTabModel.getTabGroupTitle(any(Tab.class))).thenReturn(UNSET_TAB_GROUP_TITLE);
        when(mAccessibilityNodeInfo.getExtras()).thenReturn(new Bundle());

        mModelList = new TabListModel();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        GlicEnabling.setEnabledForTesting(false);
        mTabListConfig = null;

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
                            when(mTabModel.getTabGroupTitle(tabGroupId)).thenReturn(title);
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupTitle(any(), anyString());
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void resetWithNullTabs() {
        mMediator.resetWithListOfTabs(null, null, false);

        verify(mTabModel).removeObserver(any());
        verify(mTabModel).removeTabGroupObserver(any());
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
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        String defaultTitle = TabGroupTitleUtils.getDefaultTitle(mActivity, tabs.size());
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(defaultTitle));
    }

    @Test
    public void updatesTitle_WithStoredTitle_TabGroup() {
        // Mock that tab1 and new tab are in the same group with root ID as TAB1_ID.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_GroupedLayout() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());

        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_Tab_GroupedLayout() {
        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());

        // Ignored as the tab is not in a group.
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void updatesTitle_OnTabGroupTitleChange_Empty_GroupedLayout() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, "");
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo("2 tabs"));
    }

    @Test
    public void updatesColor_OnTabGroupColorChange_Group_Grid() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabModel.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        PropertyModel model = mModelList.get(0).model;
        mMediator.updateTabGroupProperties(mTab1, model, TabGroupColorId.BLUE);
        mMediator.updateFaviconForTab(model, mTab1, null, null);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        var provider = mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Reset() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabModel.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        PropertyModel modelToUpdate = mModelList.get(0).model;
        mMediator.updateTabGroupProperties(mTab1, modelToUpdate, TabGroupColorId.BLUE);
        mMediator.updateFaviconForTab(modelToUpdate, mTab1, null, null);

        PropertyModel model = mModelList.get(0).model;
        var provider = spy(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, provider);

        mMediator.resetWithListOfTabs(null, null, false);
        verify(provider).destroy();
    }

    @Test
    public void tabGroupColorViewProviderDestroyed_Remove() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabModel.setTabGroupColor(TAB_GROUP_ID, TabGroupColorId.BLUE);
        PropertyModel modelToUpdate = mModelList.get(0).model;
        mMediator.updateTabGroupProperties(mTab1, modelToUpdate, TabGroupColorId.BLUE);
        mMediator.updateFaviconForTab(modelToUpdate, mTab1, null, null);

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
        model.set(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER, mTabGroupColorViewProvider);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertNull(model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER));
        verify(mTabGroupColorViewProvider).destroy();
    }

    @Test
    public void updatesFaviconFetcher_SingleTab_GroupedLayout() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        TabListFaviconProvider.TabFavicon[] favicon = new TabListFaviconProvider.TabFavicon[1];
        mModelList
                .get(0)
                .model
                .get(TabProperties.FAVICON_FETCHER)
                .fetch(tabFavicon -> favicon[0] = tabFavicon);
        mCallbackCaptor.getValue().onResult(mFavicon);
        assertEquals(favicon[0], mFavicon);
    }

    @Test
    public void updatesFaviconFetcher_SingleTabGroup_GroupedLayout() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        createTabGroup(List.of(mTab1), TAB_GROUP_ID);

        var oldThumbnailFetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotEquals(
                oldThumbnailFetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_SingleTab_FlatLayout() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        TabListFaviconProvider.TabFavicon[] favicon = new TabListFaviconProvider.TabFavicon[1];
        mModelList
                .get(0)
                .model
                .get(TabProperties.FAVICON_FETCHER)
                .fetch(tabFavicon -> favicon[0] = tabFavicon);
        mCallbackCaptor.getValue().onResult(mFavicon);
        assertEquals(favicon[0], mFavicon);
    }

    @Test
    public void updatesFaviconFetcher_TabGroup_GroupedLayout() {
        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        // Assert that tab1 is in a tab group.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        createTabGroup(List.of(mTab1, newTab), TAB_GROUP_ID);

        var oldThumbnailFetcher = mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotEquals(
                oldThumbnailFetcher, mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_TabGroup_NestedLayout() {
        setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        PropertyModel child1 = mModelList.get(1).model;
        child1.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(child1.get(TabProperties.FAVICON_FETCHER));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap, mFaviconUrl);

        assertNotNull(child1.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpSameDocument() {
        when(mTabListFaviconProvider.getDefaultFavicon(anyBoolean())).thenReturn(mFavicon);

        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        when(mNavigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(mNavigationHandle.isSameDocument()).thenReturn(true);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpSameUrl() {
        when(mTabListFaviconProvider.getDefaultFavicon(anyBoolean())).thenReturn(mFavicon);

        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        when(mNavigationHandle.getUrl()).thenReturn(TAB1_URL);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);

        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpNtpUrl() {
        when(mTabListFaviconProvider.getDefaultFavicon(anyBoolean())).thenReturn(mFavicon);

        GURL ntpUrl = JUnitTestGURLs.NTP_URL;
        when(mNavigationHandle.getUrl()).thenReturn(TAB2_URL);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, ntpUrl);
        mockRepresentativeTabs(mTab1, mTab2, newTab);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(newTab));
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
                .onDidStartNavigationInPrimaryMainFrame(newTab, mNavigationHandle);
        assertNull(mModelList.get(2).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));

        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NoOpGroupedLayout() {
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);

        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);

        assertNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesFaviconFetcher_Navigation_NestedLayout() {
        setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        PropertyModel child1 = mModelList.get(1).model;
        child1.set(TabProperties.FAVICON_FETCHER, null);
        assertNull(child1.get(TabProperties.FAVICON_FETCHER));

        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(TAB2_URL);
        mTabObserverCaptor
                .getValue()
                .onDidStartNavigationInPrimaryMainFrame(mTab1, mNavigationHandle);

        assertNotNull(child1.get(TabProperties.FAVICON_FETCHER));
    }

    @Test
    public void updatesLoadingState_ObserverEvents_VerticalTab() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        PropertyModel model = mModelList.get(0).model;
        assertFalse(model.get(TabProperties.IS_LOADING));

        // Same document navigation should not trigger loading state
        mTabObserverCaptor.getValue().onLoadStarted(mTab1, false);
        assertFalse(model.get(TabProperties.IS_LOADING));

        // Different document navigation should trigger loading state
        mTabObserverCaptor.getValue().onLoadStarted(mTab1, true);
        assertTrue(model.get(TabProperties.IS_LOADING));

        // Same document load stopped should not change state
        mTabObserverCaptor.getValue().onLoadStopped(mTab1, false);
        assertTrue(model.get(TabProperties.IS_LOADING));

        // Different document load stopped should reset state
        mTabObserverCaptor.getValue().onLoadStopped(mTab1, true);
        assertFalse(model.get(TabProperties.IS_LOADING));

        // Crash should reset state
        mTabObserverCaptor.getValue().onLoadStarted(mTab1, true);
        assertTrue(model.get(TabProperties.IS_LOADING));
        mTabObserverCaptor.getValue().onCrash(mTab1);
        assertFalse(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void updatesLoadingState_NtpIgnored_VerticalTab() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        when(mTab1.getUrl()).thenReturn(new GURL("chrome-native://newtab/"));
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        PropertyModel model = mModelList.get(0).model;
        assertFalse(model.get(TabProperties.IS_LOADING));

        mTabObserverCaptor.getValue().onLoadStarted(mTab1, true);
        assertFalse(model.get(TabProperties.IS_LOADING));
    }

    @Test
    public void updatesLoadingState_DisabledWhenUnsupported() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertFalse(mTabListConfig.supportsTabLoadingState);

        PropertyModel model = mModelList.get(0).model;
        assertFalse(model.get(TabProperties.IS_LOADING));

        mTabObserverCaptor.getValue().onLoadStarted(mTab1, /* toDifferentDocument= */ true);
        assertFalse(model.get(TabProperties.IS_LOADING));
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

        verify(mTabListItemOnClickListenerProvider)
                .onTabSelecting(mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testTabSelection_LogsUserAction_Vertical() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        UserActionTester userActionTester = new UserActionTester();

        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        assertTrue(userActionTester.getActions().contains("MobileTabSwitched.VerticalTabs"));
        userActionTester.tearDown();
    }

    @Test
    public void testTabSelection_LogsUserAction_Vertical_Pinned() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        when(mTab1.getIsPinned()).thenReturn(true);

        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        UserActionTester userActionTester = new UserActionTester();

        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_ID),
                        /* triggeringMotion= */ null);

        assertTrue(userActionTester.getActions().contains("MobileTabSwitched.VerticalTabsPinned"));
        userActionTester.tearDown();
    }

    @Test
    public void testTabSelection_MultiSelect_ShiftClick_Vertical() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        when(mMotionEvent.getMetaState()).thenReturn(KeyEvent.META_SHIFT_ON);
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);

        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, mTab1.getId(), info);

        // Verify normal selection is bypassed when multi-selecting.
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testTabSelection_MultiSelectDisabled_ShiftClick_SelectsTab() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertFalse(mTabListConfig.supportsModifierMultiSelect);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        when(mMotionEvent.getMetaState()).thenReturn(KeyEvent.META_SHIFT_ON);
        when(mMotionEvent.getPointerCount()).thenReturn(0);
        MotionEventInfo info = MotionEventInfo.fromMotionEvent(mMotionEvent);

        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, mTab1.getId(), info);

        // Verify normal selection occurs when modifier multi-selection is disabled.
        verify(mTabListItemOnClickListenerProvider).onTabSelecting(mTab1.getId());
    }

    @Test
    public void testOnTabsSelectionChanged_MultiSelectEnabled_UpdatesProperty() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        assertTrue(mTabListConfig.supportsModifierMultiSelect);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        when(mTabModel.isTabMultiSelected(mTab1.getId())).thenReturn(true);
        when(mTabModel.isTabMultiSelected(mTab2.getId())).thenReturn(false);

        mTabModelObserverCaptor.getValue().onTabsSelectionChanged();

        assertTrue(mModelList.get(0).model.get(TabProperties.IS_MULTI_SELECTED));
        assertFalse(mModelList.get(1).model.get(TabProperties.IS_MULTI_SELECTED));
    }

    @Test
    public void testOnTabsSelectionChanged_MultiSelectDisabled_NoOp() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertFalse(mTabListConfig.supportsModifierMultiSelect);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        when(mTabModel.isTabMultiSelected(anyInt())).thenReturn(true);

        mTabModelObserverCaptor.getValue().onTabsSelectionChanged();

        verify(mTabModel, never()).isTabMultiSelected(anyInt());
    }

    @Test
    public void sendsOpenGroupSignalCorrectly_SingleTabGroup() {
        List<Tab> tabs = List.of(mTab1);
        createTabGroup(tabs, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);
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
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);
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
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
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

        TabClosureParams params = TabClosureParams.closeTab(mTab2).allowUndo(true).build();
        verify(mTabRemover)
                .closeTabs(
                        eq(params),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
        assertTrue(mModelList.get(1).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));

        when(mTabModel.getRelatedTabList(anyInt())).thenReturn(new ArrayList<>());
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
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        initAndAssertAllProperties();
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
    public void sendsCloseSignalCorrectly_Group_TriggeringMotionFromMouse_DisallowUndo() {
        createTabGroup(List.of(mTab1, mTab2), TAB_GROUP_ID);
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_ID),
                        MotionEventTestUtils.createMouseMotionInfo(
                                /* downTime= */ SystemClock.uptimeMillis(),
                                /* eventTime= */ SystemClock.uptimeMillis() + 200,
                                MotionEvent.ACTION_UP));

        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.isTabGroup && !params.allowUndo),
                        /* allowDialog= */ eq(true),
                        /* listener= */ any());
    }

    @Test
    public void sendsCloseSignalCorrectly_Group_TriggeringMotionFromTouch_AllowUndo() {
        createTabGroup(List.of(mTab1, mTab2), TAB_GROUP_ID);
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(
                        mItemView1,
                        mModelList.get(0).model.get(TabProperties.TAB_ID),
                        MotionEventTestUtils.createTouchMotionInfo(
                                /* downTime= */ SystemClock.uptimeMillis(),
                                /* eventTime= */ SystemClock.uptimeMillis() + 200,
                                MotionEvent.ACTION_UP));

        verify(mTabRemover)
                .closeTabs(
                        argThat(params -> params.isTabGroup && params.allowUndo),
                        /* allowDialog= */ eq(true),
                        /* listener= */ any());
    }

    @Test
    public void sendsCloseSignalCorrectly_ActionOnAllRelatedTabs() {
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
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        initAndAssertAllProperties();
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
    public void sendsCloseSignalCorrectly_VerticalTabs() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
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

        TabClosureParams params =
                TabClosureParams.closeTab(mTab2)
                        .allowUndo(true)
                        .tabClosingSource(TabClosingSource.VERTICAL_TAB_STRIP)
                        .build();
        verify(mTabRemover)
                .closeTabs(
                        eq(params),
                        /* allowDialog= */ eq(true),
                        mTabModelActionListenerCaptor.capture());
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithGroup() {
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveRelatedTabs(eq(TAB1_ID), eq(1));
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
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.setRecyclerView(mRecyclerView);

        when(mRecyclerView.getAdapter()).thenReturn(mAdapter);

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mFakeViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabModel).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    // Regression test for https://crbug.com/40871078
    @Test
    public void handlesGroupMergeCorrectly_InOrder() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.getTabAt(3)).thenReturn(tab4);
        when(mItemView3.isAttachedToWindow()).thenReturn(true);
        when(mItemView4.isAttachedToWindow()).thenReturn(true);

        RecyclerView.ViewHolder fakeViewHolder3 = prepareFakeViewHolder(mItemView3, 2);
        RecyclerView.ViewHolder fakeViewHolder4 = prepareFakeViewHolder(mItemView4, 3);

        List<Tab> tabs = List.of(mTab1, mTab2, tab3, tab4);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(4));

        // Merge 2 to 1.
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.setRecyclerView(mRecyclerView);

        when(mRecyclerView.getAdapter()).thenReturn(mAdapter);

        itemTouchHelperCallback.onSelectedChanged(
                mFakeViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabModel).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab1, mTab2));
        when(mTabModel.indexOf(mTab1)).thenReturn(POSITION1);
        when(mTabModel.indexOf(mTab2)).thenReturn(POSITION2);
        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(3));
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, 0);
        fakeViewHolder3 = prepareFakeViewHolder(mItemView3, 1);
        fakeViewHolder4 = prepareFakeViewHolder(mItemView4, 2);

        // Merge 4 to 3.
        mockRepresentativeTabs(mTab1, tab3, tab4);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(2);
        itemTouchHelperCallback.setRecyclerView(mRecyclerView);

        itemTouchHelperCallback.onSelectedChanged(
                fakeViewHolder4, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabModel).mergeTabsToGroup(eq(TAB4_ID), eq(TAB3_ID));
        verify(mGridLayoutManager).removeView(mItemView4);
        verify(mTracker, times(2)).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabModel.getRelatedTabList(TAB4_ID)).thenReturn(List.of(tab3, tab4));
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(tab3, tab4));
        when(mTabModel.indexOf(tab3)).thenReturn(2);
        when(mTabModel.indexOf(tab4)).thenReturn(3);
        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(tab4, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(2));
        mFakeViewHolder1 = prepareFakeViewHolder(mItemView1, 0);
        fakeViewHolder3 = prepareFakeViewHolder(mItemView3, 1);

        // Merge 3 to 1.
        mockRepresentativeTabs(mTab1, tab3);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(0);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(1);
        itemTouchHelperCallback.setRecyclerView(mRecyclerView);

        itemTouchHelperCallback.onSelectedChanged(
                fakeViewHolder3, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabModel).mergeTabsToGroup(eq(TAB3_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView3);
        verify(mTracker, times(3)).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));

        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(mTab1, mTab2, tab3, tab4));
        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(tab3, /* isDestinationTab= */ false);

        assertThat(mModelList.size(), equalTo(1));
    }

    @Test
    public void sendsUngroupSignalCorrectly() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setRecyclerView(mRecyclerView);

        when(mRecyclerView.getAdapter()).thenReturn(mAdapter);
        when(mAdapter.getItemCount()).thenReturn(1);

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
        createTabGroup(List.of(mTab1, mTab2), TAB_GROUP_ID);
        mockRepresentativeTabs(mTab1);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(POSITION1);
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
    public void tabAddition_Restore_NestedLayout() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        createTabGroup(List.of(mTab1, mTab2), TAB_GROUP_ID);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);
        mockRepresentativeTabs(mTab1, mTab2);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(POSITION1);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(POSITION1);
        mModelList.clear();

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab2,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        // In nested layout, restoring the first tab in a group adds the group header
        // and the tab itself (2 cards).
        assertThat(mModelList.size(), equalTo(2));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        mTab1,
                        TabLaunchType.FROM_RESTORE,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        // Restoring the second tab adds its card to the list.
        assertThat(mModelList.size(), equalTo(3));
    }

    @Test
    public void tabAddition_GroupedLayout() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        mockRepresentativeTabs(mTab1, mTab2, newTab);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(newTab));
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
    public void tabAddition_FlatLayout_Dialog_delayAdd() {
        mMediator.setComponentIdForTesting(TabComponentId.TAB_GRID_DIALOG_IN_SWITCHER);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1, mTab2, newTab));
        mockRepresentativeTabs(mTab1, mTab2, newTab);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(newTab));
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

        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2, newTab).iterator());
        when(mTabModel.getTabAt(2)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(3);

        // Hide dialog to complete and ensure the delayed tab is not added.
        mMediator.resetWithListOfTabs(null, null, false);
        verify(mTabModel).removeObserver(any());
        verify(mTabModel).removeTabGroupObserver(any());

        mMediator.postHiding();
        // Assert tab was not added.
        assertThat(mModelList.size(), equalTo(0));
    }

    @Test
    public void tabAddition_GroupedLayout_delayAdd() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1, mTab2, newTab));
        mockRepresentativeTabs(mTab1, mTab2, newTab);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(newTab));
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

        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2, newTab).iterator());
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
        verify(mTabModel).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserverCaptor.getValue());
    }

    @Test
    public void tabAddition_GroupedLayout_delayAdd_WithUnexpectedUpdate() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(newTab));
        mockRepresentativeTabs(mTab1, mTab2, newTab);
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
        when(mTabModel.getTabAt(0)).thenReturn(newTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(newTab).iterator());
        mockRepresentativeTabs(newTab);

        // Hide GTS to complete tab addition and selection.
        mMediator.postHiding();
        // Assert tab added and selected. Assert old tab is de-selected.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertNull(mMediator.getTabToAddDelayedForTesting());
        verify(mTab1).removeObserver(mTabObserverCaptor.getValue());
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        verify(newTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mTabModel).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserverCaptor.getValue());
    }

    @Test
    public void tabAddition_GroupedLayout_Skip() {
        // Add a new tab to the group with mTab2.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        mockRepresentativeTabs(mTab1, mTab2);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(mTab2, newTab));
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
    public void tabAddition_GroupedLayout_Middle() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        mockRepresentativeTabs(mTab1, newTab, mTab2);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(newTab));
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
    public void tabAddition_FlatLayout_Dialog_End() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        when(mTabModel.isTabModelRestored()).thenReturn(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2, newTab).iterator());
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getRelatedTabList(eq(TAB1_ID))).thenReturn(List.of(mTab1, mTab2, newTab));
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
    public void tabAddition_FlatLayout_Dialog_Middle() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        when(mTabModel.isTabModelRestored()).thenReturn(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, newTab, mTab2).iterator());
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getRelatedTabList(eq(TAB1_ID))).thenReturn(List.of(mTab1, newTab, mTab2));
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
    public void tabAddition_FlatLayout_Dialog_Skip() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        when(mTabModel.isTabModelRestored()).thenReturn(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        // newTab is of another group.
        when(mTabModel.getRelatedTabList(eq(TAB1_ID))).thenReturn(List.of(mTab1, mTab2));
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
    public void tabAddition_NestedLayout_ExpandedGroup() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        assertThat(mModelList.size(), equalTo(3));

        // Create a new tab to add to the group.
        int newTabId = 789;
        Tab newTab = prepareTab(newTabId, "New Tab", JUnitTestGURLs.EXAMPLE_URL);
        when(newTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        // Update the mock to include the new tab.
        List<Tab> tabs = List.of(mTab1, tab3, newTab);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabById(newTabId)).thenReturn(newTab);
        mockTabIndexes(mTab1, tab3, newTab);
        when(mTabModel.getRelatedTabList(newTabId)).thenReturn(tabs);
        when(mTabModel.isTabInTabGroup(newTab)).thenReturn(true);

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        // Verify it inserted the new tab as a child row, and did not create a second header.
        assertThat(mModelList.size(), equalTo(4));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModelList.get(3).model.get(TabProperties.TAB_ID), equalTo(newTabId));
    }

    @Test
    public void tabAddition_NestedLayout_CollapsedGroup() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ true);

        assertThat(mModelList.size(), equalTo(1));

        int newTabId = 789;
        Tab newTab = prepareTab(newTabId, "New Tab", JUnitTestGURLs.EXAMPLE_URL);
        when(newTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);

        // Update the mock to include the new tab.
        List<Tab> tabs = List.of(mTab1, tab3, newTab);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabById(newTabId)).thenReturn(newTab);
        mockTabIndexes(mTab1, tab3, newTab);
        when(mTabModel.getRelatedTabList(newTabId)).thenReturn(tabs);
        when(mTabModel.isTabInTabGroup(newTab)).thenReturn(true);

        // Add the tab.
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        // Verify it did not insert any child rows because the group is collapsed.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
    }

    @Test
    public void tabAddition_NestedLayout_PinnedTab_ToBoundary() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);
        mMediator.resetWithListOfTabs(null, null, false);

        // Setup mTab1 as pinned, mTab2 as regular.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(false);

        // Prepare new tab: tab4 (pinned).
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(tab4.getIsPinned()).thenReturn(true);

        mockTabIndexes(mTab1, mTab2);

        // Reset list with mTab1 (pinned) and mTab2 (regular).
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        // List contains: [0] Pinned Tab 1, [1] Regular Tab 2.
        assertEquals(2, mModelList.size());

        mockTabIndexes(mTab1, tab4, mTab2);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        tab4,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        // List should contain: [0] Pinned Tab 1, [1] Pinned Tab 4, [2] Regular Tab 2.
        assertEquals(3, mModelList.size());
        assertEquals(TAB4_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertTrue(mModelList.get(1).model.get(TabProperties.IS_PINNED));
        assertEquals(TAB2_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void tabAddition_NestedLayout_RegularTab_AfterPinnedSection() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);
        mMediator.resetWithListOfTabs(null, null, false);

        // Setup mTab1 as pinned, mTab2 as regular.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(false);

        // Prepare new tab: tab3 (regular).
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(tab3.getIsPinned()).thenReturn(false);

        mockTabIndexes(mTab1, mTab2, tab3);

        // Reset list with mTab1 (pinned) and mTab2 (regular).
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        // List contains: [0] Pinned Tab 1, [1] Regular Tab 2.
        assertEquals(2, mModelList.size());

        // Add regular tab3 (index 2). It should go after all regular tabs.
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        tab3,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        // List should contain: [0] Pinned Tab 1, [1] Regular Tab 2, [2] Regular Tab 3.
        assertEquals(3, mModelList.size());
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void tabAddition_withArchivedTabsMessagePresent() {
        mModelList.clear();
        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(ARCHIVED_TABS_MESSAGE);
        when(mPropertyModel.containsKeyEqualTo(MESSAGE_TYPE, ARCHIVED_TABS_MESSAGE))
                .thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.ARCHIVED_TABS_MESSAGE, mPropertyModel);

        assertThat(mModelList.size(), equalTo(1));

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        mockRepresentativeTabs(newTab);
        when(mTabModel.getRelatedTabList(eq(TAB3_ID))).thenReturn(List.of(newTab));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(MESSAGE_TYPE), equalTo(ARCHIVED_TABS_MESSAGE));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
    }

    @Test
    public void testDidMoveTab_NestedLayout_Standalone() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        // Assume that moveTab in TabModel is finished.
        mockTabIndexes(mTab2, mTab1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testDidMoveTab_GroupedLayout_Standalone() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        mockTabIndexes(mTab2, mTab1);
        mockRepresentativeTabs(mTab2, mTab1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testDidMoveTab_FlatLayout_SkipStandalone() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        mockTabIndexes(mTab2, mTab1);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);

        // Should skip, so no change in ModelList.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testDidMoveTab_NestedLayout_SkipGrouped() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        // Assume that moveTab in TabModel is finished.
        mockTabIndexes(mTab2, mTab1);

        when(mTab2.getTabGroupId()).thenReturn(new Token(1, 1));

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);

        // Should skip, so no change in ModelList.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testDidMoveTab_NestedLayout_SkipUngrouping() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        // Assume that moveTab in TabModel is finished.
        mockTabIndexes(mTab2, mTab1);

        // Mock UI still thinking the tab is grouped.
        mModelList.get(1).model.set(TabProperties.TAB_GROUP_ID, new Token(1, 1));

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);

        // Should skip, so no change in ModelList.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testDidMoveTab_GroupedLayout_SkipUngrouping() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Assume that moveTab in TabModel is finished.
        mockTabIndexes(mTab2, mTab1);
        mockRepresentativeTabs(mTab2, mTab1);

        // Mock UI still thinking the tab is grouped via group card.
        mModelList.get(1).model.set(TabProperties.TAB_GROUP_HEADER_ID, new Token(1, 1));

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);

        // Should skip, so no change in ModelList.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void testUngroupAllTabs_GroupedLayout() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();

        // Group has mTab1 (representative) and mTab2.
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
        mockRepresentativeTabs(mTab1);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        assertEquals(1, mModelList.size());
        PropertyModel groupCardModel = mModelList.get(0).model;
        assertEquals(TAB1_ID, groupCardModel.get(TabProperties.TAB_ID));
        assertEquals(TAB_GROUP_ID, groupCardModel.get(TabProperties.TAB_GROUP_HEADER_ID));

        // Ungroup mTab2 (non-representative).
        when(mTab2.getTabGroupId()).thenReturn(null);
        mockTabIndexes(mTab1, mTab2);
        mockRepresentativeTabs(mTab1, mTab2);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);
        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION2, POSITION1);
        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        // Group now only has mTab1, but in TabModel it's already ungrouped.
        when(mTab1.getTabGroupId()).thenReturn(null);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(0);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);

        // Move the remaining representative tab.
        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION2, POSITION1);

        // Call didMoveTabOutOfGroup for the last tab.
        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION1);

        // Verify model list now contains two standalone tabs.
        assertEquals(2, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void testHidingClearsCardState() {
        initAndAssertAllProperties();
        TabGridItemTouchHelperCallback callback = getItemTouchHelperCallback();
        callback.setRecyclerView(mRecyclerView);

        when(mViewHolder1.getBindingAdapterPosition()).thenReturn(POSITION1);
        when(mViewHolder1.getItemViewType()).thenReturn(UiType.TAB);

        callback.onSelectedChanged(mViewHolder1, ItemTouchHelper.ACTION_STATE_DRAG);
        assertThat(
                mModelList.get(POSITION1).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.SELECTED_CARD_ZOOM_IN));
        assertThat(mModelList.get(POSITION1).model.get(CARD_ALPHA), equalTo(0.8f));

        mMediator.postHiding();

        assertThat(
                mModelList.get(POSITION1).model.get(CardProperties.CARD_ANIMATION_STATUS),
                equalTo(AnimationStatus.SELECTED_CARD_ZOOM_OUT));
        assertThat(mModelList.get(POSITION1).model.get(CARD_ALPHA), equalTo(1f));
    }

    @Test
    public void testTabGroupIdAndHeaderIdMutualExclusivity() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        Tab childTab = prepareTab(TAB1_ID, TAB1_TITLE, TAB1_URL);
        when(mTabModel.isTabInTabGroup(childTab)).thenReturn(true);
        when(childTab.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(childTab));
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(childTab));
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);

        // Force the group to be expanded so resetWithListOfTabs inserts both the header and the
        // child.
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);

        // Reset the mediator. It will process childTab, realize it's part of a group,
        // insert the header card first, and then since it's expanded, insert the child card.
        mMediator.resetWithListOfTabs(List.of(childTab), null, false);

        // The first card should be the group header.
        PropertyModel headerModel = mModelList.get(0).model;
        assertEquals(TAB_GROUP_ID, headerModel.get(TabProperties.TAB_GROUP_HEADER_ID));
        assertNull(headerModel.get(TabProperties.TAB_GROUP_ID));
        assertEquals(TAB_GROUP, headerModel.get(CARD_TYPE));
        assertEquals(TAB1_ID, headerModel.get(TabProperties.TAB_ID));

        // The second card should be the child tab.
        PropertyModel childModel = mModelList.get(1).model;
        assertEquals(TAB_GROUP_ID, childModel.get(TabProperties.TAB_GROUP_ID));
        assertNull(childModel.get(TabProperties.TAB_GROUP_HEADER_ID));
        assertEquals(TAB, childModel.get(CARD_TYPE));
        assertEquals(TAB1_ID, childModel.get(TabProperties.TAB_ID));

        // indexFromTabId should skip the header card and find the child tab.
        assertEquals(1, mModelList.indexFromTabId(TAB1_ID));
    }

    @Test
    public void tabSelection() {
        PropertyModel model0 = mModelList.get(0).model;
        PropertyModel model1 = mModelList.get(1).model;
        ThumbnailFetcher tab1Fetcher = model0.get(TabProperties.THUMBNAIL_FETCHER);
        ThumbnailFetcher tab2Fetcher = model1.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotNull(tab1Fetcher);
        assertNotNull(tab2Fetcher);
        tab1Fetcher = mThumbnailFetcher1;
        model0.set(TabProperties.THUMBNAIL_FETCHER, tab1Fetcher);
        tab2Fetcher = mThumbnailFetcher2;
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
        List<Tab> tabs = List.of(mTab2, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

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

    @Test
    public void tabSelection_Nested_Header() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);
        int tab3Index = mModelList.indexFromTabId(TAB3_ID);
        mModelList.removeAt(tab3Index);

        PropertyModel headerModel = mModelList.get(0).model;

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(tab3, TabSelectionType.FROM_UNDO, mTab2.getId());

        // Verify the header is not selected.
        assertThat(headerModel.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    // Regression test for: crbug.com/349773923.
    @Test
    public void tabSelection_LeaveGroupClears() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab2, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

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
        mMediator = new MediatorBuilder().setUndoBarExplicitTrigger(null).build();
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
        // mTabModelObserverCaptor captures on every resetWithListOfTabs call.
        verify(mTabModel, times(2)).addObserver(mTabModelObserverCaptor.capture());

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

        createTabGroup(List.of(mTab2), TAB_GROUP_ID);

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(
                mModelList.get(1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void tabClosureUndone_Nested_ExpandedGroup() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);
        assertThat(mModelList.size(), equalTo(2)); // Header, tab1

        // Simulate closing last tab in group. This should also remove the header.
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);
        mTabGroupObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB1_ID, TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);
        assertThat(mModelList.size(), equalTo(0));

        // Simulate undoing the closure of the group.
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(CUSTOMIZED_DIALOG_TITLE1);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        // Verify it created exactly one header and inserted tab1.
        assertThat(mModelList.size(), equalTo(2));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));

        // Undo tab3
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, tab3));

        mTabModelObserverCaptor.getValue().tabClosureUndone(tab3);

        // Verify it inserted tab3 as a child row, and did not create a second header.
        assertThat(mModelList.size(), equalTo(3));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
    }

    @Test
    public void tabClosureUndone_Nested_CollapsedGroup() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ true);

        assertThat(mModelList.size(), equalTo(1));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));

        // Simulate closing the collapsed group.
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);
        mTabGroupObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB1_ID, TAB_GROUP_ID, DidRemoveTabGroupReason.CLOSE);
        assertThat(mModelList.size(), equalTo(0));

        // Simulate undoing the closure of the group.
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn(CUSTOMIZED_DIALOG_TITLE1);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        // Verify it created exactly one header and no child tabs.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Undo tab3.
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1, tab3));
        mTabModelObserverCaptor.getValue().tabClosureUndone(tab3);

        // Verify it still only has one header and no child tabs.
        assertThat(mModelList.size(), equalTo(1));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TAB_GROUP_HEADER_ID),
                equalTo(TAB_GROUP_ID));
        assertThat(
                mModelList.get(0).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void tabClosureUndone_RecordsUserAction_GridTabSwitcher() {
        var userActionTester = new UserActionTester();
        initAndAssertAllProperties();

        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, TAB2_ID, /* triggeringMotion= */ null);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertTrue(userActionTester.getActions().contains("GridTabSwitch.UndoCloseTab"));
    }

    @Test
    public void tabClosureUndone_RecordsUserAction_VerticalTabs() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.GRID);
        var userActionTester = new UserActionTester();
        initAndAssertAllProperties();

        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, TAB2_ID, /* triggeringMotion= */ null);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab2);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertTrue(userActionTester.getActions().contains("Android.VerticalTabs.UndoCloseTab"));
    }

    @Test
    public void destroy_ClearsTabClosedFromTracking() {
        var userActionTester = new UserActionTester();
        initAndAssertAllProperties();

        mModelList
                .get(1)
                .model
                .get(TabProperties.TAB_ACTION_BUTTON_DATA)
                .tabActionListener
                .run(mItemView2, TAB2_ID, /* triggeringMotion= */ null);

        mMediator.destroy();

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        assertFalse(userActionTester.getActions().contains("GridTabSwitch.UndoCloseTab"));
    }

    @Test
    public void testCloseTabInGroup_withArchivedTabsMessagePresent() {
        when(mTabModel.tabGroupExists(any())).thenReturn(true);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);
        assertThat(mModelList.size(), equalTo(2));

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(ARCHIVED_TABS_MESSAGE);
        mMediator.addSpecialItemToModel(0, UiType.ARCHIVED_TABS_MESSAGE, mPropertyModel);
        assertThat(mModelList.size(), equalTo(3));

        // This crashed previously when it tried to update the message instead of the tab group
        // (crbug.com/347970497).
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(newTab);
        verify(mPropertyModel, times(0)).set(eq(TabProperties.TAB_ID), anyInt());
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

    @Test
    public void didMoveTabOutOfGroup_CreatesSingleTabGroup() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(1, mModelList.size());

        // Mock that mTab2 is moved out of the group, but immediately put into a new single tab
        // group.
        Token newGroupId = new Token(3L, 4L);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTab2.getTabGroupId()).thenReturn(newGroupId);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);
        when(mTabModel.getTabCountForGroup(newGroupId)).thenReturn(1);
        when(mTabModel.tabGroupExists(newGroupId)).thenReturn(true);
        when(mTabModel.getGroupLastShownTabId(newGroupId)).thenReturn(TAB2_ID);
        mockRepresentativeTabs(mTab1, mTab2);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertEquals(2, mModelList.size());
        // Verify that mTab2's new card was created as a Tab Group Header card!
        assertNotNull(mModelList.get(POSITION2).model.get(TabProperties.TAB_GROUP_CARD_COLOR));
        assertNull(mModelList.get(POSITION2).model.get(TabProperties.TAB_GROUP_ID));
    }

    @Test
    public void didMoveTabOutOfGroup_UndoGrouped_OneAdjacentTab() {
        // Assume there are 3 tabs in TabModel, mTab2 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Assume undo grouping mTab2 with mTab1.
        mockRepresentativeTabs(mTab1, mTab2, tab3);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(2));
    }

    @Test
    public void didMoveTabOutOfGroup_UndoForwardGrouped_OneTab() {
        // Assume there are 3 tabs in TabModel, tab3 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, mTab2);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Assume undo grouping tab3 with mTab1.
        mockRepresentativeTabs(mTab1, mTab2, tab3);
        when(mTabModel.isTabInTabGroup(tab3)).thenReturn(false);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(tab3, POSITION1);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(2));
    }

    @Test
    public void didMoveTabOutOfGroup_UndoBackwardGrouped_OneTab() {
        // Assume there are 3 tabs in TabModel, mTab1 just grouped with mTab2;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab2, tab3);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(2));

        // Assume undo grouping mTab1 from mTab2.
        mockRepresentativeTabs(mTab1, mTab2, tab3);
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(false);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(2));
    }

    @Test
    public void didMoveTabOutOfGroup_UndoForwardGrouped_BetweenGroups() {
        // Assume there are 3 tabs in TabModel, tab3, tab4, just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2, tab3, tab4).iterator());
        when(mTabModel.getCount()).thenReturn(4);
        List<Tab> tabs = List.of(mTab1);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(1));

        // Assume undo grouping tab3 with mTab1.

        // Undo tab 3.
        List<Tab> relatedTabs = List.of(tab3);
        mockRepresentativeTabs(mTab1, tab3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(tab4);
        when(mTabModel.getTabAt(3)).thenReturn(tab3);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(POSITION1);
        when(mTabModel.representativeIndexOf(tab4)).thenReturn(POSITION1);
        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(tab4)).thenReturn(2);
        when(mTabModel.indexOf(tab3)).thenReturn(3);
        when(mTabModel.isTabInTabGroup(tab3)).thenReturn(false);
        when(mTabModel.isTabInTabGroup(tab4)).thenReturn(true);
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(relatedTabs);
        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(tab3, POSITION1);
        assertThat(mModelList.size(), equalTo(2));
        assertThat(mModelList.indexFromTabId(TAB1_ID), equalTo(0));
        assertThat(mModelList.indexFromTabId(TAB2_ID), equalTo(-1));
        assertThat(mModelList.indexFromTabId(TAB3_ID), equalTo(1));
        assertThat(mModelList.indexFromTabId(TAB4_ID), equalTo(-1));

        // Undo tab 4
        relatedTabs = List.of(tab3, tab4);
        when(mTabModel.representativeIndexOf(tab4)).thenReturn(POSITION2);
        when(mTabModel.indexOf(tab3)).thenReturn(2);
        when(mTabModel.indexOf(tab4)).thenReturn(3);
        when(mTabModel.isTabInTabGroup(tab3)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(tab4)).thenReturn(true);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.getTabAt(3)).thenReturn(tab4);
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(relatedTabs);
        when(mTabModel.getRelatedTabList(TAB4_ID)).thenReturn(relatedTabs);
        when(tab4.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(2);
        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(tab4, POSITION1);
        assertThat(mModelList.size(), equalTo(2));

        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(tab4, /* isDestinationTab= */ false);

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
        assertEquals(
                TAB1_TITLE,
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true));

        when(mTab1.getTitle()).thenReturn("");
        assertEquals(
                TAB1_URL.getSpec(),
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true));
    }

    @Test
    public void getLatestTitle_FlatLayout_Dialog() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        createTabGroup(Collections.singletonList(mTab1), TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabModel.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabModel.getTabGroupTitle(mTab1.getTabGroupId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

        // Even if we have a stored title, we only show it in tab switcher.
        assertThat(
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true),
                equalTo(TAB1_TITLE));
    }

    @Test
    public void getLatestTitle_SingleTabGroupSupported_GroupedLayout() {
        createTabGroup(Collections.singletonList(mTab1), TAB_GROUP_ID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabModel.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabModel.getTabGroupTitle(mTab1.getTabGroupId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 is a single tab.
        List<Tab> tabs = List.of(mTab1);
        createTabGroup(tabs, TAB_GROUP_ID);

        // We never show stored title for single tab.
        assertThat(
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void getLatestTitle_SingleTabGroupNotSupported_GroupedLayout() {
        createTabGroup(Collections.singletonList(mTab1), TAB_GROUP_ID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabModel.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabModel.getTabGroupTitle(mTab1.getTabGroupId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 is a single tab.
        List<Tab> tabs = List.of(mTab1);
        createTabGroup(tabs, null);
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(false);

        // We never show stored title for single tab.
        assertThat(
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true),
                equalTo(TAB1_TITLE));
    }

    @Test
    public void getLatestTitle_Stored_GroupedLayout() {
        createTabGroup(Collections.singletonList(mTab1), TAB_GROUP_ID);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        mTabModel.setTabGroupTitle(mTab1.getTabGroupId(), CUSTOMIZED_DIALOG_TITLE1);
        assertThat(
                mTabModel.getTabGroupTitle(mTab1.getTabGroupId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

        assertThat(
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void getLatestTitle_Default_GroupedLayout() {
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

        assertThat(
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ true),
                equalTo("2 tabs"));
    }

    @Test
    public void getLatestTitle_NoDefault_GroupedLayout() {
        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

        assertThat(
                mMediator.getLatestTitleForTabOrGroup(mTab1, null, /* useDefault= */ false),
                equalTo(""));
    }

    @Test
    public void updateTabGroupTitle_GroupedLayout() {
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand Cool Tabs tab group with 2 tabs, color Grey.";
        assertThat(mModelList.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Mock that tab1 and newTab are in the same group and group root id is TAB1_ID.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);
        when(mTabModel.getRepresentativeTabAt(POSITION1)).thenReturn(mTab1);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(POSITION1);

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());

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
    public void updateTabGroupTitle_SingleTab_GroupedLayout() {
        setUpTabGroupCardDescriptionString();
        String targetString = "Expand Cool Tabs tab group with 1 tab, color Grey.";
        assertThat(mModelList.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        createTabGroup(List.of(mTab1), TAB_GROUP_ID);
        when(mTabModel.getRepresentativeTabAt(POSITION1)).thenReturn(mTab1);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(POSITION1);

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());

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
        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        verify(mTabModel).setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
    }

    @Test
    public void tabGroupTitleEditor_deleteTitle() {
        mTabModel.deleteTabGroupTitle(TAB_GROUP_ID);
        verify(mTabModel).deleteTabGroupTitle(TAB_GROUP_ID);
    }

    @Test
    public void addSpecialItem() {
        mMediator.resetWithListOfTabs(null, null, false);

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, mPropertyModel);

        assertFalse(mModelList.isEmpty());
        assertEquals(UiType.PRICE_MESSAGE, mModelList.get(0).type);
    }

    @Test
    public void addSpecialItem_notPersistOnReset() {
        mMediator.resetWithListOfTabs(null, null, false);

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, mPropertyModel);
        assertEquals(UiType.PRICE_MESSAGE, mModelList.get(0).type);

        List<Tab> tabs = List.of(mTab1, mTab2);
        mMediator.resetWithListOfTabs(tabs, null, /* quickMode= */ false);
        assertThat(mModelList.size(), equalTo(2));
        assertNotEquals(UiType.PRICE_MESSAGE, mModelList.get(0).type);
        assertNotEquals(UiType.PRICE_MESSAGE, mModelList.get(1).type);

        mMediator.addSpecialItemToModel(1, UiType.PRICE_MESSAGE, mPropertyModel);
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

        @MessageType int expectedMessageType = IPH;
        @MessageType int wrongMessageType = PRICE_MESSAGE;
        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        when(mPropertyModel.containsKeyEqualTo(MESSAGE_TYPE, IPH)).thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.IPH_MESSAGE, mPropertyModel);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, wrongMessageType);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, expectedMessageType);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void removeSpecialItem_Message_PriceMessage() {
        mMediator.resetWithListOfTabs(null, null, false);

        @MessageType int expectedMessageType = PRICE_MESSAGE;
        @MessageType int wrongMessageType = IPH;
        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        when(mPropertyModel.containsKeyEqualTo(MESSAGE_TYPE, PRICE_MESSAGE)).thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, mPropertyModel);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, wrongMessageType);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.PRICE_MESSAGE, expectedMessageType);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void removeSpecialItem_Message_CustomMessage() {
        mMediator.resetWithListOfTabs(null, null, false);

        @MessageType int expectedMessageType = ARCHIVED_TABS_MESSAGE;
        @MessageType int wrongMessageType = IPH;
        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(expectedMessageType);
        when(mPropertyModel.containsKeyEqualTo(MESSAGE_TYPE, ARCHIVED_TABS_MESSAGE))
                .thenReturn(true);
        mMediator.addSpecialItemToModel(0, UiType.ARCHIVED_TABS_MESSAGE, mPropertyModel);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.IPH_MESSAGE, wrongMessageType);
        assertEquals(1, mModelList.size());

        mMediator.removeSpecialItemFromModelList(UiType.ARCHIVED_TABS_MESSAGE, expectedMessageType);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void urlUpdated_forSingleTab_GroupedLayout() {
        assertNotEquals(mNewDomain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        when(mTab1.getUrl()).thenReturn(new GURL(NEW_URL));

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
    public void urlUpdated_forGroup_GroupedLayout() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(POSITION1);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(POSITION1);

        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(
                mTab1Domain + ", " + mTab2Domain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        // Update URL_DOMAIN for mTab1.
        when(mTab1.getUrl()).thenReturn(new GURL(NEW_URL));
        var oldFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(
                mNewDomain + ", " + mTab2Domain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        var newFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);

        // Update URL_DOMAIN for mTab2.
        when(mTab2.getUrl()).thenReturn(new GURL(NEW_URL));
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(
                mNewDomain + ", " + mNewDomain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        var newestFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(newFetcher, newestFetcher);
    }

    @Test
    public void urlUpdated_forGroup_FlatLayout() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
        when(mTabModel.representativeIndexOf(mTab1)).thenReturn(POSITION1);
        when(mTabModel.representativeIndexOf(mTab2)).thenReturn(POSITION1);

        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        verify(mTab2, times(1)).addObserver(mTabObserverCaptor.getValue());

        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(mTab1Domain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());

        var oldFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Update URL_DOMAIN for mTab1.
        when(mTab1.getUrl()).thenReturn(new GURL(NEW_URL));
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(mNewDomain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
        var newFetcher = mModelList.get(POSITION1).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);

        oldFetcher = mModelList.get(POSITION2).model.get(TabProperties.THUMBNAIL_FETCHER);

        // Update URL_DOMAIN for mTab2.
        when(mTab2.getUrl()).thenReturn(new GURL(NEW_URL));
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(mNewDomain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mNewDomain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));

        newFetcher = mModelList.get(POSITION2).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotEquals(oldFetcher, newFetcher);
    }

    @Test
    public void urlUpdated_forUngroup() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(mTab2, /* isDestinationTab= */ false);
        assertEquals(
                mTab1Domain + ", " + mTab2Domain,
                mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));

        // Assume that TabModel is already updated.
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(false);
        mockRepresentativeTabs(mTab1, mTab2);
        when(mTab2.getTabGroupId()).thenReturn(null);
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, POSITION1);
        assertEquals(mTab1Domain, mModelList.get(POSITION1).model.get(TabProperties.URL_DOMAIN));
        assertEquals(mTab2Domain, mModelList.get(POSITION2).model.get(TabProperties.URL_DOMAIN));
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo() {
        // Setup related mocks and initialize needed components.
        when(mItemView1.getParent()).thenReturn(mRecyclerView);
        when(mRecyclerView.getChildAdapterPosition(mItemView1)).thenReturn(0);
        AccessibilityAction action1 = new AccessibilityAction(R.id.move_tab_left, "left");
        AccessibilityAction action2 = new AccessibilityAction(R.id.move_tab_right, "right");
        AccessibilityAction action3 = new AccessibilityAction(R.id.move_tab_up, "up");
        when(mTabGridAccessibilityHelper.getPotentialActionsForView(mItemView1))
                .thenReturn(List.of(action1, action2, action3));
        when(mTabGridAccessibilityHelper.getPositionsOfReorderAction(eq(mItemView1), anyInt()))
                .thenReturn(new Pair<>(0, 1));
        InOrder accessibilityNodeInfoInOrder = Mockito.inOrder(mAccessibilityNodeInfo);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.onInitializeAccessibilityNodeInfo(mItemView1, mAccessibilityNodeInfo);

        accessibilityNodeInfoInOrder.verify(mAccessibilityNodeInfo).addAction(eq(action1));
        accessibilityNodeInfoInOrder.verify(mAccessibilityNodeInfo).addAction(eq(action2));
        accessibilityNodeInfoInOrder.verify(mAccessibilityNodeInfo).addAction(eq(action3));
    }

    @Test
    public void testPerformAccessibilityAction() {
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Setup related mocks and initialize needed components.
        int action = R.id.move_tab_left;
        // Mock that the action indicates that tab2 will move left and thus tab2 and tab1 should
        // switch positions.
        when(mTabGridAccessibilityHelper.getPositionsOfReorderAction(mItemView1, action))
                .thenReturn(new Pair<>(1, 0));
        when(mTabGridAccessibilityHelper.isReorderAction(action)).thenReturn(true);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.performAccessibilityAction(mItemView1, action, mBundle);

        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    public void testPerformAccessibilityAction_defaultAccessibilityAction() {
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Setup related mocks and initialize needed components.
        int action = ACTION_CLICK;
        // Mock that the action indicates that tab2 will move to position 2 which is invalid.
        when(mTabGridAccessibilityHelper.isReorderAction(action)).thenReturn(false);
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.performAccessibilityAction(mItemView1, action, mBundle);
        verify(mTabGridAccessibilityHelper, never())
                .getPositionsOfReorderAction(mItemView1, action);
    }

    @Test
    public void testPerformAccessibilityAction_InvalidIndex() {
        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        // Setup related mocks and initialize needed components.
        int action = R.id.move_tab_left;
        // Mock that the action indicates that tab2 will move to position 2 which is invalid.
        when(mTabGridAccessibilityHelper.getPositionsOfReorderAction(mItemView1, action))
                .thenReturn(new Pair<>(1, 2));
        assertNull(mMediator.getAccessibilityDelegateForTesting());
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.performAccessibilityAction(mItemView1, action, mBundle);

        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
    }

    @Test
    public void testInitializeAccessibilityNodeInfo_ContextMenuActions() {
        when(mItemView1.getContext()).thenReturn(mContext);
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.onInitializeAccessibilityNodeInfo(mItemView1, mAccessibilityNodeInfo);

        verify(mAccessibilityNodeInfo).addAction(eq(AccessibilityAction.ACTION_LONG_CLICK));
    }

    @Test
    public void
            testInitializeAccessibilityNodeInfo_TabGroupHeader_ExpandCollapseAndContextMenuActions() {
        mTabListConfig = new TabListConfig.Builder(TabListLayoutType.NESTED).build();
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        when(mItemView1.getContext()).thenReturn(mContext);
        when(mItemView1.getParent()).thenReturn(mRecyclerView);
        when(mRecyclerView.getChildAdapterPosition(mItemView1)).thenReturn(0);

        // Make item 0 a collapsed tab group header.
        PropertyModel model0 = mModelList.get(0).model;
        model0.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);
        model0.set(TabProperties.TITLE, "Shopping");
        model0.set(TabProperties.IS_COLLAPSED, true);

        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.onInitializeAccessibilityNodeInfo(mItemView1, mAccessibilityNodeInfo);
        verify(mAccessibilityNodeInfo).addAction(eq(AccessibilityAction.ACTION_EXPAND));
        verify(mAccessibilityNodeInfo).addAction(eq(AccessibilityAction.ACTION_LONG_CLICK));
        if (Build.VERSION.SDK_INT >= 36) {
            verify(mAccessibilityNodeInfo)
                    .setExpandedState(eq(AccessibilityNodeInfo.EXPANDED_STATE_COLLAPSED));
        } else {
            assertEquals(
                    AccessibilityNodeInfoCompat.EXPANDED_STATE_COLLAPSED,
                    AccessibilityNodeInfoCompat.wrap(mAccessibilityNodeInfo).getExpandedState());
        }

        ArgumentCaptor<AccessibilityAction> actionCaptor =
                ArgumentCaptor.forClass(AccessibilityAction.class);
        verify(mAccessibilityNodeInfo, atLeastOnce()).addAction(actionCaptor.capture());
        boolean hasCustomContextMenuAction =
                actionCaptor.getAllValues().stream()
                        .anyMatch(
                                a ->
                                        a.getId() == R.id.tab_context_menu
                                                && "Shopping tab group options"
                                                        .equals(a.getLabel()));
        assertTrue(hasCustomContextMenuAction);

        // Toggle to expanded.
        model0.set(TabProperties.IS_COLLAPSED, false);
        AccessibilityNodeInfo nodeInfo2 = Mockito.mock(AccessibilityNodeInfo.class);
        when(nodeInfo2.getExtras()).thenReturn(new Bundle());
        delegate.onInitializeAccessibilityNodeInfo(mItemView1, nodeInfo2);
        verify(nodeInfo2).addAction(eq(AccessibilityAction.ACTION_COLLAPSE));
        if (Build.VERSION.SDK_INT >= 36) {
            verify(nodeInfo2).setExpandedState(eq(AccessibilityNodeInfo.EXPANDED_STATE_FULL));
        } else {
            assertEquals(
                    AccessibilityNodeInfoCompat.EXPANDED_STATE_FULL,
                    AccessibilityNodeInfoCompat.wrap(nodeInfo2).getExpandedState());
        }
    }

    @Test
    public void testInitializeAccessibilityNodeInfo_GtsGroupCard_DoesNotAddExpandCollapseActions() {
        when(mItemView1.getContext()).thenReturn(mContext);
        when(mItemView1.getParent()).thenReturn(mRecyclerView);
        when(mRecyclerView.getChildAdapterPosition(mItemView1)).thenReturn(0);

        // GTS default mediator is GROUPED.
        PropertyModel model0 = mModelList.get(0).model;
        model0.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);
        model0.set(TabProperties.IS_COLLAPSED, true);

        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        delegate.onInitializeAccessibilityNodeInfo(mItemView1, mAccessibilityNodeInfo);
        verify(mAccessibilityNodeInfo, never()).addAction(eq(AccessibilityAction.ACTION_EXPAND));
        verify(mAccessibilityNodeInfo, never()).addAction(eq(AccessibilityAction.ACTION_COLLAPSE));
        if (Build.VERSION.SDK_INT >= 36) {
            verify(mAccessibilityNodeInfo, never()).setExpandedState(anyInt());
        } else {
            assertEquals(
                    AccessibilityNodeInfoCompat.EXPANDED_STATE_UNDEFINED,
                    AccessibilityNodeInfoCompat.wrap(mAccessibilityNodeInfo).getExpandedState());
        }

        assertFalse(
                delegate.performAccessibilityAction(
                        mItemView1, AccessibilityAction.ACTION_EXPAND.getId(), mBundle));
        assertFalse(
                delegate.performAccessibilityAction(
                        mItemView1, AccessibilityAction.ACTION_COLLAPSE.getId(), mBundle));
    }

    @Test
    public void testPerformAccessibilityAction_ExpandCollapse() {
        mTabListConfig = new TabListConfig.Builder(TabListLayoutType.NESTED).build();
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        when(mItemView1.getParent()).thenReturn(mRecyclerView);
        when(mRecyclerView.getChildAdapterPosition(mItemView1)).thenReturn(0);
        PropertyModel model0 = mModelList.get(0).model;
        model0.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);

        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        assertTrue(
                delegate.performAccessibilityAction(
                        mItemView1, AccessibilityAction.ACTION_EXPAND.getId(), mBundle));
        verify(mItemView1).performClick();

        assertTrue(
                delegate.performAccessibilityAction(
                        mItemView1, AccessibilityAction.ACTION_COLLAPSE.getId(), mBundle));
        verify(mItemView1, times(2)).performClick();
    }

    @Test
    public void testPerformAccessibilityAction_ContextMenu() {
        when(mItemView1.getParent()).thenReturn(mRecyclerView);
        when(mRecyclerView.getChildAdapterPosition(mItemView1)).thenReturn(0);

        OnLongPressTabItemEventListener listener =
                Mockito.mock(OnLongPressTabItemEventListener.class);
        mMediator.setOnLongPressTabItemEventListener(listener);
        mMediator.setupAccessibilityDelegate(mTabGridAccessibilityHelper);
        View.AccessibilityDelegate delegate = mMediator.getAccessibilityDelegateForTesting();
        assertNotNull(delegate);

        assertTrue(delegate.performAccessibilityAction(mItemView1, R.id.tab_context_menu, mBundle));
        verify(listener).onLongPressEvent(eq(TAB1_ID), eq(mItemView1));

        assertTrue(
                delegate.performAccessibilityAction(
                        mItemView1, AccessibilityAction.ACTION_LONG_CLICK.getId(), mBundle));
        verify(listener, times(2)).onLongPressEvent(eq(TAB1_ID), eq(mItemView1));

        assertTrue(
                delegate.performAccessibilityAction(
                        mItemView1, AccessibilityAction.ACTION_CONTEXT_CLICK.getId(), mBundle));
        verify(listener, times(3)).onLongPressEvent(eq(TAB1_ID), eq(mItemView1));
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

        // Assume that TabModel is already updated to reflect closed tab is undone.
        mockRepresentativeTabs(mTab1, mTab2);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));

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
        PropertyModel propertyModel = mPropertyModel;
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
                    when(mediatorSpy.isTabInTabGroup(any())).thenReturn(false);
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
                    when(mTab1.isIncognito()).thenReturn(incognito);
                    when(mTab2.isIncognito()).thenReturn(incognito);

                    for (int i = 0; i < 2; i++) {
                        long timestamp = System.currentTimeMillis();
                        Tab tab = mTabModel.getTabAt(i);
                        when(tab.getTimestampMillis()).thenReturn(timestamp);
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
                                        (shoppingPersistedTabData) ->
                                                assertThat(
                                                        shoppingPersistedTabData.getPriceMicros(),
                                                        equalTo(123456789012345L)));
                        mModelList
                                .get(1)
                                .model
                                .get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                                .fetch(
                                        (shoppingPersistedTabData) ->
                                                assertThat(
                                                        shoppingPersistedTabData.getPriceMicros(),
                                                        equalTo(
                                                                ShoppingPersistedTabData
                                                                        .NO_PRICE_KNOWN)));
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
        when(mTab1.isIncognito()).thenReturn(true);
        when(mTab2.isIncognito()).thenReturn(true);
    }

    @Test
    public void testGetPriceWelcomeMessageInsertionIndex() {
        initWithThreeTabs();

        when(mGridLayoutManager.getSpanCount())
                .thenReturn(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_COMPACT);
        assertThat(mMediator.getPriceWelcomeMessageInsertionIndex(), equalTo(2));

        when(mGridLayoutManager.getSpanCount())
                .thenReturn(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_MEDIUM);
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
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(PRICE_MESSAGE);
        mMediator.addSpecialItemToModel(1, UiType.PRICE_MESSAGE, mPropertyModel);
        assertThat(mModelList.lastIndexForMessageItemFromType(PRICE_MESSAGE), equalTo(2));
    }

    @Test
    public void testListObserver_OnItemRangeRemoved() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        setPriceTrackingEnabledForTesting(true);
        mMediator =
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initWithThreeTabs();

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(PRICE_MESSAGE);
        mMediator.addSpecialItemToModel(2, UiType.PRICE_MESSAGE, mPropertyModel);
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
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mPriceWelcomeMessageController, times(1))
                .showPriceWelcomeMessage(refEq(mPriceTabData));
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
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mPriceWelcomeMessageController, times(0))
                .showPriceWelcomeMessage(refEq(mPriceTabData));
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_SupplierIsNull() {
        prepareTestMaybeShowPriceWelcomeMessage();

        new ShoppingPersistedTabDataFetcher(mTab1, null)
                .maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0))
                .showPriceWelcomeMessage(refEq(mPriceTabData));
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_SupplierContainsNull() {
        prepareTestMaybeShowPriceWelcomeMessage();

        Supplier<PriceWelcomeMessageController> supplier = SupplierUtils.ofNull();
        new ShoppingPersistedTabDataFetcher(mTab1, supplier)
                .maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        verify(mPriceWelcomeMessageController, times(0))
                .showPriceWelcomeMessage(refEq(mPriceTabData));
    }

    @Test
    public void testMaybeShowPriceWelcomeMessage_NoPriceDrop() {
        prepareTestMaybeShowPriceWelcomeMessage();
        ShoppingPersistedTabDataFetcher fetcher =
                new ShoppingPersistedTabDataFetcher(mTab1, () -> mPriceWelcomeMessageController);

        fetcher.maybeShowPriceWelcomeMessage(null);
        verify(mPriceWelcomeMessageController, times(0))
                .showPriceWelcomeMessage(refEq(mPriceTabData));

        when(mShoppingPersistedTabData.getPriceDrop()).thenReturn(null);
        fetcher.maybeShowPriceWelcomeMessage(mShoppingPersistedTabData);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(mPriceWelcomeMessageController, times(0))
                .showPriceWelcomeMessage(refEq(mPriceTabData));
    }

    @Test
    public void testUpdateFaviconFetcherForGroup_Grid() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        mModelList.get(0).model.set(TabProperties.FAVICON_FETCHER, null);

        createTabGroup(Collections.singletonList(mTab1), TAB_GROUP_ID);
        when(mTabModel.isIncognito()).thenReturn(false);
        // Mock that we have a stored color stored with reference to root ID of tab1.
        when(mTabModel.getTabGroupColor(TAB_GROUP_ID)).thenReturn(COLOR_2);
        when(mTabModel.getTabGroupColorWithFallback(TAB_GROUP_ID)).thenReturn(COLOR_2);

        // Test a group of three.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, mTab2, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);
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
        List<Tab> group1 = List.of(mTab2, tab3);
        createTabGroup(group1, TAB_GROUP_ID);

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
        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());
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
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setComponentId(TabComponentId.ARCHIVED_TABS_DIALOG)
                        .build();
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
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
    public void testTabDescriptionString_Archived_EmptyTitle() {
        mMediator =
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setComponentId(TabComponentId.ARCHIVED_TABS_DIALOG)
                        .build();
        initAndAssertAllProperties();

        Tab newTab = prepareTab(TAB3_ID, "", TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        mMediator.resetWithListOfTabs(tabs, null, false);

        String targetString =
                mResources.getString(R.string.accessibility_restore_tab, TAB3_URL.getSpec());

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
        List<Tab> tabs = List.of(mTab1, newTab);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
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

        assertEquals(ARCHIVED_TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
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

        syncIds = List.of(SYNC_GROUP_ID2);
        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(ARCHIVED_TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
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
        List<Tab> group1 = List.of(mTab2, tab3);
        createTabGroup(group1, TAB_GROUP_ID);
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
        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.updateTabGroupTitle(mTab2.getTabGroupId());
        assertThat(
                mModelList
                        .get(POSITION2)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(nonEmptyTitleTargetString));
    }

    @Test
    public void testTabGroupExpandedDescriptionString() {
        setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        // Unnamed group targets collapse dialog plurals.
        String emptyTitleTargetString =
                mResources.getQuantityString(R.plurals.accessibility_dialog_back_button, 2, 2);

        assertThat(
                mModelList
                        .get(0)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(emptyTitleTargetString));

        // Named group targets collapse dialog plurals with group name.
        String nonEmptyTitleTargetString =
                mResources.getQuantityString(
                        R.plurals.accessibility_dialog_back_button_with_group_name,
                        2,
                        CUSTOMIZED_DIALOG_TITLE1,
                        2);

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());
        assertThat(
                mModelList
                        .get(0)
                        .model
                        .get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(nonEmptyTitleTargetString));
    }

    @Test
    public void testActionButtonDescriptionStringGroupOverflowMenu_TabSwitcher() {
        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = List.of(mTab1, tab3);
        createTabGroup(group1, TAB_GROUP_ID);
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
        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
        mMediator.updateTabGroupTitle(mTab1.getTabGroupId());
        assertThat(
                mModelList
                        .get(POSITION1)
                        .model
                        .get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext),
                equalTo(targetString));
    }

    @Test
    public void testActionButtonDescriptionString_SingleTab_EmptyTitle() {
        Tab newTab = prepareTab(TAB3_ID, "", TAB3_URL);
        List<Tab> tabs = List.of(newTab);
        mMediator.resetWithListOfTabs(tabs, null, false);

        String targetString =
                mResources.getString(
                        R.string.accessibility_tabstrip_btn_close_tab, TAB3_URL.getSpec());

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
        List<Tab> tabs = List.of(mTab1, newTab);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
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

        assertEquals(ARCHIVED_TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
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

        syncIds = List.of(SYNC_GROUP_ID2);
        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(ARCHIVED_TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
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
        List<Tab> group1 = List.of(mTab2, tab3);
        createTabGroup(group1, TAB_GROUP_ID);
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
    public void testRecordPriceAnnotationsEnabledMetrics_DisabledWhenUnsupported() {
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        String histogramName = "Commerce.PriceDrop.AnnotationsEnabled";

        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        assertFalse(mTabListConfig.supportsMessageCards);

        mMediator.recordPriceAnnotationsEnabledMetrics();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(histogramName), equalTo(0));
    }

    @Test
    public void testSelectableUpdates_withoutRelated() {
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        mMediator =
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setTabActionState(TabActionState.SELECTABLE)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(false);
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, mTab2, tab3);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(false);
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
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setTabActionState(TabActionState.SELECTABLE)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(false);
        List<Tab> tabs = List.of(mTab1, mTab2, tab3);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));

        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(true);
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
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setTabActionState(TabActionState.SELECTABLE)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Tab tab4 = prepareTab(TAB4_ID, TAB4_TITLE, TAB4_URL);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2, tab4));
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(tab3));
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(false);
        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(tab4)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(tab3)).thenReturn(false);
        List<Tab> tabs = List.of(mTab1, mTab2, tab3);
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
    public void testSelectableTab_recordsMetrics() {
        var userActionTester = new UserActionTester();
        initAndAssertAllProperties();

        mMediator.setTabActionState(TabActionState.SELECTABLE);
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));

        // Toggling to selected should record TabMultiSelectV2.TabSelected.
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, TAB1_ID, /* triggeringMotion= */ null);
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertTrue(userActionTester.getActions().contains("TabMultiSelectV2.TabSelected"));

        // Toggling back to unselected should record TabMultiSelectV2.TabUnselected.
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, TAB1_ID, /* triggeringMotion= */ null);
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertTrue(userActionTester.getActions().contains("TabMultiSelectV2.TabUnselected"));
    }

    @Test
    public void testShowLimitSnackbar_dynamicLimit() {
        if (mMediator != null) {
            mMediator.resetWithListOfTabs(null, null, false);
            mMediator.destroy();
            mMediator = null;
        }
        int allowedSelectionCount = 2;
        mMediator =
                new MediatorBuilder()
                        .setSnackbarManager(mSnackbarManager)
                        .setAllowedSelectionCount(allowedSelectionCount)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        mMediator.setTabActionState(TabActionState.SELECTABLE);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(Set.of(ITEM1_ID, ITEM2_ID));

        // Click when selection limit is reached should trigger limit snackbar.
        mModelList
                .get(0)
                .model
                .get(TabProperties.TAB_CLICK_LISTENER)
                .run(mItemView1, TAB1_ID, /* triggeringMotion= */ null);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void testChangingTabModels() {
        mCurrentTabModelSupplier.set(mIncognitoTabModel);

        verify(mTabModel).removeObserver(any());
        verify(mTabModel).removeTabGroupObserver(any());

        // Not added until the next resetWithListOfTabs call.
        verify(mIncognitoTabModel, never()).addObserver(any());
        verify(mIncognitoTabModel, never()).addTabGroupObserver(any());
    }

    @Test
    public void testSpecialItemExist() {
        mMediator.resetWithListOfTabs(null, null, false);

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(FOR_TESTING);
        mMediator.addSpecialItemToModel(0, UiType.PRICE_MESSAGE, mPropertyModel);

        assertTrue(!mModelList.isEmpty());
        assertTrue(mMediator.specialItemExistsInModel(FOR_TESTING));
        assertFalse(mMediator.specialItemExistsInModel(PRICE_MESSAGE));
        assertTrue(mMediator.specialItemExistsInModel(TabSwitcherMessageManager.MessageType.ALL));
    }

    @Test
    public void tabClosure_updatesTabGroup_inGroupedLayout() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);
        ThumbnailFetcher fetcherBefore =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModelList.size());

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTab1.isClosing()).thenReturn(false);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);

        assertEquals(2, mModelList.size());

        ThumbnailFetcher fetcherAfter =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, not(fetcherAfter));
    }

    @Test
    public void tabClosure_doesNotUpdateTabGroup_inGroupedLayout_WhenClosing() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);
        ThumbnailFetcher fetcherBefore =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModelList.size());

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTab1.isClosing()).thenReturn(true);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);

        assertEquals(2, mModelList.size());

        ThumbnailFetcher fetcherAfter =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, equalTo(fetcherAfter));
    }

    @Test
    public void tabClosure_ignoresUpdateForTabGroup_inFlatLayout() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        initAndAssertAllProperties();
        TabActionListener actionListenerBeforeUpdate =
                mModelList.get(0).model.get(TabProperties.TAB_CLICK_LISTENER);

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        assertEquals(2, mModelList.size());

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);

        assertEquals(1, mModelList.size());

        TabActionListener actionListenerAfterUpdate =
                mModelList.get(0).model.get(TabProperties.TAB_CLICK_LISTENER);
        // The selection listener should remain unchanged, since the property model of the tab group
        // should not get updated when the closure is triggered from outside the tab switcher.
        assertThat(actionListenerBeforeUpdate, equalTo(actionListenerAfterUpdate));
    }

    @Test
    public void tabClosure_updatesTabGroup_inNestedLayout() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(null, null, false);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTab1.isClosing()).thenReturn(false);

        String titleBefore = mModelList.get(0).model.get(TabProperties.TITLE);

        // Change what the title editor will return after closure.
        when(mTabModel.getTabGroupTitle(TAB_GROUP_ID)).thenReturn("1 tab");
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mTab1));

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);

        String titleAfter = mModelList.get(0).model.get(TabProperties.TITLE);
        assertThat(titleBefore, not(titleAfter));
    }

    @Test
    public void tabClosure_resetTabsListForTabGroupUpdate_inGroupedLayout() {
        initAndAssertAllProperties();

        // Mock that tab1 and tab3 are in the same group and group root id is TAB1_ID.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);
        ThumbnailFetcher fetcherBefore =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertEquals(2, mModelList.size());
        assertEquals(mModelList.get(0).model.get(TabProperties.TAB_ID), mTab1.getId());

        mMediator.resetWithListOfTabs(List.of(tab3, mTab2), null, true);

        assertEquals(2, mModelList.size());

        ThumbnailFetcher fetcherAfter =
                mModelList.get(0).model.get(TabProperties.THUMBNAIL_FETCHER);
        assertThat(fetcherBefore, not(fetcherAfter));

        assertEquals(mModelList.get(0).model.get(TabProperties.TAB_ID), tab3.getId());
    }

    @Test
    public void tabClosure_RepresentativeTab_inNestedLayout() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        initAndAssertAllProperties();

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(null, null, false);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, true);

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTab1.isClosing()).thenReturn(false);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(tab3));

        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));

        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);

        // Header should survive and transfer ID to tab3.
        assertEquals(TAB3_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.DATA_SHARING})
    public void testIsTabGroup_TabSwitcher() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);

        when(mTabGroupSyncFeaturesJniMock.isTabGroupSyncEnabled(mProfile)).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        // Create tab group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> group1 = List.of(mTab1, tab3);
        createTabGroup(group1, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        assertEquals(
                TabActionButtonType.OVERFLOW,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
    }

    @Test
    public void isTabPinned_GroupedLayout() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);

        List<Tab> tabsInModel = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabsInModel.add(mTabModel.getTabAt(i));
        }
        Tab tabToTest = tabsInModel.get(POSITION1);

        // Scenario 1: Tab is UNPINNED
        // Mock the tab at POSITION1 as unpinned.
        when(tabToTest.getIsPinned()).thenReturn(false);

        mMediator.resetWithListOfTabs(tabsInModel, null, false);

        assertEquals(
                TabActionButtonType.CLOSE,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));

        // Scenario 2: Tab is PINNED
        // Mock the tab at POSITION1 as pinned.
        when(tabToTest.getIsPinned()).thenReturn(true);

        // Re-process the tabs. The mediator should pick up the changed pinned state.
        mMediator.resetWithListOfTabs(tabsInModel, null, false);

        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
        assertTrue(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));
    }

    @Test
    public void onTabPinnedStateChanged_GroupedLayout() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);

        List<Tab> tabsInModel = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabsInModel.add(mTabModel.getTabAt(i));
        }
        Tab tabToTest = tabsInModel.get(POSITION1);

        // Set initial state to unpinned.
        when(tabToTest.getIsPinned()).thenReturn(false);
        mMediator.resetWithListOfTabs(tabsInModel, null, false);
        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));

        // Pin the tab and notify the observer.
        when(tabToTest.getIsPinned()).thenReturn(true);
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(tabToTest, true);
        assertTrue(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));
        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);

        // Unpin the tab and notify the observer.
        when(tabToTest.getIsPinned()).thenReturn(false);
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(tabToTest, false);
        assertFalse(mModelList.get(POSITION1).model.get(TabProperties.IS_PINNED));
        assertEquals(
                TabActionButtonType.CLOSE,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);
    }

    @Test
    public void onTabPinnedStateChanged_GroupedLayout_MovesTab() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(tab3.getIsPinned()).thenReturn(false);

        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);
        when(mTabModel.getTabAt(2)).thenReturn(tab3);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2, tab3).iterator());

        when(mTabModel.indexOf(mTab1)).thenReturn(0);
        when(mTabModel.indexOf(mTab2)).thenReturn(1);
        when(mTabModel.indexOf(tab3)).thenReturn(2);

        mockRepresentativeTabs(mTab1, mTab2, tab3);

        List<Tab> tabsInModel = List.of(mTab1, mTab2, tab3);
        mMediator.resetWithListOfTabs(tabsInModel, null, false);
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertFalse(mModelList.get(1).model.get(TabProperties.IS_PINNED));

        // Pin mTab2. It should move to the front.
        when(mTab2.getIsPinned()).thenReturn(true);
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
        when(mTab1.getIsPinned()).thenReturn(true);
        mockTabIndexes(mTab2, mTab1, tab3);

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, true);

        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertTrue(mModelList.get(1).model.get(TabProperties.IS_PINNED));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertEquals(
                TabActionButtonType.PIN,
                mModelList.get(1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA).type);

        // Unpin mTab2. It should return to its original position.
        when(mTab2.getIsPinned()).thenReturn(false);
        mockTabIndexes(mTab1, mTab2, tab3);

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
    public void onTabPinnedStateChanged_GroupedLayout_MovesTab_OutOfBounds() {
        mMediator.setComponentIdForTesting(TabComponentId.GRID_TAB_SWITCHER);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        // Start with tab1 pinned, others not.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(tab3.getIsPinned()).thenReturn(false);

        // TabModel has all 3 tabs, with pinned tab first.
        mockTabIndexes(mTab1, mTab2, tab3);
        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1, mTab2, tab3).iterator());

        // TabModel also represents all 3.
        mockRepresentativeTabs(mTab1, mTab2, tab3);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2));
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(tab3));

        // But TabListModel only has the first two.
        List<Tab> tabsInModel = List.of(mTab1, mTab2);
        mMediator.resetWithListOfTabs(tabsInModel, null, false);
        assertEquals(2, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertTrue(mModelList.get(0).model.get(TabProperties.IS_PINNED));

        // Now, unpin tab1. After this, its position in TabModel will be at the end of the
        // unpinned tabs. With tab2 and tab3 unpinned, and assuming stable sort, tab1 will go
        // after tab3. Let's say the new order is [tab2, tab3, tab1].
        when(mTab1.getIsPinned()).thenReturn(false);
        mockTabIndexes(mTab2, tab3, mTab1);

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
    public void onTabPinnedStateChanged_NestedLayout_PinTab() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);
        mMediator.resetWithListOfTabs(null, null, false);

        // Setup mTab2 as pinned, mTab1 as regular.
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTab1.getIsPinned()).thenReturn(false);

        mockTabIndexes(mTab2, mTab1);

        mMediator.resetWithListOfTabs(List.of(mTab2, mTab1), null, false);

        // List contains: [0] Pinned Tab 2, [1] Regular Tab 1.
        assertEquals(2, mModelList.size());
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));

        // Pin mTab1.
        when(mTab1.getIsPinned()).thenReturn(true);

        // Mock observer callback.
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, true);

        // Verifies that both are now marked as pinned, mTab1 stays at index 1 (after mTab2).
        assertTrue(mModelList.get(0).model.get(TabProperties.IS_PINNED));
        assertTrue(mModelList.get(1).model.get(TabProperties.IS_PINNED));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void onTabPinnedStateChanged_NestedLayout_UnpinTab() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);
        mMediator.resetWithListOfTabs(null, null, false);

        // Initially, both mTab1 and mTab2 are pinned.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);

        mockTabIndexes(mTab1, mTab2);

        // Reset list with both pinned.
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        // List contains: [0] Pinned Tab 1, [1] Pinned Tab 2.
        assertEquals(2, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));

        // Unpin mTab1.
        when(mTab1.getIsPinned()).thenReturn(false);

        mockTabIndexes(mTab2, mTab1);

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, false);

        // Verifies that unpinned mTab1 moved to the regular section (index 1).
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertFalse(mModelList.get(1).model.get(TabProperties.IS_PINNED));
    }

    @Test
    public void onTabPinnedStateChanged_NestedLayout_UnpinTab_DetachedPropertyUpdate() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);
        mMediator.resetWithListOfTabs(null, null, false);

        // Initially, both mTab1 and mTab2 are pinned.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab2.getIsPinned()).thenReturn(true);

        mockTabIndexes(mTab1, mTab2);

        // Reset list with both pinned.
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        // List contains: [0] Pinned Tab 1, [1] Pinned Tab 2.
        assertEquals(2, mModelList.size());
        PropertyModel model1 = mModelList.get(0).model;

        // Unpin mTab1.
        when(mTab1.getIsPinned()).thenReturn(false);

        mockTabIndexes(mTab2, mTab1);

        mModelList.addObserver(mListObserver);
        model1.addObserver(mPropertyObserver);

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, false);

        // Verifies the exact sequence to prevent temporary layout shifts:
        // 1. Pinned tab 1 is removed from mModelList.
        // 2. Its IS_PINNED property is updated while detached.
        // 3. It is added back to mModelList at the final position.
        InOrder inOrder = Mockito.inOrder(mListObserver, mPropertyObserver);
        inOrder.verify(mListObserver).onItemRangeRemoved(eq(mModelList), eq(0), eq(1));
        inOrder.verify(mPropertyObserver)
                .onPropertyChanged(eq(model1), eq(TabProperties.IS_PINNED));
        inOrder.verify(mListObserver).onItemRangeInserted(eq(mModelList), eq(1), eq(1));
    }

    @Test
    public void onTabPinnedStateChanged_NestedLayout_UnpinToGroup() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);

        // Setup mTab1 as pinned.
        when(mTab1.getIsPinned()).thenReturn(true);
        when(mTab1.getTabGroupId()).thenReturn(null);

        // Setup mTab2 and tab3 as an expanded group.
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        Token tabGroupId = new Token(1L, 2L);
        createTabGroup(List.of(mTab2, tab3), tabGroupId);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(tab3.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabGroupCollapsed(tabGroupId)).thenReturn(false);
        when(mTabModel.getTabsInGroup(tabGroupId)).thenReturn(List.of(mTab2, tab3));

        mockTabIndexes(mTab1, mTab2, tab3);

        mMediator.resetWithListOfTabs(null, null, false);
        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);

        // Initial UI: [0] Tab 1, [1] Group Header, [2] Tab 2, [3] Tab 3.
        assertEquals(4, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(CardProperties.ModelType.TAB_GROUP, mModelList.get(1).model.get(CARD_TYPE));

        // Backend unpins mTab1 (moves it to unpinned boundary, backend index 0).
        when(mTab1.getIsPinned()).thenReturn(false);
        mockTabIndexes(mTab1, mTab2, tab3);
        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab1, false);

        // UI after unpinning but before grouping should move mTab1 to the unpinned boundary.
        assertEquals(4, mModelList.size());
        assertEquals(TAB1_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(CardProperties.ModelType.TAB_GROUP, mModelList.get(1).model.get(CARD_TYPE));

        // Backend merges mTab1 into the group.
        when(mTab1.getTabGroupId()).thenReturn(tabGroupId);
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(List.of(mTab2, tab3, mTab1));
        mockTabIndexes(mTab2, tab3, mTab1);

        // Observer: didMergeTabToGroup fires second (when moving into the group)
        mTabGroupObserverCaptor.getValue().didMergeTabToGroup(mTab1, /* isDestinationTab= */ false);

        // UI after merge should be: [0] Group Header, [1] Tab 2, [2] Tab 3, [3] Tab 1
        assertEquals(4, mModelList.size());
        assertEquals(CardProperties.ModelType.TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertEquals(TAB2_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertEquals(TAB3_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
        assertEquals(TAB1_ID, mModelList.get(3).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void onTabPinnedStateChanged_NestedLayout_PinGroupedTab() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);

        // Setup mTab1 and mTab2 as an expanded group.
        Token tabGroupId = new Token(1L, 2L);
        createTabGroup(List.of(mTab1, mTab2), tabGroupId);
        when(mTab1.getIsPinned()).thenReturn(false);
        when(mTab2.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabGroupCollapsed(tabGroupId)).thenReturn(false);
        when(mTabModel.getTabsInGroup(tabGroupId)).thenReturn(List.of(mTab1, mTab2));

        mockTabIndexes(mTab1, mTab2);

        mMediator.resetWithListOfTabs(null, null, false);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        // Initial UI: [0] Group Header, [1] Tab 1, [2] Tab 2.
        assertEquals(3, mModelList.size());
        assertEquals(CardProperties.ModelType.TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertEquals(TAB1_ID, mModelList.get(1).model.get(TabProperties.TAB_ID));
        assertEquals(TAB2_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));

        // Move out of group and pin.
        when(mTab2.getTabGroupId()).thenReturn(null);
        when(mTab2.getIsPinned()).thenReturn(true);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(List.of(mTab1));
        mockTabIndexes(mTab2, mTab1);

        mTabGroupObserverCaptor.getValue().didMoveTabOutOfGroup(mTab2, /* prevFilterIndex= */ 1);

        // UI after move out: [0] Tab 2 (pinned), [1] Group Header, [2] Tab 1.
        assertEquals(3, mModelList.size());
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID));
        assertEquals(CardProperties.ModelType.TAB_GROUP, mModelList.get(1).model.get(CARD_TYPE));
        assertEquals(TAB1_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));

        mTabObserverCaptor.getValue().onTabPinnedStateChanged(mTab2, true);

        // UI should now have Tab 2 at the top in the pinned section.
        assertEquals(3, mModelList.size());
        assertEquals(TAB2_ID, mModelList.get(0).model.get(TabProperties.TAB_ID)); // Pinned
        assertEquals(CardProperties.ModelType.TAB_GROUP, mModelList.get(1).model.get(CARD_TYPE));
        assertEquals(TAB1_ID, mModelList.get(2).model.get(TabProperties.TAB_ID));
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

    @Test
    public void testOnMenuItemClickedCallback_UngroupInTabSwitcher_IncognitoNoShow() {
        mCurrentTabModelSupplier.set(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mIncognitoTabModel.getCount(); i++) {
            tabs.add(mIncognitoTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = List.of(mTab1, mTab2);
        createTabGroup(group1, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mIncognitoTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mIncognitoTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        when(mIncognitoTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
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
        mCurrentTabModelSupplier.set(mIncognitoTabModel);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mIncognitoTabModel.getCount(); i++) {
            tabs.add(mIncognitoTabModel.getTabAt(i));
        }

        // Create tab group.
        List<Tab> group1 = List.of(mTab1, mTab2);
        createTabGroup(group1, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mIncognitoTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mIncognitoTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mIncognitoTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        mMediator.onMenuItemClicked(
                R.id.delete_tab_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mIncognitoTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mIncognitoTabModel, TAB_GROUP_ID)
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
        List<Tab> group1 = List.of(mTab1);
        createTabGroup(group1, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
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
        List<Tab> group1 = List.of(mTab1);
        createTabGroup(group1, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);
        mMediator.onMenuItemClicked(
                R.id.close_tab_group,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mTabModel, TAB_GROUP_ID)
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
        List<Tab> groupTabs1 = List.of(tab3, tab4);
        createTabGroup(groupTabs1, TAB_GROUP_ID, 2);

        Token otherGroupId = new Token(74893L, 8490L);
        // Mock that tab5 and tab6 are in the same group and group root id is TAB5_ID.
        List<Tab> groupTabs2 = List.of(tab5, tab6);
        createTabGroup(groupTabs2, otherGroupId, 3);

        mockRepresentativeTabs(mTab1, mTab2, tab3, tab5, tab7);

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

        List<Tab> tabs = List.of(mTab1, mTab2, tab3, tab5, tab7);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(5));

        TreeMap<Integer, List<PropertyModel>> resultMap = new TreeMap<>();

        List<Tab> tabsToFade = List.of(mTab1, tab4, tab5, tab6, tab7);

        mMediator.getOrderOfTabsForQuickDeleteAnimation(
                mTabListRecyclerView, tabsToFade, resultMap);

        assertThat(resultMap.keySet(), contains(1, 2));

        // Tab 1 and group tab 5 & 6 should be filtered for animation.
        assertThat(resultMap.get(1), contains(mModelList.get(0).model));
        assertThat(resultMap.get(2), contains(mModelList.get(3).model));
    }

    @Test
    public void testQuickDeleteAnimationTabFiltering_nullGroupRepresentativeTab() {
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> groupTabs1 = List.of(tab3);
        createTabGroup(groupTabs1, TAB_GROUP_ID, 2);

        when(mTabModel.representativeIndexOf(tab3)).thenReturn(2);
        when(mTabModel.getRepresentativeTabAt(2)).thenReturn(null);

        mockRepresentativeTabs(mTab1, mTab2);
        List<Tab> tabs = List.of(mTab1, mTab2);
        mMediator.resetWithListOfTabs(tabs, null, false);

        TreeMap<Integer, List<PropertyModel>> resultMap = new TreeMap<>();
        List<Tab> tabsToFade = List.of(tab3);

        // Should not crash when getRepresentativeTabAt returns null.
        mMediator.getOrderOfTabsForQuickDeleteAnimation(
                mTabListRecyclerView, tabsToFade, resultMap);
        assertTrue(resultMap.isEmpty());
    }

    @Test
    public void setTabActionState_UnbindsPropertiesCorrectly() {
        when(mSelectionDelegate.isItemSelected(ITEM1_ID)).thenReturn(true);
        when(mSelectionDelegate.isItemSelected(ITEM2_ID)).thenReturn(false);
        when(mSelectionDelegate.isItemSelected(ITEM3_ID)).thenReturn(false);
        mMediator =
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        // Unique sets of keys for each of SELECTABLE/CLOSABLE.
        ArrayList<PropertyKey> uniqueClosableKeys =
                new ArrayList<>(List.of(TAB_GRID_CLOSABLE_KEYS));
        uniqueClosableKeys.removeAll(List.of(TAB_GRID_SELECTABLE_KEYS));

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
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        mModelList.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        mMediator.getOnMaybeTabClosedCallback(TAB1_ID).onResult(false);
        assertFalse(mModelList.get(0).model.get(TabProperties.USE_SHRINK_CLOSE_ANIMATION));
    }

    @Test
    public void testUnsetShrinkCloseAnimation_DidClose_NoModels() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

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
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

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
        List<Tab> tabs = List.of(mTab1, newTab);
        createTabGroup(tabs, TAB_GROUP_ID);

        mMediator.resetWithListOfTabs(tabs, null, false);

        mModelList.get(0).model.set(TabProperties.USE_SHRINK_CLOSE_ANIMATION, true);
        var callback = mMediator.getOnMaybeTabClosedCallback(TAB1_ID);

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(newTab);

        callback.onResult(true);

        assertEquals(0, mModelList.size());
    }

    @Test
    public void testUpdateTabStripNotificationBubble_hasUpdate() {
        // Setup the test such that the tab list is strip mode, with a tab group of 2 tabs.
        setUpTabListMediator(TabListMediatorType.TAB_STRIP, TabListMode.BOTTOM_STRIP);
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);

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
        Map<Integer, TabCardLabelData> dataMap = new HashMap<>();
        dataMap.put(TAB1_ID, mTabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        assertEquals(
                mTabCardLabelData,
                mModelList.get(POSITION1).model.get(TabProperties.TAB_CARD_LABEL_DATA));
        assertNull(mModelList.get(POSITION2).model.get(TabProperties.TAB_CARD_LABEL_DATA));

        dataMap.replace(TAB1_ID, null);
        dataMap.put(TAB2_ID, mTabCardLabelData);

        mMediator.updateTabCardLabels(dataMap);

        assertNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_CARD_LABEL_DATA));
        assertEquals(
                mTabCardLabelData,
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
        List<Tab> group1 = List.of(mTab2, tab3);
        createTabGroup(group1, TAB_GROUP_ID);
        setupSyncedGroup(/* isShared= */ true);

        TabCardLabelData tabCardLabelData =
                new TabCardLabelData(
                        TabCardLabelType.ACTIVITY_UPDATE,
                        (_) -> "Test label",
                        /* asyncImageFactory= */ null,
                        (_) -> "Alice changed");

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

        mTabModel.setTabGroupTitle(TAB_GROUP_ID, CUSTOMIZED_DIALOG_TITLE1);
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

        verify(mTabModel, times(1)).addObserver(mTabModelObserverCaptor.getValue());
        verify(mTabModel, times(1)).addTabGroupObserver(mTabGroupObserverCaptor.getValue());

        // Hide the GTS. The observers should be removed.
        mMediator.postHiding();
        verify(mTabModel).removeObserver(mTabModelObserverCaptor.getValue());
        verify(mTabModel).removeTabGroupObserver(mTabGroupObserverCaptor.getValue());
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
    public void testAddSpecialItemToModelList_tabGroup() {
        mMediator.resetWithListOfTabs(null, null, false);

        when(mPropertyModel.get(CARD_TYPE)).thenReturn(TAB_GROUP);
        mMediator.addSpecialItemToModel(0, UiType.TAB_GROUP, mPropertyModel);

        assertFalse(mModelList.isEmpty());
        assertEquals(UiType.TAB_GROUP, mModelList.get(0).type);
    }

    @Test
    public void testResetWithListOfTabs_withArchivedTabGroupType() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
        mMediator.setDefaultGridCardSize(new Size(100, 200));
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();

        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        // Assert that group types come before tabs and all properties are correct.
        assertEquals(ARCHIVED_TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
        assertEquals(SYNC_GROUP_ID1, mModelList.get(0).model.get(TabProperties.TAB_GROUP_SYNC_ID));
        assertEquals(GROUP_TITLE, mModelList.get(0).model.get(TabProperties.TITLE));
        var provider = mModelList.get(0).model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
        assertNotNull(provider);
        assertEquals(TabGroupColorId.BLUE, provider.getTabGroupColorIdForTesting());
    }

    @Test
    public void testBindTabGroupActionButtonData_withArchivedTabGroupType() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, newTab);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
        mMediator.setDefaultGridCardSize(new Size(100, 200));

        // Ensure the group is archived.
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();

        mMediator.resetWithListOfTabs(tabs, syncIds, false);

        assertEquals(ARCHIVED_TAB_GROUP, mModelList.get(0).model.get(CARD_TYPE));
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
        mTabListConfig = new TabListConfig.Builder(TabListLayoutType.FLAT).build();

        mMediator =
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setComponentId(TabComponentId.ARCHIVED_TABS_DIALOG)
                        .build();
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
        List<Tab> tabs = List.of(mTab1);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
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
        mMediator = new MediatorBuilder().setUndoBarExplicitTrigger(null).build();
        mMediator.registerOrientationListener(mGridLayoutManager);
        mMediator.initWithNative(mProfile);
        initAndAssertAllProperties();

        List<Tab> tabs = List.of(mTab1);
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
        mMediator.setDefaultGridCardSize(new Size(100, 200));
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();

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
        List<String> syncIds = List.of(SYNC_GROUP_ID1);
        mMediator.setDefaultGridCardSize(new Size(100, 200));
        mSavedTabGroup1.archivalTimeMs = System.currentTimeMillis();
        mMediator.resetWithListOfTabs(null, syncIds, false);

        assertEquals(1, mModelList.size());

        // Assert removing a tab type does nothing.
        mMediator.removeListItemFromModelList(UiType.TAB_GROUP, ITEM1_ID);
        assertEquals(1, mModelList.size());

        mMediator.removeListItemFromModelList(UiType.TAB_GROUP, ITEM4_ID);
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testAlertState_TabAudible() {
        assertEquals(TabAlert.NONE, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.AUDIO_PLAYING);
        assertEquals(
                TabAlert.AUDIO_PLAYING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_TabMuted() {
        assertEquals(TabAlert.NONE, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.AUDIO_PLAYING);
        assertEquals(
                TabAlert.AUDIO_PLAYING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.AUDIO_MUTING);
        assertEquals(TabAlert.AUDIO_MUTING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_TabNone() {
        updateTabAlertState(mTab1, TabAlert.AUDIO_PLAYING);
        assertEquals(
                TabAlert.AUDIO_PLAYING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.NONE);
        assertEquals(TabAlert.NONE, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_TabRecording() {
        assertEquals(TabAlert.NONE, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.MEDIA_RECORDING);
        assertEquals(
                TabAlert.MEDIA_RECORDING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_TabPiP() {
        assertEquals(TabAlert.NONE, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.PIP_PLAYING);
        assertEquals(TabAlert.PIP_PLAYING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_TabGroup() {
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_MUTING);
        when(mTab2.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);

        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // MUTING (priority 1) has priority over PLAYING (priority 0).
        assertEquals(TabAlert.AUDIO_MUTING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab2, TabAlert.AUDIO_MUTING);
        assertEquals(TabAlert.AUDIO_MUTING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        updateTabAlertState(mTab1, TabAlert.AUDIO_PLAYING);
        assertEquals(TabAlert.AUDIO_MUTING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        // MUTING has priority over NONE (no alert).
        updateTabAlertState(mTab1, TabAlert.NONE);
        assertEquals(TabAlert.AUDIO_MUTING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        // PiP (priority 2) has priority over MUTING (priority 1).
        updateTabAlertState(mTab1, TabAlert.PIP_PLAYING);
        updateTabAlertState(mTab2, TabAlert.AUDIO_PLAYING);
        assertEquals(TabAlert.PIP_PLAYING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));

        // RECORDING (priority 15) has priority over PiP (priority 2).
        updateTabAlertState(mTab2, TabAlert.MEDIA_RECORDING);
        assertEquals(
                TabAlert.MEDIA_RECORDING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_TabGroup_ContentDescription() {
        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
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

        // Description without alert state.
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

        // AlertState AUDIO_PLAYING.
        updateTabAlertState(mTab1, TabAlert.AUDIO_PLAYING);
        assertEquals(
                baseDescription + " " + playingAudio,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // AlertState AUDIO_MUTING.
        updateTabAlertState(mTab1, TabAlert.AUDIO_MUTING);
        assertEquals(
                baseDescription + " " + mutedAudio,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // AlertState MEDIA_RECORDING.
        updateTabAlertState(mTab2, TabAlert.MEDIA_RECORDING);
        assertEquals(
                baseDescription + " " + recording,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // AlertState TAB_CAPTURING.
        updateTabAlertState(mTab2, TabAlert.TAB_CAPTURING);
        assertEquals(
                baseDescription + " " + sharing,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());

        // AlertState none.
        updateTabAlertState(mTab1, TabAlert.NONE);
        updateTabAlertState(mTab2, TabAlert.NONE);
        assertEquals(
                baseDescription,
                model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER)
                        .resolve(mContext)
                        .toString());
    }

    @Test
    public void testAlertState_NestedLayout() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        assertEquals(3, mModelList.size());

        PropertyModel groupHeader = mModelList.get(0).model;
        PropertyModel child1 = mModelList.get(1).model;
        PropertyModel child2 = mModelList.get(2).model;

        // Group Header should initially have no alert indicator.
        assertEquals(TabAlert.NONE, groupHeader.get(TabProperties.ALERT_STATE));

        // Update states.
        updateTabAlertState(mTab1, TabAlert.AUDIO_MUTING);
        updateTabAlertState(tab3, TabAlert.AUDIO_PLAYING);

        // Child tabs should reflect their individual alert states.
        assertEquals(TabAlert.AUDIO_MUTING, child1.get(TabProperties.ALERT_STATE));
        assertEquals(TabAlert.AUDIO_PLAYING, child2.get(TabProperties.ALERT_STATE));

        // Update tab 3 alert state.
        updateTabAlertState(tab3, TabAlert.MEDIA_RECORDING);

        // Group header remains NONE.
        assertEquals(TabAlert.NONE, groupHeader.get(TabProperties.ALERT_STATE));
        // Child tab 3 updates directly.
        assertEquals(TabAlert.MEDIA_RECORDING, child2.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testAlertState_FlatLayout() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        when(mTab1.getAlertState()).thenReturn(TabAlert.AUDIO_MUTING);
        when(mTab2.getAlertState()).thenReturn(TabAlert.AUDIO_PLAYING);

        List<Tab> tabs = List.of(mTab1, mTab2);
        createTabGroup(tabs, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Alert states should NOT aggregate to a group header.
        assertEquals(2, mModelList.size());

        // Child tabs should reflect their individual alert states.
        assertEquals(TabAlert.AUDIO_MUTING, mModelList.get(0).model.get(TabProperties.ALERT_STATE));
        assertEquals(
                TabAlert.AUDIO_PLAYING, mModelList.get(1).model.get(TabProperties.ALERT_STATE));

        // Update tab 2 alert state.
        updateTabAlertState(mTab2, TabAlert.MEDIA_RECORDING);

        // Child tab 2 updates directly.
        assertEquals(
                TabAlert.MEDIA_RECORDING, mModelList.get(1).model.get(TabProperties.ALERT_STATE));
    }

    @Test
    public void testContextClickListener() {
        mMediator =
                new MediatorBuilder()
                        .setTabListItemOnClickListenerProvider(null)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
        assertNotNull(mModelList.get(0).model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER));

        mMediator.setTabActionState(TabActionState.SELECTABLE);
        assertNull(mModelList.get(0).model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER));
    }

    @Test
    public void testContextClickListener_VerticalTabs_ReturnsNull() {
        TabListConfig config = new TabListConfig.Builder(TabListLayoutType.NESTED).build();
        mMediator =
                new MediatorBuilder()
                        .setTabListConfig(config)
                        .setTabListItemOnClickListenerProvider(null)
                        .setUndoBarExplicitTrigger(null)
                        .build();
        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
        assertNull(mModelList.get(0).model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER));
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_InitialSet() {
        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);

        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        PropertyModel model = mModelList.get(0).model;
        UiTabState state = model.get(TabProperties.ACTOR_UI_STATE);
        assertNotNull(state);
        assertEquals(TabIndicatorStatus.DYNAMIC, state.tabIndicator);
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_ObserverUpdatesModel() {
        setUpActorState(mTab1, TabIndicatorStatus.NONE);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        PropertyModel model = mModelList.get(0).model;

        ArgumentCaptor<ActorUiTabController.Observer> observerCaptor =
                ArgumentCaptor.forClass(ActorUiTabController.Observer.class);
        verify(mActorUiTabController).addObserver(observerCaptor.capture());

        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);
        UiTabState newState =
                new UiTabState(TAB1_ID, null, null, TabIndicatorStatus.DYNAMIC, false);
        observerCaptor.getValue().onUiTabStateChanged(newState);
        assertEquals(
                TabIndicatorStatus.DYNAMIC, model.get(TabProperties.ACTOR_UI_STATE).tabIndicator);
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_ObserverRemovedOnReset() {
        setUpActorState(mTab1, TabIndicatorStatus.NONE);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        verify(mActorUiTabController, atLeastOnce()).addObserver(any());

        when(mTabModel.iterator()).thenAnswer(_ -> List.of(mTab1).iterator());

        mMediator.resetWithListOfTabs(null, null, false);
        verify(mActorUiTabController, atLeastOnce()).removeObserver(any());
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_NewTabAdded() {
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        setUpActorState(newTab, TabIndicatorStatus.STATIC);

        mockRepresentativeTabs(mTab1, mTab2, newTab);
        when(mTabModel.getRelatedTabList(TAB3_ID)).thenReturn(List.of(newTab));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        int index = mModelList.indexFromTabId(TAB3_ID);
        assertNotEquals(TabModel.INVALID_TAB_INDEX, index);

        PropertyModel newModel = mModelList.get(index).model;
        assertEquals(
                TabIndicatorStatus.STATIC, newModel.get(TabProperties.ACTOR_UI_STATE).tabIndicator);

        verify(mActorUiTabController).addObserver(any());
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_ObserverUpdatesToNone() {
        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);
        PropertyModel model = mModelList.get(0).model;

        assertNotNull(model.get(TabProperties.ACTOR_UI_STATE));
        assertEquals(
                TabIndicatorStatus.DYNAMIC, model.get(TabProperties.ACTOR_UI_STATE).tabIndicator);
        ArgumentCaptor<ActorUiTabController.Observer> observerCaptor =
                ArgumentCaptor.forClass(ActorUiTabController.Observer.class);
        verify(mActorUiTabController).addObserver(observerCaptor.capture());

        setUpActorState(mTab1, TabIndicatorStatus.NONE);
        UiTabState finishedState =
                new UiTabState(
                        TAB1_ID,
                        mActorOverlayState,
                        mHandoffButtonState,
                        TabIndicatorStatus.NONE,
                        false);
        observerCaptor.getValue().onUiTabStateChanged(finishedState);
        assertNull(model.get(TabProperties.ACTOR_UI_STATE));
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_GroupedLayout() {
        // Create a tab group with Tab 1 and Tab 2.
        List<Tab> groupTabs = List.of(mTab1, mTab2);
        Token tabGroupId = new Token(1L, 2L);
        when(mTab1.getTabGroupId()).thenReturn(tabGroupId);
        when(mTab2.getTabGroupId()).thenReturn(tabGroupId);
        when(mTabModel.getRelatedTabList(TAB1_ID)).thenReturn(groupTabs);
        when(mTabModel.getRelatedTabList(TAB2_ID)).thenReturn(groupTabs);
        when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);
        when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(true);
        when(mTabModel.getGroupLastShownTabId(any())).thenReturn(TAB1_ID);

        setUpActorState(mTab1, TabIndicatorStatus.NONE);
        setUpActorState(mTab2, TabIndicatorStatus.NONE);

        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        assertEquals(1, mModelList.size());
        PropertyModel groupModel = mModelList.get(0).model;
        assertNull(groupModel.get(TabProperties.ACTOR_UI_STATE));

        ArgumentCaptor<ActorUiTabController.Observer> observerCaptor =
                ArgumentCaptor.forClass(ActorUiTabController.Observer.class);
        verify(mActorUiTabController, atLeastOnce()).addObserver(observerCaptor.capture());
        ActorUiTabController.Observer actorObserver = observerCaptor.getValue();

        // Set actor state on Tab 2 (hidden tab).
        setUpActorState(mTab2, TabIndicatorStatus.DYNAMIC);
        UiTabState newState2 =
                new UiTabState(TAB2_ID, null, null, TabIndicatorStatus.DYNAMIC, false);
        actorObserver.onUiTabStateChanged(newState2);

        assertNull(groupModel.get(TabProperties.ACTOR_UI_STATE));

        ThumbnailFetcher fetcher = groupModel.get(TabProperties.THUMBNAIL_FETCHER);
        assertNotNull(fetcher);

        // Set actor state on Tab 1 (representative tab).
        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);
        UiTabState newState1 =
                new UiTabState(TAB1_ID, null, null, TabIndicatorStatus.DYNAMIC, false);
        actorObserver.onUiTabStateChanged(newState1);

        assertNull(groupModel.get(TabProperties.ACTOR_UI_STATE));
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_NestedLayout() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        setUpActorState(mTab1, TabIndicatorStatus.NONE);
        setUpActorState(tab3, TabIndicatorStatus.NONE);

        // Reset again so that the newly attached UserDataHost ActorUiTabControllers are picked up
        // during property model creation.
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        assertEquals(3, mModelList.size()); // GROUP_HEADER, mTab1, tab3
        PropertyModel child1Model = mModelList.get(1).model;
        assertNull(child1Model.get(TabProperties.ACTOR_UI_STATE));

        ArgumentCaptor<ActorUiTabController.Observer> observerCaptor =
                ArgumentCaptor.forClass(ActorUiTabController.Observer.class);
        // addObserver is called for mTab1 and tab3 during resetWithListOfTabs.
        verify(mActorUiTabController, atLeastOnce()).addObserver(observerCaptor.capture());
        ActorUiTabController.Observer actorObserver = observerCaptor.getValue();

        // Set actor state on Tab 1 (child tab).
        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);
        UiTabState newState1 =
                new UiTabState(TAB1_ID, null, null, TabIndicatorStatus.DYNAMIC, false);
        actorObserver.onUiTabStateChanged(newState1);

        assertNotNull(child1Model.get(TabProperties.ACTOR_UI_STATE));
        assertEquals(
                TabIndicatorStatus.DYNAMIC,
                child1Model.get(TabProperties.ACTOR_UI_STATE).tabIndicator);
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_RefreshOnReset() {
        // Initial state: Active task.
        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);
        PropertyModel model = mModelList.get(0).model;
        assertEquals(
                TabIndicatorStatus.DYNAMIC, model.get(TabProperties.ACTOR_UI_STATE).tabIndicator);

        // Exit Tab Switcher.
        mMediator.resetWithListOfTabs(null, null, false);

        // Task ends while hidden.
        setUpActorState(mTab1, TabIndicatorStatus.NONE);

        // Re-enter Tab Switcher.
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);
        model = mModelList.get(0).model;

        assertNull(model.get(TabProperties.ACTOR_UI_STATE));
    }

    @EnableFeatures(ChromeFeatureList.GLIC)
    @Test
    public void testActorUiState_RefreshOnUpdateTab() {
        // Initial state: No task.
        setUpActorState(mTab1, TabIndicatorStatus.NONE);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);
        PropertyModel model = mModelList.get(0).model;
        assertNull(model.get(TabProperties.ACTOR_UI_STATE));

        // Task starts while Tab Switcher is reset with same list.
        setUpActorState(mTab1, TabIndicatorStatus.DYNAMIC);
        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        model = mModelList.get(0).model;

        assertNotNull(model.get(TabProperties.ACTOR_UI_STATE));
        assertEquals(
                TabIndicatorStatus.DYNAMIC, model.get(TabProperties.ACTOR_UI_STATE).tabIndicator);
    }

    @Test
    public void testTabUnderlineManager_NullInGridMode() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        assertNull(mTabListConfig.tabUnderlineManager);
        verify(mTabUnderlineManager, never()).addObserver(any());
    }

    @Test
    public void testTabUnderlineManager_NotRegisteredInIncognito() {
        when(mTab1.isIncognito()).thenReturn(true);
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        verify(mTabUnderlineManager, never()).registerTab(mTab1);
    }

    @Test
    public void testTabUnderlineManager_RegisteredForNonIncognito() {
        when(mTab1.isIncognito()).thenReturn(false);
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        verify(mTabUnderlineManager).registerTab(mTab1);
    }

    @Test
    public void testTabUnderlineObserver_UpdatesModel() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        verify(mTabUnderlineManager).addObserver(mTabUnderlineObserverCaptor.capture());
        TabUnderlineManager.Observer observer = mTabUnderlineObserverCaptor.getValue();
        assertNotNull(observer);

        assertFalse(mModelList.get(0).model.get(TabProperties.IS_GLIC_ACTIVE));

        observer.onIndicatorStateChanged(TAB1_ID, /* isActive= */ true);
        assertTrue(mModelList.get(0).model.get(TabProperties.IS_GLIC_ACTIVE));

        observer.onIndicatorStateChanged(TAB1_ID, /* isActive= */ false);
        assertFalse(mModelList.get(0).model.get(TabProperties.IS_GLIC_ACTIVE));

        // Ensure no NPE occurs when an invalid or unknown tab ID is updated.
        observer.onIndicatorStateChanged(Tab.INVALID_TAB_ID, /* isActive= */ true);
    }

    @Test
    public void testDestroy_RemovesTabUnderlineObserver() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        verify(mTabUnderlineManager).addObserver(mTabUnderlineObserverCaptor.capture());

        mMediator.destroy();
        verify(mTabUnderlineManager).removeObserver(mTabUnderlineObserverCaptor.getValue());
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
            when(mTabModel.getTabAt(index)).thenReturn(tab);
            when(mTabModel.indexOf(tab)).thenReturn(index);
            index++;
        }
        when(mTabModel.getCount()).thenReturn(totalCount);
        initAndAssertAllProperties(mMediator);
    }

    // initAndAssertAllProperties called with custom mMediator (e.g. if spy needs to be used)
    private void initAndAssertAllProperties(TabListMediator mediator) {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        int tabGroupObserverCount = mTabGroupObserverCaptor.getAllValues().size();
        int tabModelObserverCount = mTabModelObserverCaptor.getAllValues().size();

        mediator.resetWithListOfTabs(tabs, null, false);

        assertEquals(mTabModelObserverCaptor.getAllValues().size(), tabModelObserverCount + 1);
        assertEquals(mTabGroupObserverCaptor.getAllValues().size(), tabGroupObserverCount + 1);

        for (Callback<TabFavicon> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(mFavicon);
        }

        assertThat(mModelList.size(), equalTo(mTabModel.getCount()));

        assertThat(mModelList.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModelList.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        if (!mTabModel.isTabInTabGroup(mTab1)) {
            assertThat(mModelList.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        }
        if (!mTabModel.isTabInTabGroup(mTab2)) {
            assertThat(mModelList.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        }

        assertNotNull(mModelList.get(0).model.get(TabProperties.FAVICON_FETCHER));
        assertNotNull(mModelList.get(1).model.get(TabProperties.FAVICON_FETCHER));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));

        // Only tab surfaces configured with a ThumbnailProvider (e.g. Grid) bind
        // THUMBNAIL_FETCHER.
        if (mThumbnailProvider != null) {
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

    @Test
    public void testSetThumbnailSpinnerVisibility() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        initAndAssertAllProperties();

        PropertyModel model = mModelList.get(0).model;
        model.addObserver(mPropertyObserver);

        mMediator.setThumbnailSpinnerVisibility(mTab1, true);
        verify(mPropertyObserver)
                .onPropertyChanged(eq(model), eq(TabProperties.SHOW_THUMBNAIL_SPINNER));
        assertTrue(model.get(TabProperties.SHOW_THUMBNAIL_SPINNER));

        mMediator.setThumbnailSpinnerVisibility(mTab1, false);
        verify(mPropertyObserver, times(2))
                .onPropertyChanged(eq(model), eq(TabProperties.SHOW_THUMBNAIL_SPINNER));
        assertFalse(model.get(TabProperties.SHOW_THUMBNAIL_SPINNER));
        verify(mPropertyObserver).onPropertyChanged(eq(model), eq(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test
    public void testSetThumbnailSpinnerVisibility_TabInGroup() {
        setUpTabListMediator(TabListMediatorType.TAB_GRID_DIALOG, TabListMode.GRID);
        initAndAssertAllProperties();

        createTabGroup(List.of(mTab1), TAB_GROUP_ID);

        PropertyModel model = mModelList.get(0).model;
        model.addObserver(mPropertyObserver);

        mMediator.setThumbnailSpinnerVisibility(mTab1, true);
        verify(mPropertyObserver)
                .onPropertyChanged(eq(model), eq(TabProperties.SHOW_THUMBNAIL_SPINNER));
        assertTrue(model.get(TabProperties.SHOW_THUMBNAIL_SPINNER));

        mMediator.setThumbnailSpinnerVisibility(mTab1, false);
        verify(mPropertyObserver, times(2))
                .onPropertyChanged(eq(model), eq(TabProperties.SHOW_THUMBNAIL_SPINNER));
        assertFalse(model.get(TabProperties.SHOW_THUMBNAIL_SPINNER));
        verify(mPropertyObserver).onPropertyChanged(eq(model), eq(TabProperties.THUMBNAIL_FETCHER));
    }

    @Test(expected = AssertionError.class)
    public void testSetThumbnailSpinnerVisibility_NotFlatLayout_Asserts() {
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);
        initAndAssertAllProperties();

        mMediator.setThumbnailSpinnerVisibility(mTab1, true);
    }

    @Test
    public void indexFromTabId_NestedLayout_PrioritizesChildOverHeader() {
        setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        // Model list contains:
        // [0] Group Header Card (shares TAB1_ID)
        // [1] First Child webpage row (shares TAB1_ID)
        // [2] Second Child webpage row (TAB3_ID)
        assertEquals(3, mModelList.size());

        // Verify that querying indexFromTabId for TAB1_ID correctly prioritizes and returns the
        // nested child webpage row index (index 1) over the parent Group Header Card (index 0)
        assertEquals(1, mModelList.indexFromTabId(TAB1_ID));
        assertEquals(2, mModelList.indexFromTabId(TAB3_ID));
    }

    @Test
    public void closeLastTabInGroup_NestedLayout_RemovesHeaderCard() {
        Tab tab3 = setUpNestedLayoutWithTwoTabGroup(/* isCollapsed= */ false);

        // Initially, list contains: [0] Group Header, [1] First Child, [2] Second Child.
        assertEquals(3, mModelList.size());

        // Simulate closing the first child tab (mTab1).
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(mTab1);
        // The group still exists because tab3 remains. The first child is removed.
        assertEquals(2, mModelList.size());

        // Update mocks to reflect only tab3 remaining in the group.
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(tab3));
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(1);

        // Simulate closing the last child tab (tab3).
        mTabModelObserverCaptor.getValue().didRemoveTabForClosure(tab3);
        // The child card is removed.
        assertEquals(1, mModelList.size());

        // Update mocks to reflect the group no longer existing.
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(new ArrayList<>());
        when(mTabModel.getTabCountForGroup(TAB_GROUP_ID)).thenReturn(0);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);

        // Simulate the TabGroupModelFilter triggering group removal didRemoveTabGroup observer.
        mTabGroupObserverCaptor
                .getValue()
                .didRemoveTabGroup(TAB1_ID, TAB_GROUP_ID, DidRemoveTabGroupReason.UNGROUP);

        // Verify that the Group Header card is also removed, leaving the list empty.
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testPriceMessageDisabled_WhenMessageCardsNotSupported() {
        mTabListConfig =
                new TabListConfig.Builder(TabListLayoutType.GROUPED)
                        .setSupportsMessageCards(false)
                        .build();
        setUpTabListMediator(TabListMediatorType.TAB_SWITCHER, TabListMode.GRID);

        // Verify getPriceWelcomeMessageInsertionIndex returns INVALID_TAB_INDEX without throwing
        // assertion errors.
        assertEquals(TabList.INVALID_TAB_INDEX, mMediator.getPriceWelcomeMessageInsertionIndex());

        // Verify updateLayout returns early without throwing assertion errors.
        mMediator.updateLayout();

        // Verify addSpecialItemToModel with an invalid index is safely ignored
        int initialSize = mModelList.size();
        mMediator.addSpecialItemToModel(
                TabList.INVALID_TAB_INDEX,
                UiType.PRICE_MESSAGE,
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build());
        assertEquals(initialSize, mModelList.size());
    }

    @Test
    public void testRailCollapseStateSupplier_updatesExistingTabsAndNewTabs() {
        SettableNonNullObservableSupplier<@RailCollapseState Integer> railCollapseStateSupplier =
                ObservableSuppliers.createNonNull(RailCollapseState.EXPANDED);
        mTabListConfig =
                new TabListConfig.Builder(TabListLayoutType.NESTED)
                        .setRailCollapseStateSupplier(railCollapseStateSupplier)
                        .build();

        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        mockTabIndexes(mTab1, mTab2, tab3);

        mMediator.resetWithListOfTabs(List.of(mTab1, mTab2), null, false);
        assertEquals(
                RailCollapseState.EXPANDED,
                mModelList.get(0).model.get(TabProperties.RAIL_COLLAPSE_STATE));
        assertEquals(
                RailCollapseState.EXPANDED,
                mModelList.get(1).model.get(TabProperties.RAIL_COLLAPSE_STATE));

        railCollapseStateSupplier.set(RailCollapseState.COLLAPSED);
        assertEquals(
                RailCollapseState.COLLAPSED,
                mModelList.get(0).model.get(TabProperties.RAIL_COLLAPSE_STATE));
        assertEquals(
                RailCollapseState.COLLAPSED,
                mModelList.get(1).model.get(TabProperties.RAIL_COLLAPSE_STATE));

        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        tab3,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        assertEquals(
                RailCollapseState.COLLAPSED,
                mModelList.get(2).model.get(TabProperties.RAIL_COLLAPSE_STATE));
    }

    @Test
    public void testRailCollapseStateSupplier_unregistersOnDestroy() {
        SettableNonNullObservableSupplier<@RailCollapseState Integer> railCollapseStateSupplier =
                ObservableSuppliers.createNonNull(RailCollapseState.EXPANDED);
        mTabListConfig =
                new TabListConfig.Builder(TabListLayoutType.NESTED)
                        .setRailCollapseStateSupplier(railCollapseStateSupplier)
                        .build();

        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        assertTrue(railCollapseStateSupplier.hasObservers());

        mMediator.destroy();
        assertFalse(railCollapseStateSupplier.hasObservers());
    }

    @Test
    public void testToggleTabGroupExpansion_LogsHistogramVertical() {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);

        List<Tab> tabs = List.of(mTab1);
        createTabGroup(tabs, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Mock tab model collapse state.
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(false);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.TabGroupCollapsed", true);

        // Toggle expansion (from expanded (false) to collapsed (true)).
        mMediator.toggleTabGroupExpansion(TAB1_ID);

        verify(mTabModel)
                .setTabGroupCollapsed(TAB_GROUP_ID, /* isCollapsed= */ true, /* animate= */ false);

        histogramWatcher.assertExpected();

        // Test expand.
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(true);
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.TabGroupCollapsed", false);

        // Toggle expansion (from collapsed (true) to expanded (false)).
        mMediator.toggleTabGroupExpansion(TAB1_ID);

        verify(mTabModel)
                .setTabGroupCollapsed(TAB_GROUP_ID, /* isCollapsed= */ false, /* animate= */ false);

        histogramWatcher.assertExpected();
    }

    private void mockTabIndexes(Tab... tabs) {
        for (int i = 0; i < tabs.length; i++) {
            Tab tab = tabs[i];
            when(mTabModel.getTabAt(i)).thenReturn(tab);
            when(mTabModel.indexOf(tab)).thenReturn(i);
        }
        when(mTabModel.getCount()).thenReturn(tabs.length);
    }

    private void mockRepresentativeTabs(Tab... representativeTabs) {
        when(mTabModel.getIndividualTabAndGroupCount()).thenReturn(representativeTabs.length);
        for (int i = 0; i < representativeTabs.length; i++) {
            when(mTabModel.getRepresentativeTabAt(i)).thenReturn(representativeTabs[i]);
            if (representativeTabs[i] != null) {
                Mockito.lenient()
                        .when(mTabModel.representativeIndexOf(representativeTabs[i]))
                        .thenReturn(i);
            }
        }
    }

    private Tab prepareTab(int id, String title, GURL url) {
        Tab tab = TabUiUnitTestUtils.prepareTab(id, title, url);
        when(tab.getView()).thenReturn(mTabView);
        when(tab.isIncognito()).thenReturn(true);
        when(tab.getTitle()).thenReturn(title);
        when(tab.getAlertState()).thenReturn(TabAlert.NONE);
        int count = mTabModel.getCount();
        when(mTabModel.getTabAt(count)).thenReturn(tab);
        when(mTabModel.getCount()).thenReturn(count);
        when(mTabModel.getTabById(id)).thenReturn(tab);
        when(mIncognitoTabModel.getTabById(id)).thenReturn(tab);
        when(tab.getProfile()).thenReturn(mProfile);
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

    private void prepareViewHolder(
            SimpleRecyclerViewAdapter.ViewHolder viewHolder, int id, int position) {
        viewHolder.model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                        .with(TabProperties.TAB_ID, id)
                        .with(CARD_TYPE, TAB)
                        .build();
        when(viewHolder.getAdapterPosition()).thenReturn(position);
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
        if (mMediator != null) {
            mMediator.resetWithListOfTabs(null, null, false);
            mMediator.destroy();
            mMediator = null;
        }
        doNothing().when(mTabModel).addTabGroupObserver(mTabGroupObserverCaptor.capture());
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());

        TabListMediator.TabGridDialogHandler handler =
                type == TabListMediatorType.TAB_GRID_DIALOG ? mTabGridDialogHandler : null;
        mThumbnailProvider = mode == TabListMode.GRID ? getTabThumbnailCallback() : null;
        @TabComponentId
        int componentId =
                type == TabListMediatorType.VERTICAL_TABS
                        ? TabComponentId.VERTICAL_TABS
                        : TabComponentId.GRID_TAB_SWITCHER;

        @TabListLayoutType int layoutType = TabListLayoutType.FLAT;
        if (type == TabListMediatorType.VERTICAL_TABS) {
            layoutType = TabListLayoutType.NESTED;
        } else if (type == TabListMediatorType.TAB_SWITCHER) {
            layoutType = TabListLayoutType.GROUPED;
        }
        // Reuse pre-configured TabListConfig if it matches the target layout; otherwise
        // derive defaults.
        boolean hasMatchingConfig =
                mTabListConfig != null && mTabListConfig.layoutType == layoutType;
        @UiType
        int tabUiType =
                hasMatchingConfig
                        ? mTabListConfig.tabUiType
                        : (mode == TabListMode.BOTTOM_STRIP ? UiType.STRIP : UiType.TAB);
        boolean supportsMessageCards =
                hasMatchingConfig
                        ? mTabListConfig.supportsMessageCards
                        : (type == TabListMediatorType.TAB_SWITCHER);
        boolean supportsModifierMultiSelect =
                hasMatchingConfig
                        ? mTabListConfig.supportsModifierMultiSelect
                        : (type == TabListMediatorType.VERTICAL_TABS);
        boolean supportsTabLoadingState =
                hasMatchingConfig
                        ? mTabListConfig.supportsTabLoadingState
                        : (type == TabListMediatorType.VERTICAL_TABS);
        boolean supportsShrinkCloseAnimation =
                hasMatchingConfig
                        ? mTabListConfig.supportsShrinkCloseAnimation
                        : (mode == TabListMode.GRID);
        boolean supportsDelayedTabAddition =
                hasMatchingConfig
                        ? mTabListConfig.supportsDelayedTabAddition
                        : (type == TabListMediatorType.TAB_SWITCHER
                                || type == TabListMediatorType.TAB_GRID_DIALOG);
        boolean supportsTabContextClick =
                hasMatchingConfig
                        ? mTabListConfig.supportsTabContextClick
                        : (type != TabListMediatorType.VERTICAL_TABS);
        @TabClosingSource
        int tabClosingSource =
                hasMatchingConfig
                        ? mTabListConfig.tabClosingSource
                        : (type == TabListMediatorType.VERTICAL_TABS
                                ? TabClosingSource.VERTICAL_TAB_STRIP
                                : TabClosingSource.UNKNOWN);
        NonNullObservableSupplier<@RailCollapseState Integer> railCollapseStateSupplier =
                hasMatchingConfig ? mTabListConfig.railCollapseStateSupplier : null;
        TabHoverCardListener tabHoverCardListener =
                hasMatchingConfig ? mTabListConfig.tabHoverCardListener : null;
        TabUnderlineManager tabUnderlineManager =
                hasMatchingConfig
                        ? mTabListConfig.tabUnderlineManager
                        : (type == TabListMediatorType.VERTICAL_TABS ? mTabUnderlineManager : null);

        mTabListConfig =
                new TabListConfig.Builder(layoutType)
                        .setTabUiType(tabUiType)
                        .setSupportsMessageCards(supportsMessageCards)
                        .setSupportsModifierMultiSelect(supportsModifierMultiSelect)
                        .setSupportsTabLoadingState(supportsTabLoadingState)
                        .setSupportsShrinkCloseAnimation(supportsShrinkCloseAnimation)
                        .setSupportsDelayedTabAddition(supportsDelayedTabAddition)
                        .setSupportsTabContextClick(supportsTabContextClick)
                        .setTabClosingSource(tabClosingSource)
                        .setRailCollapseStateSupplier(railCollapseStateSupplier)
                        .setTabHoverCardListener(tabHoverCardListener)
                        .setTabUnderlineManager(tabUnderlineManager)
                        .build();

        mMediator =
                new MediatorBuilder()
                        .setThumbnailProvider(mThumbnailProvider)
                        .setDialogHandler(handler)
                        .setComponentId(componentId)
                        .setTabListConfig(mTabListConfig)
                        .build();
        TrackerFactory.setTrackerForTests(mTracker);
        mMediator.registerOrientationListener(mGridLayoutManager);

        mMediator.initWithNative(mProfile);

        initAndAssertAllProperties();
    }

    private void createTabGroup(List<Tab> tabs, Token tabGroupId) {
        createTabGroup(tabs, tabGroupId, /* index= */ null);
    }

    private void createTabGroup(List<Tab> tabs, Token tabGroupId, @Nullable Integer index) {
        when(mTabModel.getTabCountForGroup(tabGroupId)).thenReturn(tabs.size());
        when(mTabModel.tabGroupExists(tabGroupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(tabGroupId)).thenReturn(tabs);
        when(mTabModel.getTabGroupCollapsed(tabGroupId)).thenReturn(true);
        int firstTabId = tabs.get(0).getId();
        when(mTabModel.getGroupLastShownTabId(tabGroupId)).thenReturn(firstTabId);
        for (Tab tab : tabs) {
            when(mTabModel.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
            if (index != null) {
                when(mTabModel.representativeIndexOf(tab)).thenReturn(index);
            }
        }
        int modelIndex = mModelList.indexFromTabId(firstTabId);
        if (modelIndex != TabModel.INVALID_TAB_INDEX) {
            PropertyModel model = mModelList.get(modelIndex).model;
            if (model.containsKey(TabProperties.TAB_GROUP_CARD_COLOR)) {
                model.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.GREY);
                model.set(TabProperties.TAB_GROUP_ID, null);
            }
            model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);
            @TabListLayoutType int layoutType = mTabListConfig.layoutType;
            boolean isCollapsed =
                    layoutType != TabListLayoutType.NESTED
                            || mTabModel.getTabGroupCollapsed(tabGroupId);
            model.set(TabProperties.IS_COLLAPSED, isCollapsed);
        }
    }

    private Tab setUpNestedLayoutWithTwoTabGroup(boolean isCollapsed) {
        setUpTabListMediator(TabListMediatorType.VERTICAL_TABS, TabListMode.VERTICAL);
        mMediator.initWithNative(mProfile);
        mMediator.resetWithListOfTabs(null, null, false);

        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = List.of(mTab1, tab3);
        createTabGroup(tabs, TAB_GROUP_ID);

        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getTabGroupCollapsed(TAB_GROUP_ID)).thenReturn(isCollapsed);
        when(mTabModel.getTabGroupColorWithFallback(TAB_GROUP_ID)).thenReturn(COLOR_2);
        when(mTabModel.getTabById(TAB1_ID)).thenReturn(mTab1);
        when(mTabModel.getTabById(TAB3_ID)).thenReturn(tab3);
        mockTabIndexes(mTab1, tab3);

        mMediator.resetWithListOfTabs(List.of(mTab1), null, false);

        // Verify color initialization for nested children.
        if (!isCollapsed) {
            PropertyModel child1Model = mModelList.get(1).model;
            assertEquals(mTab1.getId(), child1Model.get(TabProperties.TAB_ID));
            assertEquals(COLOR_2, (int) child1Model.get(TabProperties.TAB_GROUP_CARD_COLOR));
            PropertyModel child3Model = mModelList.get(2).model;
            assertEquals(tab3.getId(), child3Model.get(TabProperties.TAB_ID));
            assertEquals(COLOR_2, (int) child3Model.get(TabProperties.TAB_GROUP_CARD_COLOR));
        }

        return tab3;
    }

    private void mockOptimizationGuideResponse(
            @OptimizationGuideDecision int decision, Map<GURL, Any> responses) {
        for (Map.Entry<GURL, Any> responseEntry : responses.entrySet()) {
            doAnswer(
                            (Answer<Object>)
                                    invocation -> {
                                        OptimizationGuideCallback callback =
                                                invocation.getArgument(2);
                                        callback.onOptimizationGuideDecision(
                                                decision, responseEntry.getValue());
                                        return null;
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
        List<Tab> tabs = List.of(mTab1, mTab2, tab3);
        mMediator.resetWithListOfTabs(tabs, null, false);
        assertThat(mModelList.size(), equalTo(3));
        assertThat(mModelList.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModelList.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModelList.get(2).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    private void addSpecialItem(int index, @UiType int uiType, int itemIdentifier) {
        when(mPropertyModel.get(CARD_TYPE)).thenReturn(MESSAGE);
        if (isMessageCard(uiType)) {
            when(mPropertyModel.get(MESSAGE_TYPE)).thenReturn(itemIdentifier);
        }
        // Avoid auto-updating the layout when inserting the special card.
        when(mSpanSizeLookup.getSpanSize(anyInt())).thenReturn(1);
        mMediator.addSpecialItemToModel(index, uiType, mPropertyModel);
    }

    private void prepareTestMaybeShowPriceWelcomeMessage() {
        initAndAssertAllProperties();
        setPriceTrackingEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.SHARED_PREFERENCES_MANAGER.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, true);
        PriceDrop priceDrop = new PriceDrop("1", "2");
        mPriceTabData = new PriceTabData(TAB1_ID, priceDrop);
        when(mShoppingPersistedTabData.getPriceDrop()).thenReturn(priceDrop);
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
        if (propertyKey instanceof ReadableObjectPropertyKey<?> objectKey) {
            assertNull(
                    "Expected property to be unset, property=" + objectKey, model.get(objectKey));
        } else {
            throw new AssertionError(
                    "Unsupported key type passed to function, add it to #assertUnset");
        }
    }

    private void setupSyncedGroup(boolean isShared) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = isShared ? COLLABORATION_ID1 : null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
    }

    private void updateTabAlertState(Tab tab, @TabAlert int alertState) {
        when(tab.getAlertState()).thenReturn(alertState);
        when(tab.getMediaState()).thenReturn(TabUtils.getMediaStateForAlert(alertState));
        mTabObserverCaptor.getValue().onAlertStateChanged(tab, alertState);
    }

    private static ProductPrice createProductPrice(long amountMicros, String currencyCode) {
        return ProductPrice.newBuilder()
                .setCurrencyCode(currencyCode)
                .setAmountMicros(amountMicros)
                .build();
    }

    private void setUpActorState(Tab tab, @TabIndicatorStatus int status) {
        GlicEnabling.setEnabledForTesting(true);
        UiTabState state =
                new UiTabState(
                        tab.getId(),
                        mActorOverlayState,
                        mHandoffButtonState,
                        status,
                        tab.isIncognito());

        when(mActorUiTabController.getUiTabState()).thenReturn(state);
        tab.getUserDataHost().setUserData(ActorUiTabController.class, mActorUiTabController);
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
     * @param isGrouped true if the tabs should be grouped.
     */
    private void resetWithRegularTabs(boolean isGrouped) {
        mockRepresentativeTabs(mTab1, mTab2);
        if (isGrouped) {
            when(mTabModel.getRelatedTabList(eq(TAB1_ID))).thenReturn(List.of(mTab1, mTab2));
            when(mTabModel.getRelatedTabList(eq(TAB2_ID))).thenReturn(List.of(mTab1, mTab2));
            when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(true);
            when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(true);
        } else {
            when(mTabModel.getRelatedTabList(eq(TAB1_ID))).thenReturn(List.of(mTab1));
            when(mTabModel.getRelatedTabList(eq(TAB2_ID))).thenReturn(List.of(mTab2));
            when(mTabModel.isTabInTabGroup(mTab1)).thenReturn(false);
            when(mTabModel.isTabInTabGroup(mTab2)).thenReturn(false);
        }
        List<Tab> tabs = List.of(mTab1, mTab2);
        when(mTab1.isIncognito()).thenReturn(false);
        when(mTab2.isIncognito()).thenReturn(false);
        mMediator.resetWithListOfTabs(tabs, null, false);
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
        List<Tab> group = List.of(mTab1, mTab2);
        createTabGroup(group, TAB_GROUP_ID);
        mMediator.resetWithListOfTabs(tabs, null, false);

        // Assert that the callback performs as expected.
        assertNotNull(mModelList.get(POSITION1).model.get(TabProperties.TAB_ACTION_BUTTON_DATA));
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(tabs);
        when(mTabModel.getGroupLastShownTabId(TAB_GROUP_ID)).thenReturn(TAB1_ID);

        // Act
        mMediator.onMenuItemClicked(
                menuId, TAB_GROUP_ID, /* collaborationId= */ null, listViewTouchTracker);

        // Assert
        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(mTabModel, TAB_GROUP_ID)
                                        .allowUndo(shouldAllowUndo)
                                        .hideTabGroups(shouldHideTabGroups)
                                        .build()),
                        /* allowDialog= */ eq(true),
                        any());
    }
}
