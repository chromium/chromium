// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipData.Item;
import android.content.ClipDescription;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.PointF;
import android.net.Uri;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.text.format.DateUtils;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.Token;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelper;
import org.chromium.chrome.browser.compositor.overlays.strip.TestTabModel;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabStripDragHandler.TabDragShadowBuilder;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeMultiTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.widget.ToastManager;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Tests for {@link TabStripDragHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp", sdk = VERSION_CODES.S, shadows = ShadowToast.class)
public class TabStripDragHandlerTest {

    private static final int CURR_INSTANCE_ID = 100;
    private static final int ANOTHER_INSTANCE_ID = 200;
    private static final int TAB_ID = 1;
    private static final int TAB_ID_NOT_DRAGGED = 2;
    private static final int GROUPED_TAB_ID_1 = 3;
    private static final int GROUPED_TAB_ID_2 = 4;
    private static final int TAB_ID_2 = 5;
    private static final float VIEW_WIDTH = 5f;
    private static final int TAB_INDEX = 2;
    private static final float POS_X = 20f;
    private static final float DRAG_MOVE_DISTANCE = 5f;
    private static final String[] SUPPORTED_TAB_MIME_TYPES = {"chrome/tab"};
    private static final String[] SUPPORTED_GROUP_MIME_TYPES = {"chrome/tab-group"};
    private static final Token TAB_GROUP_ID = new Token(2L, 2L);
    private float mPosY;
    @Rule public MockitoRule mMockitoProcessorRule = MockitoJUnit.rule();
    @Mock private DragAndDropDelegate mDragDropDelegate;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TestTabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WeakReference<Context> mWeakReferenceContext;
    @Mock private MultiWindowUtils mMultiWindowUtils;
    @Mock private ObservableSupplier<Integer> mTabStripHeightSupplier;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;

    // Instances that differ for source and destination for invocations and verifications.
    @Mock private StripLayoutHelper mSourceStripLayoutHelper;
    @Mock private StripLayoutHelper mDestStripLayoutHelper;
    @Mock private MultiInstanceManager mSourceMultiInstanceManager;
    @Mock private MultiInstanceManager mDestMultiInstanceManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private ObservableSupplierImpl<TabGroupModelFilter> mTabGroupModelFilterSupplier;
    private TabStripDragHandler mSourceInstance;
    private TabStripDragHandler mDestInstance;

    private Activity mActivity;
    private ViewGroup mTabsToolbarView;
    private Tab mTabBeingDragged;
    private Tab mTabBeingDragged2;
    private Tab mGroupedTab1;
    private Tab mGroupedTab2;
    private final ArrayList<Tab> mTabGroupBeingDragged = new ArrayList();
    private final ArrayList<Tab> mTabsBeingDragged = new ArrayList();
    private TabGroupMetadata mTabGroupMetadata;
    private static final PointF DRAG_START_POINT = new PointF(250, 0);
    private static final float TAB_POSITION_X = 200f;
    private int mTabStripHeight;
    private final Context mContext = ContextUtils.getApplicationContext();
    private boolean mTabStripVisible;
    private SharedPreferencesManager mSharedPreferencesManager;
    private UserActionTester mUserActionTest;

    /** Resets the environment before each test. */
    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void beforeTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTabStripHeight = mActivity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        mPosY = mTabStripHeight - 2 * DRAG_MOVE_DISTANCE;
        mTabStripVisible = true;

        // Create and spy on a simulated tab view.
        mTabsToolbarView = new FrameLayout(mActivity);
        mTabsToolbarView.setLayoutParams(new MarginLayoutParams(150, 50));

        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        mTabBeingDragged = MockTab.createAndInitialize(TAB_ID, mProfile);
        mTabBeingDragged2 = MockTab.createAndInitialize(TAB_ID_2, mProfile);

        // Setup tab group being dragged.
        setupTabGroup(/* isGroupShared= */ false);

        // Setup multi-tab drag.
        mTabsBeingDragged.add(mTabBeingDragged);
        mTabsBeingDragged.add(mTabBeingDragged2);

        when(mSourceMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID);
        when(mDestMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);
        when(mDragDropDelegate.startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class)))
                .thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mWindowAndroid.getContext()).thenReturn(mWeakReferenceContext);
        when(mWeakReferenceContext.get()).thenReturn(mContext);

        when(mMultiWindowUtils.hasAtMostOneTabWithHomepageEnabled(any())).thenReturn(false);
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(true);
        MultiWindowUtils.setInstanceForTesting(mMultiWindowUtils);
        MultiWindowTestUtils.enableMultiInstance();

        when(mTabStripHeightSupplier.get()).thenReturn(mTabStripHeight);

        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);

        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTabBeingDragged);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilterProvider.getCurrentTabGroupModelFilterSupplier())
                .thenReturn(mTabGroupModelFilterSupplier);
        when(mTabGroupModelFilterSupplier.get()).thenReturn(mTabGroupModelFilter);

        Supplier<Boolean> isAppInDesktopWindow =
                () -> AppHeaderUtils.isAppInDesktopWindow(mDesktopWindowStateManager);

        Supplier<Activity> activitySupplier = () -> mWindowAndroid.getActivity().get();

        mSourceInstance =
                new TabStripDragHandler(
                        mActivity,
                        () -> mSourceStripLayoutHelper,
                        () -> mTabStripVisible,
                        new ObservableSupplierImpl<>(mTabContentManager),
                        new ObservableSupplierImpl<>(mLayerTitleCache),
                        mSourceMultiInstanceManager,
                        mDragDropDelegate,
                        mBrowserControlsStateProvider,
                        activitySupplier,
                        mTabStripHeightSupplier,
                        isAppInDesktopWindow);
        mSourceInstance.setTabModelSelector(mTabModelSelector);

        mDestInstance =
                new TabStripDragHandler(
                        mActivity,
                        () -> mDestStripLayoutHelper,
                        () -> mTabStripVisible,
                        new ObservableSupplierImpl<>(mTabContentManager),
                        new ObservableSupplierImpl<>(mLayerTitleCache),
                        mDestMultiInstanceManager,
                        mDragDropDelegate,
                        mBrowserControlsStateProvider,
                        activitySupplier,
                        mTabStripHeightSupplier,
                        isAppInDesktopWindow);
        mDestInstance.setTabModelSelector(mTabModelSelector);

        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(false);

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mUserActionTest = new UserActionTester();

        AppHeaderUtils.setAppInDesktopWindowForTesting(false);
    }

    @After
    public void cleanup() {
        if (DragDropGlobalState.hasValue()) {
            DragDropGlobalState.clearForTesting();
        }
        ShadowToast.reset();
        ToastManager.resetForTesting();
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS);
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_COUNT);
    }

    @Test
    public void test_startTabDragAction_withTabDragDropFF_returnsTrueForValidTab() {
        DeviceInfo.setIsXrForTesting(true);
        // Act and verify.
        boolean res =
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH);
        assertTrue("startTabDragAction returned false.", res);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
        assertTrue(
                "Global state instanceId not set.",
                DragDropGlobalState.getForTesting().isDragSourceInstance(CURR_INSTANCE_ID));
        assertEquals(
                "Global state tabBeingDragged not set.",
                mTabBeingDragged,
                ((ChromeTabDropDataAndroid) DragDropGlobalState.getForTesting().getData()).tab);
        assertNotNull(
                "Shadow view should not be null for XR Device.",
                mSourceInstance.getShadowViewForTesting());
    }

    @Test
    public void test_startTabDragAction_withTabLinkDragDropFF_returnsTrueForValidTab() {
        // Act and verify.
        boolean res =
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH);
        assertTrue("startTabDragAction returned false.", res);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
        assertTrue(
                "Global state instanceId not set.",
                DragDropGlobalState.getForTesting().isDragSourceInstance(CURR_INSTANCE_ID));
        assertEquals(
                "Global state tabBeingDragged not set.",
                mTabBeingDragged,
                ((ChromeTabDropDataAndroid) DragDropGlobalState.getForTesting().getData()).tab);
        assertNotNull(
                "Shadow view is unexpectedly null.", mSourceInstance.getShadowViewForTesting());
    }

    @Test
    public void test_startTabDragAction_exceptionForInvalidTab() {
        assertThrows(
                AssertionError.class,
                () ->
                        mSourceInstance.startTabDragAction(
                                mTabsToolbarView,
                                null,
                                DRAG_START_POINT,
                                TAB_POSITION_X,
                                VIEW_WIDTH));
    }

    @Test
    public void test_startTabDragAction_returnFalseForDragInProgress() {
        // Set state.
        DragDropGlobalState.store(CURR_INSTANCE_ID, mock(ChromeDropDataAndroid.class), null);

        assertFalse(
                "Tab drag should not start",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startTabDragAction_withHasOneTabWithHomepage_ReturnsFalse() {
        when(mMultiWindowUtils.hasAtMostOneTabWithHomepageEnabled(any())).thenReturn(true);
        assertFalse(
                "Should not startTabDragAction since last tab with homepage enabled.",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startMultiTabDragAction_withHasAllTabsSelectedWithHomepage_ReturnsFalse() {
        when(mMultiWindowUtils.hasAllTabsSelectedWithHomepageEnabled(any())).thenReturn(true);
        assertFalse(
                "Should not startTabDragAction since last tab with homepage enabled.",
                mSourceInstance.startMultiTabDragAction(
                        mTabsToolbarView,
                        mTabsBeingDragged,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startGroupDragAction_withHasOneTabGroupWithHomepage_ReturnsFalse() {
        when(mMultiWindowUtils.hasAtMostOneTabGroupWithHomepageEnabled(any(), any()))
                .thenReturn(true);
        assertFalse(
                "Should not startGroupDragAction since last tab group with homepage enabled.",
                mSourceInstance.startGroupDragAction(
                        mTabsToolbarView,
                        TAB_GROUP_ID,
                        /* isGroupShared= */ false,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startTabDragAction_releaseTrackerTokenWhenDragDidNotStart() {
        when(mDragDropDelegate.startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class)))
                .thenReturn(false);

        assertFalse(
                "Tab drag should not start",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
        assertFalse("Global state should not be set", DragDropGlobalState.hasValue());
    }

    @Test
    public void test_startTabDragAction_returnFalseForNonSplitScreen() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);

        // verify.
        assertFalse(
                "Tab drag should not start",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startTabDragAction_FullScreenWithMultipleTabs() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);

        // Verify.
        callAndVerifyAllowDragToCreateInstance(true);
    }

    @Test
    public void test_startMultiTabDragAction_FullScreenWithMultipleTabs() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(3);
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);

        // Verify.
        callAndVerifyAllowMultiTabDragToCreateInstance(true);
    }

    @Test
    public void test_startTabDragAction_FullScreenWithOneTab() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);

        // Verify.
        assertFalse(
                "Tab drag should not start.",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startMultiTabDragAction_FullScreenWithAllTabsSelected() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);

        // Verify.
        assertFalse(
                "Tab drag should not start.",
                mSourceInstance.startMultiTabDragAction(
                        mTabsToolbarView,
                        mTabsBeingDragged,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
    }

    @Test
    public void test_startTabDragAction_FullScreenWithMaxChromeInstances() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Verify.
        callAndVerifyAllowDragToCreateInstance(false);
    }

    @Test
    public void test_startMultiTabDragAction_FullScreenWithMaxChromeInstances() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(3);
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Verify.
        callAndVerifyAllowMultiTabDragToCreateInstance(false);
    }

    @Test
    public void test_startTabDragAction_FullScreenWithMaxInstanceAllowlistedOEM() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");

        callAndVerifyAllowDragToCreateInstance(true);
    }

    @Test
    public void test_startMultiTabDragAction_FullScreenWithMaxInstanceAllowlistedOEM() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(3);
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");

        callAndVerifyAllowMultiTabDragToCreateInstance(true);
    }

    @Test
    public void test_startTabDragAction_SplitScreenWithMaxChromeInstances() {
        // Set params.
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Verify.
        callAndVerifyAllowDragToCreateInstance(false);
    }

    @Test
    public void test_startMultiTabDragAction_SplitScreenWithMaxChromeInstances() {
        // Set params.
        when(mTabModelSelector.getTotalTabCount()).thenReturn(3);
        when(mTabModel.getMultiSelectedTabsCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Verify.
        callAndVerifyAllowMultiTabDragToCreateInstance(false);
    }

    @Test
    public void test_onProvideShadowMetrics_WithDesiredStartPosition_ReturnsSuccess() {
        DeviceInfo.setIsXrForTesting(true);
        // Prepare
        final float dragStartXPosition = 480f;
        final PointF dragStartPoint = new PointF(dragStartXPosition, 0f);
        final Resources resources = ContextUtils.getApplicationContext().getResources();
        // Call startDrag to set class variables.
        mSourceInstance.startTabDragAction(
                mTabsToolbarView, mTabBeingDragged, dragStartPoint, TAB_POSITION_X, VIEW_WIDTH);

        View.DragShadowBuilder tabDragShadowBuilder =
                mSourceInstance.createDragShadowBuilder(
                        mTabsToolbarView, dragStartPoint, TAB_POSITION_X);

        // Perform asking the TabDragShadowBuilder what is the anchor point.
        Point dragSize = new Point(0, 0);
        Point dragAnchor = new Point(0, 0);
        tabDragShadowBuilder.onProvideShadowMetrics(dragSize, dragAnchor);

        // Validate anchor.
        assertEquals(
                "Drag shadow x position is incorrect.",
                Math.round(
                        dragStartXPosition
                                - TAB_POSITION_X * resources.getDisplayMetrics().density),
                dragAnchor.x);
        assertEquals(
                "Drag shadow y position is incorrect.",
                Math.round(
                        resources.getDimension(R.dimen.tab_grid_card_header_height) / 2
                                + resources.getDimension(R.dimen.tab_grid_card_margin)),
                dragAnchor.y);
    }

    @Test
    public void test_onProvideShadowMetrics_withTabLinkDragDropFF() {
        // Call startDrag to set class variables.
        mSourceInstance.startTabDragAction(
                mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT, TAB_POSITION_X, VIEW_WIDTH);
        TabDragShadowBuilder tabDragShadowBuilder =
                (TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder();
        Resources resources = ContextUtils.getApplicationContext().getResources();

        // Perform asking the TabDragShadowBuilder what is the anchor point.
        Point dragSize = new Point(0, 0);
        Point dragAnchor = new Point(0, 0);
        tabDragShadowBuilder.onProvideShadowMetrics(dragSize, dragAnchor);

        // Validate anchor.
        assertEquals(
                "Drag shadow x position is incorrect.",
                Math.round(
                        DRAG_START_POINT.x
                                - TAB_POSITION_X * resources.getDisplayMetrics().density),
                dragAnchor.x);
        assertEquals(
                "Drag shadow y position is incorrect.",
                Math.round(
                        resources.getDimension(R.dimen.tab_grid_card_header_height) / 2
                                + resources.getDimension(R.dimen.tab_grid_card_margin)),
                dragAnchor.y);
    }

    /**
     * Below tests test end to end tab and tab group drag drop flow for following combinations:
     *
     * <pre>
     * A] drop in tab strip - source instance (ie: reorder within strip).
     * B] drop in toolbar container (but outside of tab strip) - source instance.
     * C] drop outside of toolbar container - not implemented. Should be handled by
     *  {@link ChromeDragAndDropBrowserDelegateUnitTest}.
     * D] Test (A) for drop in destination instance with sub flows:
     *  D.1] drop in current model (i.e. drop incognito tab on incognito strip).
     *  D.2] drop in different model (i.e.: drop standard tab on incognito strip).
     *  D.3] With drag as window FF - hide shadow on drag enter.
     * E] Test (B) for drops in destination instance.
     * F] drag from strip to toolbar container and back to strip, drop in strip (to test re-enter).
     * G] error cases:
     *  G.1] invalid mimetype.
     *  G.2] invalid clip data.
     *  G.3] destination strip is not visible.
     * H] drag start special cases (see crbug.com/374480348):
     *  H.1] drag starts outside of the source strip - we trigger an #onDragExit after 50ms.
     *  H.2] drag starts outside of the source strip - we cancel the runnable after drag enter.
     *  H.3] drag starts outside of the source strip - we cancel the runnable after drag end.
     *  </pre>
     */
    private static final String ONDRAG_TEST_CASES = "";

    /** Test for tab drag {@link #ONDRAG_TEST_CASES} - Scenario A */
    @Test
    public void test_onDrag_dropInStrip_source() {
        doTestOnDragDropInStripSource(/* isGroupDrag= */ false);
    }

    /** Test for tab group drag {@link #ONDRAG_TEST_CASES} - Scenario A */
    @Test
    public void test_onDrag_dropInStrip_source_tabGroup() {
        doTestOnDragDropInStripSource(/* isGroupDrag= */ true);
    }

    /** Test for multi-tab drag {@link #ONDRAG_TEST_CASES} - Scenario A */
    @Test
    public void test_onDrag_dropInStrip_source_multiTab() {
        doTestOnDragDropInStripSource_multiTab();
    }

    /** Test for tab drag {@link #ONDRAG_TEST_CASES} - Scenario B */
    @Test
    public void test_onDrag_dropInToolbarContainer_source() {
        doTestOnDragDropInToolbarContainerSource(/* isGroupDrag= */ false);
    }

    /** Test for tab group drag {@link #ONDRAG_TEST_CASES} - Scenario B */
    @Test
    public void test_onDrag_dropInToolbarContainer_source_tabGroup() {
        doTestOnDragDropInToolbarContainerSource(/* isGroupDrag= */ true);
    }

    /** Test for multi-tab drag {@link #ONDRAG_TEST_CASES} - Scenario B */
    @Test
    public void test_onDrag_dropInToolbarContainer_source_multiTab() {
        doTestOnDragDropInToolbarContainerSource_multiTab();
    }

    @Test
    public void test_onDrag_unhandledDropOutside_maxChromeInstances() {
        doTestUnhandledDropOutsideWithMaxInstances(
                /* isInDesktopWindow= */ false, /* isGroupDrag= */ false);
    }

    @Test
    public void test_onDrag_unhandledDropOutside_maxChromeInstances_desktopWindow() {
        doTestUnhandledDropOutsideWithMaxInstances(
                /* isInDesktopWindow= */ true, /* isGroupDrag= */ false);
    }

    @Test
    public void test_onDrag_unhandledDropOutside_maxChromeInstances_tabGroup() {
        doTestUnhandledDropOutsideWithMaxInstances(
                /* isInDesktopWindow= */ false, /* isGroupDrag= */ true);
    }

    @Test
    public void test_onDrag_unhandledDropOutside_maxChromeInstances_tabGroup_desktopWindow() {
        doTestUnhandledDropOutsideWithMaxInstances(
                /* isInDesktopWindow= */ true, /* isGroupDrag= */ true);
    }

    @Test
    public void test_onDrag_multipleUnhandledDropsOutside_maxChromeInstances() {
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Simulate failures on day 1.
        doTriggerUnhandledDrop(4, /* isGroupDrag= */ false);

        // Force update the count start time saved in SharedPreferences for day 1 to restart count
        // for next day.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS,
                System.currentTimeMillis() - DateUtils.DAY_IN_MILLIS - 1);

        // Simulate a failure on day 2.
        doTriggerUnhandledDrop(1, /* isGroupDrag= */ false);
    }

    @Test
    public void test_onDrag_multipleUnhandledDropsOutside_maxChromeInstances_tabGroup() {
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Simulate failures on day 1.
        doTriggerUnhandledDrop(4, /* isGroupDrag= */ true);

        // Force update the count start time saved in SharedPreferences for day 1 to restart count
        // for next day.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS,
                System.currentTimeMillis() - DateUtils.DAY_IN_MILLIS - 1);

        // Simulate a failure on day 2.
        doTriggerUnhandledDrop(1, /* isGroupDrag= */ true);
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination() {
        doTestDropInStripDestination(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ false,
                /* isGroupShared= */ false,
                /* mhtmlTabTitle= */ null);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination_tabGroup() {
        doTestDropInStripDestination(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ true,
                /* isGroupShared= */ false,
                /* mhtmlTabTitle= */ null);
    }

    /** Test for Shared Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination_sharedTabGroup() {
        setupTabGroup(/* isGroupShared= */ true);
        doTestDropInStripDestination(
                /* isInDesktopWindow= */ true,
                /* isGroupDrag= */ true,
                /* isGroupShared= */ true,
                /* mhtmlTabTitle= */ null);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_hasMhtmlTab_destination_tabGroup() {
        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.TabGroup.FromStrip.Result",
                                DragDropResult.IGNORED_MHTML_TAB);
        HistogramWatcher histogramExpectation = builder.build();
        String url = "file:///example.mhtml";
        Uri uri = Uri.parse(url);
        GURL gurl = new GURL(uri.toString());
        String mhtmlTabTitle = "mhtmlTab";
        doReturn(gurl).when(mGroupedTab1).getUrl();
        doReturn(mhtmlTabTitle).when(mGroupedTab1).getTitle();
        mTabGroupMetadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        mTabGroupModelFilter,
                        mTabGroupBeingDragged,
                        /* sourceWindowIndex= */ -1,
                        mGroupedTab1.getId(),
                        /* isGroupShared= */ false);
        doTestDropInStripDestination(
                /* isInDesktopWindow= */ false,
                /* isGroupDrag= */ true,
                /* isGroupShared= */ false,
                /* mhtmlTabTitle= */ mhtmlTabTitle);
        // Verify histogram recorded for ignored mhtml tab group .
        histogramExpectation.assertExpected();
    }

    /** Test for Desktop Window {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination_desktopWindow() {
        doTestDropInStripDestination(
                /* isInDesktopWindow= */ true,
                /* isGroupDrag= */ false,
                /* isGroupShared= */ false,
                /* mhtmlTabTitle= */ null);
    }

    /** Test for Tab Group Drag in Desktop Window {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination_tabGroup_desktopWindow() {
        doTestDropInStripDestination(
                /* isInDesktopWindow= */ true,
                /* isGroupDrag= */ true,
                /* isGroupShared= */ false,
                /* mhtmlTabTitle= */ null);
    }

    /** Test for Multi-Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination_multiTab() {
        doTestDropInStripDestinationForMultiTab();
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void test_onDrag_dropInStrip_differentModel_destination() {
        doTestDropInDestinationDifferentModel(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void test_onDrag_dropInStrip_differentModel_destination_tabGroup() {
        doTestDropInDestinationDifferentModel(/* isGroupDrag= */ true);
    }

    /** Test for Multi-Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void test_onDrag_dropInStrip_differentModel_destination_multiTab() {
        doTestDropInDestinationDifferentModel_multiTab();
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void test_onDrag_dropInStrip_differentModel_fail_incognitoAsNewWindow() {
        doTestDropInDestinationDifferentModel_fail(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void test_onDrag_dropInStrip_differentModel_fail_tabGroup_incognitoAsNewWindow() {
        doTestDropInDestinationDifferentModel_fail(/* isGroupDrag= */ true);
    }

    /** Test for Multi-Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void test_onDrag_dropInStrip_differentModel_fail_multiTab_incognitoAsNewWindow() {
        doTestDropInDestinationDifferentModel_fail_multiTab();
    }

    /**
     * Test for {@link #ONDRAG_TEST_CASES} - Scenario D.3 - XR-specific flow currently does not
     * support moving tab groups.
     */
    @Test
    public void test_onDrag_dropInStrip_withDragAsWindowFF_destination() {
        DeviceInfo.setIsXrForTesting(true);
        new DragEventInvoker(DragType.SINGLE_TAB, /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(false)
                .drop(mDestInstance)
                .end(false);
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario E */
    @Test
    public void test_onDrag_dropInToolbarContainer_destination() {
        doTestDropInDestinationToolbarContainer(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario E */
    @Test
    public void test_onDrag_dropInToolbarContainer_destination_tabGroup() {
        doTestDropInDestinationToolbarContainer(/* isGroupDrag= */ true);
    }

    /** Test for Multi-Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario E */
    @Test
    public void test_onDrag_dropInToolbarContainer_destination_multiTab() {
        doTestDropInDestinationToolbarContainer_multiTab();
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario F */
    @Test
    public void test_onDrag_exitIntoToolbarAndRenterStripAndDrop_source() {
        doTestExitIntoSourceToolbarAndRenterStripAndDrop(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario F */
    @Test
    public void test_onDrag_exitIntoToolbarAndRenterStripAndDrop_source_tabGroup() {
        doTestExitIntoSourceToolbarAndRenterStripAndDrop(/* isGroupDrag= */ true);
    }

    /** Test for Multi-Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario F */
    @Test
    public void test_onDrag_exitIntoToolbarAndRenterStripAndDrop_source_multiTab() {
        doTestExitIntoSourceToolbarAndRenterStripAndDrop_multiTab();
    }

    /** Test for Tab and Tab Group Drag{@link #ONDRAG_TEST_CASES} - Scenario G.1 */
    @Test
    public void test_onDrag_invalidMimeType() {
        // Set state.
        DragDropGlobalState.store(CURR_INSTANCE_ID, mock(ChromeDropDataAndroid.class), null);

        DragEvent event = mock(DragEvent.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(event.getX()).thenReturn(POS_X);
        when(event.getY()).thenReturn(mPosY);
        when(event.getClipDescription())
                .thenReturn(new ClipDescription("", new String[] {"some_value"}));

        assertFalse(mSourceInstance.onDrag(mTabsToolbarView, event));
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario G.2 */
    @Test
    public void test_onDrag_invalidClipData() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder().expectNoRecords("Android.DragDrop.Tab.Type").build();
        doTestOnDragInvalidClipData(/* isGroupDrag= */ false);
        histogramExpectation.assertExpected();
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario G.2 */
    @Test
    public void test_onDrag_invalidClipData_tabGroup() {
        doTestOnDragInvalidClipData(/* isGroupDrag= */ true);
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario G.3 */
    @Test
    public void test_onDrag_destinationStripNotVisible() {
        doTestOnDragDestinationStripNotVisible(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario G.3 */
    @Test
    public void test_onDrag_destinationStripNotVisible_tabGroup() {
        doTestOnDragDestinationStripNotVisible(/* isGroupDrag= */ true);
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario H.1 */
    @Test
    public void test_onDrag_startsOutsideSourceStrip_runnableSuccess() {
        doTestOnDragStartsOutsideSourceStripRunnableSuccess(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag{@link #ONDRAG_TEST_CASES} - Scenario H.1 */
    @Test
    public void test_onDrag_startsOutsideSourceStrip_runnableSuccess_tabGroup() {
        doTestOnDragStartsOutsideSourceStripRunnableSuccess(/* isGroupDrag= */ true);
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario H.2 */
    @Test
    public void test_onDrag_startsOutsideSourceStrip_runnableCancelledOnEnter() {
        doTestOnDragStartsOutsideSourceStripRunnableCancelledOnEnter(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario H.2 */
    @Test
    public void test_onDrag_startsOutsideSourceStrip_runnableCancelledOnEnter_tabGroup() {
        doTestOnDragStartsOutsideSourceStripRunnableCancelledOnEnter(/* isGroupDrag= */ true);
    }

    /** Test for Tab Drag {@link #ONDRAG_TEST_CASES} - Scenario H.3 */
    @Test
    public void test_onDrag_startsOutsideSourceStrip_runnableCancelledOnEnd() {
        testOnDragStartsOutsideSourceStripRunnableCancelledOnEnd(/* isGroupDrag= */ false);
    }

    /** Test for Tab Group Drag {@link #ONDRAG_TEST_CASES} - Scenario H.3 */
    @Test
    public void test_onDrag_startsOutsideSourceStrip_runnableCancelledOnEnd_tabGroup() {
        testOnDragStartsOutsideSourceStripRunnableCancelledOnEnd(/* isGroupDrag= */ true);
    }

    @Test
    public void testHistogram_nonLastTabDroppedInStripDoesNotCloseWindow_source() {
        // Verify histograms.
        testNonLastTabDroppedInStripHistogram();

        // Verify the user action`TabRemovedFromGroup` is not recorded.
        assertEquals(
                "TabRemovedFromGroup should not be recorded as the tab being dragged is not in a"
                        + " tab group",
                0,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    @Test
    public void testNonLastTabDroppedInStrip_RecordTabRemovedFromGroup() {
        // The tab being dragged is in a tab group.
        when(mTabGroupModelFilter.isTabInTabGroup(mTabBeingDragged)).thenReturn(true);

        // Verify histograms.
        testNonLastTabDroppedInStripHistogram();

        // Verify the user action`TabRemovedFromGroup` is recorded.
        assertEquals(
                "TabRemovedFromGroup should be recorded",
                1,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    @Test
    public void testHistogram_lastTabDroppedInStripClosesWindow_source() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DragDrop.Tab.SourceWindowClosed", true);
        // When the last tab is dragged/dropped, the source window will be closed.
        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(true);

        invokeDropInDestinationStrip(
                /* dragEndRes= */ true, /* isGroupDrag= */ false, /* isGroupShared= */ false);

        histogramExpectation.assertExpected();
    }

    @Test
    public void testHistogram_lastTabDragUnhandled_source() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.DragDrop.Tab.SourceWindowClosed")
                        .build();
        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(false);

        // Assume that the drag was not handled.
        new DragEventInvoker(DragType.SINGLE_TAB, /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .end(false);

        histogramExpectation.assertExpected();
    }

    /** Tests fix for crash reported in crbug.com/327449234. */
    @Test
    public void test_onDrag_nullClipDescription() {
        // Mock drag event with null ClipDescription.
        DragEvent event =
                mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY, DragType.SINGLE_TAB);
        when(event.getClipDescription()).thenReturn(null);

        // No exception should be thrown when #onDragStart is invoked.
        assertFalse(
                "#onDragStart should not be handled when ClipDescription is null.",
                mSourceInstance.onDrag(mTabsToolbarView, event));
    }

    /**
     * Tests fix for crash reported in crbug.com/379842187 when dropping tabs from a different
     * channel.
     */
    @Test
    public void test_onDrag_tabFromDifferentChannel() {
        assertFalse(
                "#onDragStart should return false when no global tab state exists.",
                mSourceInstance.onDrag(
                        mTabsToolbarView,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY, DragType.SINGLE_TAB)));
    }

    private void doTestOnDragDropInStripSource(boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");
        String reorderHistogram =
                String.format(
                        "Android.DragDrop.%s.ReorderStripWithDragDrop",
                        isGroupDrag ? "TabGroup" : "Tab");
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectBooleanRecord(reorderHistogram, false)
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .build();

        new DragEventInvoker(
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB,
                        /* isGroupShared= */ false)
                .drop(mSourceInstance)
                .end(true);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Stop reorder on drop and drag end.
        verify(mSourceStripLayoutHelper, times(2)).stopReorderMode();
        // Verify view not moved.
        verifyViewNotMovedToWindow(isGroupDrag);
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestOnDragDropInStripSource_multiTab() {
        String resultHistogram = "Android.DragDrop.MultiTab.FromStrip.Result";
        String reorderHistogram = "Android.DragDrop.MultiTab.ReorderStripWithDragDrop";
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectBooleanRecord(reorderHistogram, false)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .build();

        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                .drop(mSourceInstance)
                .end(true);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Stop reorder on drop and drag end.
        verify(mSourceStripLayoutHelper, times(2)).stopReorderMode();
        // Verify view not moved.
        verify(mSourceMultiInstanceManager, times(0))
                .moveTabsToWindow(any(Activity.class), any(), anyInt());
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestOnDragDropInToolbarContainerSource(boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.IGNORED_TOOLBAR)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.ReorderStripWithDragDrop")
                        .build();
        new DragEventInvoker(
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB,
                        /* isGroupShared= */ false)
                // Drag our of strip but within toolbar container.
                .dragLocationY(mSourceInstance, 3 * DRAG_MOVE_DISTANCE)
                // Shadow visible when drag moves out of strip.
                .verifyShadowVisibility(true)
                .drop(mSourceInstance)
                .end(false);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1)).handleDragExit(anyBoolean(), anyBoolean());
        // Verify view is not moved since drop is on source toolbar.
        verifyViewNotMovedToWindow(isGroupDrag);
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).stopReorderMode();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestOnDragDropInToolbarContainerSource_multiTab() {
        String resultHistogram = "Android.DragDrop.MultiTab.FromStrip.Result";
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.IGNORED_TOOLBAR)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.ReorderStripWithDragDrop")
                        .build();
        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                // Drag our of strip but within toolbar container.
                .dragLocationY(mSourceInstance, 3 * DRAG_MOVE_DISTANCE)
                // Shadow visible when drag moves out of strip.
                .verifyShadowVisibility(true)
                .drop(mSourceInstance)
                .end(false);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1)).handleDragExit(anyBoolean(), anyBoolean());
        // Verify view is not moved since drop is on source toolbar.
        verify(mSourceMultiInstanceManager, times(0))
                .moveTabsToWindow(any(Activity.class), any(), anyInt());
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).stopReorderMode();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestUnhandledDropOutsideWithMaxInstances(
            boolean isInDesktopWindow, boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");

        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropResult.IGNORED_MAX_INSTANCES)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop");

        if (isInDesktopWindow) {
            AppHeaderUtils.setAppInDesktopWindowForTesting(true);
            builder.expectIntRecord(
                    resultHistogram + ".DesktopWindow", DragDropResult.IGNORED_MAX_INSTANCES);
        } else {
            builder.expectNoRecords(resultHistogram + ".DesktopWindow");
        }
        HistogramWatcher histogramExpectation = builder.build();

        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        new DragEventInvoker(
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB,
                        /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .end(false);

        verify(mSourceMultiInstanceManager).showInstanceCreationLimitMessage(any());
        if (!isGroupDrag) {
            histogramExpectation.assertExpected();
        }
    }

    private void doTriggerUnhandledDrop(int failureCount, boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");
        String failureHistogram = "Android.DragDrop.TabOrGroup.MaxInstanceFailureCount";
        var histogramBuilder =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                resultHistogram, DragDropResult.IGNORED_MAX_INSTANCES, failureCount)
                        .expectNoRecords(resultHistogram + ".DesktopWindow");

        // Set histogram expectation.
        for (int i = 0; i < failureCount; i++) {
            histogramBuilder = histogramBuilder.expectIntRecord(failureHistogram, i + 1);
        }
        var histogramExpectation = histogramBuilder.build();

        // Simulate unhandled tab drops |failureCount| number of times.
        for (int i = 0; i < failureCount; i++) {
            new DragEventInvoker(
                            isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB,
                            /* isGroupShared= */ false)
                    .dragExit(mSourceInstance)
                    .end(false);
        }

        // Verify that the count is correctly updated in SharedPreferences and the histogram is
        // emitted as expected.
        String maxInstanceFailureKey =
                ChromePreferenceKeys.TAB_OR_GROUP_TEARING_MAX_INSTANCES_FAILURE_COUNT;
        assertEquals(
                "Tab drag max-instance failure count saved in shared prefs is incorrect.",
                failureCount,
                mSharedPreferencesManager.readInt(maxInstanceFailureKey));
        histogramExpectation.assertExpected();
    }

    private void doTestDropInStripDestination(
            boolean isInDesktopWindow,
            boolean isGroupDrag,
            boolean isGroupShared,
            String mhtmlTabTitle) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");
        String typeHistogram =
                String.format("Android.DragDrop.%s.Type", isGroupDrag ? "TabGroup" : "Tab");
        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectIntRecord(typeHistogram, DragDropType.TAB_STRIP_TO_TAB_STRIP)
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.TabGroup.ReorderStripWithDragDrop");

        if (isInDesktopWindow) {
            AppHeaderUtils.setAppInDesktopWindowForTesting(true);
            builder.expectIntRecord(resultHistogram + ".DesktopWindow", DragDropResult.SUCCESS)
                    .expectIntRecord(
                            typeHistogram + ".DesktopWindow", DragDropType.TAB_STRIP_TO_TAB_STRIP);
        }
        HistogramWatcher histogramExpectation = builder.build();

        when(mDestStripLayoutHelper.getTabIndexForTabDrop(anyFloat(), anyBoolean()))
                .thenReturn(TAB_INDEX);

        // Invoke drop.
        invokeDropInDestinationStrip(
                /* dragEndRes= */ mhtmlTabTitle == null, isGroupDrag, isGroupShared);

        // Verify - drop failed and toast is shown for group that has mhtml tab.
        if (mhtmlTabTitle != null) {
            verifyViewNotMovedToWindow(isGroupDrag);
            verifyToast(
                    ContextUtils.getApplicationContext()
                            .getString(R.string.tab_cannot_be_moved, mhtmlTabTitle));
            return;
        }

        // Verify view moved to window.
        verifyViewMovedToWindow(isGroupDrag, TAB_INDEX);

        // Verify reorder mode cleared.
        verify(mSourceStripLayoutHelper, times(1)).stopReorderMode();
        // Verify destination strip calls.
        verify(mDestStripLayoutHelper)
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        verify(mDestStripLayoutHelper).stopReorderMode();

        assertNull(ShadowToast.getLatestToast());
        histogramExpectation.assertExpected();
    }

    private void doTestDropInStripDestinationForMultiTab() {
        when(mDestStripLayoutHelper.getTabIndexForTabDrop(anyFloat(), anyBoolean()))
                .thenReturn(TAB_INDEX);

        // Invoke drop.
        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(true);

        // Verify view moved to window.
        verify(mDestMultiInstanceManager, times(1))
                .moveTabsToWindow(any(Activity.class), eq(mTabsBeingDragged), eq(TAB_INDEX));
        List<Integer> tabIds = new ArrayList<>();
        for (Tab tab : mTabsBeingDragged) {
            tabIds.add(tab.getId());
        }
        verify(mDestStripLayoutHelper, times(1))
                .maybeMergeToGroupOnDrop(eq(tabIds), eq(TAB_INDEX), eq(false));

        // Verify reorder mode cleared.
        verify(mSourceStripLayoutHelper, times(1)).stopReorderMode();
        // Verify destination strip calls.
        verify(mDestStripLayoutHelper)
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        verify(mDestStripLayoutHelper).stopReorderMode();

        assertNull(ShadowToast.getLatestToast());
    }

    private void doTestDropInDestinationDifferentModel(boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");
        String typeHistogram =
                String.format("Android.DragDrop.%s.Type", isGroupDrag ? "TabGroup" : "Tab");
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectIntRecord(typeHistogram, DragDropType.TAB_STRIP_TO_TAB_STRIP)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.ReorderStripWithDragDrop")
                        .build();

        // Destination tab model is incognito.
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        TabModel standardModelDestination = mock(TabModel.class);
        when(standardModelDestination.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(standardModelDestination);
        when(standardModelDestination.getCount()).thenReturn(5);

        // Verify - View moved to destination window at end.
        invokeDropInDestinationStrip(
                /* dragEndRes= */ true, isGroupDrag, /* isGroupShared= */ false);
        verifyViewMovedToWindow(isGroupDrag, /* index= */ 5);

        // Verify toast.
        verifyToast(
                ContextUtils.getApplicationContext()
                        .getString(R.string.tab_dropped_different_model));
        assertNotNull(ShadowToast.getLatestToast());

        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestDropInDestinationDifferentModel_multiTab() {
        String resultHistogram = "Android.DragDrop.MultiTab.FromStrip.Result";
        String typeHistogram = "Android.DragDrop.MultiTab.Type";
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectIntRecord(typeHistogram, DragDropType.TAB_STRIP_TO_TAB_STRIP)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .build();

        // Destination tab model is incognito.
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        TabModel standardModelDestination = mock(TabModel.class);
        when(standardModelDestination.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(standardModelDestination);
        when(standardModelDestination.getCount()).thenReturn(5);

        // Verify - View moved to destination window at end.
        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(true);
        verify(mDestMultiInstanceManager, times(1))
                .moveTabsToWindow(any(Activity.class), eq(mTabsBeingDragged), eq(5));

        // Verify toast.
        verifyToast(
                ContextUtils.getApplicationContext()
                        .getString(R.string.tab_dropped_different_model));
        assertNotNull(ShadowToast.getLatestToast());

        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestDropInDestinationDifferentModel_fail(boolean isGroupDrag) {
        // Destination tab model is incognito.
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        TabModel standardModelDestination = mock(TabModel.class);
        when(standardModelDestination.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(standardModelDestination);
        when(standardModelDestination.getCount()).thenReturn(5);

        // Verify - View did not moved to destination window at end.
        invokeDropInDestinationStrip(
                /* dragEndRes= */ false, isGroupDrag, /* isGroupShared= */ false);
        verifyViewNotMovedToWindow(isGroupDrag);
    }

    private void doTestDropInDestinationDifferentModel_fail_multiTab() {
        // Destination tab model is incognito.
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        TabModel standardModelDestination = mock(TabModel.class);
        when(standardModelDestination.getProfile()).thenReturn(mProfile);
        when(mTabModelSelector.getModel(false)).thenReturn(standardModelDestination);
        when(standardModelDestination.getCount()).thenReturn(5);

        // Verify - View did not moved to destination window at end.
        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(true);
        verifyViewNotMovedToWindow(/* isGroupDrag= */ false);
    }

    private void doTestDropInDestinationToolbarContainer(boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.IGNORED_TOOLBAR)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.ReorderStripWithDragDrop")
                        .build();
        new DragEventInvoker(
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB,
                        /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                // Move to toolbar container outside of tab strip.
                .dragLocationY(mDestInstance, 3 * DRAG_MOVE_DISTANCE)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(false);

        // Verify appropriate events are generated.
        // Source strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Source strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1)).handleDragExit(anyBoolean(), anyBoolean());
        // Destination strip prepares for drop on drag enter.
        verify(mDestStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Destination strip clears state for drop on drag exit.
        verify(mDestStripLayoutHelper, times(1)).handleDragExit(anyBoolean(), anyBoolean());

        // Verify not moved.
        verifyViewNotMovedToWindow(isGroupDrag);

        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).stopReorderMode();

        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestDropInDestinationToolbarContainer_multiTab() {
        String resultHistogram = "Android.DragDrop.MultiTab.FromStrip.Result";

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.IGNORED_TOOLBAR)
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.ReorderStripWithDragDrop")
                        .build();
        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                // Move to toolbar container outside of tab strip.
                .dragLocationY(mDestInstance, 3 * DRAG_MOVE_DISTANCE)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(false);

        // Verify appropriate events are generated.
        // Source strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Source strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1)).handleDragExit(anyBoolean(), anyBoolean());
        // Destination strip prepares for drop on drag enter.
        verify(mDestStripLayoutHelper, times(1))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Destination strip clears state for drop on drag exit.
        verify(mDestStripLayoutHelper, times(1)).handleDragExit(anyBoolean(), anyBoolean());

        // Verify not moved.
        verify(mSourceMultiInstanceManager, times(0))
                .moveTabsToWindow(any(Activity.class), any(), anyInt());

        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).stopReorderMode();

        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestExitIntoSourceToolbarAndRenterStripAndDrop(boolean isGroupDrag) {
        String resultHistogram =
                String.format(
                        "Android.DragDrop.%s.FromStrip.Result", isGroupDrag ? "TabGroup" : "Tab");
        String reorderHistogram =
                String.format(
                        "Android.DragDrop.%s.ReorderStripWithDragDrop",
                        isGroupDrag ? "TabGroup" : "Tab");
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectBooleanRecord(reorderHistogram, true)
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .build();

        new DragEventInvoker(
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB,
                        /* isGroupShared= */ false)
                .dragLocationY(mSourceInstance, 3 * DRAG_MOVE_DISTANCE) // move to toolbar
                .verifyShadowVisibility(true)
                .dragLocationY(mSourceInstance, -3 * DRAG_MOVE_DISTANCE) // move back to strip
                .verifyShadowVisibility(false)
                .drop(mSourceInstance)
                .end(true);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter. Entered twice.
        verify(mSourceStripLayoutHelper, times(2))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Stop reorder on drop and drag end.
        verify(mSourceStripLayoutHelper, times(2)).stopReorderMode();

        // Verify not moved.
        verifyViewNotMovedToWindow(isGroupDrag);

        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);

        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestExitIntoSourceToolbarAndRenterStripAndDrop_multiTab() {
        String resultHistogram = "Android.DragDrop.MultiTab.FromStrip.Result";
        String reorderHistogram = "Android.DragDrop.MultiTab.ReorderStripWithDragDrop";
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(resultHistogram, DragDropResult.SUCCESS)
                        .expectBooleanRecord(reorderHistogram, true)
                        .expectNoRecords("Android.DragDrop.TabGroup.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type")
                        .expectNoRecords("Android.DragDrop.TabGroup.Type.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result.DesktopWindow")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.Type.DesktopWindow")
                        .build();

        new DragEventInvoker(DragType.MULTI_TAB, /* isGroupShared= */ false)
                .dragLocationY(mSourceInstance, 3 * DRAG_MOVE_DISTANCE) // move to toolbar
                .verifyShadowVisibility(true)
                .dragLocationY(mSourceInstance, -3 * DRAG_MOVE_DISTANCE) // move back to strip
                .verifyShadowVisibility(false)
                .drop(mSourceInstance)
                .end(true);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter. Entered twice.
        verify(mSourceStripLayoutHelper, times(2))
                .handleDragEnter(anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Stop reorder on drop and drag end.
        verify(mSourceStripLayoutHelper, times(2)).stopReorderMode();

        // Verify not moved.
        verify(mSourceMultiInstanceManager, times(0))
                .moveTabsToWindow(any(Activity.class), any(), anyInt());

        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);

        // Verify histograms.
        histogramExpectation.assertExpected();
    }

    private void doTestOnDragInvalidClipData(boolean isGroupDrag) {
        // Set state.
        DragDropGlobalState.store(CURR_INSTANCE_ID, mock(ChromeDropDataAndroid.class), null);

        // Trigger drop with invalid tabId.
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_STARTED,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        DragEvent event;
        if (isGroupDrag) {
            event =
                    mockDragEvent(
                            DragEvent.ACTION_DROP,
                            POS_X,
                            mPosY,
                            /* tab= */ null,
                            mTabGroupMetadata,
                            DragType.TAB_GROUP);
            mSourceInstance.onDrag(mTabsToolbarView, event);

            // Verify - Move to new window not invoked.
            verify(mDestMultiInstanceManager, times(0))
                    .moveTabGroupToWindow(any(Activity.class), any(), anyInt());
        } else {
            event =
                    mockDragEvent(
                            DragEvent.ACTION_DROP,
                            POS_X,
                            mPosY,
                            MockTab.createAndInitialize(TAB_ID_NOT_DRAGGED, mProfile),
                            mTabGroupMetadata,
                            DragType.SINGLE_TAB);
            mSourceInstance.onDrag(mTabsToolbarView, event);

            // Verify - Move to new window not invoked.
            verify(mDestMultiInstanceManager, times(0))
                    .moveTabsToWindow(any(Activity.class), any(), anyInt());
        }
    }

    private void doTestOnDragDestinationStripNotVisible(boolean isGroupDrag) {
        mTabStripVisible = false;

        // Start drag action.
        startDragAction(isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB, false);

        boolean res =
                mDestInstance.onDrag(
                        mTabsToolbarView,
                        mockDragEvent(
                                DragEvent.ACTION_DRAG_STARTED,
                                POS_X,
                                mPosY,
                                isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        assertFalse("onDrag should return false.", res);
    }

    private void doTestOnDragStartsOutsideSourceStripRunnableSuccess(boolean isGroupDrag) {
        // Start drag action. Forgo DragEventInvoker, since it mocks the drag enter on start.
        startDragAction(
                isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB, /* isGroupShared= */ false);

        // Verify the drag shadow begins invisible after ACTION_DRAG_STARTED.
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_STARTED,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        assertFalse(
                "Drag shadow should not yet be visible.",
                ((TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder())
                        .getShadowShownForTesting());

        // Verify the drag shadow is visible after the runnable completes.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertTrue(
                "Drag shadow should now be visible.",
                ((TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder())
                        .getShadowShownForTesting());
    }

    private void doTestOnDragStartsOutsideSourceStripRunnableCancelledOnEnter(boolean isGroupDrag) {
        // Start drag action. Forgo DragEventInvoker, since it mocks the drag enter on start.
        startDragAction(
                isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB, /* isGroupShared= */ false);

        // Verify the drag shadow begins invisible after ACTION_DRAG_STARTED.
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_STARTED,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        assertFalse(
                "Drag shadow should not yet be visible.",
                ((TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder())
                        .getShadowShownForTesting());

        // Verify the drag shadow is not visible as the runnable has been cancelled after
        // #onDragEnter. Not triggered until ACTiON_DRAG_LOCATION since the drag's y-position is
        // needed to verify the tab strip part of the view was entered.
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_ENTERED,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        assertFalse(
                "Drag shadow should still not visible.",
                ((TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder())
                        .getShadowShownForTesting());
    }

    private void testOnDragStartsOutsideSourceStripRunnableCancelledOnEnd(boolean isGroupDrag) {
        // Start drag action. Forgo DragEventInvoker, since it mocks the drag enter on start.
        startDragAction(
                isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB, /* isGroupShared= */ false);

        // Verify the drag shadow begins invisible after ACTION_DRAG_STARTED.
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_STARTED,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        assertTrue(
                "#onDragExit runnable should be posted.",
                mSourceInstance
                        .getHandlerForTesting()
                        .hasCallbacks(mSourceInstance.getOnDragExitRunnableForTesting()));

        // Verify the runnable has been cancelled after ACTION_DRAG_ENDED.
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DRAG_ENDED,
                        POS_X,
                        mPosY,
                        isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB));
        assertFalse(
                "#onDragExit runnable should be cleared.",
                mSourceInstance
                        .getHandlerForTesting()
                        .hasCallbacks(mSourceInstance.getOnDragExitRunnableForTesting()));
    }

    private void testNonLastTabDroppedInStripHistogram() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DragDrop.Tab.SourceWindowClosed", false);
        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(false);

        invokeDropInDestinationStrip(
                /* dragEndRes= */ true, /* isGroupDrag= */ false, /* isGroupShared= */ false);

        histogramExpectation.assertExpected();
    }

    private void invokeDropInDestinationStrip(
            boolean dragEndRes, boolean isGroupDrag, boolean isGroupShared) {
        new DragEventInvoker(isGroupDrag ? DragType.TAB_GROUP : DragType.SINGLE_TAB, isGroupShared)
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(dragEndRes);
    }

    enum DragType {
        SINGLE_TAB,
        MULTI_TAB,
        TAB_GROUP
    }

    class DragEventInvoker {

        private final DragType mDragType;

        DragEventInvoker(DragType dragType, boolean isGroupShared) {
            mDragType = dragType;
            // Start drag action.
            startDragAction(mDragType, isGroupShared);
            // drag invokes DRAG_START and DRAG_ENTER on source and DRAG_START on destination.
            mSourceInstance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY, mDragType));
            mDestInstance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY, mDragType));
            mSourceInstance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY, mDragType));
            // Move within the tab strip area to set lastX / lastY.
            mSourceInstance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X + 10, mPosY, mDragType));
            // Verify shadow is not visible.
            verifyShadowVisibility(false);
        }

        public DragEventInvoker dragLocationY(TabStripDragHandler instance, float distance) {
            mPosY += distance;
            instance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X, mPosY, mDragType));
            return this;
        }

        public DragEventInvoker dragExit(TabStripDragHandler instance) {
            instance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_EXITED, /* x= */ 0, /* y= */ 0, mDragType));
            return this;
        }

        public DragEventInvoker dragEnter(TabStripDragHandler instance) {
            mPosY = mTabStripHeight - 2 * DRAG_MOVE_DISTANCE;
            instance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY, mDragType));
            // Also trigger DRAG_LOCATION following DRAG_ENTERED since enter is no-op in
            // implementation.
            instance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X, mPosY, mDragType));
            return this;
        }

        public DragEventInvoker drop(TabStripDragHandler instance) {
            instance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY, mDragType));
            return this;
        }

        public DragEventInvoker end(boolean res) {
            mDestInstance.onDrag(mTabsToolbarView, mockDragEndEvent(res, mDragType));
            mSourceInstance.onDrag(mTabsToolbarView, mockDragEndEvent(res, mDragType));
            assertFalse(
                    "Global state should be cleared on all drag end",
                    DragDropGlobalState.hasValue());
            return this;
        }

        public DragEventInvoker verifyShadowVisibility(boolean visible) {
            assertEquals(
                    "Drag shadow visibility does not match.",
                    visible,
                    ((TabDragShadowBuilder) DragDropGlobalState.getDragShadowBuilder())
                            .getShadowShownForTesting());
            return this;
        }
    }

    private DragEvent mockDragEndEvent(boolean res, DragType dragType) {
        DragEvent dragEvent =
                mockDragEvent(
                        DragEvent.ACTION_DRAG_ENDED,
                        /* x= */ 0f,
                        /* y= */ 0f,
                        mTabBeingDragged,
                        mTabGroupMetadata,
                        dragType);
        when(dragEvent.getResult()).thenReturn(res);
        return dragEvent;
    }

    private DragEvent mockDragEvent(int action, float x, float y, DragType dragType) {
        return mockDragEvent(action, x, y, mTabBeingDragged, mTabGroupMetadata, dragType);
    }

    private DragEvent mockDragEvent(
            int action,
            float x,
            float y,
            Tab tab,
            TabGroupMetadata tabGroupMetadata,
            DragType dragType) {
        DragEvent event = mock(DragEvent.class);
        ChromeDropDataAndroid dropData;
        String[] mimeTypes;
        switch (dragType) {
            case SINGLE_TAB:
                dropData = new ChromeTabDropDataAndroid.Builder().withTab(tab).build();
                mimeTypes = SUPPORTED_TAB_MIME_TYPES;
                break;
            case MULTI_TAB:
                dropData =
                        new ChromeMultiTabDropDataAndroid.Builder()
                                .withTabs(mTabsBeingDragged)
                                .build();
                mimeTypes = new String[] {"chrome/multi-tab"};
                break;
            case TAB_GROUP:
                dropData =
                        new ChromeTabGroupDropDataAndroid.Builder()
                                .withTabGroupMetadata(tabGroupMetadata)
                                .build();
                mimeTypes = SUPPORTED_GROUP_MIME_TYPES;
                break;
            default:
                throw new IllegalArgumentException("Invalid drag type.");
        }

        when(event.getClipData())
                .thenReturn(
                        new ClipData(
                                null,
                                mimeTypes,
                                new Item(dropData.buildTabClipDataText(mContext), null)));
        when(event.getClipDescription()).thenReturn(new ClipDescription("", mimeTypes));
        when(event.getAction()).thenReturn(action);
        when(event.getX()).thenReturn(x);
        when(event.getY()).thenReturn(y);
        return event;
    }

    private void callAndVerifyAllowDragToCreateInstance(boolean expectedAllowDragToCreateInstance) {
        // Verify.
        assertTrue(
                "Tab drag should start.",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
        var dropDataCaptor = ArgumentCaptor.forClass(ChromeDropDataAndroid.class);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        dropDataCaptor.capture());
        assertEquals(
                "DropData.allowDragToCreateInstance value is not as expected.",
                expectedAllowDragToCreateInstance,
                dropDataCaptor.getValue().allowDragToCreateInstance);
    }

    private void callAndVerifyAllowMultiTabDragToCreateInstance(
            boolean expectedAllowDragToCreateInstance) {
        // Verify.
        assertTrue(
                "Multi-Tab drag should start.",
                mSourceInstance.startMultiTabDragAction(
                        mTabsToolbarView,
                        mTabsBeingDragged,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        VIEW_WIDTH));
        var dropDataCaptor = ArgumentCaptor.forClass(ChromeDropDataAndroid.class);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        dropDataCaptor.capture());
        assertEquals(
                "DropData.allowDragToCreateInstance value is not as expected.",
                expectedAllowDragToCreateInstance,
                dropDataCaptor.getValue().allowDragToCreateInstance);
    }

    private void setupTabGroup(boolean isGroupShared) {
        mGroupedTab1 = spy(MockTab.createAndInitialize(GROUPED_TAB_ID_1, mProfile));
        mGroupedTab2 = spy(MockTab.createAndInitialize(GROUPED_TAB_ID_2, mProfile));
        doReturn(TAB_GROUP_ID).when(mGroupedTab1).getTabGroupId();
        doReturn(TAB_GROUP_ID).when(mGroupedTab2).getTabGroupId();
        mTabGroupBeingDragged.add(mGroupedTab1);
        mTabGroupBeingDragged.add(mGroupedTab2);
        mTabGroupMetadata =
                TabGroupMetadataExtractor.extractTabGroupMetadata(
                        mTabGroupModelFilter,
                        mTabGroupBeingDragged,
                        /* sourceWindowIndex= */ -1,
                        mGroupedTab1.getId(),
                        isGroupShared);
        when(mTabGroupModelFilter.getTabsInGroup(TAB_GROUP_ID)).thenReturn(mTabGroupBeingDragged);
        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.isTabInTabGroup(mGroupedTab1)).thenReturn(true);
        when(mTabGroupModelFilter.isTabInTabGroup(mGroupedTab2)).thenReturn(true);
        when(mTabModel.getTabById(mGroupedTab1.getId())).thenReturn(mGroupedTab1);
        when(mTabModel.getTabById(mGroupedTab2.getId())).thenReturn(mGroupedTab2);
    }

    private void startDragAction(DragType dragType, boolean isGroupShared) {
        switch (dragType) {
            case SINGLE_TAB:
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        new PointF(POS_X, mPosY),
                        TAB_POSITION_X,
                        VIEW_WIDTH);
                break;
            case MULTI_TAB:
                mSourceInstance.startMultiTabDragAction(
                        mTabsToolbarView,
                        mTabsBeingDragged,
                        mTabsBeingDragged.get(0),
                        new PointF(POS_X, mPosY),
                        TAB_POSITION_X,
                        VIEW_WIDTH);
                break;
            case TAB_GROUP:
                mSourceInstance.startGroupDragAction(
                        mTabsToolbarView,
                        TAB_GROUP_ID,
                        isGroupShared,
                        new PointF(POS_X, mPosY),
                        TAB_POSITION_X,
                        VIEW_WIDTH);
        }
    }

    private void verifyViewNotMovedToWindow(boolean isGroupDrag) {
        if (isGroupDrag) {
            // Verify tab group is not moved.
            verify(mDestMultiInstanceManager, times(0))
                    .moveTabGroupToWindow(any(Activity.class), eq(mTabGroupMetadata), anyInt());
        } else {
            // Verify tab is not moved.
            verify(mDestMultiInstanceManager, times(0))
                    .moveTabsToWindow(any(Activity.class), any(), anyInt());
        }
    }

    private void verifyViewMovedToWindow(boolean isGroupDrag, int index) {
        if (isGroupDrag) {
            // Verify tab group is moved.
            verify(mDestMultiInstanceManager, times(1))
                    .moveTabGroupToWindow(any(Activity.class), eq(mTabGroupMetadata), eq(index));
        } else {
            // Verify tab is moved.
            verify(mDestMultiInstanceManager, times(1))
                    .moveTabsToWindow(any(Activity.class), any(), eq(index));
        }
    }

    private void verifyToast(String expectedText) {
        assertNotNull(ShadowToast.getLatestToast());
        TextView textView = (TextView) ShadowToast.getLatestToast().getView();
        String actualText = textView == null ? "" : textView.getText().toString();
        assertEquals("Text for toast shown does not match.", expectedText, actualText);
    }
}
