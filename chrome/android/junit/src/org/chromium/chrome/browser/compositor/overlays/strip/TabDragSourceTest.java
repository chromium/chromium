// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.atMostOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.graphics.Point;
import android.graphics.PointF;
import android.os.Parcel;
import android.view.ContextThemeWrapper;
import android.view.DragEvent;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link TabDragSource}. */
@RunWith(BaseRobolectricTestRunner.class)
// clang-format off
@EnableFeatures({ChromeFeatureList.TAB_DRAG_DROP_ANDROID})
public class TabDragSourceTest {
    // clang-format on
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private MultiInstanceManager mMultiInstanceManager;
    @Mock
    private LayoutManagerHost mManagerHost;
    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private CompositorButton mModelSelectorBtn;
    @Mock
    private TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private View mToolbarContainerView;
    @Mock
    private TabDropTarget mTabDropTarget;

    private Activity mActivity;
    private Context mContext;
    private TestTabModel mModel = new TestTabModel();
    private StripLayoutHelper mStripLayoutHelper;
    private TabDragSource mTabDragSource;
    private StripLayoutTab mClickedTab;
    private View mTabsToolbarView;
    private static final String[] TEST_TAB_TITLES = {"Tab 1", "Tab 2", "Tab 3", "Tab 4", "Tab 5"};
    private static final float TOOLBAR_CONTAINER_WIDTH = 600.f;
    private static final float TOOLBAR_CONTAINER_HEIGHT = 50.f;
    private static final float TAB_STRIP_HEIGHT = 22.f;
    private static final float TAB_STRIP_X_START = 80.f;
    private static final float TAB_STRIP_Y_START = 7.f;
    private static final float TAB_STRIP_Y_STEP = 10.f;
    private static final float TAB_X_OFFSET = 80.f;
    private static final float TAB_Y_OFFSET_WITHIN = TAB_STRIP_HEIGHT - 1.f;
    private static final float TAB_Y_OFFSET_OUTSIDE = TAB_STRIP_HEIGHT + 1.f;
    private static final float TAB_WIDTH = 150.f;
    private static final long TIMESTAMP = 5000;
    private static final float DROP_X_SCREEN_POS = 1000.f;
    private static final float DROP_Y_SCREEN_POS = 500.f;
    private static final PointF DRAG_START_POINT = new PointF(TAB_X_OFFSET, TAB_Y_OFFSET_WITHIN);

    /** Resets the environment before each test. */
    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);

        // Get and spy on the singleton TabDragSource.
        mTabDragSource = Mockito.spy(TabDragSource.getInstance());

        mTabDropTarget = Mockito.spy(new TabDropTarget(mStripLayoutHelper));

        // Create and spy on a simulated tab view.
        mTabsToolbarView = Mockito.spy(new View(mActivity));
        mTabsToolbarView.setLayoutParams(new MarginLayoutParams(150, 50));
        when(mTabsToolbarView.getWidth()).thenReturn(150);
        when(mTabsToolbarView.getHeight()).thenReturn(50);
    }

    @After
    public void tearDown() {
        if (mStripLayoutHelper != null) {
            mStripLayoutHelper.stopReorderModeForTesting();
            mStripLayoutHelper.setTabAtPositionForTesting(null);
        }
        if (mTabDragSource != null) {
            mTabDragSource.resetTabDragSource();
            mTabDragSource = null;
        }
        mTabDropTarget = null;
        mTabsToolbarView = null;
    }

    private void initializeTest(boolean rtl, boolean incognito, int tabIndex, int numTabs) {
        mStripLayoutHelper = Mockito.spy(createStripLayoutHelper(rtl, incognito));
        mStripLayoutHelper.disableAnimationsForTesting();
        for (int i = 0; i < numTabs; i++) {
            mModel.addTab(TEST_TAB_TITLES[i]);
            when(mTabGroupModelFilter.getRootId(eq(mModel.getTabAt(i)))).thenReturn(i);
        }
        mModel.setIndex(tabIndex);
        mStripLayoutHelper.setTabModel(mModel, null, false);
        mStripLayoutHelper.setTabGroupModelFilter(mTabGroupModelFilter);
        mStripLayoutHelper.tabSelected(0, tabIndex, 0, false);
        mStripLayoutHelper.onSizeChanged(
                TOOLBAR_CONTAINER_WIDTH, TOOLBAR_CONTAINER_HEIGHT, false, TIMESTAMP);
        mStripLayoutHelper.updateLayout(TIMESTAMP);
        StripLayoutTab[] tabs = new StripLayoutTab[mModel.getCount()];
        for (int i = 0; i < numTabs; i++) {
            tabs[i] = mockStripTab(i, TAB_WIDTH);
        }
        mStripLayoutHelper.setStripLayoutTabsForTesting(tabs);
        mClickedTab = tabs[tabIndex];

        assertTrue(mStripLayoutHelper.getStripLayoutTabsForTesting().length == numTabs);
    }

    private StripLayoutHelper createStripLayoutHelper(boolean rtl, boolean incognito) {
        LocalizationUtils.setRtlForTesting(rtl);
        final StripLayoutHelper stripLayoutHelper =
                new StripLayoutHelper(mActivity, mManagerHost, mUpdateHost, mRenderHost, incognito,
                        mModelSelectorBtn, mMultiInstanceManager, mTabsToolbarView);
        stripLayoutHelper.onContextChanged(mActivity);
        return stripLayoutHelper;
    }

    private StripLayoutTab mockStripTab(int id, float tabWidth) {
        StripLayoutTab tab = mock(StripLayoutTab.class);
        when(tab.getWidth()).thenReturn(tabWidth);
        when(tab.getId()).thenReturn(id);
        return tab;
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

    /**
     * Tests the method {@link TabDragSource#startTabDragAction()}.
     *
     * Checks that it successfully starts drag action process.
     */
    @Test
    public void test_startTabDragAction_ReturnsTrueForValidTab() {
        initializeTest(false, false, 1, 5);
        Tab tabBeingDragged = mStripLayoutHelper.getTabById(mClickedTab.getId());

        // Act and verify.
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        assertTrue("Failed to start the tag drag action.",
                mTabDragSource.startTabDragAction(
                        mTabsToolbarView, mStripLayoutHelper, tabBeingDragged, DRAG_START_POINT));
        verify(mTabsToolbarView, atLeastOnce()).startDragAndDrop(any(), any(), any(), anyInt());
        verify(mTabsToolbarView, atMostOnce()).startDragAndDrop(any(), any(), any(), anyInt());
    }

    /**
     * Tests the method {@link TabDragSource#startTabDragAction()}.
     *
     * Checks that it fails to starts drag acion process.
     */
    @Test
    public void test_startTabDragAction_ReturnsFalseForInvalidTab() {
        initializeTest(false, false, 1, 5);

        // Create a StripLayoutTab with bad id, do fail to not get a tab.
        StripLayoutTab invalidIdStripTab = mStripLayoutHelper.createStripTab(Tab.INVALID_TAB_ID);
        Tab tabBeingDragged = mStripLayoutHelper.getTabById(invalidIdStripTab.getId());

        // Act and verify.
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        assertFalse(mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, tabBeingDragged, DRAG_START_POINT));
        verify(mTabsToolbarView, never()).startDragAndDrop(any(), any(), any(), anyInt());
    }

    /**
     * Tests the method {@link TabDragSource#prepareForDragDrop()}.
     *
     * Checks that it successfully prepares for the drag process.
     */
    @Test
    public void test_prepareForDragDrop_ReturnTrueForSettingListenerOnce() {
        // Check state
        assertTrue(mTabDragSource.getOnDragListenerImpl() == null);

        // Act
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);

        // Verify flow.
        assertTrue(mTabDropTarget.getDropContentReceiver() != null);
        assertTrue(mTabDragSource.getOnDragListenerImpl() != null);
        verify(mTabsToolbarView, atLeastOnce()).setOnDragListener(any());
        verify(mTabsToolbarView, atMostOnce()).setOnDragListener(any());
        assertTrue(mTabDragSource.getPxToDp() != 0.0f);
    }

    /**
     * Tests the method {@link TabDragSource#getDragSourceTabsToolbarHashCode()}.
     *
     * Checks that it successfully gets the source vew hashcode.
     */
    @Test
    public void test_getDragSourceTabsToolbarHashCode_ReturnHashCodeAfterDragAction() {
        initializeTest(false, false, 1, 5);
        Tab tabBeingDragged = mStripLayoutHelper.getTabById(mClickedTab.getId());

        // Act and verify.
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        assertTrue(mTabDragSource.getDragSourceTabsToolbarHashCode() == 0);
        assertTrue(mTabDragSource.startTabDragAction(
                mTabsToolbarView, mStripLayoutHelper, tabBeingDragged, DRAG_START_POINT));
        assertTrue(mTabDragSource.getDragSourceTabsToolbarHashCode()
                == System.identityHashCode(mTabsToolbarView));
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#OnDragListenerImpl}.
     *
     * Checks that it successfully sends the drag events to the {@link StripLayoutHelper}.
     */
    @Test
    public void test_OnDragListenerImpl_SimulateDragDropWithinStripLayout_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement within the strip layout.
        simulateDragDropEvents(/*withinStripLayout*/ true);

        // Verify appropriate events are generated to simulate movement within the strip layout.
        verify(mStripLayoutHelper, times(1))
                .onDownInternal(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyInt());
        verify(mStripLayoutHelper, times(4))
                .drag(anyLong(), anyFloat(), anyFloat(), anyFloat(), anyFloat(), anyFloat(),
                        anyFloat());
        verify(mStripLayoutHelper, times(1)).onUpOrCancel(anyLong());
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#OnDragListenerImpl}.
     *
     * Checks that it successfully sends the drag events to the {@link StripLayoutHelper}.
     */
    @Test
    public void test_OnDragListenerImpl_SimulateDragDropOutsideStripLayout_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        simulateDragDropEvents(/*withinStripLayout*/ false);

        // Verify appropriate events are generated to simulate movement outside the strip layout.
        verify(mStripLayoutHelper, times(1))
                .onDownInternal(anyLong(), anyFloat(), anyFloat(), anyBoolean(), anyInt());
        verify(mStripLayoutHelper, times(5))
                .drag(anyLong(), anyFloat(), anyFloat(), anyFloat(), anyFloat(), anyFloat(),
                        anyFloat());
        verify(mStripLayoutHelper, times(1)).onUpOrCancel(anyLong());
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#OnDragListenerImpl}.
     *
     * Checks that no new Chrome window is opened when the drag movement is within the srip layout.
     */
    @Test
    public void
    test_OnDragListenerImpl_ForWithinStripMovement_NoNewWindowIsOpened_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement within the strip layout.
        simulateDragDropEvents(/*withinStripLayout*/ true);

        // Verify
        // Since the drop is within the TabToolbar then no tab is move out to a new/existing Chrome
        // Window.
        verify(mTabDragSource, times(0)).openTabInNewWindow();
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#OnDragListenerImpl}.
     *
     * Checks that a new Chrome window is opened when the drag movement is outside the strip layout.
     */
    @Test
    public void test_OnDragListenerImpl_ForOutsideStripMovement_NewWindowIsOpened_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        simulateDragDropEvents(/*withinStripLayout*/ false);

        // Verify
        // Since the drop is outside the TabToolbar area the tab will be move to a new Chrome
        // Window.
        verify(mTabDragSource, times(1)).openTabInNewWindow();
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#OnDragListenerImpl}.
     * Verifies that {@link OnDragListenerImpl#resetState} is called to reset the start of
     * simulation action.
     */
    @Test
    public void test_clearActiveClickedTab_SimulateDragDrop_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);

        // Perform drag n drop simulation action.
        TabDragSource.OnDragListenerImpl onTabDragListener = simulateDragDropEvents(true);

        // Verify
        verify(onTabDragListener, times(1)).resetState();
        verify(mStripLayoutHelper, times(1)).clearActiveClickedTab();
    }

    // Simulates drag n drop action events moving within or outside the strip.
    private TabDragSource.OnDragListenerImpl simulateDragDropEvents(boolean withinStripLayout) {
        // Get the drag listener first and then call the action to send events.
        TabDragSource.OnDragListenerImpl onTabDragListener =
                Mockito.spy(mTabDragSource.getOnDragListenerImpl());
        assertTrue("OnDragListener is not set in TabDragSource.", onTabDragListener != null);

        if (withinStripLayout) {
            eventsWithinStripLayout(onTabDragListener);
        } else {
            eventsOutsideStripLayout(onTabDragListener);
        }

        assertTrue("Toolbar hash code should match.",
                mTabDragSource.getDragSourceTabsToolbarHashCode()
                        == System.identityHashCode(mTabsToolbarView));
        if (withinStripLayout) {
            onTabDragListener.onDrag(
                    mTabsToolbarView, createDragEvent(DragEvent.ACTION_DRAG_ENDED, 0f, 0f, 0));
        } else {
            onTabDragListener.onDrag(mTabsToolbarView,
                    createDragEvent(
                            DragEvent.ACTION_DRAG_ENDED, DROP_X_SCREEN_POS, DROP_Y_SCREEN_POS, 0));
        }
        assertTrue("Toolbar hash code should be cleared.",
                mTabDragSource.getDragSourceTabsToolbarHashCode() == 0);

        return onTabDragListener;
    }

    // Drag n drop action for moving within the tab strip.
    private void eventsWithinStripLayout(TabDragSource.OnDragListenerImpl onTabDragListener) {
        // The tab movement is performed by generating the motion events as if the user is moving a
        // tab. Within the tab strip movement is when horizontal/x position stays within the strip
        // width and vertical/y stays within the strip height. The top left of the strip layout is
        // (0f, 0f). For verification the y-offset is zero.
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_STARTED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_ENTERED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));

        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION, TAB_STRIP_X_START + TAB_X_OFFSET,
                        TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + 2 * TAB_X_OFFSET, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + 3 * TAB_X_OFFSET, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION,
                        TAB_STRIP_X_START + 4 * TAB_X_OFFSET, TAB_STRIP_Y_START, 0));
        // Total distance moved so far by pointer is still within the width of the tab strip.

        onTabDragListener.onDrag(
                mTabsToolbarView, createDragEvent(DragEvent.ACTION_DROP, 0f, 0f, 0));
    }

    // Drag n drop action for moving outside the tab strip.
    private void eventsOutsideStripLayout(TabDragSource.OnDragListenerImpl onTabDragListener) {
        // The tab movement is performed by generating the motion events as if the user is moving a
        // tab. Outside the tab strip movement here is when horizontal/x position stays constant
        // within the strip width and vertical/y position goes beyond the strip height (below the
        // strip in this case)). The top left of the strip layout is (0f, 0f). For verification the
        // x-offset is zero.

        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_STARTED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(
                        DragEvent.ACTION_DRAG_ENTERED, TAB_STRIP_X_START, TAB_STRIP_Y_START, 0));
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION, TAB_STRIP_X_START,
                        TAB_STRIP_Y_START + TAB_STRIP_Y_STEP, 0)); // Move within the tabs area.
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION, TAB_STRIP_X_START,
                        TAB_Y_OFFSET_WITHIN, 0)); // Still within the tabs area.
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION, TAB_STRIP_X_START,
                        TAB_Y_OFFSET_OUTSIDE, 0)); // Outside the tabs area but inside the toolbars.
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION, TAB_STRIP_X_START,
                        TAB_Y_OFFSET_WITHIN, 0)); // Back to within the tabs area.
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_LOCATION, TAB_STRIP_X_START,
                        TAB_STRIP_Y_START + TAB_Y_OFFSET_OUTSIDE, 0));
        // Total distance moved by pointer is beyond height of the tab strip.

        // This event will indicate the the pointer has moved outside of the TabToolbar view and no
        // more ACTION_DRAG_LOCATION and ACTION_DROP events will be received too.
        onTabDragListener.onDrag(mTabsToolbarView,
                createDragEvent(DragEvent.ACTION_DRAG_EXITED, TAB_STRIP_X_START,
                        TAB_STRIP_Y_START + TAB_Y_OFFSET_OUTSIDE, 0));
    }

    /**
     * Tests the instance of the local class {@link TabDragSource} get and clear methods.
     */
    @Test
    public void test_canAcceptTabDrop_SimulateDragDrops_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);
        View tabsToolbarViewForDrop = getAnotherToolbarView();
        when(tabsToolbarViewForDrop.getWidth()).thenReturn(300);
        when(tabsToolbarViewForDrop.getHeight()).thenReturn(50);

        // Perform drag n drop simulation action as if dropped on another Chrome Window and verify.
        // Drop event on another Chrome window in the in top half.
        assertTrue("Tab drop should be accepted in another tabs toolbar view.",
                mTabDragSource.getOnDragListenerImpl().canAcceptTabDrop(tabsToolbarViewForDrop,
                        createDragEvent(DragEvent.ACTION_DROP, 150f, 10f, 0)));
        assertTrue("After the drop event on top half accept next should be true.",
                mTabDragSource.getAcceptNextDrop());
        // Clear for next drop.
        mTabDragSource.clearAcceptNextDrop();
        assertFalse("After the drop event is handled the accept nNext flag is cleared.",
                mTabDragSource.getAcceptNextDrop());

        // Trigger another drop event on the same, not drag source, Chrome window in the bottom
        // half.
        assertTrue("Tab drop should be accepted in the bottom half but the flag is set to"
                        + " ignore drop.",
                mTabDragSource.getOnDragListenerImpl().canAcceptTabDrop(tabsToolbarViewForDrop,
                        createDragEvent(DragEvent.ACTION_DROP, 150f, 35f, 0)));
        assertFalse("After the drop event in the bottom half, the next accept should not be true.",
                mTabDragSource.getAcceptNextDrop());

        // Perform ACTION_DROP event on the same tabs view as the source view,
        assertFalse("Tab drop should be ignored if it is the same as drag source toolbar,"
                        + " as it means tab reordering.",
                mTabDragSource.getOnDragListenerImpl().canAcceptTabDrop(
                        mTabsToolbarView, createDragEvent(DragEvent.ACTION_DROP, 150f, 10f, 0)));
    }

    private View getAnotherToolbarView() {
        // Create another toolbar activity indicating second instance of Chrome Window.
        View anotherTabsToolbarView;
        anotherTabsToolbarView = Mockito.spy(new View(mActivity));
        anotherTabsToolbarView.setLayoutParams(new MarginLayoutParams(300, 50));
        return anotherTabsToolbarView;
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#OnDragListenerImpl}.
     *
     * Checks that a drop position of the new window is broadcasted when new window is opened.
     */
    @Test
    public void test_sendPositionInfoToSysUI_WithNewWindowIsOpened_ReturnsSuccess() {
        // Prepare
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()), DRAG_START_POINT);

        // Perform drag n drop simulation actions for movement outside the strip layout.
        simulateDragDropEvents(/*withinStripLayout*/ false);

        // Verify
        // Since the drop is outside the TabToolbar area check if intent is send to SysUI to
        // position the window.
        verify(mTabDragSource, times(1))
                .sendPositionInfoToSysUI(eq(mTabsToolbarView), anyFloat(), anyFloat(),
                        eq(DROP_X_SCREEN_POS), eq(DROP_Y_SCREEN_POS));
    }

    /**
     * Tests the instance of the local class {@link TabDragSource#TabDragShadowBuilder}.
     *
     * Checks that a drag start position of the drag is used as the anchor point.
     */
    @Test
    public void test_onProvideShadowMetrics_WithDesiredStartPosition_ReturnsSuccess() {
        // Prepare
        final float dragStartXPosition = 90f;
        final float dragStartYPosition = 45f;
        initializeTest(false, false, 1, 5);
        mTabDragSource.prepareForDragDrop(mTabsToolbarView, mMultiInstanceManager, mTabDropTarget);
        mTabDragSource.setTabsToolbarHeightInDp(TAB_STRIP_HEIGHT);
        mTabDragSource.startTabDragAction(mTabsToolbarView, mStripLayoutHelper,
                mStripLayoutHelper.getTabById(mClickedTab.getId()),
                new PointF(dragStartXPosition, dragStartYPosition));
        View.DragShadowBuilder tabDragShadowBuilder = mTabDragSource.getTabDragShadowBuilder();

        // Perform asking the TabDragShadowBuilder what is the anchor point.
        Point dragSize = new Point(0, 0);
        Point dragAnchor = new Point(0, 0);
        tabDragShadowBuilder.onProvideShadowMetrics(dragSize, dragAnchor);

        // Validate anchor.
        assertTrue(dragAnchor.x == Math.round(dragStartXPosition));
        assertTrue(dragAnchor.y == Math.round(dragStartYPosition));
    }
}
