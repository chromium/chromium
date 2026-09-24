// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;

/** Unit tests for {@link VerticalTabRailCollapseController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabRailCollapseControllerUnitTest {
    private static final String COLLAPSED_HISTOGRAM = "Android.VerticalTabs.RailCollapsed";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private VerticalTabRailCollapseController.RailStateChangeDelegate mMockDelegate;
    @Mock private Callback<@RailCollapseState Integer> mMockSetRailStateCallback;
    @Mock private Callback<Boolean> mMockCollapseButtonEnabledCallback;

    private VerticalTabRailCollapseController mController;

    @Before
    public void setUp() {
        mController = createController();
    }

    private VerticalTabRailCollapseController createController() {
        return new VerticalTabRailCollapseController(
                mMockSetRailStateCallback, mMockCollapseButtonEnabledCallback);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.VERTICAL_TABS_COLLAPSED);
    }

    @Test
    public void testInitialState() {
        assertFalse(mController.isCollapsedByUserForTesting());
        assertEquals(
                RailCollapseState.EXPANDED, (int) mController.getRailCollapseStateSupplier().get());
        assertEquals(RailCollapseState.EXPANDED, mController.getEffectiveRailCollapseState());
        assertFalse(mController.isForcedCollapsed());
    }

    @Test
    public void testInitialState_RestoredFromSharedPreferences() {
        VerticalTabUtils.setRailCollapsedInSharedPref(true);
        VerticalTabRailCollapseController controller = createController();
        assertTrue(controller.isCollapsedByUserForTesting());
        assertEquals(
                RailCollapseState.COLLAPSED, (int) controller.getRailCollapseStateSupplier().get());
        assertEquals(RailCollapseState.COLLAPSED, controller.getEffectiveRailCollapseState());
        assertFalse(controller.isForcedCollapsed());
    }

    @Test
    public void testToggleCollapseState_PersistsToSharedPreferences() {
        mController.toggleCollapseState();
        assertTrue(VerticalTabUtils.isRailCollapsedFromSharedPref());

        mController.toggleCollapseState();
        assertFalse(VerticalTabUtils.isRailCollapsedFromSharedPref());
    }

    @Test
    public void testToggleCollapseState_WithDelegate() {
        mController.setRailStateChangeDelegate(mMockDelegate);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(COLLAPSED_HISTOGRAM, true);
        mController.toggleCollapseState();
        watcher.assertExpected();

        verify(mMockDelegate).handleUserRequestedStateChange();
        verify(mMockSetRailStateCallback, never()).onResult(anyInt());
    }

    @Test
    public void testToggleCollapseState_WithoutDelegate_Fallback() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(COLLAPSED_HISTOGRAM, true);
        mController.toggleCollapseState();
        watcher.assertExpected();

        verify(mMockSetRailStateCallback).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    public void testToggleCollapseState_NotifiesDelegateOnEachToggle() {
        mController.setRailStateChangeDelegate(mMockDelegate);

        mController.toggleCollapseState();
        assertTrue(mController.isCollapsedByUserForTesting());
        verify(mMockDelegate).handleUserRequestedStateChange();

        mController.toggleCollapseState();
        assertFalse(mController.isCollapsedByUserForTesting());
        verify(mMockDelegate, times(2)).handleUserRequestedStateChange();
    }

    @Test
    public void testToggleCollapseState_DefersPublishUntilApplied() {
        mController.setRailStateChangeDelegate(mMockDelegate);

        mController.toggleCollapseState();

        // The delegate drives the Side UI transition, so the applied state is unchanged so far.
        assertEquals(
                RailCollapseState.EXPANDED, (int) mController.getRailCollapseStateSupplier().get());
        verify(mMockSetRailStateCallback, never()).onResult(anyInt());

        mController.applyEffectiveState();

        assertEquals(
                RailCollapseState.COLLAPSED,
                (int) mController.getRailCollapseStateSupplier().get());
        verify(mMockSetRailStateCallback).onResult(RailCollapseState.COLLAPSED);
    }

    @Test
    public void testToggleCollapseState_ClearsHover() {
        mController.toggleCollapseState();
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mController.getEffectiveRailCollapseState());

        // Pinning the hover-expanded rail expands it for real.
        mController.toggleCollapseState();
        assertFalse(mController.isCollapsedByUserForTesting());
        assertEquals(RailCollapseState.EXPANDED, mController.getEffectiveRailCollapseState());

        // Collapsing again ignores the still-hovering pointer.
        mController.toggleCollapseState();
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
    }

    @Test
    public void testToggleCollapseState_Disabled() {
        mController.setRailStateChangeDelegate(mMockDelegate);
        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        // The forced-collapse transition itself applies state; only the toggle is under test here.
        clearInvocations(
                (Callback<?>) mMockSetRailStateCallback,
                (Callback<?>) mMockCollapseButtonEnabledCallback);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(COLLAPSED_HISTOGRAM).build();
        mController.toggleCollapseState();
        watcher.assertExpected();

        verify(mMockDelegate, never()).handleUserRequestedStateChange();
        verify(mMockSetRailStateCallback, never()).onResult(anyInt());
    }

    @Test
    public void testExpandOrCollapseOnHover_ValidTransitions() {
        // No delegate yet, so this applies the effective state (supplier becomes COLLAPSED).
        mController.toggleCollapseState();
        mController.setRailStateChangeDelegate(mMockDelegate);

        // Hover enter: COLLAPSED -> EXPANDED_FOR_HOVERING
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mController.getEffectiveRailCollapseState());
        verify(mMockDelegate).handleUserRequestedStateChange();

        // Hover exit: EXPANDED_FOR_HOVERING -> COLLAPSED
        mController.expandOrCollapseOnHover(RailCollapseState.COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
        verify(mMockDelegate, times(2)).handleUserRequestedStateChange();
    }

    @Test
    public void testExpandOrCollapseOnHover_InvalidTransitions() {
        mController.setRailStateChangeDelegate(mMockDelegate);
        // User preference defaults to EXPANDED.

        // Hover request when user preference is EXPANDED should be ignored
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        verify(mMockDelegate, never()).handleUserRequestedStateChange();
    }

    @Test
    public void testExpandOrCollapseOnHover_HoverEnterTrackedWhileForcedCollapsed() {
        mController.toggleCollapseState();
        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        mController.setRailStateChangeDelegate(mMockDelegate);
        clearInvocations(
                (Callback<?>) mMockSetRailStateCallback,
                (Callback<?>) mMockCollapseButtonEnabledCallback);

        // The rail cannot expand in a narrow window, so the hover enter changes nothing yet.
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
        verify(mMockDelegate, never()).handleUserRequestedStateChange();
        verify(mMockSetRailStateCallback, never()).onResult(anyInt());

        // The hover was still recorded: widening the window expands the rail for hovering.
        mController.setWindowWidthBoundary(WindowWidthBoundary.FULLY_EXPANDABLE);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mController.getEffectiveRailCollapseState());
    }

    @Test
    public void testExpandOrCollapseOnHover_HoverExitTrackedWhileForcedCollapsed() {
        mController.toggleCollapseState();
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertEquals(
                RailCollapseState.EXPANDED_FOR_HOVERING,
                mController.getEffectiveRailCollapseState());

        // The window narrows: the rail is forced collapsed, and the pointer leaves the rail.
        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        mController.expandOrCollapseOnHover(RailCollapseState.COLLAPSED);

        // The hover exit must not be dropped: widening the window keeps the rail collapsed.
        mController.setWindowWidthBoundary(WindowWidthBoundary.FULLY_EXPANDABLE);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
    }

    @Test
    public void testSetWindowWidthBoundary_AppliesEffectiveStateOnChangeOnly() {
        // No-op: already not forced collapsed.
        mController.setWindowWidthBoundary(WindowWidthBoundary.FULLY_EXPANDABLE);
        verify(mMockSetRailStateCallback, never()).onResult(anyInt());

        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
        assertTrue(mController.isForcedCollapsed());
        verify(mMockSetRailStateCallback).onResult(RailCollapseState.COLLAPSED);
        verify(mMockCollapseButtonEnabledCallback).onResult(false);

        mController.setWindowWidthBoundary(WindowWidthBoundary.DYNAMIC_EXPANDABLE);
        assertEquals(RailCollapseState.EXPANDED, mController.getEffectiveRailCollapseState());
        assertFalse(mController.isForcedCollapsed());
        verify(mMockSetRailStateCallback).onResult(RailCollapseState.EXPANDED);
        verify(mMockCollapseButtonEnabledCallback).onResult(true);
    }

    @Test
    public void testSetWindowWidthBoundary_NotShowableAlsoForcesCollapse() {
        mController.setWindowWidthBoundary(WindowWidthBoundary.NOT_SHOWABLE);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
        assertTrue(mController.isForcedCollapsed());
    }

    @Test
    public void testGetEffectiveRailCollapseState_NarrowVsWide() {
        // User preference EXPANDED.
        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
        mController.setWindowWidthBoundary(WindowWidthBoundary.FULLY_EXPANDABLE);
        assertEquals(RailCollapseState.EXPANDED, mController.getEffectiveRailCollapseState());

        // User preference COLLAPSED.
        mController.toggleCollapseState();
        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
        mController.setWindowWidthBoundary(WindowWidthBoundary.FULLY_EXPANDABLE);
        assertEquals(RailCollapseState.COLLAPSED, mController.getEffectiveRailCollapseState());
    }

    @Test
    public void testIsHoverExpanded() {
        // The user preference is EXPANDED, so hovering changes nothing.
        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertFalse(mController.isHoverExpanded());

        mController.toggleCollapseState();
        assertFalse(mController.isHoverExpanded());

        mController.expandOrCollapseOnHover(RailCollapseState.EXPANDED_FOR_HOVERING);
        assertTrue(mController.isHoverExpanded());

        // A narrow window keeps the rail collapsed, hover or not.
        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        assertFalse(mController.isHoverExpanded());
        mController.setWindowWidthBoundary(WindowWidthBoundary.FULLY_EXPANDABLE);
        assertTrue(mController.isHoverExpanded());

        mController.expandOrCollapseOnHover(RailCollapseState.COLLAPSED);
        assertFalse(mController.isHoverExpanded());
    }

    @Test
    public void testApplyEffectiveState_CallsCallbacksAndPublishesSupplier() {
        mController.applyEffectiveState();
        verify(mMockSetRailStateCallback).onResult(RailCollapseState.EXPANDED);
        verify(mMockCollapseButtonEnabledCallback).onResult(true);
        assertEquals(
                RailCollapseState.EXPANDED, (int) mController.getRailCollapseStateSupplier().get());

        mController.setWindowWidthBoundary(WindowWidthBoundary.FORCED_COLLAPSED);
        assertEquals(
                RailCollapseState.COLLAPSED,
                (int) mController.getRailCollapseStateSupplier().get());
    }

    @Test
    public void testDestroy_ClearsDelegateAndFallsBackToApplying() {
        mController.setRailStateChangeDelegate(mMockDelegate);
        mController.destroy();

        mController.toggleCollapseState();

        verify(mMockDelegate, never()).handleUserRequestedStateChange();
        verify(mMockSetRailStateCallback).onResult(RailCollapseState.COLLAPSED);
    }
}
