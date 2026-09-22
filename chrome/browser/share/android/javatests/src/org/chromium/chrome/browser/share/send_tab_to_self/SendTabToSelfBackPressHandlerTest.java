// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;

import java.util.List;

/** Unit tests for {@link SendTabToSelfBackPressHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SendTabToSelfBackPressHandlerTest {
    private static final int STTS_TAB_ID = 101;
    private static final int PARENT_TAB_ID = 42;
    private static final int UNRELATED_TAB_ID = 999;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mSttsTab;
    @Mock private Tab mParentTab;

    private SettableNullableObservableSupplier<Tab> mActivityTabSupplier;
    private SettableMonotonicObservableSupplier<TabModel> mCurrentTabModelSupplier;
    private SendTabToSelfBackPressHandler mHandler;

    @Before
    public void setUp() {
        mActivityTabSupplier = ObservableSuppliers.createNullable();
        mCurrentTabModelSupplier = ObservableSuppliers.createMonotonic();

        when(mSttsTab.getId()).thenReturn(STTS_TAB_ID);
        when(mSttsTab.getLaunchType()).thenReturn(TabLaunchType.FROM_SYNC_BACKGROUND);
        when(mSttsTab.getParentId()).thenReturn(PARENT_TAB_ID);

        when(mParentTab.getId()).thenReturn(PARENT_TAB_ID);

        when(mTabModelSelector.getTabById(PARENT_TAB_ID)).thenReturn(mParentTab);
        when(mTabModelSelector.getModelForTabId(PARENT_TAB_ID)).thenReturn(mTabModel);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel));
        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mCurrentTabModelSupplier);
        when(mTabModel.getTabById(PARENT_TAB_ID)).thenReturn(mParentTab);
        when(mTabModel.indexOf(mParentTab)).thenReturn(1);
        mCurrentTabModelSupplier.set(mTabModel);

        mHandler = new SendTabToSelfBackPressHandler(mActivityTabSupplier, () -> mTabModelSelector);
    }

    /** Enables the handler with the STTS tab and makes it the active tab, as production does. */
    private void enableOnSttsTab() {
        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);
        mActivityTabSupplier.set(mSttsTab);
    }

    /** Returns the most recently registered {@link TabModelObserver}. */
    private TabModelObserver captureTabModelObserver() {
        ArgumentCaptor<TabModelObserver> captor = ArgumentCaptor.forClass(TabModelObserver.class);
        verify(mTabModel, atLeastOnce()).addObserver(captor.capture());
        return captor.getValue();
    }

    /** Returns the most recently registered {@link TabObserver} on the given tab. */
    private TabObserver captureTabObserver(Tab tab) {
        ArgumentCaptor<TabObserver> captor = ArgumentCaptor.forClass(TabObserver.class);
        verify(tab, atLeastOnce()).addObserver(captor.capture());
        return captor.getValue();
    }

    @Test
    public void testResetByDefault() {
        // Setting active tab without enabling does not enable handler.
        mActivityTabSupplier.set(mSttsTab);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testEnable_EnablesSupplier() {
        // Start on parent tab before enabling; handler is initially inactive.
        mActivityTabSupplier.set(mParentTab);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());

        // Enable handler with an eligible STTS tab.
        mHandler.enable(mSttsTab);

        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(STTS_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testShouldNotEnable_NotFromSyncBackground() {
        // Tabs not launched from sync background (e.g. standard link navigation) should not enable.
        when(mSttsTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LINK);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testShouldNotEnable_InvalidParentId() {
        // Tabs without a valid parent tab ID cannot be navigated back from.
        when(mSttsTab.getParentId()).thenReturn(Tab.INVALID_TAB_ID);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testShouldNotEnable_ParentIdMatchesTabId() {
        // Self-referencing parent ID should be rejected to avoid navigation loops.
        when(mSttsTab.getParentId()).thenReturn(STTS_TAB_ID);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testShouldNotEnable_Closing() {
        // Tab is already marked as closing.
        when(mSttsTab.isClosing()).thenReturn(true);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testShouldNotEnable_Destroyed() {
        // Tab is already marked as destroyed.
        when(mSttsTab.isDestroyed()).thenReturn(true);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testShouldNotEnable_ParentTabMissing() {
        // The parent tab is already gone, so there would be nothing to switch back to.
        when(mTabModelSelector.getTabById(PARENT_TAB_ID)).thenReturn(null);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).addObserver(any());
    }

    @Test
    public void testShouldNotEnable_ParentTabClosing() {
        when(mParentTab.isClosing()).thenReturn(true);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testShouldNotEnable_ParentTabDestroyed() {
        when(mParentTab.isDestroyed()).thenReturn(true);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testShouldNotEnable_NullTabModelSelector() {
        // Without a selector the parent tab cannot be validated or selected.
        SendTabToSelfBackPressHandler handler =
                new SendTabToSelfBackPressHandler(mActivityTabSupplier, () -> null);

        mActivityTabSupplier.set(mParentTab);
        handler.enable(mSttsTab);

        assertFalse(handler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, handler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testEnable_ReEnableOverridesPreviousTab() {
        // Create a second eligible STTS tab sharing the same parent.
        Tab secondTab = mock(Tab.class);
        when(secondTab.getId()).thenReturn(202);
        when(secondTab.getLaunchType()).thenReturn(TabLaunchType.FROM_SYNC_BACKGROUND);
        when(secondTab.getParentId()).thenReturn(PARENT_TAB_ID);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);
        assertEquals(STTS_TAB_ID, mHandler.getReceivedTabIdForTesting());

        // Re-enable with the second tab, overriding the previously enabled tab.
        mHandler.enable(secondTab);
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(202, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testEnable_ReEnableWithIneligibleTabResets() {
        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(STTS_TAB_ID, mHandler.getReceivedTabIdForTesting());

        // An ineligible tab must not leave the handler enabled for the previously enabled tab,
        // which the user is no longer on.
        Tab ineligibleTab = mock(Tab.class);
        when(ineligibleTab.getId()).thenReturn(303);
        when(ineligibleTab.getLaunchType()).thenReturn(TabLaunchType.FROM_LINK);
        when(ineligibleTab.getParentId()).thenReturn(PARENT_TAB_ID);

        mHandler.enable(ineligibleTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testHandleBackPress_SwitchesToParentAndResets() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        int result = mHandler.handleBackPress();

        // Verify back press succeeds and switches to the parent tab.
        assertEquals(BackPressResult.SUCCESS, result);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_USER);

        // Handler must be reset and consume the back press for single use only.
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        assertEquals(BackPressResult.FAILURE, mHandler.handleBackPress());
    }

    @Test
    public void testSwitchingTabResetsHandler() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        // Switch away to a different tab.
        mActivityTabSupplier.set(mParentTab);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());

        // Returning to the STTS tab does not re-enable.
        mActivityTabSupplier.set(mSttsTab);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
    }

    @Test
    public void testSwitchingToNullTabResetsHandler() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        // Switch active tab to null (e.g. entering non-browsing layout).
        mActivityTabSupplier.set(null);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testActiveTabDestroyed_ResetsHandler() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        // Trigger tab destruction callback via the attached TabObserver.
        captureTabObserver(mSttsTab).onDestroyed(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testParentTabClosed_ResetsHandler() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        // The parent tab is closed while the STTS tab remains active. This is reachable on
        // tablets, where the tab strip is visible during browsing. The handler must reset so
        // that the supplier never claims a back press it cannot consume.
        captureTabModelObserver().willCloseTab(mParentTab, /* didCloseAlone= */ true);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testParentTabClosedInBatch_ResetsHandler() {
        enableOnSttsTab();

        captureTabModelObserver()
                .willCloseTabs(List.of(mParentTab), /* isAllTabs= */ false, /* allowUndo= */ true);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testParentTabRemoved_ResetsHandler() {
        enableOnSttsTab();

        captureTabModelObserver().tabRemoved(mParentTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testUnrelatedTabClosed_DoesNotResetHandler() {
        enableOnSttsTab();

        Tab unrelatedTab = mock(Tab.class);
        when(unrelatedTab.getId()).thenReturn(UNRELATED_TAB_ID);
        captureTabModelObserver().willCloseTab(unrelatedTab, /* didCloseAlone= */ true);

        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(STTS_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testSttsTabClosed_ResetsHandler() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        captureTabModelObserver().willCloseTab(mSttsTab, /* didCloseAlone= */ true);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testSttsTabClosedInBatch_ResetsHandler() {
        enableOnSttsTab();

        captureTabModelObserver()
                .willCloseTabs(List.of(mSttsTab), /* isAllTabs= */ false, /* allowUndo= */ true);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testSttsTabRemoved_ResetsHandler() {
        enableOnSttsTab();

        captureTabModelObserver().tabRemoved(mSttsTab);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testAllTabsClosed_ResetsHandler() {
        enableOnSttsTab();
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        captureTabModelObserver()
                .willCloseTabs(List.of(), /* isAllTabs= */ true, /* allowUndo= */ false);

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testHandleBackPress_ParentTabDisappearedSilently_ReturnsFailureAndResets() {
        enableOnSttsTab();

        // Simulate the parent tab vanishing without the tab model observer firing. This should
        // not happen in practice; it exercises the defensive check in handleBackPress().
        when(mTabModelSelector.getTabById(PARENT_TAB_ID)).thenReturn(null);

        assertEquals(BackPressResult.FAILURE, mHandler.handleBackPress());
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testHandleBackPress_CurrentTabClosing_ReturnsFailureAndResets() {
        enableOnSttsTab();
        // Current STTS tab is marked as closing.
        when(mSttsTab.isClosing()).thenReturn(true);

        assertEquals(BackPressResult.FAILURE, mHandler.handleBackPress());
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testHandleBackPress_CurrentTabDestroyed_ReturnsFailureAndResets() {
        enableOnSttsTab();
        // Current STTS tab is marked as destroyed.
        when(mSttsTab.isDestroyed()).thenReturn(true);

        assertEquals(BackPressResult.FAILURE, mHandler.handleBackPress());
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testHandleBackPress_NotEnabled_ReturnsFailure() {
        // Back press without prior enabling must fail immediately without side effects.
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());

        assertEquals(BackPressResult.FAILURE, mHandler.handleBackPress());
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testHandleBackPress_ActiveTabMismatch_ReturnsFailureAndResets() {
        Tab differentTab = mock(Tab.class);
        when(differentTab.getId()).thenReturn(UNRELATED_TAB_ID);

        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);

        // Active tab is unexpectedly a different tab when handleBackPress is invoked.
        mActivityTabSupplier.set(differentTab);

        // Must fail and reset to avoid hijacking back presses on unrelated tabs.
        assertEquals(BackPressResult.FAILURE, mHandler.handleBackPress());
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
        verify(mTabModel, never()).setIndex(anyInt(), anyInt());
    }

    @Test
    public void testHandleBackPress_ParentTabInDifferentTabModel_SwitchesSuccessfully() {
        // Defensive: the parent tab normally lives in the same (regular) model, but selecting it
        // must still work if it does not.
        TabModel otherTabModel = mock(TabModel.class);
        when(mTabModelSelector.getModelForTabId(PARENT_TAB_ID)).thenReturn(otherTabModel);
        when(mTabModelSelector.getModels()).thenReturn(List.of(mTabModel, otherTabModel));
        when(otherTabModel.getTabById(PARENT_TAB_ID)).thenReturn(mParentTab);
        when(otherTabModel.indexOf(mParentTab)).thenReturn(3);

        enableOnSttsTab();
        int result = mHandler.handleBackPress();

        // Verify switching to the parent tab in the correct tab model.
        assertEquals(BackPressResult.SUCCESS, result);
        verify(otherTabModel).setIndex(3, TabSelectionType.FROM_USER);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testEnable_WhenAlreadyActiveTab_EnablesSupplierAndSucceedsOnBackPress() {
        // In production, normalTabModel.setIndex is invoked prior to enable(), so the active tab
        // is already the STTS tab when enable() is called.
        mActivityTabSupplier.set(mSttsTab);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());

        mHandler.enable(mSttsTab);

        // Flush the UI thread to run the initial callback posted by TabSupplierObserver's
        // addSyncObserverAndPostIfNonNull(), which invokes onObservingDifferentTab(mSttsTab)
        // for the already-active STTS tab.
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(STTS_TAB_ID, mHandler.getReceivedTabIdForTesting());

        // Handle back press to switch back to the parent tab.
        int result = mHandler.handleBackPress();
        assertEquals(BackPressResult.SUCCESS, result);
        verify(mTabModel).setIndex(1, TabSelectionType.FROM_USER);
        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }

    @Test
    public void testDestroy() {
        mActivityTabSupplier.set(mParentTab);
        mHandler.enable(mSttsTab);
        assertTrue(mHandler.getHandleBackPressChangedSupplier().get());

        // Destroying the handler must clean up state and reset.
        mHandler.destroy();

        assertFalse(mHandler.getHandleBackPressChangedSupplier().get());
        assertEquals(Tab.INVALID_TAB_ID, mHandler.getReceivedTabIdForTesting());
    }
}
