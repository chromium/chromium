// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.ContextMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link RecentTabsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RecentTabsCoordinatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Activity mActivity;
    @Mock private RecentTabsManager mRecentTabsManager;
    @Mock private NativePageNavigationDelegate mNavigationDelegate;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;

    @Captor private ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;
    @Captor private ArgumentCaptor<RecentTabsManager.UpdatedCallback> mUpdatedCallbackCaptor;

    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Integer> mTabStripHeightSupplier =
            ObservableSuppliers.createNonNull(0);

    private RecentTabsCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
        mCoordinator =
                new RecentTabsCoordinator(
                        mActivity,
                        mRecentTabsManager,
                        mNavigationDelegate,
                        mTabStripHeightSupplier,
                        mEdgeToEdgeSupplier,
                        /* parent= */ null);
        verify(mRecentTabsManager).setUpdatedCallback(mUpdatedCallbackCaptor.capture());
    }

    private ForeignSession createForeignSession(String tag, String name) {
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(new ForeignSessionTab(new GURL("https://google.com"), "Google", 0L, 0L, 1));
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(new ForeignSessionWindow(0L, 1, tabs));
        return new ForeignSession(tag, name, 0L, windows, 0);
    }

    @Test
    public void testViewHierarchy() {
        ViewGroup view = mCoordinator.getView();
        assertNotNull("Root view should not be null.", view);
        ExpandableListView listView = mCoordinator.getListViewForTesting();
        assertNotNull("ListView should not be null.", listView);
        assertEquals(
                "ListView should be in root view.", listView, view.findViewById(R.id.odp_listview));
    }

    @Test
    public void testEdgeToEdge() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());

        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();
        padAdjuster.overrideBottomInset(100);

        ExpandableListView listView = mCoordinator.getListViewForTesting();
        assertEquals("Bottom insets should have been applied.", 100, listView.getPaddingBottom());
        assertFalse(listView.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals("Bottom insets should have been reset.", 0, listView.getPaddingBottom());
        assertTrue(listView.getClipToPadding());

        mCoordinator.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }

    @Test
    public void testTabStripHeightChange() {
        mTabStripHeightSupplier.set(48);

        ViewGroup view = mCoordinator.getView();
        assertEquals("Top padding should match tab strip height.", 48, view.getPaddingTop());
    }

    @Test
    public void testUpdateMarginsAndGetRootViewTopMargin() {
        mCoordinator.updateMargins(168, 50, 10);

        View root = mCoordinator.getView().findViewById(R.id.recent_tabs_root);
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) root.getLayoutParams();
        assertEquals("Top margin should be updated.", 168, params.topMargin);
        assertEquals("Bottom margin should be updated.", 50, params.bottomMargin);
        assertEquals("Translation Y should be updated.", 10, (int) root.getTranslationY());
        assertEquals(
                "getRootViewTopMargin should match topMargin.",
                168,
                mCoordinator.getRootViewTopMargin());
    }

    @Test
    public void testShouldCaptureThumbnail() {
        ViewGroup view = mCoordinator.getView();
        assertFalse(
                "Thumbnail should not capture when dimensions are 0.",
                mCoordinator.shouldCaptureThumbnail());

        // Set dimensions on root view. Since initial captured dimensions were 0,
        // laying out with non-zero dimensions marks the thumbnail as needing capture.
        view.measure(
                View.MeasureSpec.makeMeasureSpec(500, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1000, View.MeasureSpec.EXACTLY));
        view.layout(0, 0, 500, 1000);

        assertTrue(
                "Thumbnail should capture when view gains non-zero size.",
                mCoordinator.shouldCaptureThumbnail());

        Canvas canvas = mock(Canvas.class);
        mCoordinator.captureThumbnail(canvas);
        assertFalse(
                "SnapshotContentChanged should be reset after capture.",
                mCoordinator.shouldCaptureThumbnail());

        // Trigger data update.
        mUpdatedCallbackCaptor.getValue().onUpdated();
        assertTrue(
                "Thumbnail should capture after onUpdated data change.",
                mCoordinator.shouldCaptureThumbnail());

        mCoordinator.captureThumbnail(canvas);
        assertFalse(
                "SnapshotContentChanged should be reset after second capture.",
                mCoordinator.shouldCaptureThumbnail());
    }

    @Test
    public void testOnUpdatedAndScrollToTargetSession_PreDataLoad() {
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        sessions.add(createForeignSession("session_2", "Device 2"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(true);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        mCoordinator.setTargetSessionTag("session_2");

        mUpdatedCallbackCaptor.getValue().onUpdated();
        ShadowLooper.idleMainLooper();

        ExpandableListView listView = mCoordinator.getListViewForTesting();
        RecentTabsRowAdapter adapter = (RecentTabsRowAdapter) listView.getExpandableListAdapter();
        int targetGroupPos = adapter.getGroupPositionForForeignSession("session_2");
        assertTrue(
                "Target session group should be expanded.",
                listView.isGroupExpanded(targetGroupPos));
    }

    @Test
    public void testOnUpdatedAndScrollToTargetSession_PostDataLoad() {
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        sessions.add(createForeignSession("session_2", "Device 2"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(true);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        mUpdatedCallbackCaptor.getValue().onUpdated();

        ExpandableListView listView = mCoordinator.getListViewForTesting();
        RecentTabsRowAdapter adapter = (RecentTabsRowAdapter) listView.getExpandableListAdapter();
        int targetGroupPos = adapter.getGroupPositionForForeignSession("session_2");
        assertFalse(
                "Group should initially be collapsed.", listView.isGroupExpanded(targetGroupPos));

        mCoordinator.setTargetSessionTag("session_2");
        ShadowLooper.idleMainLooper();

        assertTrue(
                "Target session group should be expanded after setting tag.",
                listView.isGroupExpanded(targetGroupPos));
    }

    @Test
    public void testSetTouchEnabled() {
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(false);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        mUpdatedCallbackCaptor.getValue().onUpdated();

        ExpandableListView listView = mCoordinator.getListViewForTesting();
        View childView = new View(mActivity);

        // When touch is disabled, onChildClick is intercepted and returns true immediately
        // without delegating to child group.
        mCoordinator.setTouchEnabled(false);
        assertTrue(mCoordinator.onChildClick(listView, childView, 1, 0, 0));
        verify(mRecentTabsManager, never()).openForeignSessionTab(any(), any(), anyInt());

        // When touch is enabled, onChildClick delegates to child group.
        mCoordinator.setTouchEnabled(true);
        mCoordinator.onChildClick(listView, childView, 1, 0, 0);
        verify(mRecentTabsManager).openForeignSessionTab(any(), any(), anyInt());
    }

    @Test
    public void testContextMenu() {
        ContextMenu menu = mock(ContextMenu.class);
        View anchorView = new View(mActivity);

        // 1. Null menu info should clear menu and return cleanly.
        mCoordinator.onCreateContextMenu(menu, mCoordinator.getView(), null);
        verify(menu).clear();

        // 2. Out-of-bounds group position should clear menu and return cleanly without crashing.
        ExpandableListView.ExpandableListContextMenuInfo invalidInfo =
                new ExpandableListView.ExpandableListContextMenuInfo(
                        anchorView, ExpandableListView.getPackedPositionForGroup(999), 0);
        mCoordinator.onCreateContextMenu(menu, mCoordinator.getView(), invalidInfo);

        // 3. Valid group position with loaded foreign sessions.
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(false);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);
        mUpdatedCallbackCaptor.getValue().onUpdated();

        ExpandableListView.ExpandableListContextMenuInfo groupInfo =
                new ExpandableListView.ExpandableListContextMenuInfo(
                        anchorView, ExpandableListView.getPackedPositionForGroup(0), 0);
        mCoordinator.onCreateContextMenu(menu, mCoordinator.getView(), groupInfo);

        ExpandableListView.ExpandableListContextMenuInfo childInfo =
                new ExpandableListView.ExpandableListContextMenuInfo(
                        anchorView, ExpandableListView.getPackedPositionForChild(0, 0), 0);
        mCoordinator.onCreateContextMenu(menu, mCoordinator.getView(), childInfo);
    }

    @Test
    public void testDestroy() {
        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());
        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();

        ExpandableListView listView = mCoordinator.getListViewForTesting();
        assertNotNull(listView.getAdapter());

        mCoordinator.destroy();

        verify(mRecentTabsManager).destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
        assertNull("Adapter should be detached on destroy.", listView.getAdapter());

        // Tab strip height changes after destroy should not affect padding.
        mTabStripHeightSupplier.set(100);
        assertEquals(0, mCoordinator.getView().getPaddingTop());
    }

    @Test
    public void testItemsCanFocus() {
        ExpandableListView listView = mCoordinator.getListViewForTesting();
        assertTrue(
                "List view should allow items to focus so child buttons are accessible.",
                listView.getItemsCanFocus());
    }

    @Test
    public void testIsChildSelectable_PromoGroup() {
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.shouldShowPromo()).thenReturn(true);
        mUpdatedCallbackCaptor.getValue().onUpdated();

        ExpandableListView listView = mCoordinator.getListViewForTesting();
        RecentTabsRowAdapter adapter = (RecentTabsRowAdapter) listView.getExpandableListAdapter();

        // Foreign session child should be selectable.
        int foreignSessionGroupPos = adapter.getGroupPositionForForeignSession("session_1");
        assertTrue(adapter.isChildSelectable(foreignSessionGroupPos, 0));

        // Signin promo group child should NOT be selectable so focus delegates to its child
        // buttons.
        int promoGroupPos = adapter.getGroupCount() - 1;
        assertFalse(adapter.isChildSelectable(promoGroupPos, 0));
    }
}
