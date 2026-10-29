// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.view.DragEvent;
import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabStripDragHandler;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.dragdrop.ChromeMultiTabDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.MimeTypeUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Integration tests for multi-selected Tab drag and drop across windows on the Tab Strip. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test suite tests multi-window instances.")
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MultiTabDragDropTest extends TabStripDragDropTestBase {
    @Test
    @LargeTest
    public void testDragDrop_MultiTab_ToSecondWindow_StartIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover over destination strip at start (index 0) and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 0);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering at start",
                            destHelper.getInReorderModeForTesting());
                    assertTrue(
                            "Strip start margin should be > 0",
                            destHelper.getReorderStartMarginForTesting() > 0f);
                });

        // 3. Drop at start and verify reorder mode exits and start margin resets.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should exit reorder mode after drop",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Strip start margin should reset to 0 after drop",
                            0f,
                            destHelper.getReorderStartMarginForTesting(),
                            0.01f);
                });

        // 4. Verify counts, tab presence, and that dropped tabs land at index 0 and 1.
        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 0);
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_ToSecondWindow_MiddleIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering at middle",
                            destHelper.getInReorderModeForTesting());
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    assertTrue(
                            "Hovered tab should open trailing margin to make space",
                            destTabs[0].getTrailingMargin() > 0f
                                    || destTabs[1].getTrailingMargin() > 0f);
                });

        // 3. Drop at index 1 and verify reorder mode exits and space is cleared.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should exit reorder mode after drop",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Trailing margin should reset to 0 after drop",
                            0f,
                            destHelper.getStripLayoutTabsForTesting()[0].getTrailingMargin(),
                            0.01f);
                });

        // 4. Verify counts, tab presence, and that dropped tabs land at index 1 and 2.
        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_ToSecondWindow_EndIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover over destination strip near the end and verify space is created.
        float hoverXPx = getDropXPxForIndex(destActivity, /* index= */ 2);
        dispatchDragHover(destActivity, hoverXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering at end",
                            destHelper.getInReorderModeForTesting());
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    assertTrue(
                            "Hovered tab should open trailing margin to make space",
                            destTabs[1].getTrailingMargin() > 0f
                                    || destTabs[2].getTrailingMargin() > 0f);
                });

        // 3. Drop beyond last tab and verify reorder mode exits and space is cleared.
        float dropEndXPx = getDropXPxBeyondEnd(destActivity);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropEndXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should exit reorder mode after drop",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Trailing margin should reset to 0 after drop",
                            0f,
                            destHelper.getStripLayoutTabsForTesting()[0].getTrailingMargin(),
                            0.01f);
                });

        // 4. Verify counts, tab presence, and that dropped tabs land at end of strip.
        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ initialDestCount);
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_ToSecondWindow_Rtl() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> LocalizationUtils.setRtlForTesting(true));

        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. In RTL, the start of the strip is on the right edge. Hover near the right edge.
        float density = destActivity.getResources().getDisplayMetrics().density;
        float dropXPx = (destHelper.getWidthForTesting() - 10f) * density;
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering at start"
                                    + " (RTL)",
                            destHelper.getInReorderModeForTesting());
                    assertTrue(
                            "Strip start margin should be > 0 in RTL",
                            destHelper.getReorderStartMarginForTesting() > 0f);
                });

        // 3. Drop at start and verify reorder mode exits and start margin resets.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should exit reorder mode after drop",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Strip start margin should reset to 0 after drop",
                            0f,
                            destHelper.getReorderStartMarginForTesting(),
                            0.01f);
                });

        // 4. Verify counts, tab presence, and that the dropped tabs land at index 0 and 1.
        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 0);
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_LastTabsCloseSourceWindow() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 1);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of all tabs (tabs 0 and 1) from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover over destination window and dispatch drop.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DROP,
                                            dropXPx,
                                            /* y= */ 0f,
                                            MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));
                });

        // 3. Wait for the tabs to be reparented into destination window and removed from source.
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 2,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ 0,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        // 4. Dispatch ACTION_DRAG_ENDED to simulate drag session completion once drop is processed.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DragEvent dragEndEvent =
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_ENDED,
                                    /* x= */ 0f,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB,
                                    /* result= */ true);
                    getTabStripDragHandler(sourceActivity)
                            .onDrag(
                                    sourceActivity.findViewById(R.id.control_container),
                                    dragEndEvent);
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    dragEndEvent);
                });

        // 5. Verify source window is closed and dest window has the tabs.
        CriteriaHelper.pollUiThread(
                sourceActivity::isActivityFinishingOrDestroyed,
                "Source window should close when all its tabs are dragged out.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Destination should have received all dragged tabs",
                            initialDestCount + 2,
                            ChromeTabUtils.getNumOpenTabs(destActivity));
                    for (int tabId : draggedTabIds) {
                        assertNotNull(
                                "Dropped tab should exist in destination window",
                                destActivity.getCurrentTabModel().getTabById(tabId));
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_MergeIntoExistingGroup() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        // Group tab 0 in destination activity.
        Token[] destGroupId = new Token[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel destModel = destActivity.getCurrentTabModel();
                    destModel.createSingleTabGroup(destModel.getTabAt(0));
                    destGroupId[0] = destModel.getTabAt(0).getTabGroupId();
                });
        TabStripUtils.settleDownCompositor(destHelper);

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover directly over destination group (tab 0) and drop.
        // Hover at 75% of Tab 0's width so that adjustXForTabDrop (which shifts backward by
        // halfTabWidth) lands squarely inside Tab 0 and clear of the start gap.
        float dropXPx =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                            float density = destActivity.getResources().getDisplayMetrics().density;
                            return (destTabs[0].getDrawX() + destTabs[0].getWidth() * 0.75f)
                                    * density;
                        });
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        // 3. Verify counts, tab presence, and group merge for all tabs.
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 2,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 2,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        CriteriaHelper.pollUiThread(
                () -> {
                    TabModel destModel = destActivity.getCurrentTabModel();
                    for (int tabId : draggedTabIds) {
                        Tab destTab = destModel.getTabById(tabId);
                        if (destTab == null || !destGroupId[0].equals(destTab.getTabGroupId())) {
                            return false;
                        }
                    }
                    return destModel.getTabsInGroup(destGroupId[0]).size() == 3;
                },
                "Dropped tabs should merge into destination group.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int tabId : draggedTabIds) {
                        assertNull(
                                "Dropped tab should no longer exist in source window",
                                sourceActivity.getCurrentTabModel().getTabById(tabId));
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_CollapsedGroupDoesNotMerge() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        // Group tab 0 in destination activity and collapse it.
        Token[] destGroupId = new Token[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel destModel = destActivity.getCurrentTabModel();
                    destModel.createSingleTabGroup(destModel.getTabAt(0));
                    destGroupId[0] = destModel.getTabAt(0).getTabGroupId();
                    destModel.setTabGroupCollapsed(destGroupId[0], /* isCollapsed= */ true);
                });
        TabStripUtils.settleDownCompositor(destHelper);

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover directly over the collapsed group title and drop.
        float dropXPx =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            StripLayoutGroupTitle title = destHelper.findGroupTitle(destGroupId[0]);
                            float density = destActivity.getResources().getDisplayMetrics().density;
                            return (title.getDrawX() + title.getWidth() / 2) * density;
                        });
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        // 3. Verify counts, tab presence, and that tabs did NOT merge into collapsed group.
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 2,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 2,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int tabId : draggedTabIds) {
                        assertNull(
                                "Dropped tab should no longer exist in source window",
                                sourceActivity.getCurrentTabModel().getTabById(tabId));
                        Tab destTab = destActivity.getCurrentTabModel().getTabById(tabId);
                        assertNotNull("Dropped tab should exist in destination window", destTab);
                        assertNotEquals(
                                "Dropped tab should not merge into collapsed group",
                                destGroupId[0],
                                destTab.getTabGroupId());
                    }
                    assertEquals(
                            "Destination collapsed group should still have 1 tab",
                            1,
                            destActivity
                                    .getCurrentTabModel()
                                    .getTabsInGroup(destGroupId[0])
                                    .size());
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_DragOutOfSourceGroup() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        TabStripTestUtils.createTabGroup(
                sourceActivity,
                /* isIncognito= */ false,
                /* firstIndex= */ 0,
                /* secondIndex= */ 1);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of grouped tabs 0 and 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Drop into destination window at index 1.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        // 3. Verify counts, tab presence, landing indices, and that the tabs were ungrouped upon
        // cross-window drop.
        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int tabId : draggedTabIds) {
                        Tab destTab = destActivity.getCurrentTabModel().getTabById(tabId);
                        assertNotNull("Dropped tab should exist in destination window", destTab);
                        assertNull(
                                "Dropped tab should be ungrouped in destination window",
                                destTab.getTabGroupId());
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_CancelDrag_RestoresState() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering",
                            destHelper.getInReorderModeForTesting());
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    assertTrue(
                            "Hovered tab should open trailing margin to make space",
                            destTabs[0].getTrailingMargin() > 0f
                                    || destTabs[1].getTrailingMargin() > 0f);
                });

        // 3. Cancel drag without dropping (dispatch ACTION_DRAG_ENDED directly).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DragEvent dragEndEvent =
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_ENDED,
                                    /* x= */ 0f,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    dragEndEvent);

                    getTabStripDragHandler(sourceActivity)
                            .onDrag(
                                    sourceActivity.findViewById(R.id.control_container),
                                    dragEndEvent);

                    destHelper.finishAnimations();
                });

        // 4. Verify destination strip exits reorder mode, space resets, and source tabs are
        // unchanged.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should exit reorder mode after drag cancellation",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Trailing margin should reset to 0 after cancellation",
                            0f,
                            destHelper.getStripLayoutTabsForTesting()[0].getTrailingMargin(),
                            0.01f);
                    assertEquals(
                            "Source tab count should remain unchanged",
                            initialSourceCount,
                            ChromeTabUtils.getNumOpenTabs(sourceActivity));
                    assertEquals(
                            "Destination tab count should remain unchanged",
                            initialDestCount,
                            ChromeTabUtils.getNumOpenTabs(destActivity));
                    for (int tabId : draggedTabIds) {
                        assertNotNull(
                                "Dragged tab should still exist in source window",
                                sourceActivity.getCurrentTabModel().getTabById(tabId));
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_DropOnToolbar_Ignored() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Dispatch drop on the toolbar area below the tab strip (large Y coordinate).
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripDragHandler destDragHandler = getTabStripDragHandler(destActivity);
                    View destContainerView = destActivity.findViewById(R.id.control_container);
                    float toolbarYPx = destContainerView.getHeight() - 10f;

                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_STARTED,
                                    /* x= */ 0f,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DROP,
                                    dropXPx,
                                    toolbarYPx,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));

                    getTabStripDragHandler(sourceActivity)
                            .onDrag(
                                    sourceActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DRAG_ENDED,
                                            /* x= */ 0f,
                                            /* y= */ 0f,
                                            MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));

                    destHelper.finishAnimations();
                });

        // 3. Verify drop was ignored: tab counts remain unchanged and tabs didn't move.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Source tab count should remain unchanged",
                            initialSourceCount,
                            ChromeTabUtils.getNumOpenTabs(sourceActivity));
                    assertEquals(
                            "Destination tab count should remain unchanged",
                            initialDestCount,
                            ChromeTabUtils.getNumOpenTabs(destActivity));
                    for (int tabId : draggedTabIds) {
                        assertNotNull(
                                "Dragged tab should still exist in source window",
                                sourceActivity.getCurrentTabModel().getTabById(tabId));
                        assertNull(
                                "Dragged tab should not exist in destination window",
                                destActivity.getCurrentTabModel().getTabById(tabId));
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_HoverExitReenterDrop() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering",
                            destHelper.getInReorderModeForTesting());
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    assertTrue(
                            "Hovered tab should open trailing margin to make space",
                            destTabs[0].getTrailingMargin() > 0f
                                    || destTabs[1].getTrailingMargin() > 0f);
                });

        // 3. Move drag pointer down into toolbar (below tab strip) to exit the strip.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripDragHandler destDragHandler = getTabStripDragHandler(destActivity);
                    View destContainerView = destActivity.findViewById(R.id.control_container);
                    float toolbarYPx = destContainerView.getHeight() - 10f;

                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    dropXPx,
                                    toolbarYPx,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));

                    destHelper.finishAnimations();
                });

        // 4. Verify reorder mode exited and space was cleared.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should exit reorder mode after moving to toolbar",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Trailing margin should reset to 0 after exiting strip",
                            0f,
                            destHelper.getStripLayoutTabsForTesting()[0].getTrailingMargin(),
                            0.01f);
                });

        // 5. Move drag pointer back up into the tab strip (re-enter) and verify space re-opens.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripDragHandler destDragHandler = getTabStripDragHandler(destActivity);
                    View destContainerView = destActivity.findViewById(R.id.control_container);

                    // First event triggers onDragEnter.
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    dropXPx,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));
                    // Second event triggers onDragLocationInStrip / updateReorderPosition.
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    dropXPx,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB));

                    destHelper.finishAnimations();
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should re-enter reorder mode",
                            destHelper.getInReorderModeForTesting());
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    assertTrue(
                            "Hovered tab should re-open trailing margin to make space",
                            destTabs[0].getTrailingMargin() > 0f
                                    || destTabs[1].getTrailingMargin() > 0f);
                });

        // 6. Complete drop and verify tabs reparent successfully at index 1 and 2.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_IncognitoMultiTab_ToRegularWindow_Rejected() throws Exception {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 0);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of incognito multi-tabs from source window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab incognitoTab0 = mock(Tab.class);
                    doReturn(true).when(incognitoTab0).isIncognito();
                    doReturn(true).when(incognitoTab0).isIncognitoBranded();
                    doReturn(9998).when(incognitoTab0).getId();

                    Tab incognitoTab1 = mock(Tab.class);
                    doReturn(true).when(incognitoTab1).isIncognito();
                    doReturn(true).when(incognitoTab1).isIncognitoBranded();
                    doReturn(9999).when(incognitoTab1).getId();

                    List<Tab> tabsToDrag = Arrays.asList(incognitoTab1, incognitoTab0);
                    ChromeDropDataAndroid dropData =
                            new ChromeMultiTabDropDataAndroid.Builder()
                                    .withTabs(tabsToDrag)
                                    .withPrimaryTab(incognitoTab0)
                                    .build();
                    startSimulatedDrag(sourceActivity, dropData);
                });

        // 2. Hover over destination strip at index 0 and verify space is NOT created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 0);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should not enter reorder mode for incompatible"
                                    + " model",
                            destHelper.getInReorderModeForTesting());
                });

        // 3. Dispatch drop and verify drop is rejected.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Source tab count should remain unchanged",
                            initialSourceCount,
                            ChromeTabUtils.getNumOpenTabs(sourceActivity));
                    assertEquals(
                            "Destination tab count should remain unchanged",
                            initialDestCount,
                            ChromeTabUtils.getNumOpenTabs(destActivity));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_DropOnSourceStrip_RestoresState() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 and tab 1 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 1), /* primaryTabIndex= */ 0);

        // 2. Hover and drop back onto the SOURCE strip at index 2.
        float dropXPx = getDropXPxForIndex(sourceActivity, /* index= */ 2);
        dispatchDragHover(sourceActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        dispatchDropAndEnd(
                sourceActivity, sourceActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        // 3. Verify counts, tab presence, and that the tabs remain in the source window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Source strip should exit reorder mode after drop",
                            mStripLayoutHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Source tab count should remain unchanged",
                            initialSourceCount,
                            ChromeTabUtils.getNumOpenTabs(sourceActivity));
                    assertEquals(
                            "Destination tab count should remain unchanged",
                            initialDestCount,
                            ChromeTabUtils.getNumOpenTabs(destActivity));
                    for (int tabId : draggedTabIds) {
                        assertNotNull(
                                "Source tab should still exist in source window",
                                sourceActivity.getCurrentTabModel().getTabById(tabId));
                        assertNull(
                                "Source tab should not exist in destination window",
                                destActivity.getCurrentTabModel().getTabById(tabId));
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_MultiTab_NonContiguousSelection() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        int unselectedTabId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> sourceActivity.getCurrentTabModel().getTabAt(1).getId());

        // 1. Simulate active drag of non-contiguous tabs 0 and 2 from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForTabs(
                        sourceActivity, Arrays.asList(0, 2), /* primaryTabIndex= */ 0);

        // 2. Hover and drop into destination window at index 1.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_MULTI_TAB);

        // 3. Verify dragged tabs land contiguously at indices 1 and 2 in destination window, while
        // the unselected middle tab remains in source window.
        verifyTabsDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab remainingSourceTab =
                            sourceActivity.getCurrentTabModel().getTabById(unselectedTabId);
                    assertNotNull(
                            "Unselected middle tab should remain in source window",
                            remainingSourceTab);
                    assertEquals(
                            "Unselected middle tab should shift to index 0 in source window",
                            0,
                            sourceActivity.getCurrentTabModel().indexOf(remainingSourceTab));
                    assertNull(
                            "Unselected middle tab should not exist in destination window",
                            destActivity.getCurrentTabModel().getTabById(unselectedTabId));
                });
    }

    private List<Integer> startSimulatedDragForTabs(
            ChromeTabbedActivity sourceActivity,
            List<Integer> tabIndicesToDrag,
            int primaryTabIndex) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel sourceModel = sourceActivity.getCurrentTabModel();
                    List<Tab> tabsToDrag = new ArrayList<>();
                    List<Integer> draggedTabIds = new ArrayList<>();
                    // Multi-tab reparenting inserts each tab at the same start index, which
                    // reverses their final order in the destination window. Reverse the list
                    // so tabs land in their original sequence.
                    for (int i = tabIndicesToDrag.size() - 1; i >= 0; i--) {
                        tabsToDrag.add(sourceModel.getTabAt(tabIndicesToDrag.get(i)));
                    }
                    for (int index : tabIndicesToDrag) {
                        draggedTabIds.add(sourceModel.getTabAt(index).getId());
                    }
                    Tab primaryTab = sourceModel.getTabAt(primaryTabIndex);
                    ChromeDropDataAndroid dropData =
                            new ChromeMultiTabDropDataAndroid.Builder()
                                    .withTabs(tabsToDrag)
                                    .withPrimaryTab(primaryTab)
                                    .build();
                    startSimulatedDrag(sourceActivity, dropData);
                    return draggedTabIds;
                });
    }

    private void verifyTabsDroppedAtIndex(
            ChromeTabbedActivity sourceActivity,
            ChromeTabbedActivity destActivity,
            List<Integer> draggedTabIds,
            int initialSourceCount,
            int initialDestCount,
            int expectedStartIndex) {
        int numTabs = draggedTabIds.size();
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - numTabs,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + numTabs,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel sourceModel = sourceActivity.getCurrentTabModel();
                    TabModel destModel = destActivity.getCurrentTabModel();
                    for (int tabId : draggedTabIds) {
                        assertNull(
                                "Dropped tab should no longer exist in source window",
                                sourceModel.getTabById(tabId));
                        assertNotNull(
                                "Dropped tab should exist in destination window",
                                destModel.getTabById(tabId));
                    }
                    for (int i = 0; i < numTabs; i++) {
                        Tab tab = destModel.getTabById(draggedTabIds.get(i));
                        assertEquals(
                                "Dropped tab should land at expected index",
                                expectedStartIndex + i,
                                destModel.indexOf(tab));
                    }
                });
    }
}
