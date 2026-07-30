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
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.InputDevice;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.AnchorInfo;
import org.chromium.chrome.browser.compositor.overlays.strip.TabGroupContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestratorFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherDragHandler;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardHelper.TabHoverCardListener;
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
import org.chromium.components.tab_groups.TabGroupsFeatureMap;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Supplier;

/** Unit tests for {@link VerticalTabListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY,
    ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS,
    ChromeFeatureList.TASK_MANAGER_CLANK,
    TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS,
    ChromeFeatureList.ANIMATED_IMAGE_DRAG_SHADOW
})
public class VerticalTabListCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Profile mProfile;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mSelectorObserverCaptor;
    @Mock private VerticalTabsActionDelegate mVerticalTabsActionDelegate;
    @Mock private WindowAndroid mWindowAndroid;
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
    @Mock private Supplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private TabHoverCardView mTabHoverCardView;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private TabModel mEmptyTabModel;
    @Mock private TabModel mNewTabModel;
    @Mock private View mMockChildView;
    @Mock private Tab mMockTab1;

    private static final int TAB_ID_1 = 1;
    private static final int PINNED_TAB_ID = 3;
    private static final int NON_EXISTENT_TAB_ID = 999;
    private static final GURL MOCK_URL = new GURL("https://google.com");

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
    private VerticalTabListCoordinator mCoordinator;

    @Before
    public void setUp() {
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
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

        mCurrentTabModelSupplier.set(mTabModel);
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.isTabModelRestored()).thenReturn(true);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.iterator()).thenReturn(Collections.emptyIterator());
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        GlicEnabling.setEnabledForTesting(false);
        MultiInstanceOrchestratorFactory.setInstanceForTesting(mMultiInstanceOrchestrator);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);

        when(mTabHoverCardViewStub.getParent()).thenReturn(mock(ViewGroup.class));
        when(mTabHoverCardView.getContext()).thenReturn(mActivity);
        doAnswer(
                        invocation -> {
                            ViewStub.OnInflateListener listener = invocation.getArgument(0);
                            listener.onInflate(mTabHoverCardViewStub, mTabHoverCardView);
                            return null;
                        })
                .when(mTabHoverCardViewStub)
                .setOnInflateListener(any());

        doAnswer(
                        invocation -> {
                            mTabGroupObservers.add(invocation.getArgument(0));
                            return null;
                        })
                .when(mTabModel)
                .addTabGroupObserver(any(TabGroupObserver.class));
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
    public void testVTGridButtonTouch_UpdatesLastTouchPointToLocalCoordinates() {
        createCoordinator();
        View gridButton = mCoordinator.getView().findViewById(R.id.grid_button);
        assertViewTouchUpdatesLastTouchPoint(gridButton, 15, 25);
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
    public void testTabGroupHeaderInteraction_LaunchesGroupHeaderContextMenu_Fallback() {
        Token tabGroupId = new Token(1L, 2L);
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(mMockTab1, TAB_ID_1);
        when(mMockTab1.getTabGroupId()).thenReturn(tabGroupId);

        assertNull(mCoordinator.getTabGroupContextMenuCoordinatorForTesting());

        // Inject the mock coordinator here before calling #handleContextMenuInteractionForTesting
        // so that we can verify the rect captor later in this test.
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) recyclerViewSpy.getAdapter();
        PropertyModel groupPropertyModel = adapter.getModelList().get(0).model;
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);

        assertEquals(
                "The adapter lookup should resolve this list item row layout as a TAB_GROUP type.",
                UiType.TAB_GROUP,
                adapter.getItemViewType(0));

        // Create a mock View layout box (child of the recycler view) that renders the tab card on
        // the screen.
        when(mMockChildView.getWidth()).thenReturn(400);
        when(mMockChildView.getHeight()).thenReturn(80);

        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 40;
                            pos[1] = 120;
                            return null;
                        })
                .when(mMockChildView)
                .getLocationInWindow(any());

        when(recyclerViewSpy.findChildViewUnder(200f, 150f)).thenReturn(mMockChildView);
        when(recyclerViewSpy.getChildAdapterPosition(mMockChildView)).thenReturn(0);

        boolean handled =
                mCoordinator.handleContextMenuInteractionForTesting(
                        mActivity, recyclerViewSpy, /* localX= */ 200f, /* localY= */ 150f);

        assertTrue(
                "Context gesture interaction on an active group header card should return true.",
                handled);

        ArgumentCaptor<RectProvider> rectCaptor = ArgumentCaptor.forClass(RectProvider.class);
        verify(mTabGroupContextMenuCoordinator).showMenu(rectCaptor.capture(), eq(tabGroupId));

        Rect descriptiveBoundRect = rectCaptor.getValue().getRect();
        assertEquals(
                "Anchor bounding box left edge must be mapped.", 40, descriptiveBoundRect.left);
        assertEquals("Anchor bounding box top edge must be mapped.", 120, descriptiveBoundRect.top);
        assertEquals(
                "Anchor bounding box right edge must be mapped.", 440, descriptiveBoundRect.right);
        assertEquals(
                "Anchor bounding box bottom edge must be mapped.",
                200,
                descriptiveBoundRect.bottom);

        if (mCoordinator.getTabGroupContextMenuCoordinatorForTesting() != null) {
            // Dismiss/destroy the instantiated context menu tracker to satisfy LifetimeAssert.
            mCoordinator.getTabGroupContextMenuCoordinatorForTesting().destroy();
        }
    }

    @Test
    @SmallTest
    public void testTabItemInteraction_LaunchesTabContextMenu_Fallback() {
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
        assertNull(groupModel.get(TabProperties.TAB_ACTION_BUTTON_DATA));

        mCoordinator.toggleTabGroupExpansion(TAB_ID_1);
        assertFalse(
                "Tab group should be expanded (IS_COLLAPSED = false) after first toggle click.",
                groupModel.get(TabProperties.IS_COLLAPSED));
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
    public void testGridButtonClick() {
        createCoordinator();
        ImageButton gridButton = mCoordinator.getView().findViewById(R.id.grid_button);
        assertNotNull(gridButton);
        gridButton.performClick();
        verify(mVerticalTabsActionDelegate).openHubPane(PaneId.TAB_GROUPS);
    }

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
                        mMultiInstanceManager,
                        mSnackbarManager,
                        mDesktopWindowStateManager,
                        mShareDelegateSupplier,
                        mDataSharingTabManager,
                        mIsVerticalTabsActiveSupplier,
                        widthSupplier,
                        /* canActivateTabLayoutToggleMenuSupplier= */ null,
                        mTabHoverCardViewStub,
                        mTabContentManagerSupplier,
                        mUndoBarThrottle);

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
                .onRailCollapseStateChangeRequestedByUser(RailCollapseState.COLLAPSED);
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
                .onRailCollapseStateChangeRequestedByUser(RailCollapseState.EXPANDED);
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
                .onRailCollapseStateChangeRequestedByUser(anyInt());

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
        mCoordinator.setRailCollapseState(RailCollapseState.COLLAPSED);

        View containerView = mCoordinator.getView();
        containerView.layout(0, 0, 200, 500);

        // 1. Mouse hover inside -> requests EXPANDED_FOR_HOVERING.
        MotionEvent hoverEnter =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 50f, 50f, 0);
        hoverEnter.setSource(InputDevice.SOURCE_MOUSE);
        containerView.dispatchGenericMotionEvent(hoverEnter);
        verify(mMockRailCollapseListener)
                .onRailCollapseStateChangeRequestedByUser(RailCollapseState.EXPANDED_FOR_HOVERING);

        // 2. Mouse hover exit (outside container bounds) -> requests COLLAPSED.
        mCoordinator.setRailCollapseState(RailCollapseState.EXPANDED_FOR_HOVERING);
        MotionEvent hoverExit =
                MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_EXIT, 500f, 50f, 0);
        hoverExit.setSource(InputDevice.SOURCE_MOUSE);
        containerView.dispatchGenericMotionEvent(hoverExit);
        verify(mMockRailCollapseListener)
                .onRailCollapseStateChangeRequestedByUser(RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    public void testHoverCard_ShowAndHide() {
        createCoordinator();
        int tabId = 1;
        Tab tab = prepareMockTab(mMockTab1, tabId);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(tab);
        // Set tab not selected
        when(mTabModelSelector.getCurrentTabId()).thenReturn(tabId + 1);

        TabHoverCardListener hoverListener = mCoordinator.getTabHoverCardListenerForTesting();
        assertNotNull(hoverListener);
        hoverListener.onTabHoverCardStateChanged(tabId, mMockChildView, true);

        verify(mTabHoverCardViewStub).inflate();
        verify(mTabHoverCardView).show(eq(tab), anyFloat(), anyFloat());

        hoverListener.onTabHoverCardStateChanged(tabId, mMockChildView, false);
        verify(mTabHoverCardView).hide();
    }

    @Test
    @SmallTest
    public void testHoverCard_SelectedTab_DoNotShow() {
        createCoordinator();
        int tabId = 1;
        Tab tab = prepareMockTab(mMockTab1, tabId);
        when(mTabModelSelector.getTabById(tabId)).thenReturn(tab);
        // Set tab selected
        when(mTabModelSelector.getCurrentTabId()).thenReturn(tabId);

        TabHoverCardListener hoverListener = mCoordinator.getTabHoverCardListenerForTesting();
        assertNotNull(hoverListener);
        hoverListener.onTabHoverCardStateChanged(tabId, mMockChildView, true);

        verify(mTabHoverCardViewStub, never()).inflate();
        verify(mTabHoverCardView, never()).show(any(), anyFloat(), anyFloat());
    }

    @Test
    @SmallTest
    public void testDynamicSpanCountOnWidthChange() {
        createCoordinator();
        int defaultSpanCount = mCoordinator.getPinnedLayoutManagerForTesting().getSpanCount();
        assertEquals(VerticalTabListCoordinator.DEFAULT_GRID_SPAN_COUNT, defaultSpanCount);

        // Simulate measuring container with a width that fits exactly 2 columns.
        View containerView = mCoordinator.getView();
        int itemWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_width);
        int itemMarginPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_margin_bottom);
        int testWidthPx =
                itemWidthPx * 2
                        + itemMarginPx
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
        int itemWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_width);
        int itemMarginPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_item_margin_bottom);
        int testWidthPx =
                itemWidthPx * 2
                        + itemMarginPx
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
    public void testGroupHeaderDragParam_DefaultDisabled() {
        assertFalse(
                "Group header drag feature parameter should be disabled by default.",
                VerticalTabUtils.isGroupHeaderDragEnabled());
    }

    @Test
    @SmallTest
    public void testGroupHeaderDragParam_EnabledViaOverride() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.GROUP_HEADER_DRAG_PARAM,
                /* testValue= */ true);
        assertTrue(
                "Group header drag feature parameter should be enabled when overrideParam is set to"
                        + " true.",
                VerticalTabUtils.isGroupHeaderDragEnabled());
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
    public void testGroupHeaderDragOut_DisabledParam() {
        createCoordinator();
        Token tabGroupId = new Token(1L, 2L);
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);

        // Group header drag is disabled by default, so onDragOut returns early.
        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler, never())
                .startGroupDragAction(any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testGroupHeaderDragOut_AllTabsInWindow() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.GROUP_HEADER_DRAG_PARAM,
                /* testValue= */ true);

        Token tabGroupId = new Token(1L, 2L);
        Tab tab =
                setupMockTabGroup(
                        TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));
        when(mTabModel.getCount()).thenReturn(1);

        createCoordinator();
        Token groupId = new Token(1L, 2L);
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, groupId);

        // When group contains all tabs in window (1 == 1), drag out is blocked.
        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler, never())
                .startGroupDragAction(any(), any(), any(), any());
    }

    @Test
    @SmallTest
    public void testGroupHeaderDragOut_Success() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.GROUP_HEADER_DRAG_PARAM,
                /* testValue= */ true);

        Token tabGroupId = new Token(1L, 2L);
        Tab tab1 =
                setupMockTabGroup(
                        TAB_ID_1, tabGroupId, List.of(prepareMockTab(mMockTab1, TAB_ID_1)));
        when(mTabModel.getCount()).thenReturn(2);

        createCoordinator();
        PropertyModel model = createTabPropertyModel();
        model.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);

        getOnDragOutListener().onDragOut(createViewHolder(model), /* dX= */ 100f, /* dY= */ 50f);
        verify(mMainTabSwitcherDragHandler)
                .startGroupDragAction(any(), eq(tabGroupId), any(), any());
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

        mCoordinator =
                new VerticalTabListCoordinator(
                        mActivity,
                        mTabModelSelector,
                        mProfile,
                        mVerticalTabsActionDelegate,
                        mWindowAndroid,
                        mMultiInstanceManager,
                        mSnackbarManager,
                        mDesktopWindowStateManager,
                        mShareDelegateSupplier,
                        mDataSharingTabManager,
                        mIsVerticalTabsActiveSupplier,
                        mVerticalTabsWidthSupplier,
                        /* canActivateTabLayoutToggleMenuSupplier= */ null,
                        mTabHoverCardViewStub,
                        mTabContentManagerSupplier,
                        mUndoBarThrottle);

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
                                new PropertyKey[] {TabProperties.TAB_ACTION_STATE}))
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
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                new SimpleRecyclerViewAdapter.ViewHolder(new View(mActivity), /* binder= */ null);
        viewHolder.model = model;
        return viewHolder;
    }

    /** Helper to wire mock tab group data into {@link TabModel}. */
    private Tab setupMockTabGroup(int repTabId, Token groupId, List<Tab> tabsInGroup) {
        Tab repTab = tabsInGroup.get(0);
        when(repTab.getTabGroupId()).thenReturn(groupId);
        for (Tab tab : tabsInGroup) {
            when(mTabModel.getTabById(tab.getId())).thenReturn(tab);
            when(mTabModel.isTabInTabGroup(tab)).thenReturn(true);
            when(mTabModel.getRelatedTabList(tab.getId())).thenReturn(tabsInGroup);
        }
        when(mTabModel.getTabsInGroup(groupId)).thenReturn(tabsInGroup);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(repTab));
        when(mTabModel.getGroupLastShownTabId(groupId)).thenReturn(repTabId);
        return repTab;
    }
}
