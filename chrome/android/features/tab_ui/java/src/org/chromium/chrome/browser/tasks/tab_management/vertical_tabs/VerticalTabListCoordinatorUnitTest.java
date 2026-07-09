// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
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

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
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
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
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
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.RectProvider;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link VerticalTabListCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.DATA_SHARING,
    ChromeFeatureList.DATA_SHARING_JOIN_ONLY,
    ChromeFeatureList.ANDROID_CONTEXT_MENU_NEW_ACTIONS,
    ChromeFeatureList.TASK_MANAGER_CLANK,
    TabGroupsFeatureMap.UPDATE_TAB_GROUP_COLORS
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
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;

    private Activity mActivity;
    private final SettableMonotonicObservableSupplier<ShareDelegate> mShareDelegateSupplier =
            ObservableSuppliers.createMonotonic(mShareDelegate);
    private final SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier =
            ObservableSuppliers.createNonNull(false);
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
        ServiceStatus serviceStatus = mock(ServiceStatus.class);
        when(mCollaborationService.getServiceStatus()).thenReturn(serviceStatus);
        when(serviceStatus.isAllowedToJoin()).thenReturn(false);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
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

    private void createCoordinator() {
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
                        mIsVerticalTabsActiveSupplier);
    }

    private Tab prepareMockTab(int id) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.isInitialized()).thenReturn(true);
        when(tab.getTitle()).thenReturn("Tab " + id);
        GURL gurl = new GURL("https://google.com");
        when(tab.getUrl()).thenReturn(gurl);
        return tab;
    }

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

    private TabListRecyclerView setupMockRecyclerViewWithTab(int tabId) {
        // Mock the backend model.
        Tab mockTab = prepareMockTab(tabId);
        when(mTabModel.getTabById(tabId)).thenReturn(mockTab);
        when(mTabModel.getRelatedTabList(tabId)).thenReturn(Collections.singletonList(mockTab));

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
        assertEquals(4, pinnedLayoutManager.getSpanCount());

        assertNotNull(mSelectorObserverCaptor.getValue());
        verify(mTabModelSelector).addObserver(mSelectorObserverCaptor.getValue());
    }

    @Test
    @SmallTest
    public void testPinnedTabsVisibility() {
        // Set up the tab model with a pinned tab.
        Tab pinnedTab = prepareMockTab(123);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(pinnedTab));
        when(mTabModel.iterator()).thenReturn(List.of(pinnedTab).iterator());
        when(mTabModel.getTabById(123)).thenReturn(pinnedTab);
        when(mTabModel.getCount()).thenReturn(1);
        when(mTabModel.getTabAt(0)).thenReturn(pinnedTab);

        createCoordinator();

        // Verify the pinned tabs RecyclerView becomes visible when it contains elements.
        ViewGroup view = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView pinnedRecyclerView = view.findViewById(R.id.pinned_tabs_recycler_view);
        assertNotNull(pinnedRecyclerView);
        assertEquals(View.VISIBLE, pinnedRecyclerView.getVisibility());

        // Swap to an empty tab model.
        TabModel emptyTabModel = mock(TabModel.class);
        when(emptyTabModel.getProfile()).thenReturn(mProfile);
        when(emptyTabModel.isTabModelRestored()).thenReturn(true);
        when(emptyTabModel.getRepresentativeTabList()).thenReturn(Collections.emptyList());
        when(emptyTabModel.iterator()).thenReturn(Collections.emptyIterator());

        mCurrentTabModelSupplier.set(emptyTabModel);

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
    }

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

    @Test
    @SmallTest
    public void testVTEmptySpaceLongPress_LaunchesContextMenu() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertNotNull(recyclerView);

        // Ensure the context menu coordinator reference starts fresh as null.
        assertNull(mCoordinator.getTabStripContextMenuCoordinatorForTesting());

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

    @Test
    @SmallTest
    public void testVTEmptySpaceRightClick_LaunchesContextMenu() {
        createCoordinator();
        TabListRecyclerView recyclerView =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);

        assertNotNull(recyclerView);
        assertNull(mCoordinator.getTabStripContextMenuCoordinatorForTesting());

        // Simulate a mouse right-click.
        recyclerView.performContextClick();

        assertNotNull(
                "Right click on empty space should instantiate the context menu coordinator.",
                mCoordinator.getTabStripContextMenuCoordinatorForTesting());
    }

    @Test
    @SmallTest
    public void testTabItemInteraction_WithTouchPointLaunchesMenuAtPreciseLocation() {
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(456);

        View mockChildView = mock(View.class);
        doReturn(mockChildView).when(recyclerViewSpy).findChildViewUnder(150f, 250f);
        doReturn(0).when(recyclerViewSpy).getChildAdapterPosition(mockChildView);

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
        // Mock the backend model.
        Tab mockTab = prepareMockTab(123);
        Token tabGroupId = new Token(1L, 2L);
        when(mockTab.getTabGroupId()).thenReturn(tabGroupId);
        when(mTabModel.getTabById(123)).thenReturn(mockTab);
        when(mTabModel.getRelatedTabList(123)).thenReturn(Collections.singletonList(mockTab));

        createCoordinator();
        ViewGroup container = (ViewGroup) mCoordinator.getView();
        TabListRecyclerView realRecyclerView = container.findViewById(R.id.tab_list_recycler_view);
        assertNotNull(realRecyclerView);

        // Wrap the real inflated Recycler View in a spy.
        TabListRecyclerView recyclerViewSpy = spy(realRecyclerView);
        assertNull(mCoordinator.getTabGroupContextMenuCoordinatorForTesting());

        // Inject the mock coordinator here before calling #handleContextMenuInteractionForTesting
        // so that we can verify the rect captor later in this test.
        mCoordinator.setTabGroupContextMenuCoordinatorForTesting(mTabGroupContextMenuCoordinator);

        SimpleRecyclerViewAdapter adapter =
                (SimpleRecyclerViewAdapter) realRecyclerView.getAdapter();
        PropertyModel groupPropertyModel = new PropertyModel(TabProperties.ALL_KEYS_VERTICAL_TAB);
        groupPropertyModel.set(TabProperties.TAB_ID, 123);
        groupPropertyModel.set(TabProperties.TAB_GROUP_HEADER_ID, tabGroupId);

        // This forces item.type == UiType.TAB but inner header id is not null, mimicking the real
        // list behavior.
        adapter.getModelList().add(new MVCListAdapter.ListItem(UiType.TAB, groupPropertyModel));

        assertEquals(
                "The adapter lookup should resolve this list item row layout as a TAB_GROUP type.",
                UiType.TAB_GROUP,
                adapter.getItemViewType(0));

        // Create a mock View layout box (child of the recycler view) that renders the tab card on
        // the screen.
        View mockChildView = mock(View.class);
        when(mockChildView.getWidth()).thenReturn(400);
        when(mockChildView.getHeight()).thenReturn(80);

        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 40;
                            pos[1] = 120;
                            return null;
                        })
                .when(mockChildView)
                .getLocationInWindow(any());

        doReturn(mockChildView).when(recyclerViewSpy).findChildViewUnder(200f, 150f);
        doReturn(0).when(recyclerViewSpy).getChildAdapterPosition(mockChildView);

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
        TabListRecyclerView recyclerViewSpy = setupMockRecyclerViewWithTab(456);
        assertNull(mCoordinator.getTabContextMenuCoordinatorForTesting());

        // Create a mock View layout box (child of the recycler view) that renders the tab card on
        // the screen.
        View mockChildView = mock(View.class);
        when(mockChildView.getWidth()).thenReturn(300);
        when(mockChildView.getHeight()).thenReturn(100);

        doAnswer(
                        invocation -> {
                            int[] pos = invocation.getArgument(0);
                            pos[0] = 50;
                            pos[1] = 100;
                            return null;
                        })
                .when(mockChildView)
                .getLocationInWindow(any());

        doReturn(mockChildView).when(recyclerViewSpy).findChildViewUnder(150f, 250f);
        doReturn(0).when(recyclerViewSpy).getChildAdapterPosition(mockChildView);

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
    public void testDestroy_RemovesSupplierObserver() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        mCoordinator.destroy();

        TabModel newTabModel = mock(TabModel.class);
        when(newTabModel.getProfile()).thenReturn(mProfile);
        when(newTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(789);
        when(newTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));

        mCurrentTabModelSupplier.set(newTabModel);
        assertEquals(0, adapter.getModelList().size());
    }

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
        Tab tab123 = prepareMockTab(123);
        Token tabGroupId123 = new Token(1L, 2L);
        when(tab123.getTabGroupId()).thenReturn(tabGroupId123);

        when(mTabModel.getTabById(anyInt())).thenReturn(tab123);
        when(mTabModel.getTabsInGroup(tabGroupId123)).thenReturn(List.of(tab123));
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab123));
        when(mTabModel.getGroupLastShownTabId(tabGroupId123)).thenReturn(123);
        when(mTabModel.getRelatedTabList(123)).thenReturn(List.of(tab123));
        when(mTabModel.isTabInTabGroup(tab123)).thenReturn(true);

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

        mCoordinator.toggleTabGroupExpansion(123);
        assertFalse(
                "Tab group should be expanded (IS_COLLAPSED = false) after first toggle click.",
                groupModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_Collapse() {
        Tab tab123 = prepareMockTab(123);
        Token tabGroupId123 = new Token(1L, 2L);
        when(tab123.getTabGroupId()).thenReturn(tabGroupId123);

        when(mTabModel.getTabById(anyInt())).thenReturn(tab123);
        when(mTabModel.getTabsInGroup(tabGroupId123)).thenReturn(List.of(tab123));
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab123));
        when(mTabModel.getGroupLastShownTabId(tabGroupId123)).thenReturn(123);
        when(mTabModel.getRelatedTabList(123)).thenReturn(List.of(tab123));
        when(mTabModel.isTabInTabGroup(tab123)).thenReturn(true);

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

        mCoordinator.toggleTabGroupExpansion(123);
        assertTrue(
                "Tab group should be collapsed (IS_COLLAPSED = true) after toggle click from"
                        + " expanded state.",
                groupModel.get(TabProperties.IS_COLLAPSED));
        assertEquals(1, adapter.getModelList().size());
    }

    @Test
    @SmallTest
    public void testToggleTabGroupExpansion_RegularTabCannotToggle() {
        Tab tab456 = prepareMockTab(456);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab456);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab456));
        when(mTabModel.isTabInTabGroup(tab456)).thenReturn(false);

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        mCoordinator.toggleTabGroupExpansion(456);
        assertFalse(tabModel.get(TabProperties.IS_COLLAPSED));
    }

    @Test
    @SmallTest
    public void testTabSelection_SelectsTabInSelector() {
        Tab tab456 = prepareMockTab(456);
        when(mTabModelSelector.getModelForTabId(456)).thenReturn(mTabModel);
        when(mTabModel.getTabById(anyInt())).thenReturn(tab456);
        when(mTabModel.indexOf(tab456)).thenReturn(0);
        when(mTabModel.getRepresentativeTabList()).thenReturn(List.of(tab456));
        when(mTabModel.isTabInTabGroup(tab456)).thenReturn(false);
        when(mTabModel.iterator()).thenReturn(List.of(tab456).iterator());

        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        assertEquals(1, adapter.getModelList().size());
        PropertyModel tabModel = adapter.getModelList().get(0).model;

        TabActionListener clickListener = tabModel.get(TabProperties.TAB_CLICK_LISTENER);
        assertNotNull("Tab click listener should be bound to model", clickListener);
        clickListener.run(null, 456, null);

        verify(mTabModel).setIndex(0, TabSelectionType.FROM_USER);
    }

    @Test
    @SmallTest
    public void testTabModelSwap_ResetsTabs() {
        createCoordinator();
        TabListRecyclerView recycler =
                mCoordinator.getView().findViewById(R.id.tab_list_recycler_view);
        SimpleRecyclerViewAdapter adapter = (SimpleRecyclerViewAdapter) recycler.getAdapter();

        TabModel newTabModel = mock(TabModel.class);
        when(newTabModel.getProfile()).thenReturn(mProfile);
        when(newTabModel.isTabModelRestored()).thenReturn(true);
        Tab newTab = prepareMockTab(789);
        when(newTabModel.getRepresentativeTabList()).thenReturn(List.of(newTab));
        when(newTabModel.iterator()).thenReturn(List.of(newTab).iterator());
        when(newTabModel.getTabById(789)).thenReturn(newTab);

        mCurrentTabModelSupplier.set(newTabModel);

        assertEquals(1, adapter.getModelList().size());
        assertEquals(789, adapter.getModelList().get(0).model.get(TabProperties.TAB_ID));
    }

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
    public void testTabSearchButtonClick() {
        createCoordinator();
        ImageButton tabSearchButton = mCoordinator.getView().findViewById(R.id.tab_search_button);
        assertNotNull(tabSearchButton);
        tabSearchButton.performClick();
        verify(mVerticalTabsActionDelegate).openTabSearch();
    }

    @Test
    @SmallTest
    public void testNewTabButtonClick() {
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        createCoordinator();
        ImageButton newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        newTabButton.performClick();
        verify(mTabModel).commitAllTabClosures();
        verify(mTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    @Test
    @SmallTest
    public void testNewTabButtonClick_Incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        createCoordinator();
        ImageButton newTabButton = mCoordinator.getView().findViewById(R.id.new_tab_button);
        assertNotNull(newTabButton);
        newTabButton.performClick();
        verify(mTabModel, never()).commitAllTabClosures();
        verify(mTabCreator).launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    @Test
    @SmallTest
    public void testSpacerViewVisibilityInDesktopWindow() {
        // Mock DesktopWindowStateManager to say we are in desktop windowing mode.
        var appHeaderState =
                new AppHeaderState(new Rect(0, 0, 100, 100), new Rect(10, 0, 80, 100), true);
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
    public void testScrollActiveTabIntoView_PinnedTab_NoScroll() {
        int pinnedTabId = 111;
        Tab pinnedTab = prepareMockTab(pinnedTabId);
        when(pinnedTab.getIsPinned()).thenReturn(true);
        when(mTabModel.getTabById(pinnedTabId)).thenReturn(pinnedTab);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(pinnedTabId);

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
        int regTabId = 222;
        Tab regTab = prepareMockTab(regTabId);
        when(regTab.getIsPinned()).thenReturn(false);
        when(mTabModel.getTabById(regTabId)).thenReturn(regTab);
        when(mTabModelSelector.getCurrentTabId()).thenReturn(regTabId);

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
                        .with(TabProperties.TAB_ID, regTabId)
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
}
