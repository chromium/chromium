// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipDescription;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageButton;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.Token;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupMergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListItemOnClickListenerProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY,
    ChromeFeatureList.TASK_MANAGER_CLANK,
    TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS,
    ChromeFeatureList.ANIMATED_IMAGE_DRAG_SHADOW
})
public class VerticalTabListCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Profile mProfile;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private SendTabToSelfAndroidBridge.Natives mSendTabToSelfAndroidBridgeNatives;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mSelectorObserverCaptor;
    @Captor private ArgumentCaptor<View> mShadowViewCaptor;
    @Mock private VerticalTabsActionDelegate mVerticalTabsActionDelegate;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ActivityResultTracker mActivityResultTracker;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private TabStripContextMenuCoordinator mTabStripContextMenuCoordinator;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private TabContextMenuCoordinator mTabContextMenuCoordinator;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private MultiInstanceOrchestrator mMultiInstanceOrchestrator;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private TabGroupContextMenuCoordinator mTabGroupContextMenuCoordinator;
    @Mock private TabSwitcherDragHandler mMainTabSwitcherDragHandler;
    @Mock private TabSwitcherDragHandler mPinnedTabSwitcherDragHandler;
    @Mock private UndoBarThrottle mUndoBarThrottle;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock private VerticalTabRailCollapseController.RailCollapseListener mMockRailCollapseListener;
    @Mock private ViewStub mTabHoverCardViewStub;
    @Mock private ViewStub mTabGroupHoverCardViewStub;
    @Mock private ViewGroup mHoverCardParent;
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private TabContentManager mTabContentManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabHoverCardView mTabHoverCardView;
    @Mock private TabGroupHoverCardView mTabGroupHoverCardView;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private TabModel mEmptyTabModel;
    @Mock private TabModel mNewTabModel;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabUngrouper mTabUngrouper;
    @Mock private View mMockChildView;
    @Mock private Tab mMockTab1;
    @Mock private Tab mMockTab2;
    @Mock private Tab mMockTab3;
    @Mock private Tab mIncognitoTab;
    @Mock private DragEvent mDragEvent;

    private static final int TAB_ID_1 = 1;
    private static final int TAB_ID_2 = 2;
    private static final int TAB_ID_3 = 3;
    private static final int PINNED_TAB_ID = 3;
    private static final int NON_EXISTENT_TAB_ID = 999;
    private static final GURL MOCK_URL = new GURL("https://google.com");
    private static final int TEST_CONTAINER_WIDTH_PX = 800;
    private static final int TEST_CONTAINER_HEIGHT_PX = 1000;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    private Activity mActivity;
    private final SettableMonotonicObservableSupplier<ShareDelegate> mShareDelegateSupplier =
            ObservableSuppliers.createMonotonic(mShareDelegate);
    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableNonNullObservableSupplier<Integer> mVerticalTabsWidthSupplier =
            ObservableSuppliers.createNonNull(0);
    private final List<TabGroupObserver> mTabGroupObservers = new ArrayList<>();
    private final List<TabModelObserver> mTabModelObservers = new ArrayList<>();
    private VerticalTabListCoordinator mCoordinator;
    private int mMinPinnedTabGap;
    private int mMinPinnedTabWidth;

    @Before
    public void setUp() {
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mSendTabToSelfAndroidBridgeNatives);
        when(mSendTabToSelfAndroidBridgeNatives.getEntryPointDisplayReason(any(), any()))
                .thenReturn(null);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[0]);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(false);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        when(mShoppingServiceFactoryJniMock.getForProfile(any())).thenReturn(mShoppingService);
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mMinPinnedTabGap =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_gap);
        mMinPinnedTabWidth =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_min_width);
        IncognitoUtils.setEnabledForTesting(true);

        mCurrentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel, mIncognitoTabModel));
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.isTabModelRestored()).thenReturn(true);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.iterator()).thenReturn(Collections.emptyIterator());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mTabContentManagerSupplier.get()).thenReturn(mTabContentManager);
        GlicEnabling.setEnabledForTesting(false);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);

        when(mTabHoverCardViewStub.getParent()).thenReturn(mHoverCardParent);
        when(mTabHoverCardView.getContext()).thenReturn(mActivity);
        doAnswer(
                        invocation -> {
                            ViewStub.OnInflateListener listener = invocation.getArgument(0);
                            if (listener != null) {
                                listener.onInflate(mTabHoverCardViewStub, mTabHoverCardView);
                            }
                            return null;
                        })
                .when(mTabHoverCardViewStub)
                .setOnInflateListener(any());

        when(mTabGroupHoverCardViewStub.getParent()).thenReturn(mHoverCardParent);
        when(mTabGroupHoverCardView.getContext()).thenReturn(mActivity);
        doAnswer(
                        invocation -> {
                            ViewStub.OnInflateListener listener = invocation.getArgument(0);
                            if (listener != null) {
                                listener.onInflate(
                                        mTabGroupHoverCardViewStub, mTabGroupHoverCardView);
                            }
                            return null;
                        })
                .when(mTabGroupHoverCardViewStub)
                .setOnInflateListener(any());

        doAnswer(
                        invocation -> {
                            mTabGroupObservers.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mTabModel)
                .addTabGroupObserver(any(TabGroupObserver.class));

        doAnswer(
                        invocation -> {
                            TabModelObserver observer = invocation.getArgument(0);
                            if (!mTabModelObservers.contains(observer)) {
                                mTabModelObservers.add(observer);
                            }
                            return null;
                        })
                .when(mTabModel)
                .addObserver(any(TabModelObserver.class));

        doAnswer(
                        invocation -> {
                            TabModelObserver observer = invocation.getArgument(0);
                            if (!mTabModelObservers.contains(observer)) {
                                mTabModelObservers.add(observer);
                            }
                            return null;
                        })
                .when(mIncognitoTabModel)
                .addObserver(any(TabModelObserver.class));
    }

    @After
    public void tearDown() {
        MultiInstanceOrchestratorFactory.setInstanceForTesting(null);
    }

    // =============================================================================================
    // Initialization & Lifecycle Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testConstructor() {
        doNothing().when(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        createCoordinator();
        assertNotNull(mCoordinator.getView());

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        assertNotNull(recyclerView);
        assertNotNull(recyclerView.getAdapter());
        assertNotNull(recyclerView.getLayoutManager());

        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        assertEquals(LinearLayoutManager.VERTICAL, layoutManager.getOrientation());

        // Verify the pinned tabs RecyclerView is initialized but hidden when empty.
        TabListRecyclerView pinnedRecyclerView = view.findViewById(R.id.pinned_tabs_recycler_view);
        assertNotNull(pinnedRecyclerView);
        assertNotNull(pinnedRecyclerView.getAdapter());
        assertNotNull(pinnedRecyclerView.getLayoutManager());
        assertEquals(View.GONE, pinnedRecyclerView.getVisibility());

        GridLayoutManager pinnedLayoutManager =
                (GridLayoutManager) pinnedRecyclerView.getLayoutManager();
        assertEquals(
                VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT,
                pinnedLayoutManager.getSpanCount());

        assertNotNull(mSelectorObserverCaptor.getValue());
        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testPinnedTabsVisibility() {
        // Set up the tab model with a pinned tab.
        Tab pinnedTab = prepareMockTab(mMockTab1, PINNED_TAB_ID);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(pinnedTab));
        when(mTabModel.iterator()).thenReturn(List.of(pinnedTab).iterator());
        when(mTabModel.getTabById(PINNED_TAB_ID)).thenReturn(pinnedTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(pinnedTab);

        createCoordinator();

        // Verify the pinned tabs RecyclerView becomes visible when it contains elements.
        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView pinnedRecyclerView = view.findViewById(R.id.pinned_tabs_recycler_view);
        assertNotNull(pinnedRecyclerView);
        assertEquals(View.VISIBLE, pinnedRecyclerView.getVisibility());

        // Swap to an empty tab model.
        when(mEmptyTabModel.getProfile()).thenReturn(mProfile);
        when(mEmptyTabModel.isTabModelRestored()).thenReturn(true);
        when(mEmptyTabModel.getRepresentativeTabList()).thenReturn(Collections.emptyList());
        when(mEmptyTabModel.iterator()).thenReturn(Collections.emptyIterator());

        mCurrentTabModelSupplier.set(mEmptyTabModel);

        // Verify the pinned tabs RecyclerView becomes hidden when empty.
        assertEquals(View.GONE, pinnedRecyclerView.getVisibility());
    }

    @Test
    @SmallTest
    public void testConstructor_AddsSpineDecoration() {
        createCoordinator();
        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        assertNotNull(recyclerView);

        boolean hasSpineDecoration = false;
        for (int i = 0; i < recyclerView.getItemDecorationCount(); i++) {
            if (recyclerView.getItemDecorationAt(i) instanceof VerticalTabGroupSpineDecoration) {
                hasSpineDecoration = true;
                break;
            }
        }
        assertTrue(
                "VerticalTabGroupSpineDecoration should be added to RecyclerView.",
                hasSpineDecoration);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        doNothing().when(mTabModelSelector).addObserver(mSelectorObserverCaptor.capture());
        createCoordinator();
        mCoordinator.setTabStripContextMenuCoordinatorForTesting(mTabStripContextMenuCoordinator);
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        TabModelSelectorObserver observer = mSelectorObserverCaptor.getValue();
        assertNotNull(observer);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        TabListRecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        SimpleRecyclerViewAdapter pinnedAdapter =
                (SimpleRecyclerViewAdapter) pinnedRecyclerView.getAdapter();

        mCoordinator.destroy();

        verify(mTabModelSelector).removeObserver(observer);
        verify(mTabStripContextMenuCoordinator).destroy();
        assertNull(
                "The tab strip context menu reference must be nullified upon destruction.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        verify(mTabContextMenuCoordinator).dismiss();
        assertNull(
                "The tab context menu reference must be nullified upon destruction.",
                mCoordinator.getTabContextMenuCoordinatorForTesting());

        verify(mTabGroupContextMenuCoordinator).destroy();
        assertNull(
                "The tab group context menu coordinator reference must be nullified on lifecycle"
                        + " teardown.",
                mCoordinator.getTabGroupContextMenuCoordinatorForTesting());
        assertTrue(
                "The drag handlers list must be cleared on destruction.",
                mCoordinator.getTabSwitcherDragHandlersForTesting().isEmpty());
        assertNull(
                "The tab list recycler view adapter must be set to null on destruction.",
                recyclerView.getAdapter());
        assertNull(
                "The pinned tab list recycler view adapter must be set to null on destruction.",
                pinnedRecyclerView.getAdapter());
        assertEquals(0, adapter.getModelList().size());
        assertEquals(0, pinnedAdapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testDestroy_RemovesSupplierObserver() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        mCoordinator.destroy();

        when(mNewTabModel.getProfile()).thenReturn(mProfile);
        when(mNewTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mNewTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));

        mCurrentTabModelSupplier.set(mNewTabModel);
        assertEquals(0, adapter.getModelList().size());
    }

    // =============================================================================================
    // Touch & Interception Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testItemTouchListener_OnInterceptTouchEventOverride() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertNotNull("RecyclerView should be initialized inside the layout.", recyclerView);

        // Simulate an action down at coordinates (250, 400).
        MotionEvent downEvent = obtainMotionEvent(MotionEvent.ACTION_DOWN, 250f, 400f);

        // Dispatch the touch event directly into the real RecyclerView's touch pipeline.
        boolean intercepted = recyclerView.onInterceptTouchEvent(downEvent);
        assertFalse("Touch interceptor must remain passive.", intercepted);

        Point savedPoint = mCoordinator.getLastTouchPointForTesting();
        assertEquals("X coordinate should be saved.", 250, savedPoint.x);
        assertEquals("Y coordinate should be saved.", 400, savedPoint.y);

        // The tab strip coordinator should not have been instantiated because we have not advanced
        // the shadow looper to reach the long-press (500ms) milestone.
        assertNull(mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    // =============================================================================================
    // Context Menu & Long-Press Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testVTEmptySpaceLongPress_LaunchesContextMenu() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertRecyclerViewLongPressLaunchesEmptySpaceContextMenu(recyclerView);
    }

    @Test
    @SmallTest
    public void testVTTabListRecyclerView_EmptySpaceRightClick_LaunchesContextMenu() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertEmptySpaceContextMenuRightClick(recyclerView);
    }

    @Test
    @SmallTest
    public void testEmptySpaceContextClickListener_OverChildItem_ConsumesWithoutShowingMenu() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);

        // Position the coordinates over the mock child view.
        mCoordinator.getLastTouchPointForTesting().set(150, 250);
        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);

        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        // Invoke createEmptySpaceContextClickListener with recyclerViewSpy.
        View.OnContextClickListener listener =
                mCoordinator.createEmptySpaceContextClickListenerForTesting(
                        mActivity, recyclerViewSpy);

        boolean consumed = listener.onContextClick(recyclerViewSpy);

        // Verify the event was consumed (returns true) and no context menus of any kind were
        // launched because the mouse is over an actual child, so the context click is already
        // consumed via onInterceptTouchEvent.
        assertTrue("Context click over a tab child item should be consumed.", consumed);
        verify(mTabContextMenuCoordinator, never()).showMenu(any(), any());
        assertNull(
                "Empty space menu must not be launched when click is over a tab item.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testVTHeaderContainerLongPress_LaunchesEmptySpaceContextMenu() {
        createCoordinator();
        // vertical_tab_rail_container.
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        View headerContainer = container.findViewById(R.id.vertical_tab_header_container);
        assertStandardViewLongPressLaunchesMenu(headerContainer);
    }

    @Test
    @SmallTest
    public void testVTHeaderContainerRightClick_LaunchesEmptySpaceContextMenu() {
        createCoordinator();
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        View headerContainer = container.findViewById(R.id.vertical_tab_header_container);

        assertEmptySpaceContextMenuRightClick(headerContainer);
    }

    @Test
    @SmallTest
    public void testVTPinnedTabsEmptySpaceLongPress_LaunchesEmptySpaceContextMenu() {
        createCoordinator();
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView pinnedRecyclerView =
                container.findViewById(R.id.pinned_tabs_recycler_view);
        assertRecyclerViewLongPressLaunchesEmptySpaceContextMenu(pinnedRecyclerView);
    }

    @Test
    @SmallTest
    public void testVTPinnedTabsEmptySpaceRightClick_LaunchesEmptySpaceContextMenu() {
        createCoordinator();
        TabListRecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);

        assertEmptySpaceContextMenuRightClick(pinnedRecyclerView);
    }

    @Test
    @SmallTest
    public void testVTRootContainerLongPress_LaunchesEmptySpaceContextMenu() {
        createCoordinator();
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        assertStandardViewLongPressLaunchesMenu(container);
    }

    @Test
    @SmallTest
    public void testVTRootContainerRightClick_LaunchesContextMenu() {
        createCoordinator();
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        assertEmptySpaceContextMenuRightClick(container);
    }

    @Test
    @SmallTest
    public void testNewTabButtonRightClick_DoesNotLaunchEmptySpaceContextMenu() {
        createCoordinator();
        View newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertContextClickDoesNotLaunchEmptySpaceContextMenu(newTabButton);
    }

    @Test
    @SmallTest
    public void testVTTabSearchButtonTouch_UpdatesLastTouchPointToLocalCoordinates() {
        createCoordinator();
        View tabSearchButton = mCoordinator.getView().findViewById(R.id.tab_search_button);
        assertViewTouchUpdatesLastTouchPoint(tabSearchButton, 30, 35);
    }

    @Test
    @SmallTest
    public void testCollapseButtonRightClick_DoesNotLaunchEmptySpaceContextMenu() {
        createCoordinator();
        View collapseButton = mCoordinator.getView().findViewById(R.id.collapse_button);
        assertContextClickDoesNotLaunchEmptySpaceContextMenu(collapseButton);
    }

    @Test
    @SmallTest
    public void testTabItemInteraction_WithTouchPointLaunchesMenuAtPreciseLocation() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);

        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        // Inject our mock coordinator so we can intercept the rect capture bounds.
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        // Simulate setting the last touch coordinates to a non-zero point.
        mCoordinator.getLastTouchPointForTesting().set(150, 250);

        // Trigger the context menu interaction.
        mCoordinator.handleContextMenuInteractionForTesting(
                mActivity, recyclerViewSpy, /* localX= */ 150f, /* localY= */ 250f);

        // Verify the RectProvider bounds match our 1x1 tight pixel calculation.
        ArgumentCaptor<RectProvider> rectCaptor = ArgumentCaptor.forClass(RectProvider.class);
        ArgumentCaptor<AnchorInfo> anchorInfoCaptor = ArgumentCaptor.forClass(AnchorInfo.class);

        verify(mTabContextMenuCoordinator)
                .showMenu(rectCaptor.capture(), anchorInfoCaptor.capture());

        Rect descriptiveBoundRect = rectCaptor.getValue().getRect();
        assertEquals("Width must be exactly 1 pixel.", 1, descriptiveBoundRect.width());
        assertEquals("Height must be exactly 1 pixel.", 1, descriptiveBoundRect.height());
        assertNotNull(
                "Anchor info parameters must be provided to showMenu.",
                anchorInfoCaptor.getValue());

        if (mCoordinator.getTabContextMenuCoordinatorForTesting() != null) {
            mCoordinator.getTabContextMenuCoordinatorForTesting().dismiss();
        }
    }

    @Test
    @SmallTest
    public void testTabGroupHeaderInteraction_LaunchesGroupHeaderContextMenu() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        when(mMockTab1.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mMockTab1));

        assertNull(mCoordinator.getTabGroupContextMenuCoordinatorForTesting());

        // Inject the mock coordinator here before calling #handleContextMenuInteractionForTesting
        // so that we can verify the rect captor later in this test.
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) recyclerViewSpy.getAdapter();
        PropertyModel groupPropertyModel = adapter.getModelList().get(0).model;
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);

        assertEquals(
                "The adapter lookup should resolve this list item row layout as a TAB_GROUP type.",
                UiType.TAB_GROUP,
                adapter.getItemViewType(0));

        when(recyclerViewSpy.findChildViewUnder(200f, 150f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 200f, /* localY= */ 150f);

        assertTrue(
                "Context gesture interaction on an active group header card should return true.",
                handled);

        ArgumentCaptor<RectProvider> rectCaptor = ArgumentCaptor.forClass(RectProvider.class);
        verify(mTabGroupContextMenuCoordinator).showMenu(rectCaptor.capture(), eq(TAB_GROUP_ID));

        Rect descriptiveBoundRect = rectCaptor.getValue().getRect();
        assertEquals("Width must be exactly 1 pixel.", 1, descriptiveBoundRect.width());
        assertEquals("Height must be exactly 1 pixel.", 1, descriptiveBoundRect.height());

        if (mCoordinator.getTabGroupContextMenuCoordinatorForTesting() != null) {
            // Dismiss/destroy the instantiated context menu tracker to satisfy LifetimeAssert.
            mCoordinator.getTabGroupContextMenuCoordinatorForTesting().destroy();
        }
    }

    @Test
    @SmallTest
    public void testShowTabGroupHeaderContextMenuForGroupId_Success() {
        Tab tab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(tab.getTabGroupId()).thenReturn(TAB_GROUP_ID);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab));
        when(mTabModel.iterator()).thenReturn(List.of(tab).iterator());
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(tab);
        when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(tab));
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(tab));

        createCoordinator();
        mActivity.setContentView(mCoordinator.getView());
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        RecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        PropertyModel groupPropertyModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);
        groupPropertyModel.set(CardProperties.CARD_TYPE, CardProperties.ModelType.TAB_GROUP);
        adapter.getModelList()
                .add(0, new MVCListAdapter.ListItem(UiType.TAB_GROUP, groupPropertyModel));
        measureAndLayoutContainer();

        mCoordinator.showTabGroupHeaderContextMenuForGroupIdForTesting(TAB_GROUP_ID);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabGroupContextMenuCoordinator).showMenu(any(RectProvider.class), eq(TAB_GROUP_ID));

        if (mCoordinator.getTabGroupContextMenuCoordinatorForTesting() != null) {
            mCoordinator.getTabGroupContextMenuCoordinatorForTesting().destroy();
        }
    }

    @Test
    @SmallTest
    public void testTabItemInteraction_LaunchesTabContextMenu() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        assertNull(mCoordinator.getTabContextMenuCoordinatorForTesting());

        // Create a mock View layout box (child of the recycler view) that renders the tab card on
        when(mMockChildView.getWidth()).thenReturn(300);
        when(mMockChildView.getHeight()).thenReturn(100);

        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 50;
                            pos[1] = 100;
                            return null;
                        })
                .when(mMockChildView)
                .getLocationInWindow(any());

        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        // Directly trigger the context interaction mapping.
        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 150f, /* localY= */ 250f);

        assertTrue("Context gesture interaction on an active tab row should return true.", handled);
        assertNotNull(
                "Long press/right click interaction on a tab item view should launch the"
                        + " TabContextMenuCoordinator.",
                mCoordinator.getTabContextMenuCoordinatorForTesting());

        if (mCoordinator.getTabContextMenuCoordinatorForTesting() != null) {
            // Dismiss/destroy the instantiated context menu tracker to satisfy LifetimeAssert.
            mCoordinator.getTabContextMenuCoordinatorForTesting().dismiss();
        }
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testShowTabItemContextMenu_MultiSelect() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);

        // Set up multiple selection state on the mock model.
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        when(mTabModel.isTabMultiSelected(TAB_ID_1)).thenReturn(true);
        List<Integer> multiSelectedIds = List.of(TAB_ID_1, TAB_ID_2);
        when(mTabModel.getOrderedMultiSelectedTabIds()).thenReturn(multiSelectedIds);

        // Create a mock View layout box for the tab card.
        when(mMockChildView.getWidth()).thenReturn(300);
        when(mMockChildView.getHeight()).thenReturn(100);

        doAnswer(
                        (InvocationOnMock invocation) -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 50;
                            pos[1] = 100;
                            return null;
                        })
                .when(mMockChildView)
                .getLocationInWindow(any());

        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        // Inject the mock context menu coordinator to intercept showMenu().
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        // Trigger context menu interaction.
        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 150f, /* localY= */ 250f);

        assertTrue("Context gesture interaction on an active tab row should return true.", handled);

        // Verify the AnchorInfo passed to the coordinator contains all the selected tab IDs.
        ArgumentCaptor<AnchorInfo> anchorInfoCaptor = ArgumentCaptor.forClass(AnchorInfo.class);
        verify(mTabContextMenuCoordinator)
                .showMenu(any(RectProvider.class), anchorInfoCaptor.capture());

        assertEquals(
                "Anchor info should contain all multi-selected tab IDs.",
                multiSelectedIds,
                anchorInfoCaptor.getValue().getAllTabIds());
        assertEquals(
                "Anchor info anchor ID should match the clicked tab ID.",
                TAB_ID_1,
                anchorInfoCaptor.getValue().getAnchorTabId());

        if (mCoordinator.getTabContextMenuCoordinatorForTesting() != null) {
            mCoordinator.getTabContextMenuCoordinatorForTesting().dismiss();
        }
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testShowTabItemContextMenu_ClosingTab_ReturnsFalse() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        when(mMockTab1.isClosing()).thenReturn(true);
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 150f, /* localY= */ 250f);

        assertFalse("Context interaction on a closing tab should return false.", handled);
        verify(mTabContextMenuCoordinator, never()).showMenu(any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testShowTabItemContextMenu_DeletedTab_ReturnsFalse() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(null);
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 150f, /* localY= */ 250f);

        assertFalse("Context interaction on a deleted tab should return false.", handled);
        verify(mTabContextMenuCoordinator, never()).showMenu(any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testShowTabItemContextMenu_InvalidTabId_ReturnsFalse() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) recyclerViewSpy.getAdapter();
        adapter.getModelList().get(0).model.set(TabProperties.TAB_ID, Tab.INVALID_TAB_ID);
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        when(recyclerViewSpy.findChildViewUnder(150f, 250f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 150f, /* localY= */ 250f);

        assertFalse("Context interaction on an invalid tab ID should return false.", handled);
        verify(mTabContextMenuCoordinator, never()).showMenu(any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testShowTabGroupHeaderContextMenu_NonExistentGroup_ReturnsFalse() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) recyclerViewSpy.getAdapter();
        PropertyModel groupPropertyModel = adapter.getModelList().get(0).model;
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);
        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(false);

        when(recyclerViewSpy.findChildViewUnder(200f, 150f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 200f, /* localY= */ 150f);

        assertFalse(
                "Context interaction on a non-existent tab group should return false.", handled);
        verify(mTabGroupContextMenuCoordinator, never()).showMenu(any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
    public void testShowTabGroupHeaderContextMenu_ClosingGroup_ReturnsFalse() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) recyclerViewSpy.getAdapter();
        PropertyModel groupPropertyModel = adapter.getModelList().get(0).model;
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, TAB_GROUP_ID);

        when(mTabModel.tabGroupExists(TAB_GROUP_ID)).thenReturn(true);
        when(mMockTab1.isClosing()).thenReturn(true);
        when(mTabModel.getTabsInGroup(TAB_GROUP_ID)).thenReturn(List.of(mMockTab1));

        when(recyclerViewSpy.findChildViewUnder(200f, 150f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 200f, /* localY= */ 150f);

        assertFalse("Context interaction on a closing tab group should return false.", handled);
        verify(mTabGroupContextMenuCoordinator, never()).showMenu(any(), any());
    }

    // =============================================================================================
    // Adapter, Selection & Tab Group Expansion Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testAdapterInterception() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();
        assertNotNull(recycler.getLayoutManager());

        PropertyModel reg = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        PropertyModel pin = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        PropertyModel group = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        pin.set(TabProperties.IS_PINNED, true);
        group.set(TabProperties.TAB_GROUP_HEADER_ID, new Token(1L, 2L));

        assertNotNull(adapter);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, reg));
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, pin));
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, group));

        assertEquals(UiType.TAB, adapter.getItemViewType(0));
        assertEquals(UiType.PINNED_TAB, adapter.getItemViewType(1));
        assertEquals(UiType.TAB_GROUP, adapter.getItemViewType(2));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Expand() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));

        final boolean[] collapsedState = {true};
        doAnswer(invocation -> collapsedState[0])
                .when(mTabModel)
                .getTabGroupCollapsed(any(Token.class));
        doAnswer(
                        invocation -> {
                            collapsedState[0] = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        invocation.getArgument(0), collapsedState[0], false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(any(Token.class), anyBoolean(), anyBoolean());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertTrue(groupModel.get(TabProperties.IS_COLLAPSED));
        assertNotNull(groupModel.get(TabProperties.TAB_ACTION_BUTTON_DATA));

        mCoordinator.toggleTabGroupExpansion(TAB_ID_1);
        assertFalse(
                "Tab group should be expanded (IS_COLLAPSED = false) after first toggle click.",
                groupModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testTabGroupHeaderActionButton_ClickShowsContextMenu() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));

        createCoordinator();
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertTrue(TabProperties.isTabGroupHeader(groupModel));
        TabActionButtonData actionButtonData = groupModel.get(TabProperties.TAB_ACTION_BUTTON_DATA);
        assertNotNull(actionButtonData);
        assertEquals(TabActionButtonType.OVERFLOW, actionButtonData.type);
        assertNotNull(actionButtonData.tabActionListener);

        UserActionTester userActionTester = new UserActionTester();
        View mockButtonView = mock(View.class);
        actionButtonData.tabActionListener.run(
                mockButtonView, TAB_ID_1, /* triggeringMotion= */ null);

        verify(mTabGroupContextMenuCoordinator).showMenu(any(RectProvider.class), eq(tabGroupId));
        assertTrue(
                userActionTester
                        .getActions()
                        .contains("Android.VerticalTabs.GroupHeaderMenuButtonClicked"));
    }

    @Test
    @SmallTest
    public void testTabGroupHeaderActionButton_SyncId_NoOp() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));

        createCoordinator();
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        PropertyModel groupModel = adapter.getModelList().get(0).model;
        TabActionButtonData actionButtonData = groupModel.get(TabProperties.TAB_ACTION_BUTTON_DATA);
        assertNotNull(actionButtonData);

        View mockButtonView = mock(View.class);
        actionButtonData.tabActionListener.run(
                mockButtonView, "sync_id_123", /* triggeringMotion= */ null);

        verify(mTabGroupContextMenuCoordinator, never()).showMenu(any(), any());
    }

    @Test
    @SmallTest
    public void testTabGroupHeaderActionButton_NullGroupId_ReturnsNull() {
        createCoordinator();
        Tab tab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(tab.getTabGroupId()).thenReturn(null);

        TabListMediator mediator = ReflectionHelpers.getField(mCoordinator, "mMediator");
        TabListItemOnClickListenerProvider clickHandler =
                ReflectionHelpers.getField(mediator, "mTabListItemOnClickListenerProvider");
        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();

        assertNull(
                clickHandler.getTabGroupActionButtonData(
                        tab, model, /* defaultOverflowListenerSupplier= */ SupplierUtils.ofNull()));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Collapse() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));

        final boolean[] collapsedState = {false};
        doAnswer(invocation -> collapsedState[0])
                .when(mTabModel)
                .getTabGroupCollapsed(any(Token.class));
        doAnswer(
                        invocation -> {
                            collapsedState[0] = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        invocation.getArgument(0), collapsedState[0], false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(any(Token.class), anyBoolean(), anyBoolean());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(2, adapter.getModelList().size());
        PropertyModel groupModel = adapter.getModelList().get(0).model;
        assertFalse(groupModel.get(TabProperties.IS_COLLAPSED));

        mCoordinator.toggleTabGroupExpansion(TAB_ID_1);
        assertTrue(
                "Tab group should be collapsed (IS_COLLAPSED = true) after toggle click from"
                        + " expanded state.",
                groupModel.get(TabProperties.IS_COLLAPSED));
        assertEquals(1, adapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_RegularTabCannotToggle() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab1);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab1));
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(false);

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        mCoordinator.toggleTabGroupExpansion(TAB_ID_1);
        assertFalse(tabModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testTabSelection_SelectsTabInSelector() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModelSelector.getModelForTabId(TAB_ID_1)).thenReturn(mTabModel);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab1);
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab1));
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(false);
        when(mTabModel.iterator()).thenReturn(List.of(tab1).iterator());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        TabActionListener clickListener = tabModel.get(TabProperties.TAB_CLICK_LISTENER);
        assertNotNull("Tab click listener should be bound to model", clickListener);
        clickListener.run(null, TAB_ID_1, null);

        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    @SmallTest
    public void testTabModelSwap_ResetsTabs() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        when(mNewTabModel.getProfile()).thenReturn(mProfile);
        when(mNewTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mNewTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));
        when(mNewTabModel.iterator()).thenReturn(List.of(newTab).iterator());
        when(mNewTabModel.getTabById(TAB_ID_1)).thenReturn(newTab);

        mCurrentTabModelSupplier.set(mNewTabModel);

        assertEquals(1, adapter.getModelList().size());
        assertEquals(TAB_ID_1, adapter.getModelList().get(0).model.get(TabProperties.TAB_ID));
    }

    // =============================================================================================
    // Header Controls & Window Layout Tests
    // =============================================================================================

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP)
    public void testTabSearchButtonClick_TabSearchForALEnabled() {
        createCoordinator();
        ImageButton tabSearchButton = mCoordinator.getView().findViewById(R.id.tab_search_button);
        assertNotNull(tabSearchButton);
        UserActionTester userActionTester = new UserActionTester();
        tabSearchButton.performClick();
        verify(mVerticalTabsActionDelegate).openTabSearch();
        assertTrue(
                userActionTester.getActions().contains("Android.VerticalTabs.SearchButtonClicked"));
        userActionTester.tearDown();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP)
    public void testTabSearchButtonClick_TabSearchForALDisabled() {
        createCoordinator();
        ImageButton tabSearchButton = mCoordinator.getView().findViewById(R.id.tab_search_button);
        assertNotNull(tabSearchButton);
        UserActionTester userActionTester = new UserActionTester();
        tabSearchButton.performClick();
        verify(mVerticalTabsActionDelegate).openHubSearch();
        assertTrue(
                userActionTester.getActions().contains("Android.VerticalTabs.SearchButtonClicked"));
        userActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testNewTabButtonClick() {
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        createCoordinator();
        ImageButton newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        UserActionTester userActionTester = new UserActionTester();
        newTabButton.performClick();
        verify(mTabModel).commitAllTabClosures();
        verify(mTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
        assertTrue(userActionTester.getActions().contains("MobileNewTabOpened.VerticalTabs"));
        userActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testNewTabButtonClick_Incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        createCoordinator();
        ImageButton newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        UserActionTester userActionTester = new UserActionTester();
        newTabButton.performClick();
        verify(mTabModel, never()).commitAllTabClosures();
        verify(mTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
        assertTrue(userActionTester.getActions().contains("MobileNewTabOpened.VerticalTabs"));
        userActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testIncognitoButtonClick() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.INCOGNITO_BUTTON_PARAM,
                true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);

        createCoordinator();
        ImageButton incognitoButton =
                mCoordinator.getView().findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        UserActionTester userActionTester = new UserActionTester();
        incognitoButton.performClick();
        verify(mTabModelSelector).selectModel(/* incognito= */ true);
        assertTrue(userActionTester.getActions().contains("MobileToolbarModelSelected"));
        userActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testIncognitoButtonVisibility_TabletUnder10Inches() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.INCOGNITO_BUTTON_PARAM,
                true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        IncognitoUtils.setEnabledForTesting(true);
        createCoordinator();
        ImageButton incognitoButton =
                mCoordinator.getView().findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        assertEquals(View.VISIBLE, incognitoButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testIncognitoButtonVisibility_TabletOver10Inches() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.INCOGNITO_BUTTON_PARAM,
                true);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        IncognitoUtils.setEnabledForTesting(true);
        createCoordinator();
        ImageButton incognitoButton =
                mCoordinator.getView().findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        assertEquals(View.GONE, incognitoButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testIncognitoButtonVisibility_ParamDisabled() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.INCOGNITO_BUTTON_PARAM,
                false);
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        IncognitoUtils.setEnabledForTesting(true);
        createCoordinator();
        ImageButton incognitoButton =
                mCoordinator.getView().findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        assertEquals(View.GONE, incognitoButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testIncognitoButtonVisibility_NoIncognitoTabs_Gone() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.INCOGNITO_BUTTON_PARAM,
                true);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        IncognitoUtils.setEnabledForTesting(true);
        createCoordinator();
        ImageButton incognitoButton =
                mCoordinator.getView().findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        assertEquals(View.GONE, incognitoButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testIncognitoButtonVisibility_UpdatesDynamically() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.INCOGNITO_BUTTON_PARAM,
                true);
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(false);
        IncognitoUtils.setEnabledForTesting(true);
        createCoordinator();
        ImageButton incognitoButton =
                mCoordinator.getView().findViewById(R.id.new_incognito_tab_button);
        assertNotNull(incognitoButton);
        assertEquals(View.GONE, incognitoButton.getVisibility());

        // When an incognito tab is added
        when(mIncognitoTabModel.getCount()).thenReturn(1);
        Tab incognitoTab = mMockTab1;
        for (TabModelObserver observer : mTabModelObservers) {
            observer.didAddTab(
                    incognitoTab,
                    TabLaunchType.FROM_CHROME_UI,
                    TabCreationState.LIVE_IN_FOREGROUND,
                    /* markedForSelection= */ true);
        }
        assertEquals(View.VISIBLE, incognitoButton.getVisibility());

        // When all tabs are closed
        when(mIncognitoTabModel.getCount()).thenReturn(0);
        for (TabModelObserver observer : mTabModelObservers) {
            observer.didRemoveTabForClosure(incognitoTab);
        }
        assertEquals(View.GONE, incognitoButton.getVisibility());
    }

    @Test
    @SmallTest
    public void testSpacerViewVisibilityInDesktopWindow() {
        // Mock DesktopWindowStateManager to say we are in desktop windowing mode.
        var appHeaderState =
                new AppHeaderState(
                        new Rect(0, 0, 100, 100),
                        new Rect(10, 0, 80, 100),
                        /* isInDesktopWindow= */ true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState);

        // Capture AppHeaderObserver.
        ArgumentCaptor<DesktopWindowStateManager.AppHeaderObserver> observerCaptor =
                ArgumentCaptor.forClass(DesktopWindowStateManager.AppHeaderObserver.class);

        createCoordinator();

        verify(mDesktopWindowStateManager).addObserver(observerCaptor.capture());
        var observer = observerCaptor.getValue();
        assertNotNull(observer);

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        View spacer = view.findViewById(R.id.desktop_window_spacer);
        assertNotNull("Spacer view should exist.", spacer);
        assertEquals("Spacer view should be visible.", View.VISIBLE, spacer.getVisibility());

        // Exit desktop window.
        var appHeaderState2 =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), false);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(appHeaderState2);
        observer.onAppHeaderStateChanged(appHeaderState2);

        assertEquals("Spacer view should be hidden.", View.GONE, spacer.getVisibility());
    }

    @Test
    @SmallTest
    public void testLayoutChange_UpdatesVerticalTabsWidthSupplier() {
        SettableNonNullObservableSupplier<Integer> widthSupplier =
                ObservableSuppliers.createNonNull(0);

        // Pass our custom widthSupplier to the constructor.
        mCoordinator =
                new VerticalTabListCoordinator(
                        mActivity,
                        mTabModelSelector,
                        mProfile,
                        mVerticalTabsActionDelegate,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mMultiInstanceManager,
                        mSnackbarManager,
                        mDesktopWindowStateManager,
                        mShareDelegateSupplier,
                        mDataSharingTabManager,
                        mIsVerticalTabsActiveSupplier,
                        widthSupplier,
                        /* canActivateTabLayoutToggleMenuSupplier= */ null,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabContentManagerSupplier,
                        mUndoBarThrottle,
                        mBrowserControlsStateProvider);

        View containerView = mCoordinator.getView();
        containerView.setVisibility(View.VISIBLE);

        // Simulate a layout pass where the width is 200px.
        containerView.layout(0, 0, 200, 500);
        assertEquals(
                "Width supplier should update to 200 when container is visible.",
                200,
                (int) widthSupplier.get());

        // Simulate container resizing to 300px wide. This is needed to test visibility GONE because
        // changing bounds triggers the layout change listener.
        containerView.layout(0, 0, 300, 500);
        assertEquals(
                "Width supplier should update to 300 when container resizes.",
                300,
                (int) widthSupplier.get());

        // Simulate layout change when container is hidden (visibility GONE).
        containerView.setVisibility(View.GONE);
        containerView.layout(0, 0, 0, 0);
        assertEquals(
                "Width supplier should update to 0 when container is hidden.",
                0,
                (int) widthSupplier.get());
    }

    // =============================================================================================
    // Scroll & Active Tab Visibility Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testScrollActiveTabIntoView_PinnedTab_NoScroll() {
        Tab pinnedTab = prepareMockTab(mMockTab1, PINNED_TAB_ID);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getTabById(PINNED_TAB_ID)).thenReturn(pinnedTab);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(PINNED_TAB_ID);

        mIsVerticalTabsActiveSupplier.set(false);
        createCoordinator();
        mActivity.setContentView(mCoordinator.getView());

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        recyclerView.setLayoutManager(null);
        assert layoutManager != null;
        LinearLayoutManager spyLayoutManager = spy(layoutManager);
        recyclerView.setLayoutManager(spyLayoutManager);

        mIsVerticalTabsActiveSupplier.set(true);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(spyLayoutManager, never()).scrollToPositionWithOffset(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testScrollActiveTabIntoView_RegularTab_Scrolls() {
        Tab regTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(regTab.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(regTab);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        mIsVerticalTabsActiveSupplier.set(false);
        createCoordinator();
        mActivity.setContentView(mCoordinator.getView());

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        assertNotNull(adapter);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(CardProperties.CARD_TYPE, CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, model));

        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        recyclerView.setLayoutManager(null);
        assert layoutManager != null;
        LinearLayoutManager spyLayoutManager = spy(layoutManager);
        recyclerView.setLayoutManager(spyLayoutManager);

        mIsVerticalTabsActiveSupplier.set(true);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(spyLayoutManager).scrollToPositionWithOffset(eq(0), anyInt());
    }

    @Test
    @SmallTest
    public void testScrollActiveTabIntoView_UnpinActiveTab_Scrolls() {
        Tab unpinnedTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(unpinnedTab.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(unpinnedTab);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_1);

        mIsVerticalTabsActiveSupplier.set(true);
        createCoordinator();
        mActivity.setContentView(mCoordinator.getView());

        View view = mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        assertNotNull(adapter);

        PropertyModel model =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(CardProperties.CARD_TYPE, CardProperties.ModelType.TAB)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .build();
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, model));

        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        recyclerView.setLayoutManager(null);
        assertNotNull(layoutManager);
        LinearLayoutManager spyLayoutManager = spy(layoutManager);
        recyclerView.setLayoutManager(spyLayoutManager);

        assertFalse(mTabModelObservers.isEmpty());
        for (TabModelObserver observer : mTabModelObservers) {
            observer.didChangePinState(unpinnedTab);
        }

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(spyLayoutManager).scrollToPositionWithOffset(eq(0), anyInt());
    }

    @Test
    @SmallTest
    public void testGroupExpansion_ChildrenOffScreen_Scrolls() {
        Token groupId = new Token(1L, 2L);
        Tab headerTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(headerTab.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(headerTab);

        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        Tab tab3 = prepareMockTab(mMockTab3, TAB_ID_3);
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(headerTab, tab2, tab3));
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(headerTab, tab2, tab3));
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(3);

        doAnswer(
                        invocation -> {
                            Token token = invocation.getArgument(0);
                            boolean collapsed = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        token, collapsed, /* animate= */ false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(eq(groupId), anyBoolean(), anyBoolean());
        when(mTabModel.getTabGroupCollapsed(groupId)).thenReturn(true);

        mIsVerticalTabsActiveSupplier.set(true);
        createCoordinator();
        mActivity.setContentView(mCoordinator.getView());

        View view = mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        assertNotNull(adapter);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(CardProperties.CARD_TYPE, CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .build();
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, headerModel));

        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        recyclerView.setLayoutManager(null);
        assertNotNull(layoutManager);
        LinearLayoutManager spyLayoutManager = spy(layoutManager);
        recyclerView.setLayoutManager(spyLayoutManager);

        when(spyLayoutManager.findLastCompletelyVisibleItemPosition()).thenReturn(2);

        TabListMediator mediator = ReflectionHelpers.getField(mCoordinator, "mMediator");
        TabListItemOnClickListenerProvider clickHandler =
                ReflectionHelpers.getField(mediator, "mTabListItemOnClickListenerProvider");
        TabActionListener listener = clickHandler.onTabGroupClicked(headerTab);
        assertNotNull(listener);
        listener.run(view, TAB_ID_1, /* triggeringMotion= */ null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(spyLayoutManager).scrollToPositionWithOffset(eq(0), eq(0));
    }

    @Test
    @SmallTest
    public void testGroupExpansion_ChildrenVisible_NoScroll() {
        Token groupId = new Token(1L, 2L);
        Tab headerTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(headerTab.getTabGroupId()).thenReturn(groupId);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(headerTab);

        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        Tab tab3 = prepareMockTab(mMockTab3, TAB_ID_3);
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(headerTab, tab2, tab3));
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(List.of(headerTab, tab2, tab3));
        when(mTabModel.getTabCountForGroup(groupId)).thenReturn(3);

        doAnswer(
                        invocation -> {
                            Token token = invocation.getArgument(0);
                            boolean collapsed = invocation.getArgument(1);
                            for (TabGroupObserver observer : mTabGroupObservers) {
                                observer.didChangeTabGroupCollapsed(
                                        token, collapsed, /* animate= */ false);
                            }
                            return null;
                        })
                .when(mTabModel)
                .setTabGroupCollapsed(eq(groupId), anyBoolean(), anyBoolean());
        when(mTabModel.getTabGroupCollapsed(groupId)).thenReturn(true);

        mIsVerticalTabsActiveSupplier.set(true);
        createCoordinator();
        mActivity.setContentView(mCoordinator.getView());

        View view = mCoordinator.getView();
        TabListRecyclerView recyclerView = view.findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        assertNotNull(adapter);

        PropertyModel headerModel =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(CardProperties.CARD_TYPE, CardProperties.ModelType.TAB_GROUP)
                        .with(TabProperties.TAB_ID, TAB_ID_1)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, groupId)
                        .with(TabProperties.IS_COLLAPSED, true)
                        .build();
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, headerModel));

        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        recyclerView.setLayoutManager(null);
        assertNotNull(layoutManager);
        LinearLayoutManager spyLayoutManager = spy(layoutManager);
        recyclerView.setLayoutManager(spyLayoutManager);

        when(spyLayoutManager.findLastCompletelyVisibleItemPosition()).thenReturn(4);

        TabListMediator mediator = ReflectionHelpers.getField(mCoordinator, "mMediator");
        TabListItemOnClickListenerProvider clickHandler =
                ReflectionHelpers.getField(mediator, "mTabListItemOnClickListenerProvider");
        TabActionListener listener = clickHandler.onTabGroupClicked(headerTab);
        assertNotNull(listener);
        listener.run(view, TAB_ID_1, /* triggeringMotion= */ null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(spyLayoutManager, never()).scrollToPositionWithOffset(anyInt(), anyInt());
    }

    // =============================================================================================
    // Side-Rail Collapse & Hover Expand & Hover Card Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testCollapseListenerAndModelToggle() {
        createCoordinator();

        ViewGroup view = (ViewGroup) mCoordinator.getView();
        View collapseButton = view.findViewById(R.id.collapse_button);
        assertNotNull(collapseButton);
        assertEquals(View.VISIBLE, collapseButton.getVisibility());

        // Initially expanded.
        assertEquals(
                RailCollapseState.EXPANDED,
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.COLLAPSE_STATE));
        assertEquals(
                VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT,
                mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Android.VerticalTabs.RailCollapsed", true);

        // Click collapse.
        collapseButton.performClick();
        histogramWatcher.assertExpected();

        // Verify listener requested collapse, but model is NOT updated yet (deferred).
        verify(mMockRailCollapseListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.EXPANDED, RailCollapseState.COLLAPSED);
        assertEquals(
                RailCollapseState.EXPANDED,
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.COLLAPSE_STATE));

        // Apply the collapsed state manually (simulating the SideUiCoordinator transition flow).
        mCoordinator.setRailCollapseState(RailCollapseState.COLLAPSED);

        // Verify model is now updated.
        assertEquals(
                RailCollapseState.COLLAPSED,
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.COLLAPSE_STATE));
        assertEquals(
                VerticalTabListCoordinator.COLLAPSED_GRID_SPAN_COUNT,
                mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.RailCollapsed", false);

        // Click again to expand.
        collapseButton.performClick();
        histogramWatcher.assertExpected();

        // Verify listener requested expand, but model is still collapsed.
        verify(mMockRailCollapseListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED);
        assertEquals(
                RailCollapseState.COLLAPSED,
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.COLLAPSE_STATE));

        // Apply the expanded state manually.
        mCoordinator.setRailCollapseState(RailCollapseState.EXPANDED);

        // Verify model is now expanded.
        assertEquals(
                RailCollapseState.EXPANDED,
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.COLLAPSE_STATE));
        assertEquals(
                VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT,
                mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());
    }

    @Test
    @SmallTest
    public void testSetCollapseButtonEnabled() {
        createCoordinator();

        View collapseButton = mCoordinator.getView().findViewById(R.id.collapse_button);
        assertTrue(
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED));

        mCoordinator.setCollapseButtonEnabled(false);
        assertFalse(
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED));
        assertFalse(collapseButton.isEnabled());
        assertTrue(collapseButton.getAlpha() < 1.0f);

        // Attempting click when disabled should be ignored.
        collapseButton.performClick();
        verify(mMockRailCollapseListener, never())
                .onRailCollapseStateChangeRequestedByUser(anyInt(), anyInt());

        mCoordinator.setCollapseButtonEnabled(true);
        assertTrue(
                mCoordinator
                        .getContainerModelForTesting()
                        .get(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED));
        assertTrue(collapseButton.isEnabled());
        assertEquals(1.0f, collapseButton.getAlpha(), 0.01f);
    }

    @Test
    @SmallTest
    public void testExpandOrCollapseOnHover_DispatchesHoverEventsToController() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        createCoordinator();
        mCoordinator
                .getCollapseController()
                .setRailCollapseStateByUser(RailCollapseState.COLLAPSED);
        mCoordinator.setRailCollapseState(RailCollapseState.COLLAPSED);

        View containerView = mCoordinator.getView();
        containerView.layout(0, 0, 200, 500);

        // 1. Mouse hover inside -> requests EXPANDED_FOR_HOVERING.
        MotionEvent hoverEnter =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 50f, 50f, 0);
        hoverEnter.setSource(InputDevice.SOURCE_MOUSE);
        containerView.dispatchGenericMotionEvent(hoverEnter);
        verify(mMockRailCollapseListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED_FOR_HOVERING);

        // 2. Mouse hover exit (outside container bounds) -> requests COLLAPSED.
        mCoordinator.setRailCollapseState(RailCollapseState.EXPANDED_FOR_HOVERING);
        MotionEvent hoverExit =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 500f, 50f, 0);
        hoverExit.setSource(InputDevice.SOURCE_MOUSE);
        containerView.dispatchGenericMotionEvent(hoverExit);
        verify(mMockRailCollapseListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.EXPANDED_FOR_HOVERING, RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testHoverCard_TabClosed_HidesHoverCard() {
        Tab tab = prepareAndShowHoverCard(mMockTab1);

        // Notify tab model that tab will close
        for (TabModelObserver observer : mTabModelObservers) {
            observer.willCloseTab(tab, /* didCloseAlone= */ false);
        }
        verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_CLOSURE_METHOD_REFACTOR)
    public void testHoverCard_TabClosed_HidesHoverCard_WillCloseTabs() {
        Tab tab = prepareAndShowHoverCard(mMockTab1);

        // Notify tab model that tab will close
        for (TabModelObserver observer : mTabModelObservers) {
            observer.willCloseTabs(
                    List.of(tab), /* isAllTabs= */ false, /* allowUndo= */ false);
        }
        verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testHoverCard_Deactivate_HidesHoverCard() {
        mIsVerticalTabsActiveSupplier.set(true);
        prepareAndShowHoverCard(mMockTab1);

        // Deactivating vertical tabs should hide the active hover card.
        mIsVerticalTabsActiveSupplier.set(false);
        verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testHoverCard_Scroll_HidesHoverCard() {
        prepareAndShowHoverCard(mMockTab1);

        // Dragging the recycler view during scroll should hide the active hover card.
        TabListRecyclerView mainRecyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        assertNotNull(mainRecyclerView);

        RecyclerView.OnScrollListener scrollListener = mCoordinator.getOnScrollListenerForTesting();
        assertNotNull(scrollListener);
        scrollListener.onScrollStateChanged(mainRecyclerView, RecyclerView.SCROLL_STATE_DRAGGING);
        verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testHoverCard_ContextMenu_HidesHoverCard() {
        Tab tab = prepareAndShowHoverCard(mMockTab1);

        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        assertNotNull(recyclerView);

        // Showing context menu should hide hover card view.
        mCoordinator.handleContextMenuInteractionForTesting(
                mActivity, recyclerView, /* localX= */ 100f, /* localY= */ 100f);

        verify(mTabHoverCardView).hide();

        // While context menu is showing, attempting to hover a tab should not show a hover card.
        when(mTabContextMenuCoordinator.isMenuShowing()).thenReturn(true);
        TabHoverCardListener hoverListener = mCoordinator.getTabHoverCardListenerForTesting();
        assertNotNull(hoverListener);
        hoverListener.onTabHoverCardStateChanged(
                tab.getId(), mMockChildView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testHoverCard_DragStart_HidesHoverCard() {
        Tab tab = prepareAndShowHoverCard(mMockTab1);
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, tab.getId());

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> captor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce()).setDragHandlerDelegate(captor.capture());

        captor.getValue().handleDragStart(10f, 10f);

        verify(mTabHoverCardView).hide();
    }

    // =============================================================================================
    // Dynamically Balancing Pinned Tabs
    // =============================================================================================

    @Test
    @SmallTest
    public void testDynamicSpanCountOnWidthChange() {
        createCoordinator();
        int defaultSpanCount = mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount();
        assertEquals(VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT, defaultSpanCount);

        // Simulate measuring container with a width that fits exactly 2 columns.
        View containerView = mCoordinator.getView();
        int testWidthPx =
                mMinPinnedTabWidth * 2
                        + mMinPinnedTabGap
                        + containerView.getPaddingStart()
                        + containerView.getPaddingEnd();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(testWidthPx, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, testWidthPx, 1000);

        assertEquals(2, mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        // Verify collapse reduces span count to 1 even when dynamically measured.
        mCoordinator.setRailCollapseState(RailCollapseState.COLLAPSED);
        assertEquals(
                VerticalTabListCoordinator.COLLAPSED_GRID_SPAN_COUNT,
                mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        // Verify expand restores dynamically calculated span count (2).
        mCoordinator.setRailCollapseState(RailCollapseState.EXPANDED);
        assertEquals(2, mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        // Verify narrow width (e.g. 90dp equivalent) allows 1 column when width only fits 1.
        int narrowWidthPx =
                mMinPinnedTabWidth
                        + containerView.getPaddingStart()
                        + containerView.getPaddingEnd();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(narrowWidthPx, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, narrowWidthPx, 1000);
        assertEquals(1, mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());
    }

    @Test
    @SmallTest
    public void testDynamicSpanCountIgnoredWhenHidden() {
        createCoordinator();
        int defaultSpanCount = mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount();
        assertEquals(VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT, defaultSpanCount);

        View containerView = mCoordinator.getView();
        containerView.setVisibility(View.GONE);

        // Simulate layout change while hidden with a width that would fit 2 columns.
        int testWidthPx =
                mMinPinnedTabWidth * 2
                        + mMinPinnedTabGap
                        + containerView.getPaddingStart()
                        + containerView.getPaddingEnd();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(testWidthPx, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, testWidthPx, 1000);

        // Should remain at default span count because the container is hidden.
        assertEquals(
                VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT,
                mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());
    }

    @Test
    @SmallTest
    public void testDynamicSpanCount_GridOptimizationWithPinnedTabs() {
        createCoordinator();

        View containerView = mCoordinator.getView();

        // Simulate width that fits exactly 5 columns: 5 * itemWidth + 4 * itemMargin + container
        // paddings.
        int widthFor5Cols =
                mMinPinnedTabWidth * 5
                        + mMinPinnedTabGap * 4
                        + containerView.getPaddingStart()
                        + containerView.getPaddingEnd();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(widthFor5Cols, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, widthFor5Cols, 1000);

        TabListModel pinnedModelList = mCoordinator.getPinnedTabsModelListForTesting();

        // 0 pinned tabs -> returns maxFitSpans (5)
        assertEquals(5, mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        // Expected span counts for N pinned tabs from 1 to 11 when C_max = 5:
        // N = 1: R = 1, C = 1
        // N = 2: R = 1, C = 2
        // N = 3: R = 1, C = 3
        // N = 4: R = 1, C = 4
        // N = 5: R = 1, C = 5
        // N = 6: R = 2, C = 3 (3 + 3)
        // N = 7: R = 2, C = 4 (4 + 3)
        // N = 8: R = 2, C = 4 (4 + 4)
        // N = 9: R = 2, C = 5 (5 + 4)
        // N = 10: R = 2, C = 5 (5 + 5)
        // N = 11: R = 3, C = 4 (4 + 4 + 3)
        int[] expectedSpans = {1, 2, 3, 4, 5, 3, 4, 4, 5, 5, 4};

        for (int i = 0; i < expectedSpans.length; i++) {
            pinnedModelList.add(
                    new MVCListAdapter.ListItem(UiType.PINNED_TAB, createTabPropertyModel()));
            assertEquals(
                    "Span count mismatch for " + (i + 1) + " pinned tab(s)",
                    expectedSpans[i],
                    mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());
        }

        // Test removing tabs back down to 1 tab
        for (int i = expectedSpans.length - 1; i >= 1; i--) {
            pinnedModelList.removeAt(i);
            assertEquals(
                    "Span count mismatch after removal to " + i + " pinned tab(s)",
                    expectedSpans[i - 1],
                    mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());
        }
    }

    @Test
    @SmallTest
    public void testDynamicSpanCount_WideContainer_CappedAtMaxSpan() {
        createCoordinator();

        View containerView = mCoordinator.getView();

        // Container width that can fit 7 columns physically.
        int widthFor7Cols =
                mMinPinnedTabWidth * 7
                        + mMinPinnedTabGap * 6
                        + containerView.getPaddingStart()
                        + containerView.getPaddingEnd();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(widthFor7Cols, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, widthFor7Cols, 1000);

        TabListModel pinnedModelList = mCoordinator.getPinnedTabsModelListForTesting();

        // 0 pinned tabs -> capped at MAX_SINGLE_ROW_SPAN_COUNT = 5
        assertEquals(5, mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        // Add up to 10 pinned tabs and verify span count never exceeds 5.
        for (int i = 1; i <= 10; i++) {
            pinnedModelList.add(
                    new MVCListAdapter.ListItem(UiType.PINNED_TAB, createTabPropertyModel()));
            int spanCount = mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount();
            assertTrue(
                    "Span count (" + spanCount + ") for " + i + " tabs should be <= 5",
                    spanCount <= 5);
        }
    }

    @Test
    @SmallTest
    public void testDynamicSpanCount_NarrowContainer_TwoColumnCap() {
        createCoordinator();

        View containerView = mCoordinator.getView();

        // Container width that fits exactly 2 columns (e.g. 92dp).
        int widthFor2Cols =
                mMinPinnedTabWidth * 2
                        + mMinPinnedTabGap
                        + containerView.getPaddingStart()
                        + containerView.getPaddingEnd();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(widthFor2Cols, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, widthFor2Cols, 1000);

        TabListModel pinnedModelList = mCoordinator.getPinnedTabsModelListForTesting();

        // 0 pinned tabs -> returns maxFitSpans = 2
        assertEquals(2, mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());

        // N=1 -> 1, N=2 -> 2, N=3 -> 2, N=4 -> 2, N=5 -> 2
        int[] expectedSpans = {1, 2, 2, 2, 2};
        for (int i = 0; i < expectedSpans.length; i++) {
            pinnedModelList.add(
                    new MVCListAdapter.ListItem(UiType.PINNED_TAB, createTabPropertyModel()));
            assertEquals(
                    "Span count mismatch for " + (i + 1) + " pinned tab(s)",
                    expectedSpans[i],
                    mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount());
        }
    }

    @Test
    @SmallTest
    public void testPinnedTabsItemDecoration_OffsetsAcrossColumnsAndRows() {
        createCoordinator();
        RecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        RecyclerView.ItemDecoration decoration = pinnedRecyclerView.getItemDecorationAt(0);
        assertNotNull(decoration);

        Rect outRect = new Rect();
        View child0 = new View(mActivity);
        GridLayoutManager.LayoutParams lp0 =
                new GridLayoutManager.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        child0.setLayoutParams(lp0);
        pinnedRecyclerView.addView(child0);

        decoration.getItemOffsets(outRect, child0, pinnedRecyclerView, new RecyclerView.State());
        assertEquals(0, outRect.left);
        assertEquals(mMinPinnedTabGap - mMinPinnedTabGap / 4, outRect.right);
    }

    @Test
    @SmallTest
    public void testPinnedTabsItemDecoration_OffsetsAcrossColumnsAndRows_Rtl() {
        LocalizationUtils.setRtlForTesting(true);
        createCoordinator();
        RecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        RecyclerView.ItemDecoration decoration = pinnedRecyclerView.getItemDecorationAt(0);
        assertNotNull(decoration);

        Rect outRect = new Rect();
        View child0 = new View(mActivity);
        GridLayoutManager.LayoutParams lp0 =
                new GridLayoutManager.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        child0.setLayoutParams(lp0);
        pinnedRecyclerView.addView(child0);

        decoration.getItemOffsets(outRect, child0, pinnedRecyclerView, new RecyclerView.State());
        assertEquals(mMinPinnedTabGap - mMinPinnedTabGap / 4, outRect.left);
        assertEquals(0, outRect.right);
    }

    @Test
    @SmallTest
    public void testPinnedTabsItemDecoration_OffsetsCorrectAcrossColumnsAndAfterMove() {
        createCoordinator();
        RecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        RecyclerView.ItemDecoration decoration = pinnedRecyclerView.getItemDecorationAt(0);
        assertNotNull(decoration);

        // Add 4 children representing 4 columns (spanCount = 4).
        View child0 = new View(mActivity);
        View child1 = new View(mActivity);
        View child2 = new View(mActivity);
        View child3 = new View(mActivity);

        // Give child1 a stale LayoutParams with spanIndex = 0 (as if it was moved from position 0).
        GridLayoutManager.LayoutParams lp1 =
                new GridLayoutManager.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        ReflectionHelpers.setField(lp1, "mSpanIndex", 0);
        child1.setLayoutParams(lp1);

        pinnedRecyclerView.addView(child0);
        pinnedRecyclerView.addView(child1);
        pinnedRecyclerView.addView(child2);
        pinnedRecyclerView.addView(child3);

        Rect outRect0 = new Rect();
        Rect outRect1 = new Rect();
        Rect outRect2 = new Rect();
        Rect outRect3 = new Rect();

        decoration.getItemOffsets(outRect0, child0, pinnedRecyclerView, new RecyclerView.State());
        decoration.getItemOffsets(outRect1, child1, pinnedRecyclerView, new RecyclerView.State());
        decoration.getItemOffsets(outRect2, child2, pinnedRecyclerView, new RecyclerView.State());
        decoration.getItemOffsets(outRect3, child3, pinnedRecyclerView, new RecyclerView.State());

        // Column 0: 0px left, 3/4 gap right
        assertEquals(0, outRect0.left);
        assertEquals(mMinPinnedTabGap - mMinPinnedTabGap / 4, outRect0.right);

        // Column 1 (despite stale spanIndex=0): 1/4 gap left, 2/4 gap right
        assertEquals(mMinPinnedTabGap / 4, outRect1.left);
        assertEquals(mMinPinnedTabGap - 2 * mMinPinnedTabGap / 4, outRect1.right);

        // Inter-item gap between child 0 and child 1 equals mMinPinnedTabGap.
        assertEquals(mMinPinnedTabGap, outRect0.right + outRect1.left);

        // Column 2: 2/4 gap left, 1/4 gap right
        assertEquals(2 * mMinPinnedTabGap / 4, outRect2.left);
        assertEquals(mMinPinnedTabGap - 3 * mMinPinnedTabGap / 4, outRect2.right);
        assertEquals(mMinPinnedTabGap, outRect1.right + outRect2.left);

        // Column 3: 3/4 gap left, 0px right
        assertEquals(3 * mMinPinnedTabGap / 4, outRect3.left);
        assertEquals(0, outRect3.right);
        assertEquals(mMinPinnedTabGap, outRect2.right + outRect3.left);
    }

    @Test
    @SmallTest
    public void testPinnedTabs_ItemMoved_InvalidatesItemDecorations() {
        createCoordinator();
        TabListRecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        TabListRecyclerView spyRecyclerView = spy(pinnedRecyclerView);
        ReflectionHelpers.setField(mCoordinator, "mPinnedTabsRecyclerView", spyRecyclerView);

        TabListModel pinnedTabsModelList = mCoordinator.getPinnedTabsModelListForTesting();
        PropertyModel model0 = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        PropertyModel model1 = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID).build();
        pinnedTabsModelList.add(new MVCListAdapter.ListItem(UiType.PINNED_TAB, model0));
        pinnedTabsModelList.add(new MVCListAdapter.ListItem(UiType.PINNED_TAB, model1));

        clearInvocations(spyRecyclerView);
        pinnedTabsModelList.moveItem(0, 1);
        verify(spyRecyclerView).invalidateItemDecorations();
    }

    // =============================================================================================
    // Drag-and-Drop & Drag-Out Tests
    // =============================================================================================

    @Test
    @SmallTest
    public void testDragListenerRegisteredForBothRecyclerViews() {
        createCoordinator();
        View container = mCoordinator.getView();
        TabListRecyclerView mainRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        TabListRecyclerView pinnedRecyclerView =
                container.findViewById(R.id.pinned_tabs_recycler_view);

        assertNotNull("Main RecyclerView must not be null.", mainRecyclerView);
        assertNotNull("Pinned RecyclerView must not be null.", pinnedRecyclerView);

        Object mainListenerInfo = ReflectionHelpers.getField(mainRecyclerView, "mListenerInfo");
        Object pinnedListenerInfo = ReflectionHelpers.getField(pinnedRecyclerView, "mListenerInfo");

        assertNotNull("Main ListenerInfo must not be null.", mainListenerInfo);
        assertNotNull("Pinned ListenerInfo must not be null.", pinnedListenerInfo);

        View.OnDragListener mainDragListener =
                ReflectionHelpers.getField(mainListenerInfo, "mOnDragListener");
        View.OnDragListener pinnedDragListener =
                ReflectionHelpers.getField(pinnedListenerInfo, "mOnDragListener");

        assertNotNull("Main RecyclerView must have OnDragListener registered.", mainDragListener);
        assertNotNull(
                "Pinned RecyclerView must have OnDragListener registered.", pinnedDragListener);
        assertNotSame(
                "Each RecyclerView must have a separate TabSwitcherDragHandler instance.",
                mainDragListener,
                pinnedDragListener);
    }

    @Test
    @SmallTest
    public void testSingleTabDragOut_InvalidOrNullTab() {
        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, Tab.INVALID_TAB_ID);

        // Invalid tab ID should return early.
        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler, never()).startTabDragAction(any(), any(), any(), any());

        // Valid tab ID but null tab returned from tabModel.
        model.set(TabProperties.TAB_ID, NON_EXISTENT_TAB_ID);
        when(mTabModel.getTabById(NON_EXISTENT_TAB_ID)).thenReturn(null);
        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler, never()).startTabDragAction(any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testSingleTabDragOut_LastTabInGroupBlocked() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(true);
        when(mTabModel.getRelatedTabList(TAB_ID_1)).thenReturn(List.of(tab1));

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);

        // Dragging out the last tab in a group (relatedTabList size == 1) is blocked.
        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler, never()).startTabDragAction(any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testSingleTabDragOut_Success() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(false);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler).startTabDragAction(any(), eq(tab1), any(), any());
    }

    @Test
    @SmallTest
    public void testGroupHeaderDragOut_AllTabsInWindow() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));
        when(mTabModel.getCount()).thenReturn(1);

        createCoordinator();
        Token groupId = new Token(1L, 2L);
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        // When group contains all tabs in window (1 == 1), drag out is delegated to
        // TabSwitcherDragHandler to support multi-window drag.
        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler).startGroupDragAction(any(), eq(groupId), any(), any());
    }

    @Test
    @SmallTest
    public void testGroupHeaderDragOut_Success() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));
        when(mTabModel.getCount()).thenReturn(2);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler)
                .startGroupDragAction(any(), eq(tabGroupId), any(), any());
    }

    @Test
    @SmallTest
    public void testPinnedTabDragOut_MovesToEndOfPinnedTabs() {
        prepareMockPinnedTab(mMockTab1, TAB_ID_1, 0);
        prepareMockPinnedTab(mMockTab2, TAB_ID_2, 1);
        prepareMockPinnedTab(mMockTab3, TAB_ID_3, 2);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getPinnedTabsCount()).thenReturn(3);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, true);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());

        delegateCaptor.getValue().handleDragExit();
        verify(mTabModel).moveTab(TAB_ID_1, 2);
    }

    @Test
    @SmallTest
    public void testPinnedTabDragOut_LastPinnedTab_DoesNotMove() {
        prepareMockPinnedTab(mMockTab1, TAB_ID_1, 0);
        prepareMockPinnedTab(mMockTab2, TAB_ID_2, 1);
        prepareMockPinnedTab(mMockTab3, TAB_ID_3, 2);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.getPinnedTabsCount()).thenReturn(3);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_3);
        model.set(TabProperties.IS_PINNED, true);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());

        delegateCaptor.getValue().handleDragExit();
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testPinnedTabDragOut_SinglePinnedTab_DoesNotMove() {
        prepareMockPinnedTab(mMockTab1, TAB_ID_1, 0);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(1);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, true);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());

        delegateCaptor.getValue().handleDragExit();
        verify(mTabModel, never()).moveTab(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testSinglePinnedTabDrag_SetsMinHeight() {
        prepareMockPinnedTab(mMockTab1, TAB_ID_1, 0);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(1);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, true);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        View container = mCoordinator.getView();
        TabListRecyclerView pinnedRecyclerView =
                container.findViewById(R.id.pinned_tabs_recycler_view);
        int expectedMinHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_favicon_height);

        delegate.handleDragStart(0f, 0f);
        assertEquals(expectedMinHeight, pinnedRecyclerView.getMinimumHeight());

        delegate.handleDragEnter();
        assertEquals(0, pinnedRecyclerView.getMinimumHeight());

        delegate.handleDragExit();
        assertEquals(expectedMinHeight, pinnedRecyclerView.getMinimumHeight());

        delegate.handleExternalDragEnd(0f, 0f, /* isOSNewWindowDrop= */ true);
        assertEquals(0, pinnedRecyclerView.getMinimumHeight());

        delegate.handleDragStart(0f, 0f);
        assertEquals(expectedMinHeight, pinnedRecyclerView.getMinimumHeight());
        delegate.handleInternalDragEnd();
        assertEquals(0, pinnedRecyclerView.getMinimumHeight());
    }

    @Test
    @SmallTest
    public void testSingleRegularTabDrag_SetsMinHeight() {
        prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mMockTab1);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        View container = mCoordinator.getView();
        TabListRecyclerView mainRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        int expectedMinHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_favicon_height);

        delegate.handleDragStart(0f, 0f);
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        delegate.handleDragEnter();
        assertEquals(0, mainRecyclerView.getMinimumHeight());

        delegate.handleDragExit();
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        delegate.handleExternalDragEnd(0f, 0f, /* isOSNewWindowDrop= */ true);
        assertEquals(0, mainRecyclerView.getMinimumHeight());

        delegate.handleDragStart(0f, 0f);
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());
        delegate.handleInternalDragEnd();
        assertEquals(0, mainRecyclerView.getMinimumHeight());
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_DragEnterExitOnNonListViews_DoesNotToggleShadowOrMinHeight() {
        prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(mMockTab1);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        View container = mCoordinator.getView();
        TabListRecyclerView mainRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        View newTabButton = container.findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        int expectedMinHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.pinned_tab_strip_item_favicon_height);

        delegate.handleDragStart(0f, 0f);
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        // Exiting the main RecyclerView should show shadow and maintain list min height.
        clearInvocations(mMainTabSwitcherDragHandler);
        delegate.handleDragExit(mainRecyclerView);
        verify(mMainTabSwitcherDragHandler).showDragShadow(eq(mainRecyclerView), eq(true));
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        // Entering newTabButton or container (non-list views) should NOT hide shadow or clear min
        // height.
        clearInvocations(mMainTabSwitcherDragHandler);
        delegate.handleDragEnter(newTabButton);
        verify(mMainTabSwitcherDragHandler, never()).showDragShadow(any(), anyBoolean());
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        // Exiting newTabButton should NOT re-trigger shadow or collapse logic.
        clearInvocations(mMainTabSwitcherDragHandler);
        delegate.handleDragExit(newTabButton);
        verify(mMainTabSwitcherDragHandler, never()).showDragShadow(any(), anyBoolean());
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        // Entering container should NOT hide shadow.
        clearInvocations(mMainTabSwitcherDragHandler);
        delegate.handleDragEnter(container);
        verify(mMainTabSwitcherDragHandler, never()).showDragShadow(any(), anyBoolean());
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        // Exiting container should NOT re-trigger shadow.
        clearInvocations(mMainTabSwitcherDragHandler);
        delegate.handleDragExit(container);
        verify(mMainTabSwitcherDragHandler, never()).showDragShadow(any(), anyBoolean());
        assertEquals(expectedMinHeight, mainRecyclerView.getMinimumHeight());

        // Re-entering mainRecyclerView SHOULD hide shadow and restore min height.
        clearInvocations(mMainTabSwitcherDragHandler);
        delegate.handleDragEnter(mainRecyclerView);
        verify(mMainTabSwitcherDragHandler).showDragShadow(eq(mainRecyclerView), eq(false));
        assertEquals(0, mainRecyclerView.getMinimumHeight());
    }

    @Test
    @SmallTest
    public void testMultiTabDrag_DoesNotSetMinHeight() {
        prepareMockPinnedTab(mMockTab1, TAB_ID_1, 0);
        prepareMockPinnedTab(mMockTab2, TAB_ID_2, 1);
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getPinnedTabsCount()).thenReturn(2);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, true);

        View container = mCoordinator.getView();
        TabListRecyclerView pinnedRecyclerView =
                container.findViewById(R.id.pinned_tabs_recycler_view);
        SimpleRecyclerViewAdapter pinnedAdapter =
                (SimpleRecyclerViewAdapter) pinnedRecyclerView.getAdapter();
        pinnedAdapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, model));
        pinnedAdapter
                .getModelList()
                .add(new MVCListAdapter.ListItem(UiType.TAB, createTabPropertyModel()));

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        assertEquals(0, pinnedRecyclerView.getMinimumHeight());

        PropertyModel regModel = createTabPropertyModel();
        regModel.set(TabProperties.TAB_ID, TAB_ID_1);
        regModel.set(TabProperties.IS_PINNED, false);

        TabListRecyclerView mainRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter mainAdapter =
                (SimpleRecyclerViewAdapter) mainRecyclerView.getAdapter();
        mainAdapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, regModel));
        mainAdapter
                .getModelList()
                .add(new MVCListAdapter.ListItem(UiType.TAB, createTabPropertyModel()));

        delegate.handleDragStart(0f, 0f);
        assertEquals(0, mainRecyclerView.getMinimumHeight());
    }

    @Test
    @SmallTest
    public void testRegularTabDragHandler_AttachedToContainerAndNewTabButton() {
        createCoordinator();

        View container = mCoordinator.getView();
        Object listenerInfo = ReflectionHelpers.getField(container, "mListenerInfo");
        assertNotNull(listenerInfo);
        View.OnDragListener containerListener =
                ReflectionHelpers.getField(listenerInfo, "mOnDragListener");
        assertEquals(mMainTabSwitcherDragHandler, containerListener);

        View newTabButton = container.findViewById(R.id.new_tab_button);
        assertNotNull("new_tab_button should exist in the layout", newTabButton);
        Object newTabListenerInfo = ReflectionHelpers.getField(newTabButton, "mListenerInfo");
        assertNotNull(newTabListenerInfo);
        View.OnDragListener newTabButtonListener =
                ReflectionHelpers.getField(newTabListenerInfo, "mOnDragListener");
        assertEquals(mMainTabSwitcherDragHandler, newTabButtonListener);
    }

    @Test
    @SmallTest
    public void testRequestKeyboardFocus_WithPinnedTabs() {
        Tab pinnedTab = prepareMockTab(mMockTab1, PINNED_TAB_ID);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(pinnedTab));
        when(mTabModel.iterator()).thenReturn(List.of(pinnedTab).iterator());
        when(mTabModel.getTabById(PINNED_TAB_ID)).thenReturn(pinnedTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(pinnedTab);

        createCoordinator();
        View containerView = mCoordinator.getView();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(TEST_CONTAINER_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(
                        TEST_CONTAINER_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, TEST_CONTAINER_WIDTH_PX, TEST_CONTAINER_HEIGHT_PX);

        mCoordinator.requestKeyboardFocus();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        RecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        RecyclerView.ViewHolder holder = recyclerView.findViewHolderForAdapterPosition(0);
        assertNotNull(holder);
        assertEquals(holder.itemView, mCoordinator.getView().findFocus());
    }

    @Test
    @SmallTest
    public void testRequestKeyboardFocus_WithUnpinnedTabs() {
        Tab unpinnedTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(unpinnedTab));
        when(mTabModel.iterator()).thenReturn(List.of(unpinnedTab).iterator());
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(unpinnedTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(unpinnedTab);

        createCoordinator();
        View containerView = mCoordinator.getView();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(TEST_CONTAINER_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(
                        TEST_CONTAINER_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, TEST_CONTAINER_WIDTH_PX, TEST_CONTAINER_HEIGHT_PX);

        mCoordinator.requestKeyboardFocus();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        RecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        RecyclerView.ViewHolder holder = recyclerView.findViewHolderForAdapterPosition(0);
        assertNotNull(holder);
        assertEquals(holder.itemView, mCoordinator.getView().findFocus());
    }

    @Test
    @SmallTest
    public void testRequestKeyboardFocus_EmptyList_FallsBackToCollapseButton() {
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of());
        when(mTabModel.iterator()).thenReturn(Collections.emptyIterator());
        when(mTabModel.getCount()).thenReturn(0);

        createCoordinator();
        View containerView = mCoordinator.getView();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(TEST_CONTAINER_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(
                        TEST_CONTAINER_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, TEST_CONTAINER_WIDTH_PX, TEST_CONTAINER_HEIGHT_PX);

        View collapseButton = mCoordinator.getView().findViewById(R.id.collapse_button);
        assertNotNull(collapseButton);

        mCoordinator.requestKeyboardFocus();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        assertTrue(collapseButton.isFocused());
    }

    @Test
    @SmallTest
    public void testOpenKeyboardFocusedContextMenu_WithFocusedUnpinnedTab() {
        Tab unpinnedTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(unpinnedTab));
        when(mTabModel.iterator()).thenReturn(List.of(unpinnedTab).iterator());
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(unpinnedTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(unpinnedTab);

        assertContextMenuOpenedForFocusedTab(TAB_ID_1);
    }

    @Test
    @SmallTest
    public void testOpenKeyboardFocusedContextMenu_WithFocusedPinnedTab() {
        prepareMockPinnedTab(mMockTab1, PINNED_TAB_ID, 0);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(mMockTab1));
        when(mTabModel.iterator()).thenReturn(List.of(mMockTab1).iterator());
        when(mTabModel.getCount()).thenReturn(1);

        assertContextMenuOpenedForFocusedTab(PINNED_TAB_ID);
    }

    @Test
    @SmallTest
    public void testOpenKeyboardFocusedContextMenu_WithFocusedTabGroupHeader() {
        Token tabGroupId = new Token(1L, 2L);
        Tab unpinnedTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(unpinnedTab.getTabGroupId()).thenReturn(tabGroupId);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(unpinnedTab));
        when(mTabModel.iterator()).thenReturn(List.of(unpinnedTab).iterator());
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(unpinnedTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(unpinnedTab);
        when(mTabModel.tabGroupExists(tabGroupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(tabGroupId)).thenReturn(List.of(unpinnedTab));

        createCoordinator();

        RecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        PropertyModel groupPropertyModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);
        adapter.getModelList()
                .add(0, new MVCListAdapter.ListItem(UiType.TAB_GROUP, groupPropertyModel));
        measureAndLayoutContainer();

        mCoordinator.requestKeyboardFocus();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);
        assertTrue(mCoordinator.openKeyboardFocusedContextMenu());

        ArgumentCaptor<RectProvider> rectCaptor = ArgumentCaptor.forClass(RectProvider.class);
        verify(mTabGroupContextMenuCoordinator).showMenu(rectCaptor.capture(), eq(tabGroupId));
        assertNotNull(rectCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testOpenKeyboardFocusedContextMenu_NoFocus_ReturnsFalse() {
        createCoordinator();
        assertFalse(mCoordinator.openKeyboardFocusedContextMenu());
    }

    private void assertContextMenuOpenedForFocusedTab(int expectedTabId) {
        createCoordinator();
        measureAndLayoutContainer();

        mCoordinator.requestKeyboardFocus();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        assertTrue(mCoordinator.openKeyboardFocusedContextMenu());

        ArgumentCaptor<RectProvider> rectCaptor = ArgumentCaptor.forClass(RectProvider.class);
        ArgumentCaptor<AnchorInfo> anchorCaptor = ArgumentCaptor.forClass(AnchorInfo.class);
        verify(mTabContextMenuCoordinator).showMenu(rectCaptor.capture(), anchorCaptor.capture());
        assertEquals(expectedTabId, anchorCaptor.getValue().getAnchorTabId());
        assertNotNull(rectCaptor.getValue());
    }

    private void measureAndLayoutContainer() {
        View containerView = mCoordinator.getView();
        containerView.measure(
                View.MeasureSpec.makeMeasureSpec(TEST_CONTAINER_WIDTH_PX, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(
                        TEST_CONTAINER_HEIGHT_PX, View.MeasureSpec.EXACTLY));
        containerView.layout(0, 0, TEST_CONTAINER_WIDTH_PX, TEST_CONTAINER_HEIGHT_PX);
    }

    @Test
    @SmallTest
    public void testKeyboardHandler_InitializedAndRegisteredOnContainer() {
        createCoordinator();
        assertNotNull(mCoordinator.getKeyboardHandlerForTesting());

        KeyEvent event = new KeyEvent(0, 0, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_DPAD_UP, 0, 0);
        assertFalse(mCoordinator.getView().dispatchKeyEvent(event));
    }

    @Test
    public void testNonOriginatingDrag_ShadowTogglesOnEnterAndExit() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        delegate.handleDragEnter();
        verify(mMainTabSwitcherDragHandler).showDragShadow(any(RecyclerView.class), eq(false));

        delegate.handleDragExit();
        verify(mMainTabSwitcherDragHandler).showDragShadow(any(RecyclerView.class), eq(true));
    }

    @Test
    public void testNonOriginatingDrag_InitializesDelegateAtStartup() {
        createCoordinator();
        verify(mMainTabSwitcherDragHandler)
                .setDragHandlerDelegate(any(TabSwitcherDragHandler.DragHandlerDelegate.class));
        verify(mPinnedTabSwitcherDragHandler)
                .setDragHandlerDelegate(any(TabSwitcherDragHandler.DragHandlerDelegate.class));
    }

    @Test
    public void
            testNonOriginatingDrag_OriginatingWindow_DoesNotToggleShadowInNonOriginatingDelegate() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(true);

        delegate.handleDragEnter();
        verify(mMainTabSwitcherDragHandler, never()).showDragShadow(any(), anyBoolean());

        delegate.handleDragExit();
        verify(mMainTabSwitcherDragHandler, never()).showDragShadow(any(), anyBoolean());
    }

    @Test
    public void testNonOriginatingDrag_IncognitoMismatch_Rejected() {
        // Create coordinator with real TabSwitcherDragHandler instances.
        VerticalTabListCoordinator.setTabSwitcherDragHandlerSupplierForTesting(null);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        createCoordinatorWithoutMockDragHandlers();

        // Dragged item is incognito.
        when(mIncognitoTab.isIncognitoBranded()).thenReturn(true);
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(mIncognitoTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View container = mCoordinator.getView();
        TabListRecyclerView mainRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        Object listenerInfo = ReflectionHelpers.getField(mainRecyclerView, "mListenerInfo");
        View.OnDragListener dragListener =
                ReflectionHelpers.getField(listenerInfo, "mOnDragListener");

        ClipDescription clipDescription =
                new ClipDescription("tab", new String[] {MimeTypeUtils.CHROME_MIMETYPE_TAB});
        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(mDragEvent.getClipDescription()).thenReturn(clipDescription);

        assertFalse(
                "Non-originating window must reject drag on incognito mismatch.",
                dragListener.onDrag(mainRecyclerView, mDragEvent));

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testNonOriginatingDrag_IncognitoMatch_Accepted() {
        // Create coordinator with real TabSwitcherDragHandler instances.
        VerticalTabListCoordinator.setTabSwitcherDragHandlerSupplierForTesting(null);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        createCoordinatorWithoutMockDragHandlers();

        // Dragged item is regular (non-incognito).
        when(mMockTab1.isIncognitoBranded()).thenReturn(false);
        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(mMockTab1).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        View container = mCoordinator.getView();
        TabListRecyclerView mainRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        Object listenerInfo = ReflectionHelpers.getField(mainRecyclerView, "mListenerInfo");
        View.OnDragListener dragListener =
                ReflectionHelpers.getField(listenerInfo, "mOnDragListener");

        ClipDescription clipDescription =
                new ClipDescription("tab", new String[] {MimeTypeUtils.CHROME_MIMETYPE_TAB});
        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(mDragEvent.getClipDescription()).thenReturn(clipDescription);
        when(mDragEvent.getX()).thenReturn(10f);
        when(mDragEvent.getY()).thenReturn(20f);

        assertTrue(
                "Non-originating window must accept drag when incognito matches.",
                dragListener.onDrag(mainRecyclerView, mDragEvent));

        DragDropGlobalState.clear(token);
    }

    @Test
    public void testSingleTabDragOut_StartDragFailure_RestoresDelegate() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(false);

        createCoordinator();
        when(mMainTabSwitcherDragHandler.startTabDragAction(any(), any(), any(), any()))
                .thenReturn(false);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler).startTabDragAction(any(), eq(tab1), any(), any());

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> restoredCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(restoredCaptor.capture());
        assertEquals(
                "Non-originating delegate must be restored on tab drag start failure.",
                nonOriginatingDelegate,
                restoredCaptor.getValue());
    }

    @Test
    public void testGroupHeaderDragOut_StartDragFailure_RestoresDelegate() {
        Token tabGroupId = new Token(1L, 2L);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));
        when(mTabModel.getCount()).thenReturn(2);

        createCoordinator();
        when(mMainTabSwitcherDragHandler.startGroupDragAction(any(), any(), any(), any()))
                .thenReturn(false);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler)
                .startGroupDragAction(any(), eq(tabGroupId), any(), any());

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> restoredCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(restoredCaptor.capture());
        assertEquals(
                "Non-originating delegate must be restored on group drag start failure.",
                nonOriginatingDelegate,
                restoredCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testGroupHeaderDragOut_CollapsedGroup_PassesStripDragShadowView() {
        Token tabGroupId = new Token(1L, 2L);
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(tab1, tab2));
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getTabGroupColor(tabGroupId)).thenReturn(TabGroupColorId.GREY);
        when(mTabModel.getTabGroupTitle(tabGroupId)).thenReturn("Test Group");

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);
        model.set(TabProperties.IS_COLLAPSED, true);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        verify(mMainTabSwitcherDragHandler)
                .startGroupDragAction(any(), eq(tabGroupId), any(), mShadowViewCaptor.capture());
        View shadowView = mShadowViewCaptor.getValue();
        assertNotNull("Shadow view should not be null.", shadowView);
        assertNotNull(
                "Thumbnail view should be present in drag shadow.",
                shadowView.findViewById(R.id.tab_thumbnail));
    }

    @Test
    @SmallTest
    public void testSingleTabDragOut_PassesStripDragShadowView() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(false);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        verify(mMainTabSwitcherDragHandler)
                .startTabDragAction(any(), eq(tab1), any(), mShadowViewCaptor.capture());
        View shadowView = mShadowViewCaptor.getValue();
        assertNotNull("Shadow view should not be null.", shadowView);
        assertNotNull(
                "Thumbnail view should be present in drag shadow.",
                shadowView.findViewById(R.id.tab_thumbnail));
    }

    @Test
    public void testDragEnd_ReusesNonOriginatingDelegateInstance() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.isTabInTabGroup(tab1)).thenReturn(false);
        when(mMainTabSwitcherDragHandler.startTabDragAction(any(), any(), any(), any()))
                .thenReturn(true);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> initialCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(initialCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                initialCaptor.getValue();

        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> activeCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(activeCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate activeDelegate = activeCaptor.getValue();
        assertNotSame(nonOriginatingDelegate, activeDelegate);

        activeDelegate.handleExternalDragEnd(0f, 0f, false);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> restoredCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(restoredCaptor.capture());
        assertEquals(
                "The exact same non-originating delegate instance must be restored on drag end.",
                nonOriginatingDelegate,
                restoredCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testDragOut_AlreadyInProgress_DoesNotStartDrag() {
        createCoordinator();
        when(mMainTabSwitcherDragHandler.isViewDraggingInProgress()).thenReturn(true);

        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        verify(mMainTabSwitcherDragHandler, never()).startTabDragAction(any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testDropIndicatorDecorations_AttachedToRecyclerViews() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        TabListRecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);

        boolean hasMainDropDecorator = false;
        for (int i = 0; i < recyclerView.getItemDecorationCount(); i++) {
            if (recyclerView.getItemDecorationAt(i) instanceof VerticalTabDropIndicatorDecoration) {
                hasMainDropDecorator = true;
                break;
            }
        }
        assertTrue(
                "VerticalTabDropIndicatorDecoration must be attached to main RecyclerView.",
                hasMainDropDecorator);

        boolean hasPinnedDropDecorator = false;
        for (int i = 0; i < pinnedRecyclerView.getItemDecorationCount(); i++) {
            if (pinnedRecyclerView.getItemDecorationAt(i)
                    instanceof VerticalTabPinnedDropIndicatorDecoration) {
                hasPinnedDropDecorator = true;
                break;
            }
        }
        assertTrue(
                "VerticalTabPinnedDropIndicatorDecoration must be attached to pinned RecyclerView.",
                hasPinnedDropDecorator);
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_DragLocation_UpdatesDecorators() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDragLocation(recyclerView, 50f, 50f);

        assertNotNull(
                "Non-originating drag location must update drop target result on decorator.",
                mCoordinator.getDropIndicatorDecorationForTesting().getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_DragExit_ClearsDecorators() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDragLocation(recyclerView, 50f, 50f);

        nonOriginatingDelegate.handleDragExit();
        assertNull(
                "Drag exit must clear drop indicator decorator.",
                mCoordinator.getDropIndicatorDecorationForTesting().getDropTargetResult());
        assertNull(
                "Drag exit must clear pinned drop indicator decorator.",
                mCoordinator.getPinnedDropIndicatorDecorationForTesting().getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_ExternalDragEnd_ClearsDecorators() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDragLocation(recyclerView, 50f, 50f);

        nonOriginatingDelegate.handleExternalDragEnd(recyclerView, 50f, 50f, false);
        assertNull(
                "External drag end must clear drop indicator decorator.",
                mCoordinator.getDropIndicatorDecorationForTesting().getDropTargetResult());
        assertNull(
                "External drag end must clear pinned drop indicator decorator.",
                mCoordinator.getPinnedDropIndicatorDecorationForTesting().getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_Drop_ClearsDecorators() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDragLocation(recyclerView, 50f, 50f);

        nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);
        assertNull(
                "Drop must clear drop indicator decorator.",
                mCoordinator.getDropIndicatorDecorationForTesting().getDropTargetResult());
        assertNull(
                "Drop must clear pinned drop indicator decorator.",
                mCoordinator.getPinnedDropIndicatorDecorationForTesting().getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_NeverUpdatesDecorators() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(true);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDragLocation(recyclerView, 50f, 50f);

        assertNull(
                "Originating window drag must never set drop target result on decorator.",
                mCoordinator.getDropIndicatorDecorationForTesting().getDropTargetResult());
        assertNull(
                "Originating window drag must never set drop target result on pinned decorator.",
                mCoordinator.getPinnedDropIndicatorDecorationForTesting().getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testDestroy_ClearsDecorators() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDragLocation(recyclerView, 50f, 50f);

        mCoordinator.destroy();

        assertNull(
                "Coordinator destroy must clear drop indicator decorator.",
                mCoordinator.getDropIndicatorDecorationForTesting().getDropTargetResult());
        assertNull(
                "Coordinator destroy must clear pinned drop indicator decorator.",
                mCoordinator.getPinnedDropIndicatorDecorationForTesting().getDropTargetResult());
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_SingleTab_Drop_ReparentsToDestinationWindow() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab1);
        when(mTabModel.indexOf(tab1)).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(draggedTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertTrue("Drop must be handled successfully.", handled);
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2),
                        eq(Collections.singletonList(draggedTab)),
                        eq(0),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_SingleTabIntoGroup_Drop_ReparentsIntoTargetGroup() {
        Token groupId = new Token(1L, 2L);
        Tab childTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(childTab.getTabGroupId()).thenReturn(groupId);
        setupMockTabGroup(TAB_ID_1, groupId, List.of(childTab));
        when(mTabModel.indexOf(childTab)).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();

        // Populate ModelList with group header
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        PropertyModel headerModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        headerModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        headerModel.set(TabProperties.TAB_ID, TAB_ID_1);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, headerModel));

        SimpleRecyclerViewAdapter.ViewHolder vh = createViewHolder(headerModel);
        ReflectionHelpers.setField(vh.itemView.getLayoutParams(), "mViewHolder", vh);
        vh.itemView.layout(0, 0, 100, 100);
        recyclerView.addView(vh.itemView);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(draggedTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertTrue("Drop into group must be handled successfully.", handled);
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2),
                        eq(Collections.singletonList(draggedTab)),
                        eq(0),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(Collections.singletonList(draggedTab)),
                        eq(childTab),
                        eq(0),
                        eq(TabGroupMergeNotificationType.DONT_NOTIFY));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void
            testNonOriginatingDrag_SingleTabIntoZeroPinnedWindow_Drop_ReparentsToPinnedIndexZero() {
        when(mTabModel.getPinnedTabsCount()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(3);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab draggedPinnedTab = mock(Tab.class);
        when(draggedPinnedTab.getId()).thenReturn(200);
        when(draggedPinnedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedPinnedTab.getIsPinned()).thenReturn(true);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(draggedPinnedTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 10f);

        assertTrue("Drop pinned tab into zero-pinned window must be handled.", handled);
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(3),
                        eq(Collections.singletonList(draggedPinnedTab)),
                        eq(0),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_TabGroup_Drop_ReparentsTabGroupToDestinationWindow() {
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(5);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Token sourceGroupId = new Token(10L, 20L);
        ArrayList<Map.Entry<Integer, String>> tabIdsToUrls = new ArrayList<>();
        tabIdsToUrls.add(new AbstractMap.SimpleEntry<>(101, "https://google.com"));
        tabIdsToUrls.add(new AbstractMap.SimpleEntry<>(102, "https://chromium.org"));
        TabGroupMetadata metadata =
                new TabGroupMetadata(
                        /* selectedTabId= */ 101,
                        /* sourceWindowId= */ 1,
                        sourceGroupId,
                        tabIdsToUrls,
                        /* tabGroupColor= */ 1,
                        /* tabGroupTitle= */ "Test Group",
                        /* mhtmlTabTitle= */ null,
                        /* tabGroupCollapsed= */ false,
                        /* isGroupShared= */ false,
                        /* isIncognito= */ false);

        Tab mockTab = prepareMockTab(mMockTab1, 101);
        ChromeDropDataAndroid dropData =
                new ChromeTabGroupDropDataAndroid.Builder()
                        .withTabGroupMetadata(metadata)
                        .withTabs(List.of(mockTab))
                        .build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertTrue("Tab group drop must be handled successfully.", handled);
        verify(mMultiInstanceOrchestrator)
                .moveTabGroupToWindowByIdChecked(eq(5), eq(metadata), eq(0), eq(true));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_IncognitoMismatch_Drop_RejectedAndNoReparenting() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab regularTab = mock(Tab.class);
        when(regularTab.getId()).thenReturn(100);
        when(regularTab.isIncognitoBranded()).thenReturn(false);
        when(regularTab.getIsPinned()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(regularTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertFalse("Cross-incognito drop must be rejected.", handled);
        verify(mMultiInstanceOrchestrator, never())
                .moveTabsToWindowByIdChecked(anyInt(), any(), anyInt(), anyInt(), anyBoolean());
        verify(mMultiInstanceOrchestrator, never())
                .moveTabGroupToWindowByIdChecked(anyInt(), any(), anyInt(), anyBoolean());

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_Drop_DoesNotCallMultiInstanceOrchestrator() {
        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(true);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertTrue("Originating drop must return true.", handled);
        verify(mMultiInstanceOrchestrator, never())
                .moveTabsToWindowByIdChecked(anyInt(), any(), anyInt(), anyInt(), anyBoolean());
        verify(mMultiInstanceOrchestrator, never())
                .moveTabGroupToWindowByIdChecked(anyInt(), any(), anyInt(), anyBoolean());
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_Drop_RecordsMetrics() {
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(draggedTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        DragDropMetricUtils.HISTOGRAM_DRAG_DROP_TAB_TYPE,
                        DragDropType.TAB_STRIP_TO_TAB_STRIP);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        watcher.assertExpected();
        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void
            testNonOriginatingDrag_GroupedTab_DropIntoGroup_UngroupsAndReparentsIntoTargetGroup() {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        Token sourceGroupId = new Token(3L, 4L);
        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);
        when(draggedTab.getTabGroupId()).thenReturn(sourceGroupId);

        TabModel sourceTabModel = mock(TabModel.class);
        TabModelSelector sourceSelector = mock(TabModelSelector.class);
        when(sourceSelector.getModel(false)).thenReturn(sourceTabModel);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(sourceSelector);
        when(mTabWindowManager.getTabModelForTab(draggedTab)).thenReturn(sourceTabModel);
        when(sourceTabModel.isTabInTabGroup(draggedTab)).thenReturn(true);
        when(sourceTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);

        Token destGroupId = new Token(1L, 2L);
        Tab childTab = prepareMockTab(mMockTab1, TAB_ID_1);
        when(childTab.getTabGroupId()).thenReturn(destGroupId);
        setupMockTabGroup(TAB_ID_1, destGroupId, List.of(childTab));
        when(mTabModel.indexOf(childTab)).thenReturn(0);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();
        PropertyModel headerModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        headerModel.set(TabProperties.TAB_GROUP_HEADER_ID, destGroupId);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        headerModel.set(TabProperties.TAB_ID, TAB_ID_1);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, headerModel));

        SimpleRecyclerViewAdapter.ViewHolder vh = createViewHolder(headerModel);
        ReflectionHelpers.setField(vh.itemView.getLayoutParams(), "mViewHolder", vh);
        vh.itemView.layout(0, 0, 100, 100);
        recyclerView.addView(vh.itemView);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder()
                        .withTab(draggedTab)
                        .withTabInGroup(true)
                        .withWindowId(1)
                        .build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertTrue("Drop into group must be handled successfully.", handled);
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(Collections.singletonList(draggedTab)),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(false));
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2),
                        eq(Collections.singletonList(draggedTab)),
                        eq(0),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(Collections.singletonList(draggedTab)),
                        eq(childTab),
                        eq(0),
                        eq(TabGroupMergeNotificationType.DONT_NOTIFY));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_SingleTab_MidGroupDrop_TopHalf_InsertsBeforeChild() {
        Token groupId = new Token(1L, 2L);
        Tab childTab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab childTab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        Tab childTab3 = prepareMockTab(mMockTab3, TAB_ID_3);
        when(childTab1.getTabGroupId()).thenReturn(groupId);
        when(childTab2.getTabGroupId()).thenReturn(groupId);
        when(childTab3.getTabGroupId()).thenReturn(groupId);
        setupMockTabGroup(TAB_ID_1, groupId, List.of(childTab1, childTab2, childTab3));
        when(mTabModel.indexOf(childTab1)).thenReturn(0);
        when(mTabModel.indexOf(childTab2)).thenReturn(1);
        when(mTabModel.indexOf(childTab3)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();

        PropertyModel headerModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        headerModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        headerModel.set(TabProperties.TAB_ID, TAB_ID_1);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, headerModel));
        SimpleRecyclerViewAdapter.ViewHolder headerVh = createViewHolder(headerModel);
        ReflectionHelpers.setField(headerVh.itemView.getLayoutParams(), "mViewHolder", headerVh);
        headerVh.itemView.layout(0, 0, 300, 50);
        recyclerView.addView(headerVh.itemView);

        PropertyModel child1Model = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        child1Model.set(TabProperties.TAB_GROUP_ID, groupId);
        child1Model.set(TabProperties.TAB_ID, TAB_ID_1);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, child1Model));
        SimpleRecyclerViewAdapter.ViewHolder child1Vh = createViewHolder(child1Model);
        ReflectionHelpers.setField(child1Vh.itemView.getLayoutParams(), "mViewHolder", child1Vh);
        child1Vh.itemView.layout(0, 50, 300, 100);
        recyclerView.addView(child1Vh.itemView);

        PropertyModel child2Model = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        child2Model.set(TabProperties.TAB_GROUP_ID, groupId);
        child2Model.set(TabProperties.TAB_ID, TAB_ID_2);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, child2Model));
        SimpleRecyclerViewAdapter.ViewHolder child2Vh = createViewHolder(child2Model);
        ReflectionHelpers.setField(child2Vh.itemView.getLayoutParams(), "mViewHolder", child2Vh);
        child2Vh.itemView.layout(0, 100, 300, 150);
        recyclerView.addView(child2Vh.itemView);

        PropertyModel child3Model = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        child3Model.set(TabProperties.TAB_GROUP_ID, groupId);
        child3Model.set(TabProperties.TAB_ID, TAB_ID_3);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, child3Model));
        SimpleRecyclerViewAdapter.ViewHolder child3Vh = createViewHolder(child3Model);
        ReflectionHelpers.setField(child3Vh.itemView.getLayoutParams(), "mViewHolder", child3Vh);
        child3Vh.itemView.layout(0, 150, 300, 200);
        recyclerView.addView(child3Vh.itemView);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(draggedTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        // Hover over Child 2 at top half (y = 110 in range 100..150) -> destTabIndex = 1,
        // indexInGroup = 1
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 150f, 110f);

        assertTrue("Drop into mid-group top half must be handled successfully.", handled);
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2),
                        eq(Collections.singletonList(draggedTab)),
                        eq(1),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(Collections.singletonList(draggedTab)),
                        eq(childTab1),
                        eq(1),
                        eq(TabGroupMergeNotificationType.DONT_NOTIFY));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void testNonOriginatingDrag_SingleTab_MidGroupDrop_BottomHalf_InsertsAfterChild() {
        Token groupId = new Token(1L, 2L);
        Tab childTab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab childTab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        Tab childTab3 = prepareMockTab(mMockTab3, TAB_ID_3);
        when(childTab1.getTabGroupId()).thenReturn(groupId);
        when(childTab2.getTabGroupId()).thenReturn(groupId);
        when(childTab3.getTabGroupId()).thenReturn(groupId);
        setupMockTabGroup(TAB_ID_1, groupId, List.of(childTab1, childTab2, childTab3));
        when(mTabModel.indexOf(childTab1)).thenReturn(0);
        when(mTabModel.indexOf(childTab2)).thenReturn(1);
        when(mTabModel.indexOf(childTab3)).thenReturn(2);
        when(mTabModel.getCount()).thenReturn(3);
        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recyclerView.getAdapter();

        PropertyModel headerModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        headerModel.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);
        headerModel.set(TabProperties.IS_COLLAPSED, false);
        headerModel.set(TabProperties.TAB_ID, TAB_ID_1);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, headerModel));
        SimpleRecyclerViewAdapter.ViewHolder headerVh = createViewHolder(headerModel);
        ReflectionHelpers.setField(headerVh.itemView.getLayoutParams(), "mViewHolder", headerVh);
        headerVh.itemView.layout(0, 0, 300, 50);
        recyclerView.addView(headerVh.itemView);

        PropertyModel child1Model = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        child1Model.set(TabProperties.TAB_GROUP_ID, groupId);
        child1Model.set(TabProperties.TAB_ID, TAB_ID_1);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, child1Model));
        SimpleRecyclerViewAdapter.ViewHolder child1Vh = createViewHolder(child1Model);
        ReflectionHelpers.setField(child1Vh.itemView.getLayoutParams(), "mViewHolder", child1Vh);
        child1Vh.itemView.layout(0, 50, 300, 100);
        recyclerView.addView(child1Vh.itemView);

        PropertyModel child2Model = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        child2Model.set(TabProperties.TAB_GROUP_ID, groupId);
        child2Model.set(TabProperties.TAB_ID, TAB_ID_2);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, child2Model));
        SimpleRecyclerViewAdapter.ViewHolder child2Vh = createViewHolder(child2Model);
        ReflectionHelpers.setField(child2Vh.itemView.getLayoutParams(), "mViewHolder", child2Vh);
        child2Vh.itemView.layout(0, 100, 300, 150);
        recyclerView.addView(child2Vh.itemView);

        PropertyModel child3Model = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        child3Model.set(TabProperties.TAB_GROUP_ID, groupId);
        child3Model.set(TabProperties.TAB_ID, TAB_ID_3);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, child3Model));
        SimpleRecyclerViewAdapter.ViewHolder child3Vh = createViewHolder(child3Model);
        ReflectionHelpers.setField(child3Vh.itemView.getLayoutParams(), "mViewHolder", child3Vh);
        child3Vh.itemView.layout(0, 150, 300, 200);
        recyclerView.addView(child3Vh.itemView);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder().withTab(draggedTab).build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        // Hover over Child 2 at bottom half (y = 140 in range 100..150) -> destTabIndex = 2,
        // indexInGroup = 2
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 150f, 140f);

        assertTrue("Drop into mid-group bottom half must be handled successfully.", handled);
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2),
                        eq(Collections.singletonList(draggedTab)),
                        eq(2),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));
        verify(mTabModel)
                .mergeListOfTabsToGroup(
                        eq(Collections.singletonList(draggedTab)),
                        eq(childTab1),
                        eq(2),
                        eq(TabGroupMergeNotificationType.DONT_NOTIFY));

        DragDropGlobalState.clear(token);
    }

    @Test
    @SmallTest
    public void
            testNonOriginatingDrag_GroupedTab_DropIntoStandaloneList_UngroupsAndReparentsToDestTabIndex() {
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);

        Token sourceGroupId = new Token(3L, 4L);
        Tab draggedTab = mock(Tab.class);
        when(draggedTab.getId()).thenReturn(100);
        when(draggedTab.isIncognitoBranded()).thenReturn(false);
        when(draggedTab.getIsPinned()).thenReturn(false);
        when(draggedTab.getTabGroupId()).thenReturn(sourceGroupId);

        TabModel sourceTabModel = mock(TabModel.class);
        TabModelSelector sourceSelector = mock(TabModelSelector.class);
        when(sourceSelector.getModel(false)).thenReturn(sourceTabModel);
        when(mTabWindowManager.getTabModelSelectorById(1)).thenReturn(sourceSelector);
        when(mTabWindowManager.getTabModelForTab(draggedTab)).thenReturn(sourceTabModel);
        when(sourceTabModel.isTabInTabGroup(draggedTab)).thenReturn(true);
        when(sourceTabModel.getTabUngrouper()).thenReturn(mTabUngrouper);

        when(mTabModel.findFirstNonPinnedTabIndex()).thenReturn(0);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(2);

        createCoordinator();
        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler).setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate nonOriginatingDelegate =
                delegateCaptor.getValue();

        when(mMainTabSwitcherDragHandler.isDragSourceInstance()).thenReturn(false);

        ChromeDropDataAndroid dropData =
                new ChromeTabDropDataAndroid.Builder()
                        .withTab(draggedTab)
                        .withTabInGroup(true)
                        .withWindowId(1)
                        .build();
        Token token = DragDropGlobalState.store(1, dropData, null);
        TabDragHandlerBase.setDragTokenForTesting(token);

        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        boolean handled = nonOriginatingDelegate.handleDrop(recyclerView, 50f, 50f);

        assertTrue("Drop must be handled successfully.", handled);
        verify(mTabUngrouper)
                .ungroupTabs(
                        eq(Collections.singletonList(draggedTab)),
                        /* trailing= */ eq(true),
                        /* allowDialog= */ eq(false));
        verify(mMultiInstanceOrchestrator)
                .moveTabsToWindowByIdChecked(
                        eq(2),
                        eq(Collections.singletonList(draggedTab)),
                        eq(0),
                        eq(TabList.INVALID_TAB_INDEX),
                        eq(true));

        DragDropGlobalState.clear(token);
    }

    // =============================================================================================
    // Helper Methods
    // =============================================================================================

    /** Helper method to instantiate {@link VerticalTabListCoordinator} for testing. */
    private void createCoordinator() {
        AtomicInteger dragHandlerCallCount = new AtomicInteger(0);
        VerticalTabListCoordinator.setTabSwitcherDragHandlerSupplierForTesting(
                () ->
                        dragHandlerCallCount.getAndIncrement() == 0
                                ? mMainTabSwitcherDragHandler
                                : mPinnedTabSwitcherDragHandler);

        when(mMainTabSwitcherDragHandler.startTabDragAction(any(), any(), any(), any()))
                .thenReturn(true);
        when(mMainTabSwitcherDragHandler.startGroupDragAction(any(), any(), any(), any()))
                .thenReturn(true);
        when(mPinnedTabSwitcherDragHandler.startTabDragAction(any(), any(), any(), any()))
                .thenReturn(true);
        when(mPinnedTabSwitcherDragHandler.startGroupDragAction(any(), any(), any(), any()))
                .thenReturn(true);

        mCoordinator =
                new VerticalTabListCoordinator(
                        mActivity,
                        mTabModelSelector,
                        mProfile,
                        mVerticalTabsActionDelegate,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mMultiInstanceManager,
                        mSnackbarManager,
                        mDesktopWindowStateManager,
                        mShareDelegateSupplier,
                        mDataSharingTabManager,
                        mIsVerticalTabsActiveSupplier,
                        mVerticalTabsWidthSupplier,
                        /* canActivateTabLayoutToggleMenuSupplier= */ null,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabContentManagerSupplier,
                        mUndoBarThrottle,
                        mBrowserControlsStateProvider);

        mCoordinator.getCollapseController().setRailCollapseListener(mMockRailCollapseListener);
    }

    private void createCoordinatorWithoutMockDragHandlers() {
        mCoordinator =
                new VerticalTabListCoordinator(
                        mActivity,
                        mTabModelSelector,
                        mProfile,
                        mVerticalTabsActionDelegate,
                        mWindowAndroid,
                        mActivityResultTracker,
                        mMultiInstanceManager,
                        mSnackbarManager,
                        mDesktopWindowStateManager,
                        mShareDelegateSupplier,
                        mDataSharingTabManager,
                        mIsVerticalTabsActiveSupplier,
                        mVerticalTabsWidthSupplier,
                        /* canActivateTabLayoutToggleMenuSupplier= */ null,
                        mTabHoverCardViewStub,
                        mTabGroupHoverCardViewStub,
                        mTabContentManagerSupplier,
                        mUndoBarThrottle,
                        mBrowserControlsStateProvider);

        mCoordinator.getCollapseController().setRailCollapseListener(mMockRailCollapseListener);
    }

    /** Helper method to create a basic mock {@link Tab} with initialized state and URL. */
    private Tab prepareMockTab(Tab tab, int id) {
        when(tab.getId()).thenReturn(id);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.getTitle()).thenReturn("Tab " + id);
        when(tab.getUrl()).thenReturn(MOCK_URL);
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        return tab;
    }

    /** Helper method to create a mock pinned {@link Tab} and wire it into {@link #mTabModel}. */
    private void prepareMockPinnedTab(Tab tab, int id, int index) {
        Tab preparedTab = prepareMockTab(tab, id);
        when(preparedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getTabById(id)).thenReturn(preparedTab);
        when(mTabModel.getTabAt(index)).thenReturn(preparedTab);
        when(mTabModel.indexOf(preparedTab)).thenReturn(index);
    }

    /** Helper method to manufacture synthetic {@link MotionEvent} objects for touch testing. */
    private MotionEvent obtainMotionEvent(int action, float x, float y) {
        // We get the current time since Android rejects times that are 0 or in the past.
        // When the finger/mouse first touches the screen.
        long downTime = SystemClock.uptimeMillis();
        // When the specific event happens (finger down, lift finger, etc.).
        long eventTime = SystemClock.uptimeMillis();

        // Manually create a fake event.
        return MotionEvent.obtain(
                /* downTime= */ downTime,
                /* eventTime= */ eventTime,
                /* action= */ action,
                /* x= */ x,
                /* y= */ y,
                /* metaState= */ 0);
    }

    /** Helper method to wire a spy {@link TabListRecyclerView} populated with a mock tab. */
    private TabListRecyclerView setupMockRecyclerViewWithTab(Tab tab, int tabId) {
        // Mock the backend model.
        Tab mockTab = prepareMockTab(tab, tabId);
        when(mTabModel.getTabById(tabId)).thenReturn(mockTab);
        when(mTabModel.getRelatedTabList(tabId)).thenReturn(Collections.singletonList(mockTab));

        RecyclerView.LayoutParams layoutParams = new RecyclerView.LayoutParams(0, 0);
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(mMockChildView, /* binder= */ null);
        ReflectionHelpers.setField(layoutParams, "mViewHolder", viewHolder);
        when(mMockChildView.getLayoutParams()).thenReturn(layoutParams);

        createCoordinator();
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView realRecyclerView = container.findViewById(R.id.tab_list_recycler_view);

        // Populate the real UI list dataset with a placeholder tab item data properties bundle.
        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) realRecyclerView.getAdapter();
        PropertyModel tabPropertyModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        tabPropertyModel.set(TabProperties.TAB_ID, tabId);
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, tabPropertyModel));

        // Wrap the real inflated Recycler View in a spy.
        return spy(realRecyclerView);
    }

    /** Asserts that performing a right click on the view does not instantiate context menu. */
    private void assertContextClickDoesNotLaunchEmptySpaceContextMenu(View targetView) {
        assertNotNull("Target view must not be null.", targetView);
        assertNull(
                "Context menu coordinator should start as null.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate a right-click context interaction.
        targetView.performContextClick();

        // The listener must consume the event, meaning the menu coordinator stays null.
        assertNull(
                "Right-clicking this view should not instantiate the context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    /** Asserts that performing a right click on the view instantiates context menu. */
    private void assertEmptySpaceContextMenuRightClick(View targetView) {
        assertNotNull("Target view for context click must not be null.", targetView);
        assertNull(
                "Context menu coordinator should start as null.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate a right-click context interaction.
        targetView.performContextClick();

        assertNotNull(
                "Right click should instantiate the context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    /** Asserts that long-pressing a view instantiates the context menu coordinator. */
    private void assertStandardViewLongPressLaunchesMenu(View targetView) {
        // Ensure the context menu coordinator reference starts fresh as null.
        assertNotNull("Target view for long press must not be null.", targetView);
        assertNull(
                "Context menu coordinator should start as null.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate touch down at coordinates (10, 10) inside the header container.
        MotionEvent downEvent = obtainMotionEvent(MotionEvent.ACTION_DOWN, 10f, 10f);

        // To simulate a touch on a normal view (not recycler view) so that its OnTouchListener
        // triggers, we call dispatchTouchEvent(downEvent).
        targetView.dispatchTouchEvent(downEvent);

        // Advance Robolectric's clock by 500ms to trigger the long-press gesture.
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);

        assertNotNull(
                "Long press should instantiate the empty space context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    /** Asserts that long-pressing empty space on the RecyclerView instantiates context menu. */
    private void assertRecyclerViewLongPressLaunchesEmptySpaceContextMenu(
            RecyclerView recyclerView) {
        assertNotNull("RecyclerView target for long press must not be null.", recyclerView);

        // Ensure the context menu coordinator reference starts fresh as null.
        assertNull(
                "Tab Strip Context menu coordinator should start as null.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate an action down at coordinates (250, 400).
        MotionEvent downEvent = obtainMotionEvent(MotionEvent.ACTION_DOWN, 250f, 400f);
        recyclerView.onInterceptTouchEvent(downEvent);

        // Advance Robolectric's clock by 500ms to trigger the long-press timeout.
        // This triggers the gestureDetector's long-press callback that we overrode.
        ShadowLooper.idleMainLooper(500, TimeUnit.MILLISECONDS);
        assertNotNull(
                "Long press on empty space should instantiate the context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    /** Asserts that dispatching a touch updates the captured last touch point matrix. */
    private void assertViewTouchUpdatesLastTouchPoint(
            View targetView, int expectedX, int expectedY) {
        assertNotNull("Target view must not be null.", targetView);

        // Reset tracking coordinates to a baseline (0, 0).
        mCoordinator.getLastTouchPointForTesting().set(0, 0);

        // Simulate a touch down at local button coordinates (15, 25).
        MotionEvent downEvent =
                obtainMotionEvent(MotionEvent.ACTION_DOWN, (float) expectedX, (float) expectedY);
        targetView.dispatchTouchEvent(downEvent);

        // Verify that mLastTouchPoint successfully captured the localized touch matrix.
        Point savedPoint = mCoordinator.getLastTouchPointForTesting();
        assertEquals("X touch point should map local to the view bounds.", expectedX, savedPoint.x);
        assertEquals("Y touch point should map local to the view bounds.", expectedY, savedPoint.y);

        downEvent.recycle();
    }

    // TODO(crbug.com/509226293): Add TAB_ACTION_STATE to ALL_KEYS_VERTICAL_TAB in TabProperties
    // instead.
    /** Creates a {@link PropertyModel} with keys needed for drag shadow binding. */
    private PropertyModel createTabPropertyModel() {
        return new PropertyModel.Builder(
                        PropertyModel.concatKeys(
                                TabProperties.ALL_KEYS_VERTICAL_TAB,
                                new PropertyKey[] {
                                    TabProperties.TAB_ACTION_STATE,
                                    TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER
                                }))
                .with(TabProperties.TAB_ACTION_STATE, TabActionState.CLOSABLE)
                .build();
    }

    /** Helper to retrieve {@link VerticalTabListItemTouchHelperCallback.OnDragOutListener}. */
    private VerticalTabListItemTouchHelperCallback.OnDragOutListener getOnDragOutListener() {
        VerticalTabListItemTouchHelperCallback callback =
                mCoordinator.getMainTouchHelperCallbackForTesting();
        assertNotNull("Touch helper callback must not be null.", callback);
        VerticalTabListItemTouchHelperCallback.OnDragOutListener listener =
                callback.getOnDragOutListenerForTesting();
        assertNotNull("OnDragOutListener must not be null.", listener);
        return listener;
    }

    /** Helper to construct a {@link SimpleRecyclerViewAdapter.ViewHolder} with model. */
    private SimpleRecyclerViewAdapter.ViewHolder createViewHolder(PropertyModel model) {
        View view = new View(mActivity);
        view.setLayoutParams(new RecyclerView.LayoutParams(100, 100));
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(view, /* binder= */ null);
        viewHolder.model = model;
        return viewHolder;
    }

    /** Helper to wire mock tab group data into {@link TabModel}. */
    private void setupMockTabGroup(int repTabId, Token groupId, List<Tab> tabsInGroup) {
        Tab repTab = tabsInGroup.get(0);
        when(repTab.getTabGroupId()).thenReturn(groupId);
        for (Tab tab : tabsInGroup) {
            when(mTabModel.getTabById(tab.getId())).thenReturn(tab);
            when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);
            when(mTabModel.getRelatedTabList(tab.getId())).thenReturn(tabsInGroup);
        }
        when(mTabModel.tabGroupExists(groupId)).thenReturn(true);
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(tabsInGroup);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(repTab));
        when(mTabModel.getGroupLastShownTabId(groupId)).thenReturn(repTabId);
    }

    private Tab prepareAndShowHoverCard(Tab mockTab) {
        createCoordinator();
        Tab tab = prepareMockTab(mockTab, TAB_ID_1);
        when(mTabModelSelector.getTabById(TAB_ID_1)).thenReturn(tab);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID_2);
        when(mTabModel.getTabById(TAB_ID_1)).thenReturn(tab);

        TabHoverCardListener hoverListener = mCoordinator.getTabHoverCardListenerForTesting();
        assertNotNull(hoverListener);
        hoverListener.onTabHoverCardStateChanged(TAB_ID_1, mMockChildView, /* isHovered= */ true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(mTabHoverCardView).show(anyFloat(), anyFloat());

        // Clear initial hide() invocation from setup/setActive(false)
        clearInvocations(mTabHoverCardView);
        return tab;
    }

    @Test
    @SmallTest
    public void testMediatorOnLongPressTabItemEventListener_ShowsItemContextMenu() {
        TabListRecyclerView recyclerView = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        when(mMockChildView.getParent()).thenReturn(recyclerView);
        when(recyclerView.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        when(mMockChildView.getWidth()).thenReturn(300);
        when(mMockChildView.getHeight()).thenReturn(100);
        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 50;
                            pos[1] = 100;
                            return null;
                        })
                .when(mMockChildView)
                .getLocationInWindow(any());

        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);

        TabListMediator mediator = mCoordinator.getMediatorForTesting();
        var listener = mediator.getOnLongPressTabItemEventListenerForTesting();
        assertNotNull(listener);

        listener.onLongPressEvent(TAB_ID_1, mMockChildView);

        ArgumentCaptor<RectProvider> rectCaptor = ArgumentCaptor.forClass(RectProvider.class);
        ArgumentCaptor<AnchorInfo> anchorInfoCaptor = ArgumentCaptor.forClass(AnchorInfo.class);
        verify(mTabContextMenuCoordinator)
                .showMenu(rectCaptor.capture(), anchorInfoCaptor.capture());

        Rect bounds = rectCaptor.getValue().getRect();
        assertEquals(50, bounds.left);
        assertEquals(100, bounds.top);
        assertEquals(350, bounds.right);
        assertEquals(200, bounds.bottom);
        assertEquals(TAB_ID_1, anchorInfoCaptor.getValue().getAnchorTabId());
    }

    @Test
    @SmallTest
    public void testOnScrollStateChanged_DraggingDismissesContextMenus() {
        createCoordinator();
        mCoordinator.setTabContextMenuCoordinatorForTesting(mTabContextMenuCoordinator);
        mCoordinator.setTabStripContextMenuCoordinatorForTesting(mTabStripContextMenuCoordinator);
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        RecyclerView.OnScrollListener scrollListener = mCoordinator.getOnScrollListenerForTesting();
        assertNotNull(scrollListener);

        scrollListener.onScrollStateChanged(
                mCoordinator.getRecyclerViewForTesting(), RecyclerView.SCROLL_STATE_DRAGGING);

        verify(mTabContextMenuCoordinator).dismiss();
        verify(mTabStripContextMenuCoordinator).dismiss();
        verify(mTabGroupContextMenuCoordinator).dismiss();
    }

    private void setupMockTabModelWithTabs(List<Tab> tabs, int initialSelectedIndex) {
        int[] currentIndex = new int[] {initialSelectedIndex};
        when(mTabModel.getCount()).thenReturn(tabs.size());
        when(mTabModel.iterator()).thenAnswer(inv -> tabs.iterator());
        when(mTabModel.index()).thenAnswer(inv -> currentIndex[0]);
        when(mTabModel.getTabAt(anyInt()))
                .thenAnswer(
                        inv -> {
                            int idx = inv.getArgument(0);
                            return idx >= 0 && idx < tabs.size() ? tabs.get(idx) : null;
                        });
        when(mTabModel.indexOf(any(Tab.class))).thenAnswer(inv -> tabs.indexOf(inv.getArgument(0)));
        for (Tab tab : tabs) {
            when(mTabModel.getTabById(tab.getId())).thenReturn(tab);
        }
        SettableNullableObservableSupplier<Tab> currentTabSupplier =
                ObservableSuppliers.createNullable();
        currentTabSupplier.set(
                initialSelectedIndex >= 0 && initialSelectedIndex < tabs.size()
                        ? tabs.get(initialSelectedIndex)
                        : null);
        when(mTabModel.getCurrentTabSupplier()).thenReturn(currentTabSupplier);
        doAnswer(
                        inv -> {
                            int newIndex = inv.getArgument(0);
                            currentIndex[0] = newIndex;
                            if (newIndex >= 0 && newIndex < tabs.size()) {
                                currentTabSupplier.set(tabs.get(newIndex));
                            }
                            return null;
                        })
                .when(mTabModel)
                .setIndex(anyInt(), anyInt());
        NextTabPolicySupplier nextTabPolicySupplier = mock(NextTabPolicySupplier.class);
        when(nextTabPolicySupplier.get()).thenReturn(NextTabPolicy.HIERARCHICAL);
        when(mTabModel.getNextTabPolicySupplier()).thenReturn(nextTabPolicySupplier);
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_SelectedTabDraggedOut_DeselectsTab() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabModelWithTabs(List.of(tab1, tab2), 0);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_DRAG);
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_NonSelectedTabDraggedOut_DoesNotDeselectTab() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabModelWithTabs(List.of(tab1, tab2), 1);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        // Deselect should not occur, so next tab (index 1) is never selected by deselect.
        verify(mTabModel, never()).setIndex(eq(1), anyInt());
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_DragEnterAndExit_ReselectsAndDeselects() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabModelWithTabs(List.of(tab1, tab2), 0);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_DRAG);

        delegate.handleDragEnter();
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_DRAG);

        delegate.handleDragExit();
        verify(mTabModel, times(2)).setIndex(1, TabSelectionType.FROM_DRAG);
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_ExternalDragEndCancelled_ReselectsTab() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabModelWithTabs(List.of(tab1, tab2), 0);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_DRAG);

        delegate.handleExternalDragEnd(0f, 0f, /* isOSNewWindowDrop= */ false);
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_DRAG);
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_ExternalDragEndReparented_KeepsNextTabSelected() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabModelWithTabs(List.of(tab1, tab2), 0);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_DRAG);

        delegate.handleExternalDragEnd(0f, 0f, /* isOSNewWindowDrop= */ true);
        verify(mTabModel, never()).setIndex(eq(0), anyInt());
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_GroupDragContainingSelectedTab_DeselectsAndReselects() {
        Token tabGroupId = new Token(1L, 2L);
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        Tab tab3 = prepareMockTab(mMockTab3, TAB_ID_3);
        setupMockTabGroup(TAB_ID_1, tabGroupId, List.of(tab1, tab2));
        setupMockTabModelWithTabs(List.of(tab1, tab2, tab3), 0);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab1, tab3));

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);
        model.set(TabProperties.IS_COLLAPSED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        verify(mTabModel).setIndex(2, TabSelectionType.FROM_DRAG);

        delegate.handleDragEnter();
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_DRAG);
    }

    @Test
    @SmallTest
    public void testOriginatingDrag_InternalDragEndCancelled_ReselectsTab() {
        Tab tab1 = prepareMockTab(mMockTab1, TAB_ID_1);
        Tab tab2 = prepareMockTab(mMockTab2, TAB_ID_2);
        setupMockTabModelWithTabs(List.of(tab1, tab2), 0);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_ID, TAB_ID_1);
        model.set(TabProperties.IS_PINNED, false);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);

        ArgumentCaptor<TabSwitcherDragHandler.DragHandlerDelegate> delegateCaptor =
                ArgumentCaptor.forClass(TabSwitcherDragHandler.DragHandlerDelegate.class);
        verify(mMainTabSwitcherDragHandler, atLeastOnce())
                .setDragHandlerDelegate(delegateCaptor.capture());
        TabSwitcherDragHandler.DragHandlerDelegate delegate = delegateCaptor.getValue();

        delegate.handleDragStart(0f, 0f);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_DRAG);

        delegate.handleInternalDragEnd();
        verify(mTabModel).setIndex(0, TabSelectionType.FROM_DRAG);
    }

    @Test
    @SmallTest
    public void testSetInTransition_RequestsLayout() {
        createCoordinator();
        TabListRecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        TabListRecyclerView spyPinnedRecyclerView = spy(pinnedRecyclerView);
        ReflectionHelpers.setField(mCoordinator, "mPinnedTabsRecyclerView", spyPinnedRecyclerView);

        clearInvocations(spyPinnedRecyclerView);

        // Transition start does not request layout (handled by container width change).
        mCoordinator.setInTransition(true);
        ReflectionHelpers.callInstanceMethod(
                verify(spyPinnedRecyclerView, never()), "requestLayout");

        clearInvocations(spyPinnedRecyclerView);

        // Transition end triggers layout to recycle extra items back to viewport bounds.
        mCoordinator.setInTransition(false);
        ReflectionHelpers.callInstanceMethod(verify(spyPinnedRecyclerView), "requestLayout");
    }

    @Test
    @SmallTest
    public void testCalculatePinnedExtraLayoutSpace_NotTransitioning() {
        createCoordinator();
        int[] extraLayoutSpace = new int[2];
        RecyclerView.State state = mock(RecyclerView.State.class);
        when(state.getItemCount()).thenReturn(30);

        mCoordinator.calculatePinnedExtraLayoutSpace(mActivity, state, extraLayoutSpace);

        assertEquals(0, extraLayoutSpace[0]);
        assertEquals(0, extraLayoutSpace[1]);
    }

    @Test
    @SmallTest
    public void testCalculatePinnedExtraLayoutSpace_InTransition() {
        createCoordinator();
        mCoordinator.setInTransition(true);
        int[] extraLayoutSpace = new int[2];
        RecyclerView.State state = mock(RecyclerView.State.class);

        int itemHeight =
                TabVerticalViewBinder.getPinnedItemHeight(mActivity)
                        + mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.vertical_tab_pinned_item_margin_bottom);
        TabListRecyclerView pinnedRecyclerView =
                mCoordinator.getView().findViewById(R.id.pinned_tabs_recycler_view);
        int padding = pinnedRecyclerView.getPaddingTop() + pinnedRecyclerView.getPaddingBottom();

        // 0 items: falls back to container/display height.
        when(state.getItemCount()).thenReturn(0);
        mCoordinator.calculatePinnedExtraLayoutSpace(mActivity, state, extraLayoutSpace);
        int baseHeight = extraLayoutSpace[0];
        assertTrue(baseHeight > 0);
        assertEquals(baseHeight, extraLayoutSpace[1]);

        // Many items: scales with total content height.
        extraLayoutSpace[0] = 0;
        extraLayoutSpace[1] = 0;
        when(state.getItemCount()).thenReturn(30);
        mCoordinator.calculatePinnedExtraLayoutSpace(mActivity, state, extraLayoutSpace);
        int expectedHeight = 30 * itemHeight + padding;
        assertEquals(expectedHeight, extraLayoutSpace[0]);
        assertEquals(expectedHeight, extraLayoutSpace[1]);

        // Excessive items: capped at baseHeight * MAX_SINGLE_ROW_SPAN_COUNT + padding.
        extraLayoutSpace[0] = 0;
        extraLayoutSpace[1] = 0;
        when(state.getItemCount()).thenReturn(500);
        mCoordinator.calculatePinnedExtraLayoutSpace(mActivity, state, extraLayoutSpace);
        int expectedCap =
                baseHeight * VerticalTabListCoordinator.MAX_SINGLE_ROW_SPAN_COUNT + padding;
        assertEquals(expectedCap, extraLayoutSpace[0]);
        assertEquals(expectedCap, extraLayoutSpace[1]);
    }
}
