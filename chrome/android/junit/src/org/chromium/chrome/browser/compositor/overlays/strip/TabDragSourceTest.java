// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Point;
import android.graphics.PointF;
import android.os.Parcel;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.View.OnDragListener;
import android.view.ViewGroup.MarginLayoutParams;

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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.dragdrop.DragDropGlobalState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** Tests for {@link TabDragSource}. */
@EnableFeatures(ChromeFeatureList.TAB_LINK_DRAG_DROP_ANDROID)
@RunWith(BaseRobolectricTestRunner.class)
public class TabDragSourceTest {

    public static final int CURR_INSTANCE_ID = 100;
    public static final int TAB_ID = 1;
    private static final int ANOTHER_INSTANCE_ID = 101;
    @Rule public MockitoRule mMockitoProcessorRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private DragAndDropDelegate mDragDropDelegate;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private StripLayoutHelper mStripLayoutHelper;
    @Mock private Profile mProfile;

    private Activity mActivity;
    private TabDragSource mTabDragSource;
    private View mTabsToolbarView;
    private Tab mTabBeingDragged;
    private static final float TAB_STRIP_HEIGHT = 22.f;
    private static final float TAB_STRIP_X_START = 80.f;
    private static final float TAB_STRIP_Y_START = 7.f;
    private static final float TAB_STRIP_Y_STEP = 10.f;
    private static final float TAB_X_OFFSET = 80.f;
    private static final float TAB_Y_OFFSET_WITHIN = TAB_STRIP_HEIGHT - 1.f;
    private static final float TAB_Y_OFFSET_OUTSIDE = TAB_STRIP_HEIGHT + 1.f;
    private static final float DROP_X_SCREEN_POS = 1000.f;
    private static final float DROP_Y_SCREEN_POS = 500.f;
    private static final PointF DRAG_START_POINT = new PointF(TAB_X_OFFSET, TAB_Y_OFFSET_WITHIN);

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);

        // Create and spy on a simulated tab view.
        mTabsToolbarView = new View(mActivity);
        mTabsToolbarView.setLayoutParams(new MarginLayoutParams(150, 50));

        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        mTabBeingDragged = MockTab.createAndInitialize(TAB_ID, mProfile);
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID);

        mTabDragSource =
                new TabDragSource(
                        mTabsToolbarView,
                        mMultiInstanceManager,
                        mDragDropDelegate,
                        mBrowserControlsStateProvider);
    }

    @After
    public void reset() {
        DragDropGlobalState.getInstance().reset();
    }

    private DragEvent createDragEvent(int action, float x, float y, int result) {
        Parcel parcel = Parcel.obtain();
        parcel.writeInt(action);
        parcel.writeFloat(x);
        parcel.writeFloat(y);
        parcel.writeInt(result); // Result
        parcel.writeInt(0); // No Clipdata
        parcel.writeInt(0); // No Clip Description
        parcel.setDataPosition(0);
        return DragEvent.CREATOR.createFromParcel(parcel);
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
                        mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);
        assertTrue("startTabDragAction returned false.", res);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
        assertEquals(
                "Global state instanceId not set",
                CURR_INSTANCE_ID,
                DragDropGlobalState.getInstance().dragSourceInstanceId);
        assertEquals(
                "Global state tabBeingDragged not set",
                mTabBeingDragged,
                DragDropGlobalState.getInstance().tabBeingDragged);
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
                        mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);
        assertTrue("startTabDragAction returned false.", res);
        verify(mDragDropDelegate)
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
        assertEquals(
                "Global state instanceId not set",
                CURR_INSTANCE_ID,
                DragDropGlobalState.getInstance().dragSourceInstanceId);
        assertEquals(
                "Global state tabBeingDragged not set",
                mTabBeingDragged,
                DragDropGlobalState.getInstance().tabBeingDragged);
    }

    @Test
    public void test_startTabDragAction_ReturnsFalseForInvalidTab() {
        // Act and verify.
        boolean res =
                mTabDragSource.startTabDragAction(
                        mTabsToolbarView, mStripLayoutHelper, null, DRAG_START_POINT);
        assertFalse("startTabDragAction returned true.", res);
        verify(mDragDropDelegate, never())
                .startDragAndDrop(
                        eq(mTabsToolbarView),
                        any(DragShadowBuilder.class),
                        any(DropDataAndroid.class));
    }

    @Test
    public void test_OnDragListenerImpl_SimulateDragDropWithinStripLayout_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement within the strip layout.
        simulateDragDropEvents(/* withinStripLayout= */ true);

        // Verify appropriate events are generated to simulate movement within the strip layout.
        verify(mStripLayoutHelper, times(1))
                .onDownInternal(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyInt());
        verify(mStripLayoutHelper, times(4))
                .drag(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
        verify(mStripLayoutHelper, times(TAB_ID)).onUpOrCancel(anyLong());
    }

    @Test
    public void test_OnDragListenerImpl_SimulateDragDropOutsideStripLayout_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        simulateDragDropEvents(/* withinStripLayout= */ false);

        // Verify appropriate events are generated to simulate movement outside the strip layout.
        verify(mStripLayoutHelper, times(TAB_ID))
                .onDownInternal(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyInt());
        verify(mStripLayoutHelper, times(5))
                .drag(
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat());
        verify(mStripLayoutHelper, times(TAB_ID)).onUpOrCancel(anyLong());
    }

    @Test
    public void
            test_OnDragListenerImpl_ForWithinStripMovement_NoNewWindowIsOpened_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement within the strip layout.
        simulateDragDropEvents(/* withinStripLayout= */ true);

        // Verify Since the drop is within the TabToolbar then no tab is move out to a
        // new/existing Chrome Window.
        verify(mMultiInstanceManager, times(0)).moveTabToNewWindow(mTabBeingDragged);
    }

    @Test
    public void test_OnDragListenerImpl_ForOutsideStripMovement_NewWindowIsOpened_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        simulateDragDropEvents(/* withinStripLayout= */ false);

        // Verify Since the drop is outside the TabToolbar area the tab will be move to a new
        // Chrome Window.
        verify(mMultiInstanceManager, times(TAB_ID)).moveTabToNewWindow(mTabBeingDragged);
    }

    @Test
    public void test_clearActiveClickedTab_SimulateDragDrop_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation action.
        simulateDragDropEvents(true);

        // Verify
        verify(mStripLayoutHelper, times(TAB_ID)).clearActiveClickedTab();
    }

    // Simulates drag n drop action events moving within or outside the strip.
    private void simulateDragDropEvents(boolean withinStripLayout) {
        // Mock same source
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID);

        OnDragListener dragListener = mTabDragSource.getDragListenerForTesting();
        if (withinStripLayout) {
            eventsWithinStripLayout(dragListener);
        } else {
            eventsOutsideStripLayout(dragListener);
        }

        if (withinStripLayout) {
            dragListener.onDrag(
                    mTabsToolbarView, createDragEvent(DragEvent.ACTION_DRAG_ENDED, 0f, 0f, 0));
        } else {
            dragListener.onDrag(
                    mTabsToolbarView,
                    createDragEvent(
                            DragEvent.ACTION_DRAG_ENDED, DROP_X_SCREEN_POS, DROP_Y_SCREEN_POS, 0));
        }
    }

    // Drag n drop action for moving within the tab strip.
    private void eventsWithinStripLayout(OnDragListener onTabDragListener) {
        // The tab movement is performed by generating the motion events as if the user is moving a
        // tab. Within the tab strip movement is when horizontal/x position stays within the strip
        // width and vertical/y stays within the strip height. The top left of the strip layout is
        // (0f, 0f). For verification the y-offset is zero.
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_STARTED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_ENTERED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));

        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + TAB_X_OFFSET,
                        TAB_STRIP_Y_START,
                        0));
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + 2 * TAB_X_OFFSET,
                        TAB_STRIP_Y_START,
                        0));
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + 3 * TAB_X_OFFSET,
                        TAB_STRIP_Y_START,
                        0));
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + 4 * TAB_X_OFFSET,
                        TAB_STRIP_Y_START,
                        0));
        // Total distance moved so far by pointer is still within the width of the tab strip.

        onTabDragListener.onDrag(
                mTabsToolbarView, createDragEvent(DragEvent.ACTION_DROP, 0f, 0f, 0));
    }

    // Drag n drop action for moving outside the tab strip.
    private void eventsOutsideStripLayout(OnDragListener onTabDragListener) {
        // The tab movement is performed by generating the motion events as if the user is moving a
        // tab. Outside the tab strip movement here is when horizontal/x position stays constant
        // within the strip width and vertical/y position goes beyond the strip height (below the
        // strip in this case)). The top left of the strip layout is (0f, 0f). For verification the
        // x-offset is zero.

        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_STARTED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_ENTERED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START,
                        TAB_STRIP_Y_START + TAB_STRIP_Y_STEP,
                        0)); // Move within the tabs area.
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START,
                        TAB_Y_OFFSET_WITHIN,
                        0)); // Still within the tabs area.
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START,
                        TAB_Y_OFFSET_OUTSIDE,
                        0)); // Outside the tabs area but inside the toolbars.
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START,
                        TAB_Y_OFFSET_WITHIN,
                        0)); // Back to within the tabs area.
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START,
                        TAB_STRIP_Y_START + TAB_Y_OFFSET_OUTSIDE,
                        0));
        // Total distance moved by pointer is beyond height of the tab strip.

        // This event will indicate the the pointer has moved outside of the TabToolbar view and no
        // more ACTION_DRAG_LOCATION and ACTION_DROP events will be received too.
        onTabDragListener.onDrag(
                mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_EXITED,
                        TAB_STRIP_X_START,
                        TAB_STRIP_Y_START + TAB_Y_OFFSET_OUTSIDE,
                        0));
    }

    @Test
    @Config(qualifiers = "sw600dp-w600dp")
    public void test_canAcceptTabDrop_SimulateDragDrops_ReturnsSuccess() {

        // Perform drag n drop simulation action as if dropped on another Chrome Window and verify.
        // Drop event on another Chrome window in the in top half.
        DragDropGlobalState.getInstance().dragSourceInstanceId = CURR_INSTANCE_ID;
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);
        TabDragSource.OnDragListenerImpl dragListener = mTabDragSource.getDragListenerForTesting();
        boolean res =
                dragListener.canAcceptTabDrop(createDragEvent(DragEvent.ACTION_DROP, 150f, 10f, 0));
        assertTrue("Tab drop should be accepted in another tabs toolbar view.", res);
        assertTrue(
                "After the drop event on top half accept next should be true.",
                DragDropGlobalState.getInstance().acceptNextDrop);
        DragDropGlobalState.getInstance().reset();

        // Trigger another drop event on the same, not drag source, Chrome window in the bottom
        // half.
        DragDropGlobalState.getInstance().dragSourceInstanceId = CURR_INSTANCE_ID;
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(ANOTHER_INSTANCE_ID);
        res = dragListener.canAcceptTabDrop(createDragEvent(DragEvent.ACTION_DROP, 150f, 45f, 0));
        assertTrue(
                "Tab drop should be accepted in the bottom half but the flag is set to"
                        + " ignore drop.",
                res);
        assertFalse(
                "After the drop event in the bottom half, the next accept should not be true.",
                DragDropGlobalState.getInstance().acceptNextDrop);
        DragDropGlobalState.getInstance().reset();

        // Perform ACTION_DROP event on the same tabs view as the source view
        DragDropGlobalState.getInstance().dragSourceInstanceId = CURR_INSTANCE_ID;
        when(mMultiInstanceManager.getCurrentInstanceId()).thenReturn(CURR_INSTANCE_ID);
        res = dragListener.canAcceptTabDrop(createDragEvent(DragEvent.ACTION_DROP, 150f, 35f, 0));
        assertFalse("Tab drop should be ignored if it is the same as drag source toolbar", res);
        assertFalse(
                "After the drop event in the bottom half, the next accept should not be true.",
                DragDropGlobalState.getInstance().acceptNextDrop);
        DragDropGlobalState.getInstance().reset();
    }

    @Test
    public void test_sendPositionInfoToSysUI_WithNewWindowIsOpened_ReturnsSuccess() {
        // Call startDrag to set class variables.
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        simulateDragDropEvents(/* withinStripLayout= */ false);

        // Verify Since the drop is outside the TabToolbar area verify new window opened
        // TODO (crbug.com/1495815): check if intent is send to SysUI to position the window.
        verify(mMultiInstanceManager).moveTabToNewWindow(mTabBeingDragged);
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
        mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, mTabBeingDragged, dragStartPoint);

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
}
