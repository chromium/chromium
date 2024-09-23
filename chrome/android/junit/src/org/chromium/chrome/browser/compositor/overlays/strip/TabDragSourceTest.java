// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

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
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
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
import org.robolectric.shadows.ShadowToast;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.TabDragSource.TabDragShadowBuilder;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
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
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DragDropGlobalState;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropTabResult;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DropDataAndroid;
import org.chromium.ui.widget.ToastManager;

import java.lang.ref.WeakReference;

/** Tests for {@link TabDragSource}. */
@DisableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
@EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING_ENABLE_OEM)
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp", sdk = VERSION_CODES.S, shadows = ShadowToast.class)
public class TabDragSourceTest {

    private static final int CURR_INSTANCE_ID = 100;
    private static final int ANOTHER_INSTANCE_ID = 200;
    private static final int TAB_ID = 1;
    private static final int TAB_ID_NOT_DRAGGED = 2;
    private static final float TAB_WIDTH = 5f;
    private static final int TAB_INDEX = 2;
    private static final float POS_X = 20f;
    private static final float DRAG_MOVE_DISTANCE = 5f;
    private static final String[] SUPPORTED_MIME_TYPES = {"chrome/tab"};
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

    // Instances that differ for source and destination for invocations and verifications.
    @Mock private StripLayoutHelper mSourceStripLayoutHelper;
    @Mock private StripLayoutHelper mDestStripLayoutHelper;
    @Mock private MultiInstanceManager mSourceMultiInstanceManager;
    @Mock private MultiInstanceManager mDestMultiInstanceManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModelFilterProvider mTabModelFilterProvider;
    private TabDragSource mSourceInstance;
    private TabDragSource mDestInstance;

    private Activity mActivity;
    private ViewGroup mTabsToolbarView;
    private Tab mTabBeingDragged;
    private static final PointF DRAG_START_POINT = new PointF(250, 0);
    private static final float TAB_POSITION_X = 200f;
    private int mTabStripHeight;
    private final Context mContext = ContextUtils.getApplicationContext();
    private boolean mTabStripVisible;
    private SharedPreferencesManager mSharedPreferencesManager;
    private UserActionTester mUserActionTest;

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);
        mTabStripHeight = mActivity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        mPosY = mTabStripHeight - 2 * DRAG_MOVE_DISTANCE;
        mTabStripVisible = true;

        // Create and spy on a simulated tab view.
        mTabsToolbarView = new FrameLayout(mActivity);
        mTabsToolbarView.setLayoutParams(new MarginLayoutParams(150, 50));

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        mTabBeingDragged = MockTab.createAndInitialize(TAB_ID, mProfile);
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
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        when(mTabStripHeightSupplier.get()).thenReturn(mTabStripHeight);
        when(mTabModelSelector.getTabModelFilterProvider()).thenReturn(mTabModelFilterProvider);
        when(mTabModelFilterProvider.getCurrentTabModelFilter()).thenReturn(mTabGroupModelFilter);

        mSourceInstance =
                new TabDragSource(
                        mActivity,
                        () -> mSourceStripLayoutHelper,
                        () -> mTabStripVisible,
                        () -> mTabContentManager,
                        () -> mLayerTitleCache,
                        mSourceMultiInstanceManager,
                        mDragDropDelegate,
                        mBrowserControlsStateProvider,
                        mWindowAndroid,
                        mTabStripHeightSupplier);
        mSourceInstance.setTabModelSelector(mTabModelSelector);

        mDestInstance =
                new TabDragSource(
                        mActivity,
                        () -> mDestStripLayoutHelper,
                        () -> mTabStripVisible,
                        () -> mTabContentManager,
                        () -> mLayerTitleCache,
                        mDestMultiInstanceManager,
                        mDragDropDelegate,
                        mBrowserControlsStateProvider,
                        mWindowAndroid,
                        mTabStripHeightSupplier);
        mDestInstance.setTabModelSelector(mTabModelSelector);

        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(false);

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mUserActionTest = new UserActionTester();
    }

    @After
    public void cleanup() {
        if (DragDropGlobalState.hasValue()) {
            DragDropGlobalState.clearForTesting();
        }
        ShadowToast.reset();
        ToastManager.resetForTesting();
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS);
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_COUNT);
    }

    @EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
    @Test
    public void test_startTabDragAction_withTabDragDropFF_returnsTrueForValidTab() {
        // Act and verify.
        boolean res =
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        TAB_WIDTH);
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
                ((ChromeDropDataAndroid) DragDropGlobalState.getForTesting().getData()).tab);
        assertNull("Shadow view should be null.", mSourceInstance.getShadowViewForTesting());
    }

    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @Test
    public void test_startTabDragAction_withTabLinkDragDropFF_returnsTrueForValidTab() {
        // Act and verify.
        boolean res =
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        TAB_WIDTH);
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
                ((ChromeDropDataAndroid) DragDropGlobalState.getForTesting().getData()).tab);
        assertNotNull(
                "Shadow view is unexpectedly null.", mSourceInstance.getShadowViewForTesting());
    }

    @Test
    public void test_startTabDragAction_exceptionForInvalidTab() {
        assertThrows(
                NullPointerException.class,
                () ->
                        mSourceInstance.startTabDragAction(
                                mTabsToolbarView,
                                null,
                                DRAG_START_POINT,
                                TAB_POSITION_X,
                                TAB_WIDTH));
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
                        TAB_WIDTH));
    }

    @EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
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
                        TAB_WIDTH));
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
                        TAB_WIDTH));
        assertFalse("Global state should not be set", DragDropGlobalState.hasValue());
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.TAB_DRAG_DROP_ANDROID,
        ChromeFeatureList.DRAG_DROP_TAB_TEARING
    })
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
                        TAB_WIDTH));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void test_startTabDragAction_FullScreenWithMultipleTabs() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);

        // Verify.
        callAndVerifyAllowTabDragToCreateInstance(true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
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
                        TAB_WIDTH));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void test_startTabDragAction_FullScreenWithMaxChromeInstances() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Verify.
        callAndVerifyAllowTabDragToCreateInstance(false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void test_startTabDragAction_FullScreenWithMaxInstanceAllowlistedOEM() {
        // Set params.
        when(mMultiWindowUtils.isInMultiWindowMode(mActivity)).thenReturn(false);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);
        ReflectionHelpers.setStaticField(Build.class, "MANUFACTURER", "samsung");

        callAndVerifyAllowTabDragToCreateInstance(true);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void test_startTabDragAction_SplitScreenWithMaxChromeInstances() {
        // Set params.
        when(mTabModelSelector.getTotalTabCount()).thenReturn(2);
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Verify.
        callAndVerifyAllowTabDragToCreateInstance(false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void test_onProvideShadowMetrics_WithDesiredStartPosition_ReturnsSuccess() {
        // Prepare
        final float dragStartXPosition = 90f;
        final float dragStartYPosition = 45f;
        final PointF dragStartPoint = new PointF(dragStartXPosition, dragStartYPosition);
        // Call startDrag to set class variables.
        mSourceInstance.startTabDragAction(
                mTabsToolbarView, mTabBeingDragged, dragStartPoint, TAB_POSITION_X, TAB_WIDTH);

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
                Math.round(dragStartXPosition),
                dragAnchor.x);
        assertEquals(
                "Drag shadow y position is incorrect.",
                Math.round(dragStartYPosition),
                dragAnchor.y);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void test_onProvideShadowMetrics_withTabLinkDragDropFF() {
        // Call startDrag to set class variables.
        mSourceInstance.startTabDragAction(
                mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT, TAB_POSITION_X, TAB_WIDTH);
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
     * Below tests test end to end tab drag drop flow for following combinations:
     *
     * <pre>
     * A] drop in tab strip - source instance (ie: reorder within strip).
     * B] drop in toolbar container (but outside of tab strip) - source instance.
     * C] drop outside of toolbar container with sub flows:
     *  C.1] With drag as tab FF - no-op.
     *  C.2] With drag as window FF - open new window.
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
     *  </pre>
     */
    private static final String ONDRAG_TEST_CASES = "";

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario A */
    @Test
    public void test_onDrag_dropInStrip_source() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result", DragDropTabResult.SUCCESS)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectBooleanRecord("Android.DragDrop.Tab.ReorderStripWithDragDrop", false)
                        .expectNoRecords("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        new DragEventInvoker().drop(mSourceInstance).end(true);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Stop reorder on drop.
        verify(mSourceStripLayoutHelper, times(1)).onUpOrCancel(anyLong());
        // Verify tab is not moved.
        verify(mSourceMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mSourceMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        // Verify clear.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario B */
    @Test
    public void test_onDrag_dropInToolbarContainer_source() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropTabResult.IGNORED_TOOLBAR)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        new DragEventInvoker()
                // Drag our of strip but within toolbar container.
                .dragLocationY(mSourceInstance, 3 * DRAG_MOVE_DISTANCE)
                // Shadow visible when drag moves out of strip.
                .verifyShadowVisibility(true)
                .drop(mSourceInstance)
                .end(false);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1))
                .clearForTabDrop(anyLong(), anyBoolean(), anyBoolean());
        // Verify tab is not moved since drop is on source toolbar.
        verify(mSourceMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mSourceMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario C.1 */
    @Test
    public void test_onDrag_dropOutsideToolbarContainer() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.DragDrop.Tab.FromStrip.Result")
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        new DragEventInvoker().dragExit(mSourceInstance).verifyShadowVisibility(true).end(false);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1))
                .clearForTabDrop(anyLong(), anyBoolean(), anyBoolean());
        // Verify tab is not moved since drop is outside strip.
        verify(mSourceMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mSourceMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario C.2 */
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @Test
    public void test_onDrag_dropOutsideToolbarContainer_dragAsWindow() {
        // Verify tab is successfully dropped as a window.
        verifyDropOutsideToolbarContainerAsWindow();

        // Verify the user action `TabRemovedFromGroup` is not recorded.
        assertEquals(
                "TabRemovedFromGroup should not be recorded as the tab being dragged is not in a"
                        + " tab group",
                0,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void test_dragAsWindow_recordTabRemovedFromGroup() {
        // The tab being dragged is in a tab group.
        when(mTabGroupModelFilter.isTabInTabGroup(mTabBeingDragged)).thenReturn(true);

        // Verify tab is successfully dropped as a window.
        verifyDropOutsideToolbarContainerAsWindow();

        // Verify the user action`TabRemovedFromGroup` is recorded.
        assertEquals(
                "TabRemovedFromGroup should be recorded",
                1,
                mUserActionTest.getActionCount("MobileToolbarReorderTab.TabRemovedFromGroup"));
    }

    private void verifyDropOutsideToolbarContainerAsWindow() {
        new DragEventInvoker().dragExit(mSourceInstance).verifyShadowVisibility(true).end(false);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter.
        verify(mSourceStripLayoutHelper, times(1))
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1))
                .clearForTabDrop(anyLong(), anyBoolean(), anyBoolean());
        // Verify Since the drop is outside the TabToolbar area the tab will be move to a new
        // Chrome Window.
        verify(mSourceMultiInstanceManager, times(1)).moveTabToNewWindow(mTabBeingDragged);
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void test_onDrag_unhandledDropOutside_maxChromeInstances() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropTabResult.IGNORED_MAX_INSTANCES)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectNoRecords("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();

        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        new DragEventInvoker().dragExit(mSourceInstance).end(false);

        assertNotNull(ShadowToast.getLatestToast());
        TextView textView = (TextView) ShadowToast.getLatestToast().getView();
        String actualText = textView == null ? "" : textView.getText().toString();
        assertEquals(
                "Text for toast shown does not match.",
                ContextUtils.getApplicationContext().getString(R.string.max_number_of_windows),
                actualText);
        histogramExpectation.assertExpected();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.DRAG_DROP_TAB_TEARING)
    public void test_onDrag_multipleUnhandledDropsOutside_maxChromeInstances() {
        MultiWindowUtils.setInstanceCountForTesting(5);
        MultiWindowUtils.setMaxInstancesForTesting(5);

        // Simulate failures on day 1.
        doTriggerUnhandledDrop(4);

        // Force update the count start time saved in SharedPreferences for day 1 to restart count
        // for next day.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_START_TIME_MS,
                System.currentTimeMillis() - DateUtils.DAY_IN_MILLIS - 1);

        // Simulate a failure on day 2.
        doTriggerUnhandledDrop(1);
    }

    private void doTriggerUnhandledDrop(int failureCount) {
        var histogramBuilder =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropTabResult.IGNORED_MAX_INSTANCES,
                                failureCount);

        // Set histogram expectation.
        for (int i = 0; i < failureCount; i++) {
            histogramBuilder =
                    histogramBuilder.expectIntRecord(
                            "Android.DragDrop.Tab.MaxInstanceFailureCount", i + 1);
        }
        var histogramExpectation = histogramBuilder.build();

        // Simulate unhandled tab drops |failureCount| number of times.
        for (int i = 0; i < failureCount; i++) {
            new DragEventInvoker().dragExit(mSourceInstance).end(false);
        }

        // Verify that the count is correctly updated in SharedPreferences and the histogram is
        // emitted as expected.
        assertEquals(
                "Tab drag max-instance failure count saved in shared prefs is incorrect.",
                failureCount,
                mSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.TAB_TEARING_MAX_INSTANCES_FAILURE_COUNT));
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario D.1 */
    @Test
    public void test_onDrag_dropInStrip_destination() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result", DragDropTabResult.SUCCESS)
                        .expectIntRecord(
                                "Android.DragDrop.Tab.Type", DragDropType.TAB_STRIP_TO_TAB_STRIP)
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectAnyRecord("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        when(mDestStripLayoutHelper.getTabIndexForTabDrop(anyFloat())).thenReturn(TAB_INDEX);

        invokeDropInDestinationStrip(true);

        // Verify - Tab moved to destination window at TAB_INDEX.
        verify(mDestMultiInstanceManager, times(1))
                .moveTabToWindow(any(), eq(mTabBeingDragged), eq(TAB_INDEX));
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        // Verify destination strip calls.
        verify(mDestStripLayoutHelper)
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        verify(mDestStripLayoutHelper).onUpOrCancel(anyLong());

        assertNull(ShadowToast.getLatestToast());
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario D.2 */
    @Test
    public void test_onDrag_dropInStrip_differentModel_destination() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result", DragDropTabResult.SUCCESS)
                        .expectIntRecord(
                                "Android.DragDrop.Tab.Type", DragDropType.TAB_STRIP_TO_TAB_STRIP)
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectAnyRecord("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        // Destination tab model is incognito.
        when(mTabModel.isIncognito()).thenReturn(true);
        TabModel standardModelDestination = mock(TabModel.class);
        when(mTabModelSelector.getModel(false)).thenReturn(standardModelDestination);
        when(standardModelDestination.getCount()).thenReturn(5);

        invokeDropInDestinationStrip(true);

        // Verify - Tab moved to destination window at end.
        verify(mDestMultiInstanceManager, times(1))
                .moveTabToWindow(any(), eq(mTabBeingDragged), eq(5));

        assertNotNull(ShadowToast.getLatestToast());
        TextView textView = (TextView) ShadowToast.getLatestToast().getView();
        String actualText = textView == null ? "" : textView.getText().toString();
        assertEquals(
                "Text for toast shown does not match.",
                ContextUtils.getApplicationContext()
                        .getString(R.string.tab_dropped_different_model),
                actualText);
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario D.3 */
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @Test
    public void test_onDrag_dropInStrip_withDragAsWindowFF_destination() {
        mockStripTabPosition(TAB_WIDTH, POS_X - (TAB_WIDTH / 2), TAB_INDEX);

        new DragEventInvoker()
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(false)
                .drop(mDestInstance)
                .end(false);
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario E */
    @Test
    public void test_onDrag_dropInToolbarContainer_destination() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result",
                                DragDropTabResult.IGNORED_TOOLBAR)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectNoRecords("Android.DragDrop.Tab.ReorderStripWithDragDrop")
                        .expectAnyRecord("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        new DragEventInvoker()
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
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Source strip clears state for drop on drag exit.
        verify(mSourceStripLayoutHelper, times(1))
                .clearForTabDrop(anyLong(), anyBoolean(), anyBoolean());
        // Destination strip prepares for drop on drag enter.
        verify(mDestStripLayoutHelper, times(1))
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Destination strip clears state for drop on drag exit.
        verify(mDestStripLayoutHelper, times(1))
                .clearForTabDrop(anyLong(), anyBoolean(), anyBoolean());
        // Verify tab is not moved since drop is on source toolbar.
        verify(mSourceMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mSourceMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        // Verify tab cleared.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario F */
    @Test
    public void test_onDrag_exitIntoToolbarAndRenterStripAndDrop_source() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.DragDrop.Tab.FromStrip.Result", DragDropTabResult.SUCCESS)
                        .expectNoRecords("Android.DragDrop.Tab.Type")
                        .expectBooleanRecord("Android.DragDrop.Tab.ReorderStripWithDragDrop", true)
                        .expectNoRecords("Android.DragDrop.Tab.Duration.WithinDestStrip")
                        .build();
        new DragEventInvoker()
                .dragLocationY(mSourceInstance, 3 * DRAG_MOVE_DISTANCE) // move to toolbar
                .verifyShadowVisibility(true)
                .dragLocationY(mSourceInstance, -3 * DRAG_MOVE_DISTANCE) // move back to strip
                .verifyShadowVisibility(false)
                .drop(mSourceInstance)
                .end(true);

        // Verify appropriate events are generated.
        // Strip prepares for drop on drag enter. Entered twice.
        verify(mSourceStripLayoutHelper, times(2))
                .prepareForTabDrop(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyBoolean());
        // Stop reorder on drop.
        verify(mSourceStripLayoutHelper, times(1)).onUpOrCancel(anyLong());
        // Verify tab is not moved.
        verify(mSourceMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mSourceMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        // Verify clear.
        verify(mSourceStripLayoutHelper, times(1)).clearTabDragState();
        // Verify destination strip not invoked.
        verifyNoInteractions(mDestStripLayoutHelper);
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario G.1 */
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

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario G.2 */
    @Test
    public void test_onDrag_invalidClipData() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder().expectNoRecords("Android.DragDrop.Tab.Type").build();
        // Set state.
        DragDropGlobalState.store(CURR_INSTANCE_ID, mock(ChromeDropDataAndroid.class), null);

        // Trigger drop with invalid tabId.
        mSourceInstance.onDrag(
                mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY));
        mSourceInstance.onDrag(
                mTabsToolbarView,
                mockDragEvent(
                        DragEvent.ACTION_DROP,
                        POS_X,
                        mPosY,
                        MockTab.createAndInitialize(TAB_ID_NOT_DRAGGED, mProfile)));
        mSourceInstance.onDrag(
                mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY));

        // Verify - Move to new window not invoked.
        verify(mDestMultiInstanceManager, times(0))
                .moveTabToWindow(any(), eq(mTabBeingDragged), anyInt());
        histogramExpectation.assertExpected();
    }

    /** Test for {@link #ONDRAG_TEST_CASES} - Scenario G.3 */
    @Test
    public void test_onDrag_destinationStripNotVisible() {
        mTabStripVisible = false;

        // Start tab drag action.
        mSourceInstance.startTabDragAction(
                mTabsToolbarView,
                mTabBeingDragged,
                new PointF(POS_X, mPosY),
                TAB_POSITION_X,
                TAB_WIDTH);

        boolean res =
                mDestInstance.onDrag(
                        mTabsToolbarView,
                        mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY));
        assertFalse("onDrag should return false.", res);
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

    private void testNonLastTabDroppedInStripHistogram() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DragDrop.Tab.SourceWindowClosed", false);
        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(false);

        invokeDropInDestinationStrip(true);

        histogramExpectation.assertExpected();
    }

    @Test
    public void testHistogram_lastTabDroppedInStripClosesWindow_source() {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DragDrop.Tab.SourceWindowClosed", true);
        // When the last tab is dragged/dropped, the source window will be closed.
        when(mSourceMultiInstanceManager.closeChromeWindowIfEmpty(anyInt())).thenReturn(true);

        invokeDropInDestinationStrip(true);

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
        new DragEventInvoker().dragExit(mSourceInstance).end(false);

        histogramExpectation.assertExpected();
    }

    /** Tests fix for crash reported in crbug.com/327449234. */
    @Test
    public void test_onDrag_nullClipDescription() {
        // Mock drag event with null ClipDescription.
        DragEvent event = mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        when(event.getClipDescription()).thenReturn(null);

        // No exception should be thrown when #onDragStart is invoked.
        assertFalse(
                "#onDragStart should not be handled when ClipDescription is null.",
                mSourceInstance.onDrag(mTabsToolbarView, event));
    }

    private void invokeDropInDestinationStrip(boolean dragEndRes) {
        new DragEventInvoker()
                .dragExit(mSourceInstance)
                .verifyShadowVisibility(true)
                .dragEnter(mDestInstance)
                .verifyShadowVisibility(true)
                .drop(mDestInstance)
                .end(dragEndRes);
    }

    private void mockStripTabPosition(float tabWidth, float drawX, int tabIndex) {
        StripLayoutTab stripTab = mock(StripLayoutTab.class);
        when(stripTab.getWidth()).thenReturn(tabWidth);
        when(stripTab.getDrawX()).thenReturn(drawX);
        when(stripTab.getTabId()).thenReturn(10);
        when(mDestStripLayoutHelper.getTabAtPosition(POS_X)).thenReturn(stripTab);
    }

    class DragEventInvoker {
        DragEventInvoker() {
            // Start tab drag action.
            mSourceInstance.startTabDragAction(
                    mTabsToolbarView,
                    mTabBeingDragged,
                    new PointF(POS_X, mPosY),
                    TAB_POSITION_X,
                    TAB_WIDTH);
            // drag invokes DRAG_START and DRAG_ENTER on source and DRAG_START on destination.
            mSourceInstance.onDrag(
                    mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY));
            mDestInstance.onDrag(
                    mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY));

            mSourceInstance.onDrag(
                    mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY));
            // Move within the tab strip area to set lastX / lastY.
            mSourceInstance.onDrag(
                    mTabsToolbarView,
                    mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X + 10, mPosY));
            // Verify shadow is not visible.
            verifyShadowVisibility(false);
        }

        public DragEventInvoker dragLocationY(TabDragSource instance, float distance) {
            mPosY += distance;
            instance.onDrag(
                    mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X, mPosY));
            return this;
        }

        public DragEventInvoker dragExit(TabDragSource instance) {
            instance.onDrag(mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_EXITED, 0, 0));
            return this;
        }

        public DragEventInvoker dragEnter(TabDragSource instance) {
            mPosY = mTabStripHeight - 2 * DRAG_MOVE_DISTANCE;
            instance.onDrag(
                    mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY));
            // Also trigger DRAG_LOCATION following DRAG_ENTERED since enter is no-op in
            // implementation.
            instance.onDrag(
                    mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X, mPosY));
            return this;
        }

        public DragEventInvoker drop(TabDragSource instance) {
            instance.onDrag(mTabsToolbarView, mockDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY));
            return this;
        }

        public DragEventInvoker end(boolean res) {
            mSourceInstance.onDrag(mTabsToolbarView, mockDragEndEvent(res));
            mDestInstance.onDrag(mTabsToolbarView, mockDragEndEvent(res));
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

    private DragEvent mockDragEndEvent(boolean res) {
        DragEvent dragEvent = mockDragEvent(DragEvent.ACTION_DRAG_ENDED, 0f, 0f);
        when(dragEvent.getResult()).thenReturn(res);
        return dragEvent;
    }

    private DragEvent mockDragEvent(int action, float x, float y) {
        return mockDragEvent(action, x, y, mTabBeingDragged);
    }

    private DragEvent mockDragEvent(int action, float x, float y, Tab tab) {
        ChromeDropDataAndroid dropData = new ChromeDropDataAndroid.Builder().withTab(tab).build();
        DragEvent event = mock(DragEvent.class);
        when(event.getAction()).thenReturn(action);
        when(event.getX()).thenReturn(x);
        when(event.getY()).thenReturn(y);
        when(event.getClipData())
                .thenReturn(
                        new ClipData(
                                null,
                                SUPPORTED_MIME_TYPES,
                                new Item(dropData.buildTabClipDataText(), null)));
        when(event.getClipDescription()).thenReturn(new ClipDescription("", SUPPORTED_MIME_TYPES));
        return event;
    }

    private void callAndVerifyAllowTabDragToCreateInstance(
            boolean expectedAllowTabDragToCreateInstance) {
        // Verify.
        assertTrue(
                "Tab drag should start.",
                mSourceInstance.startTabDragAction(
                        mTabsToolbarView,
                        mTabBeingDragged,
                        DRAG_START_POINT,
                        TAB_POSITION_X,
                        TAB_WIDTH));
        var dropDataCaptor = ArgumentCaptor.forClass(ChromeDropDataAndroid.class);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        dropDataCaptor.capture());
        assertEquals(
                "DropData.allowTabDragToCreateInstance value is not as expected.",
                expectedAllowTabDragToCreateInstance,
                dropDataCaptor.getValue().allowTabDragToCreateInstance);
    }
}
