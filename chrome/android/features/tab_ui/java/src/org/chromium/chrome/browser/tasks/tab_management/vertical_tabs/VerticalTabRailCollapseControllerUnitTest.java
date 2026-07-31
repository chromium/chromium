// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;

/** Unit tests for {@link VerticalTabRailCollapseController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabRailCollapseControllerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabRailCollapseController.RailCollapseListener mMockListener;
    @Mock private Callback<@RailCollapseState Integer> mMockFallbackCallback;

    private VerticalTabRailCollapseController mController;

    @Before
    public void setUp() {
        mController = new VerticalTabRailCollapseController(mMockFallbackCallback);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.VERTICAL_TABS_COLLAPSED);
    }

    @Test
    @SmallTest
    public void testInitialState() {
        assertEquals(RailCollapseState.EXPANDED, mController.getRailCollapseStateByUser());
        assertEquals(
                RailCollapseState.EXPANDED, (int) mController.getRailCollapseStateSupplier().get());
        assertTrue(mController.isCollapseButtonEnabled());
    }

    @Test
    @SmallTest
    public void testInitialState_RestoredFromSharedPreferences() {
        VerticalTabUtils.setRailCollapsedInSharedPref(true);
        VerticalTabRailCollapseController controller =
                new VerticalTabRailCollapseController(mMockFallbackCallback);
        assertEquals(RailCollapseState.COLLAPSED, controller.getRailCollapseStateByUser());
        assertEquals(
                RailCollapseState.COLLAPSED, (int) controller.getRailCollapseStateSupplier().get());
    }

    @Test
    @SmallTest
    public void testDispatchRailCollapseStateUpdateSupplierValue() {
        assertEquals(
                RailCollapseState.EXPANDED, (int) mController.getRailCollapseStateSupplier().get());

        mController.setRailCollapseStateSupplierValue(RailCollapseState.COLLAPSED);
        assertEquals(
                RailCollapseState.COLLAPSED,
                (int) mController.getRailCollapseStateSupplier().get());

        mController.setRailCollapseStateSupplierValue(RailCollapseState.EXPANDED);
        assertEquals(
                RailCollapseState.EXPANDED, (int) mController.getRailCollapseStateSupplier().get());
    }

    @Test
    @SmallTest
    public void testDispatchRailCollapseState_Update_CallsCallback() {
        mController.dispatchRailCollapseStateUpdate(RailCollapseState.COLLAPSED);
        verify(mMockFallbackCallback).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    public void testGetEffectiveRailCollapseState_NarrowVsWide() {
        mController.setRailCollapseStateByUser(RailCollapseState.EXPANDED);
        assertEquals(
                RailCollapseState.COLLAPSED,
                mController.getEffectiveRailCollapseState(/* isNarrow= */ true));
        assertEquals(
                RailCollapseState.EXPANDED,
                mController.getEffectiveRailCollapseState(/* isNarrow= */ false));

        mController.setRailCollapseStateByUser(RailCollapseState.COLLAPSED);
        assertEquals(
                RailCollapseState.COLLAPSED,
                mController.getEffectiveRailCollapseState(/* isNarrow= */ true));
        assertEquals(
                RailCollapseState.COLLAPSED,
                mController.getEffectiveRailCollapseState(/* isNarrow= */ false));
    }

    @Test
    @SmallTest
    public void testToggleCollapseState_PersistsToSharedPreferences() {
        mController.toggleCollapseState();
        assertTrue(VerticalTabUtils.isRailCollapsedFromSharedPref());

        mController.toggleCollapseState();
        assertFalse(VerticalTabUtils.isRailCollapsedFromSharedPref());
    }

    @Test
    @SmallTest
    public void testToggleCollapseState_WithListener() {
        mController.setRailCollapseListener(mMockListener);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Android.VerticalTabs.RailCollapsed", true);
        mController.toggleCollapseState();
        watcher.assertExpected();

        verify(mMockListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.EXPANDED, RailCollapseState.COLLAPSED);
        verify(mMockFallbackCallback, never()).onResult(anyInt());
    }

    @Test
    @SmallTest
    public void testToggleCollapseState_WithoutListener_Fallback() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher("Android.VerticalTabs.RailCollapsed", true);
        mController.toggleCollapseState();
        watcher.assertExpected();

        verify(mMockFallbackCallback).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    public void testToggleCollapseState_Disabled() {
        mController.setRailCollapseListener(mMockListener);
        mController.setCollapseButtonEnabled(false);

        mController.toggleCollapseState();

        verify(mMockListener, never()).onRailCollapseStateChangeRequestedByUser(anyInt(), anyInt());
        verify(mMockFallbackCallback, never()).onResult(anyInt());
    }

    @Test
    @SmallTest
    public void testExpandOrCollapseOnHover_ValidTransitions() {
        mController.setRailCollapseListener(mMockListener);
        mController.setRailCollapseStateByUser(RailCollapseState.COLLAPSED);
        mController.setRailCollapseStateSupplierValue(RailCollapseState.COLLAPSED);

        // Hover enter: COLLAPSED -> EXPANDED_FOR_HOVERING
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        verify(mMockListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED_FOR_HOVERING);

        // Hover exit: EXPANDED_FOR_HOVERING -> COLLAPSED
        mController.setRailCollapseStateSupplierValue(RailCollapseState.EXPANDED_FOR_HOVERING);
        mController.expandOrCollapseOnHover(RailCollapseState.COLLAPSED);
        verify(mMockListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.EXPANDED_FOR_HOVERING, RailCollapseState.COLLAPSED);
    }

    @Test
    @SmallTest
    public void testExpandOrCollapseOnHover_InvalidTransitions() {
        mController.setRailCollapseListener(mMockListener);
        mController.setRailCollapseStateByUser(RailCollapseState.EXPANDED);

        // Hover request when user preference is EXPANDED should be ignored
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        verify(mMockListener, never()).onRailCollapseStateChangeRequestedByUser(anyInt(), anyInt());
    }

    @Test
    @SmallTest
    public void testRequestRailCollapseStateChange_NoOpIfSameState() {
        mController.setRailCollapseListener(mMockListener);
        mController.requestRailCollapseStateChangeByUser(
                RailCollapseState.EXPANDED, RailCollapseState.EXPANDED);

        verify(mMockListener, never()).onRailCollapseStateChangeRequestedByUser(anyInt(), anyInt());
        verify(mMockFallbackCallback, never()).onResult(anyInt());
    }

    @Test
    @SmallTest
    public void testRequestRailCollapseStateChange_UpdateStateByUser() {
        mController.setRailCollapseListener(mMockListener);
        mController.requestRailCollapseStateChangeByUser(
                RailCollapseState.EXPANDED, RailCollapseState.COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mController.getRailCollapseStateByUser());
        verify(mMockListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.EXPANDED, RailCollapseState.COLLAPSED);

        mController.requestRailCollapseStateChangeByUser(
                RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED_FOR_HOVERING);

        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING, mController.getRailCollapseStateByUser());
        verify(mMockListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED_FOR_HOVERING);

        mController.requestRailCollapseStateChangeByUser(
                RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED);
        assertEquals(RailCollapseState.EXPANDED, mController.getRailCollapseStateByUser());
        verify(mMockListener)
                .onRailCollapseStateChangeRequestedByUser(
                        RailCollapseState.COLLAPSED, RailCollapseState.EXPANDED);
    }

    @Test
    @SmallTest
    public void testIsExpanded() {
        assertTrue(VerticalTabRailCollapseController.isExpanded(RailCollapseState.EXPANDED));
        assertTrue(
                VerticalTabRailCollapseController.isExpanded(
                        RailCollapseState.EXPANDED_FOR_HOVERING));
        assertFalse(VerticalTabRailCollapseController.isExpanded(RailCollapseState.COLLAPSED));
    }
}
