// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
public class RecentTabsPageUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Activity mActivity;
    @Mock private RecentTabsManager mRecentTabsManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;

    @Captor ArgumentCaptor<EdgeToEdgePadAdjuster> mPadAdjusterCaptor;

    private RecentTabsPage mRecentTabsPage;
    private final SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier =
            ObservableSuppliers.createMonotonic();

    @Before
    public void setup() {
        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
        mRecentTabsPage =
                new RecentTabsPage(
                        mActivity,
                        mRecentTabsManager,
                        /* navigationDelegate= */ null,
                        mBrowserControlsStateProvider,
                        ObservableSuppliers.alwaysZero(),
                        mEdgeToEdgeSupplier,
                        UrlConstants.RECENT_TABS_URL);
    }

    @Test
    public void testEdgeToEdge() {
        assertTrue("Recent tabs do support E2E.", mRecentTabsPage.supportsEdgeToEdge());

        mEdgeToEdgeSupplier.set(mEdgeToEdgeController);
        verify(mEdgeToEdgeController).registerAdjuster(mPadAdjusterCaptor.capture());

        EdgeToEdgePadAdjuster padAdjuster = mPadAdjusterCaptor.getValue();

        padAdjuster.overrideBottomInset(100);
        ViewGroup listView = mRecentTabsPage.getView().findViewById(R.id.odp_listview);
        assertEquals("Bottom insets should have been applied.", 100, listView.getPaddingBottom());
        assertFalse(listView.getClipToPadding());

        padAdjuster.overrideBottomInset(0);
        assertEquals("Bottom insets should have been reset.", 0, listView.getPaddingBottom());
        assertTrue(listView.getClipToPadding());

        mRecentTabsPage.destroy();
        verify(mEdgeToEdgeController).unregisterAdjuster(padAdjuster);
    }

    @Test
    public void testUpdateMargins_onControlsPositionChanged() {
        when(mBrowserControlsStateProvider.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.TOP);
        when(mBrowserControlsStateProvider.getTopControlsHeight()).thenReturn(168);
        when(mBrowserControlsStateProvider.getContentOffset()).thenReturn(0);

        mRecentTabsPage.onControlsPositionChanged(
                BrowserControlsStateProvider.ControlsPosition.TOP);

        View root = mRecentTabsPage.getView().findViewById(R.id.recent_tabs_root);
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) root.getLayoutParams();
        assertEquals(
                "Top margin should equal top controls height when top-anchored.",
                168,
                params.topMargin);
        assertEquals("Translation Y should be 0.", 0, (int) root.getTranslationY());
    }

    private ForeignSession createForeignSession(String tag, String name) {
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(new ForeignSessionTab(new GURL("https://google.com"), "Google", 0L, 0L, 1));
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(new ForeignSessionWindow(0L, 1, tabs));
        return new ForeignSession(tag, name, 0L, windows, 0);
    }

    @Test
    public void testScrollToTargetSession_OnConstruction() {
        // Test that deep-linking to a device via the URL fragment works when specified at
        // construction time.
        clearInvocations(mRecentTabsManager);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        sessions.add(createForeignSession("session_2", "Device 2"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(true);
        when(mRecentTabsManager.isRecentlyClosedTabsCollapsed()).thenReturn(true);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        ArgumentCaptor<RecentTabsManager.UpdatedCallback> callbackCaptor =
                ArgumentCaptor.forClass(RecentTabsManager.UpdatedCallback.class);

        // Open page with deep-link fragment for session_2.
        String url = UrlConstants.RECENT_TABS_URL + "#session_2";
        RecentTabsPage page =
                new RecentTabsPage(
                        mActivity,
                        mRecentTabsManager,
                        /* navigationDelegate= */ null,
                        mBrowserControlsStateProvider,
                        ObservableSuppliers.alwaysZero(),
                        mEdgeToEdgeSupplier,
                        url);

        verify(mRecentTabsManager).setUpdatedCallback(callbackCaptor.capture());
        RecentTabsManager.UpdatedCallback callback = callbackCaptor.getValue();

        // Trigger sync data load.
        callback.onUpdated();

        // Verify that the deep-linked group (session_2) is now expanded.
        ExpandableListView listView = page.getView().findViewById(R.id.odp_listview);
        RecentTabsRowAdapter adapter = (RecentTabsRowAdapter) listView.getExpandableListAdapter();
        int expectedGroupPos = adapter.getGroupPositionForForeignSession("session_2");
        assertTrue(listView.isGroupExpanded(expectedGroupPos));

        page.destroy();
    }

    @Test
    public void testScrollToTargetSession_OnUpdateForUrl() {
        // Test that updating the URL with a deep-link fragment *after* the page has already loaded
        // data correctly expands the target group.
        clearInvocations(mRecentTabsManager);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        sessions.add(createForeignSession("session_2", "Device 2"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(true);
        when(mRecentTabsManager.isRecentlyClosedTabsCollapsed()).thenReturn(true);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        ArgumentCaptor<RecentTabsManager.UpdatedCallback> callbackCaptor =
                ArgumentCaptor.forClass(RecentTabsManager.UpdatedCallback.class);

        // Construct page without a fragment.
        RecentTabsPage page =
                new RecentTabsPage(
                        mActivity,
                        mRecentTabsManager,
                        /* navigationDelegate= */ null,
                        mBrowserControlsStateProvider,
                        ObservableSuppliers.alwaysZero(),
                        mEdgeToEdgeSupplier,
                        UrlConstants.RECENT_TABS_URL);

        verify(mRecentTabsManager).setUpdatedCallback(callbackCaptor.capture());
        RecentTabsManager.UpdatedCallback callback = callbackCaptor.getValue();

        // Load sync data first.
        callback.onUpdated();

        // Ensure everything is initially collapsed.
        ExpandableListView listView = page.getView().findViewById(R.id.odp_listview);
        for (int i = 0; i < listView.getExpandableListAdapter().getGroupCount(); i++) {
            assertFalse(listView.isGroupExpanded(i));
        }

        // Navigate to a URL with deep-link fragment for session_2.
        page.updateForUrl(UrlConstants.RECENT_TABS_URL + "#session_2");
        ShadowLooper.idleMainLooper();

        // Verify that the deep-linked group is now expanded.
        RecentTabsRowAdapter adapter = (RecentTabsRowAdapter) listView.getExpandableListAdapter();
        int expectedGroupPos = adapter.getGroupPositionForForeignSession("session_2");
        assertTrue(listView.isGroupExpanded(expectedGroupPos));

        page.destroy();
    }

    @Test
    public void testScrollToTargetSession_UpdateForUrlBeforeDataLoaded() {
        // Test that updating the URL with a deep-link fragment *before* the sync data is loaded
        // correctly queues the scroll target and expands it as soon as the data loads.
        clearInvocations(mRecentTabsManager);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        sessions.add(createForeignSession("session_2", "Device 2"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(true);
        when(mRecentTabsManager.isRecentlyClosedTabsCollapsed()).thenReturn(true);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        ArgumentCaptor<RecentTabsManager.UpdatedCallback> callbackCaptor =
                ArgumentCaptor.forClass(RecentTabsManager.UpdatedCallback.class);

        // Construct page without a fragment.
        RecentTabsPage page =
                new RecentTabsPage(
                        mActivity,
                        mRecentTabsManager,
                        /* navigationDelegate= */ null,
                        mBrowserControlsStateProvider,
                        ObservableSuppliers.alwaysZero(),
                        mEdgeToEdgeSupplier,
                        UrlConstants.RECENT_TABS_URL);

        verify(mRecentTabsManager).setUpdatedCallback(callbackCaptor.capture());
        RecentTabsManager.UpdatedCallback callback = callbackCaptor.getValue();

        // Update URL with deep-link fragment BEFORE data loads.
        page.updateForUrl(UrlConstants.RECENT_TABS_URL + "#session_2");

        // Verify it isn't expanded yet since there is no data.
        ExpandableListView listView = page.getView().findViewById(R.id.odp_listview);
        for (int i = 0; i < listView.getExpandableListAdapter().getGroupCount(); i++) {
            assertFalse(listView.isGroupExpanded(i));
        }

        // Trigger sync data load.
        callback.onUpdated();

        // Verify that the queued deep-link group is expanded now that data is available.
        RecentTabsRowAdapter adapter = (RecentTabsRowAdapter) listView.getExpandableListAdapter();
        int expectedGroupPos = adapter.getGroupPositionForForeignSession("session_2");
        assertTrue(listView.isGroupExpanded(expectedGroupPos));

        page.destroy();
    }

    @Test
    public void testUpdateForUrl_ClearsTargetSessionTag() {
        // Test that updating the URL to one without a fragment clears any previously pending
        // deep-link scroll targets. This prevents stale deep-links from triggering later when data
        // finishes loading.
        clearInvocations(mRecentTabsManager);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(createForeignSession("session_1", "Device 1"));
        sessions.add(createForeignSession("session_2", "Device 2"));
        when(mRecentTabsManager.getForeignSessions()).thenReturn(sessions);
        when(mRecentTabsManager.getForeignSessionCollapsed(any())).thenReturn(true);
        when(mRecentTabsManager.isRecentlyClosedTabsCollapsed()).thenReturn(true);
        when(mRecentTabsManager.isPromoCollapsed()).thenReturn(true);

        ArgumentCaptor<RecentTabsManager.UpdatedCallback> callbackCaptor =
                ArgumentCaptor.forClass(RecentTabsManager.UpdatedCallback.class);

        // Start with a URL deep-linked to session_2.
        String url = UrlConstants.RECENT_TABS_URL + "#session_2";
        RecentTabsPage page =
                new RecentTabsPage(
                        mActivity,
                        mRecentTabsManager,
                        /* navigationDelegate= */ null,
                        mBrowserControlsStateProvider,
                        ObservableSuppliers.alwaysZero(),
                        mEdgeToEdgeSupplier,
                        url);

        verify(mRecentTabsManager).setUpdatedCallback(callbackCaptor.capture());
        RecentTabsManager.UpdatedCallback callback = callbackCaptor.getValue();

        // Navigate away to a URL without a fragment. This should clear the target scroll
        // destination.
        page.updateForUrl(UrlConstants.RECENT_TABS_URL);

        // Load sync data now.
        callback.onUpdated();

        // Verify that no groups are expanded. If the target tag wasn't cleared, session_2 would
        // have been automatically expanded here.
        ExpandableListView listView = page.getView().findViewById(R.id.odp_listview);
        for (int i = 0; i < listView.getExpandableListAdapter().getGroupCount(); i++) {
            assertFalse(listView.isGroupExpanded(i));
        }

        page.destroy();
    }
}
