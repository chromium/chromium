// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import android.content.pm.PackageManager.NameNotFoundException;
import android.graphics.Point;
import android.graphics.PointF;
import android.os.Build.VERSION_CODES;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.dragdrop.DragDropGlobalState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DropDataAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link TabDragSource}. */
@EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp", sdk = VERSION_CODES.S)
public class TabDragSourceTest {

    public static final int CURR_INSTANCE_ID = 100;
    public static final int TAB_ID = 1;
    private static final float POS_X = 20f;
    private static final float DRAG_MOVE_DISTANCE = 5f;
    private static final String[] SUPPORTED_MIME_TYPES = {"chrome/tab"};
    private static final int TAB_ID_NOT_DRAGGED = 2;
    private float mPosY;
    @Rule public MockitoRule mMockitoProcessorRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private DragAndDropDelegate mDragDropDelegate;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private TabContentManager mTabContentManager;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private StripLayoutHelper mStripLayoutHelper;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TestTabModel mTabModel;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private MultiWindowUtils mMultiWindowUtils;

    private Activity mActivity;
    private TabDragSource mTabDragSource;
    private ViewGroup mTabsToolbarView;
    private Tab mTabBeingDragged;
    private static final float DROP_X_SCREEN_POS = 1000.f;
    private static final float DROP_Y_SCREEN_POS = 500.f;
    private static final PointF DRAG_START_POINT = new PointF(0, 0);
    private int mTabStripHeight;

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() throws NameNotFoundException {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);
        mTabStripHeight = mActivity.getResources().getDimensionPixelSize(R.dimen.tab_strip_height);
        mPosY = mTabStripHeight - 2 * DRAG_MOVE_DISTANCE;

        // Create and spy on a simulated tab view.
        mTabsToolbarView = new FrameLayout(mActivity);
        mTabsToolbarView.setLayoutParams(new MarginLayoutParams(150, 50));

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        mTabBeingDragged = MockTab.createAndInitialize(TAB_ID, mProfile);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        when(mMultiWindowUtils.isMoveToOtherWindowSupported(any(), any())).thenReturn(true);
        MultiWindowUtils.setInstanceForTesting(mMultiWindowUtils);
        MultiWindowTestUtils.enableMultiInstance();

        mTabDragSource =
                new TabDragSource(
                        mActivity,
                        () -> mStripLayoutHelper,
                        () -> mTabContentManager,
                        () -> mLayerTitleCache,
                        mMultiInstanceManager,
                        mDragDropDelegate,
                        mBrowserControlsStateProvider,
                        mWindowAndroid);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        mTabDragSource.setTabModelSelector(mTabModelSelector);
    }

    @After
    public void reset() {
        DragDropGlobalState.getInstance().reset();
    }

    @EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
    @DisableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    @Test
    public void test_startTabDragAction_withTabDragDropFF_ReturnsTrueForValidTab() {
        when(mDragDropDelegate.startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class)))
                .thenReturn(true);
        // Act and verify.
        boolean res =
                mTabDragSource.startTabDragAction(
                        mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT);
        assertTrue("startTabDragAction returned false.", res);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
        assertEquals(
                "Global state instanceId not set.",
                CURR_INSTANCE_ID,
                DragDropGlobalState.getInstance().dragSourceInstanceId);
        assertEquals(
                "Global state tabBeingDragged not set.",
                mTabBeingDragged,
                DragDropGlobalState.getInstance().tabBeingDragged);
        assertNotNull(
                "Shadow view is unexpectedly null.", mTabDragSource.getShadowViewForTesting());
    }

    @DisableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    @Test
    public void test_startTabDragAction_withTabLinkDragDropFF_ReturnsTrueForValidTab() {
        when(mDragDropDelegate.startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class)))
                .thenReturn(true);
        // Act and verify.
        boolean res =
                mTabDragSource.startTabDragAction(
                        mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT);
        assertTrue("startTabDragAction returned false.", res);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
        assertEquals(
                "Global state instanceId not set.",
                CURR_INSTANCE_ID,
                DragDropGlobalState.getInstance().dragSourceInstanceId);
        assertEquals(
                "Global state tabBeingDragged not set.",
                mTabBeingDragged,
                DragDropGlobalState.getInstance().tabBeingDragged);
        assertNotNull(
                "Shadow view is unexpectedly null.", mTabDragSource.getShadowViewForTesting());
    }

    @Test
    public void test_startTabDragAction_ExceptionForInvalidTab() {
        assertThrows(
                NullPointerException.class,
                () -> mTabDragSource.startTabDragAction(mTabsToolbarView, null, DRAG_START_POINT));
    }

    @EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
    @DisableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    @Test
    public void test_startTabDragAction_withMoveToOtherWindowNotSupported_ReturnsFalse() {
        when(mMultiWindowUtils.isMoveToOtherWindowSupported(any(), any())).thenReturn(false);
        assertFalse(
                "Should not startTabDragAction when move to other window is not supported",
                mTabDragSource.startTabDragAction(
                        mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT));
    }

    @Test
    public void test_DragDropWithinStrip_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT);

        /**
         * Perform drag n drop simulation actions for movement within the strip layout. Drag start
         * -> enter -> 2 moves within strip -> drop -> end.
         */
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X + DRAG_MOVE_DISTANCE, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X + 2 * DRAG_MOVE_DISTANCE, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, 0f, 0f);

        // Verify appropriate events are generated to simulate movement within the strip layout.
        // Drag tab onto strip on drag enter.
        verify(mStripLayoutHelper, times(1)).dragActiveClickedTabOntoStrip(anyLong(), anyFloat());
        // Invoke drag on drag moves.
        verify(mStripLayoutHelper, times(2)).drag(anyLong(), anyFloat(), anyFloat(), anyFloat());
        // Stop reorder on drop.
        verify(mStripLayoutHelper, times(1)).onUpOrCancel(anyLong());
        // Verify tab is not moved.
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        // Verify clear.
        verify(mStripLayoutHelper, times(1)).clearActiveClickedTab();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void test_DragOutsideStrip_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT);

        /**
         * Perform drag n drop simulation actions for movement outside the strip area. Drag start ->
         * enter -> move within strip -> move out of strip but within toolbar -> move back to strip
         * -> exit.
         */
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY);
        // Move within the tab strip area.
        triggerDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X, mPosY + DRAG_MOVE_DISTANCE);
        // Move outside the tab strip area but inside the toolbar.
        triggerDragEvent(DragEvent.ACTION_DRAG_LOCATION, POS_X, mPosY + 3 * DRAG_MOVE_DISTANCE);
        triggerDragEvent(
                DragEvent.ACTION_DRAG_LOCATION,
                POS_X,
                mPosY + 2 * DRAG_MOVE_DISTANCE); // Back to within the tabs area.
        triggerDragEvent(
                DragEvent.ACTION_DRAG_EXITED, POS_X, mTabStripHeight + 10 * DRAG_MOVE_DISTANCE);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, DROP_X_SCREEN_POS, DROP_Y_SCREEN_POS);

        // Verify appropriate events are generated to simulate movement outside the strip area.
        // Drag tab onto strip on drag enter. Enter occurs twice.
        verify(mStripLayoutHelper, times(2)).dragActiveClickedTabOntoStrip(anyLong(), anyFloat());
        // Move within strip.
        verify(mStripLayoutHelper, times(1)).drag(anyLong(), anyFloat(), anyFloat(), anyFloat());
        // Drag tab out of strip on drag exit. Exit occurs twice.
        verify(mStripLayoutHelper, times(2)).dragActiveClickedTabOutOfStrip(anyLong());
        // Verify Since the drop is outside the TabToolbar area the tab will be move to a new
        // Chrome Window.
        verify(mMultiInstanceManager, times(1)).moveTabToNewWindow(mTabBeingDragged);
        // Verify tab cleared.
        verify(mStripLayoutHelper, times(1)).clearActiveClickedTab();
    }

    @Test
    public void test_DropInSourceTabStrip_DontMoveTabToOtherWindow() {
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY);

        // Verify - Move tab is not invoked.
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verify(mMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
    }

    @Test
    public void test_DropInSourceToolbar_NoOp() {
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mTabStripHeight + 5);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mTabStripHeight + 5);

        verifyNoInteractions(mStripLayoutHelper);
        verify(mMultiInstanceManager, times(0)).moveTabToWindow(any(), any(), anyInt());
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(any());
    }

    @Test
    public void test_DropInDestinationStripOnFirstHalfOfTab_MoveTabToDestinationAtIndex() {
        // Set state.
        mTabDragSource.setGlobalState(mTabBeingDragged);
        // Simulate destination instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID + 1);
        // Mock strip actions.
        StripLayoutTab stripTab = mock(StripLayoutTab.class);
        float tabWidth = 5f;
        when(stripTab.getWidth()).thenReturn(tabWidth);
        // Tab drop POS_X is at left half of tab.
        when(stripTab.getDrawX()).thenReturn(POS_X - (tabWidth / 2));
        when(stripTab.getId()).thenReturn(10);
        when(mStripLayoutHelper.findIndexForTab(10)).thenReturn(2);
        when(mStripLayoutHelper.getTabAtPosition(POS_X)).thenReturn(stripTab);

        // Trigger drop in tab strip.
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY);

        // Verify - Tab moved to destination window at index.
        verify(mMultiInstanceManager, times(1)).moveTabToWindow(any(), eq(mTabBeingDragged), eq(2));
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void test_DropInDestinationStripOnLaterHalfOfTab_MoveTabToDestinationAtIndex() {
        // Set state.
        mTabDragSource.setGlobalState(mTabBeingDragged);
        // Simulate destination instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID + 1);
        // Mock strip actions.
        StripLayoutTab stripTab = mock(StripLayoutTab.class);
        float tabWidth = 5f;
        when(stripTab.getWidth()).thenReturn(tabWidth);
        // Tab drop POS_X is at right half of tab.
        when(stripTab.getDrawX()).thenReturn(POS_X - (tabWidth / 2 + 1));
        when(stripTab.getId()).thenReturn(10);
        when(mStripLayoutHelper.findIndexForTab(10)).thenReturn(2);
        when(mStripLayoutHelper.getTabAtPosition(POS_X)).thenReturn(stripTab);

        // Trigger drop in tab strip.
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY);

        // Verify - Tab moved to destination window at index.
        verify(mMultiInstanceManager, times(1)).moveTabToWindow(any(), eq(mTabBeingDragged), eq(3));
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
    }

    @Test
    public void test_DropInDestinationTabStripOnNonTab_MoveTabToDestinationWindowAtEnd() {
        // Set state.
        mTabDragSource.setGlobalState(mTabBeingDragged);
        // Simulate destination instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID + 1);
        // Mock strip actions.
        when(mStripLayoutHelper.getTabAtPosition(POS_X)).thenReturn(null);
        when(mTabModel.getCount()).thenReturn(10);

        // Trigger drop in tab strip.
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY);

        // Verify - Tab moved to destination window at index.
        verify(mMultiInstanceManager, times(1))
                .moveTabToWindow(any(), eq(mTabBeingDragged), eq(10));
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
    }

    @Test
    public void test_DropInDestination_WithIncorrectClipData_DoesNotMoveTab() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT);
        // Simulate destination instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID + 1);

        // Trigger drop.
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY, TAB_ID_NOT_DRAGGED);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY);

        // Verify - Move to new window not invoked.
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
        verifyNoInteractions(mStripLayoutHelper);
    }

    @Test
    public void test_DropInDestinationWithDifferentModel_MoveTabToDestinationAtEnd() {
        // Set selector
        mTabDragSource.setTabModelSelector(mTabModelSelector);
        // Set state - tab created in standard model.
        mTabDragSource.setGlobalState(mTabBeingDragged);
        // Simulate destination instance.
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID + 1);
        // Destination tab model is incognito.
        when(mTabModel.isIncognito()).thenReturn(true);
        TabModel standardModelDestination = mock(TabModel.class);
        when(mTabModelSelector.getModel(false)).thenReturn(standardModelDestination);
        when(standardModelDestination.getCount()).thenReturn(5);

        // Mock strip actions - tab exists at drop position.
        StripLayoutTab stripTab = mock(StripLayoutTab.class);
        when(mStripLayoutHelper.getTabAtPosition(POS_X)).thenReturn(stripTab);

        // Trigger drop in tab strip.
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DROP, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, POS_X, mPosY);

        // Verify - Tab moved to destination window at end.
        verify(mMultiInstanceManager, times(1)).moveTabToWindow(any(), eq(mTabBeingDragged), eq(5));
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
    }

    @Test
    public void test_DragStartWithInvalidMime_ReturnsFalse() {
        // Set state.
        mTabDragSource.setGlobalState(mTabBeingDragged);

        DragEvent event = mock(DragEvent.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(event.getX()).thenReturn(POS_X);
        when(event.getY()).thenReturn(mPosY);
        when(event.getClipDescription())
                .thenReturn(new ClipDescription("", new String[] {"some_value"}));

        assertFalse(mTabDragSource.onDrag(mTabsToolbarView, event));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    @DisableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    public void test_onProvideShadowMetrics_WithDesiredStartPosition_ReturnsSuccess() {
        // Prepare
        final float dragStartXPosition = 90f;
        final float dragStartYPosition = 45f;
        final PointF dragStartPoint = new PointF(dragStartXPosition, dragStartYPosition);
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(mTabsToolbarView, mTabBeingDragged, dragStartPoint);

        View.DragShadowBuilder tabDragShadowBuilder =
                mTabDragSource.createTabDragShadowBuilder(mActivity, true);

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
    @DisableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
    @EnableFeatures(ChromeFeatureList.TAB_DRAG_DROP_ANDROID)
    public void test_OnDragEndAfterExit_NewWindowIsOpened() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(mTabsToolbarView, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        triggerDragEvent(DragEvent.ACTION_DRAG_STARTED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENTERED, POS_X, mPosY);
        triggerDragEvent(DragEvent.ACTION_DRAG_EXITED, DROP_X_SCREEN_POS, DROP_Y_SCREEN_POS);
        triggerDragEvent(DragEvent.ACTION_DRAG_ENDED, DROP_X_SCREEN_POS, DROP_Y_SCREEN_POS);

        // Verify Since the drop is outside the TabToolbar area verify new window opened
        // TODO (crbug.com/1495815): check if intent is send to SysUI to position the window.
        verify(mMultiInstanceManager).moveTabToNewWindow(mTabBeingDragged);
    }

    private void triggerDragEvent(int action, float x, float y) {
        triggerDragEvent(action, x, y, TAB_ID);
    }

    private void triggerDragEvent(int action, float x, float y, int tabId) {
        DragEvent event = mock(DragEvent.class);
        when(event.getAction()).thenReturn(action);
        when(event.getX()).thenReturn(x);
        when(event.getY()).thenReturn(y);
        when(event.getClipData())
                .thenReturn(
                        new ClipData(null, SUPPORTED_MIME_TYPES, new Item("TabId=" + tabId, null)));
        when(event.getClipDescription()).thenReturn(new ClipDescription("", SUPPORTED_MIME_TYPES));
        mTabDragSource.onDrag(mTabsToolbarView, event);
    }
}
