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
import org.chromium.chrome.browser.dragdrop.ChromeTabGroupDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabstrip.StripVisibilityState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.MimeTypeUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Integration tests for Tab Group drag and drop across windows on the Tab Strip. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test suite tests multi-window instances.")
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabGroupDragDropTest extends TabStripDragDropTestBase {
    @Test
    @LargeTest
    public void testDragDrop_TabGroup_ToSecondWindow_StartIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover over destination strip at the start (x = 0f) and verify start margin is opened.
        dispatchDragHover(
                destActivity, /* hoverXPx= */ 0f, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(
                            "Destination strip should enter reorder mode while hovering at start",
                            destHelper.getInReorderModeForTesting());
                    assertTrue(
                            "Strip start margin should be > 0 to create space at start",
                            destHelper.getReorderStartMarginForTesting() > 0f);
                });

        // 3. Drop at start and verify reorder mode exits and start margin resets.
        dispatchDropAndEnd(
                sourceActivity,
                destActivity,
                /* dropXPx= */ 0f,
                MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    destHelper.finishAnimations();
                    assertFalse(
                            "Destination strip should exit reorder mode after drop",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Strip start margin should reset to 0 after drop",
                            0f,
                            destHelper.getReorderStartMarginForTesting(),
                            0.01f);
                });

        // 4. Verify counts, tab presence, and that the dropped group lands at index 0.
        verifyGroupDroppedAtIndex(
                sourceActivity,
                destActivity,
                sourceGroupId,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 0);
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_ToSecondWindow_Rtl() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> LocalizationUtils.setRtlForTesting(true));

        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. In RTL, start of the strip is on the right edge. Hover near right edge.
        float density = destActivity.getResources().getDisplayMetrics().density;
        float dropXPx = (destHelper.getWidthForTesting() - 10f) * density;
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    destHelper.finishAnimations();
                    assertFalse(
                            "Destination strip should exit reorder mode after drop",
                            destHelper.getInReorderModeForTesting());
                    assertEquals(
                            "Strip start margin should reset to 0 after drop",
                            0f,
                            destHelper.getReorderStartMarginForTesting(),
                            0.01f);
                });

        // 4. Verify counts, tab presence, and that the dropped group lands at index 0.
        verifyGroupDroppedAtIndex(
                sourceActivity,
                destActivity,
                sourceGroupId,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 0);
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_ToSecondWindow_MiddleIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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

        // 3. Drop at index 1 and verify reorder mode exits and space is cleared.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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

        // 4. Verify counts, tab presence, and correct landing index in destination window.
        verifyGroupDroppedAtIndex(
                sourceActivity,
                destActivity,
                sourceGroupId,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_ToSecondWindow_EndIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover over destination strip near the end and verify space is created.
        float hoverXPx = getDropXPxForIndex(destActivity, /* index= */ 2);
        dispatchDragHover(destActivity, hoverXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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
                sourceActivity, destActivity, dropEndXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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

        // 4. Verify counts, tab presence, and that the dropped group lands at end of strip.
        verifyGroupDroppedAtIndex(
                sourceActivity,
                destActivity,
                sourceGroupId,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ initialDestCount);
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_MultipleTabsInGroup() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        // Group tabs 0 and 1 into a multi-tab group.
        TabStripTestUtils.createTabGroup(
                sourceActivity,
                /* isIncognito= */ false,
                /* firstIndex= */ 0,
                /* secondIndex= */ 1);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);
        Token sourceGroupId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> sourceActivity.getCurrentTabModel().getTabAt(0).getTabGroupId());

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of the multi-tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);
        assertEquals("Group being dragged should contain 2 tabs", 2, draggedTabIds.size());

        // 2. Drop into destination window at index 1.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, group membership, and order in destination window.
        verifyGroupDroppedAtIndex(
                sourceActivity,
                destActivity,
                sourceGroupId,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_LastTabGroupClosesSourceWindow() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 1);
        // Group all tabs in source window.
        TabStripTestUtils.createTabGroup(
                sourceActivity,
                /* isIncognito= */ false,
                /* firstIndex= */ 0,
                /* secondIndex= */ 1);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of the only tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover over destination window and dispatch drop.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DROP,
                                            dropXPx,
                                            /* y= */ 0f,
                                            MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));
                });

        // 3. Wait for all tabs in group to be reparented into destination and leave source.
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + draggedTabIds.size(),
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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP,
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

        // 5. Verify source window is closed and dest window has all tabs.
        CriteriaHelper.pollUiThread(
                sourceActivity::isActivityFinishingOrDestroyed,
                "Source window should close when its last tab group is dragged out.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (int tabId : draggedTabIds) {
                        Tab destTab = destActivity.getCurrentTabModel().getTabById(tabId);
                        assertNotNull("Dropped tab should exist in destination window", destTab);
                    }
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_NearExistingGroup_RemainsSeparate() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        // Group tab 0 in destination activity.
        Token destGroupId = createSingleTabGroup(destActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(destHelper);

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover directly over destination group (tab 0) and drop.
        float dropXPx = getDropXPxForTabCenter(destActivity, /* index= */ 0);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, and that the groups remain separate.
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(
                            "Dropped grouped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                    Tab destTab =
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0));
                    assertNotNull(
                            "Dropped grouped tab should exist in destination window", destTab);

                    // Confirm groups did not merge: both groups remain distinct.
                    assertEquals(
                            "Dropped tab should keep its original group ID",
                            sourceGroupId,
                            destTab.getTabGroupId());
                    assertNotEquals(
                            "Dropped tab group should not merge into existing group",
                            destGroupId,
                            destTab.getTabGroupId());
                    assertEquals(
                            "Source group should have 1 tab in destination window",
                            1,
                            destActivity.getCurrentTabModel().getTabsInGroup(sourceGroupId).size());
                    assertEquals(
                            "Existing destination group should still have 1 tab",
                            1,
                            destActivity.getCurrentTabModel().getTabsInGroup(destGroupId).size());
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_OverExpandedGroup_RemainsSeparate() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        // Group tab 0 and tab 1 into an expanded group in destination activity.
        TabStripTestUtils.createTabGroup(
                destActivity, /* isIncognito= */ false, /* firstIndex= */ 0, /* secondIndex= */ 1);
        TabStripUtils.settleDownCompositor(destHelper);
        Token destGroupId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> destActivity.getCurrentTabModel().getTabAt(0).getTabGroupId());

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover directly over the first tab of destination expanded group (tab 0).
        float dropXPx = getDropXPxForTabCenter(destActivity, /* index= */ 0);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, and that both groups remain separate and intact.
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(
                            "Dropped grouped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                    Tab destTab =
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0));
                    assertNotNull(
                            "Dropped grouped tab should exist in destination window", destTab);

                    // Destination expanded group should still have its original 2 tabs.
                    assertEquals(
                            "Destination expanded group should still have 2 tabs",
                            2,
                            destActivity.getCurrentTabModel().getTabsInGroup(destGroupId).size());
                    // Dropped tab keeps its source group ID and did not merge into destination
                    // group.
                    assertEquals(
                            "Dropped tab should keep its original group ID",
                            sourceGroupId,
                            destTab.getTabGroupId());
                    assertNotEquals(
                            "Dropped tab group should not merge into destination group",
                            destGroupId,
                            destTab.getTabGroupId());
                    assertEquals(
                            "Source group should have 1 tab in destination window",
                            1,
                            destActivity.getCurrentTabModel().getTabsInGroup(sourceGroupId).size());
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_CollapsedGroupDoesNotMerge() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

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

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover directly over the destination collapsed group title and drop.
        float dropXPx = getDropXPxForGroupTitle(destActivity, destGroupId[0]);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, and that the group did NOT merge into collapsed group.
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(
                            "Dropped grouped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                    Tab destTab =
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0));
                    assertNotNull(
                            "Dropped grouped tab should exist in destination window", destTab);
                    assertNotEquals(
                            "Dropped tab group should not merge into collapsed group",
                            destGroupId[0],
                            destTab.getTabGroupId());
                    assertEquals(
                            "Dropped tab should retain its original group ID",
                            sourceGroupId,
                            destTab.getTabGroupId());
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
    public void testDragDrop_TabGroup_CancelDrag_RestoresState() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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
                    assertNotNull(
                            "Source grouped tab should still exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_DropOnToolbar_Ignored() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DROP,
                                    dropXPx,
                                    toolbarYPx,
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));

                    getTabStripDragHandler(sourceActivity)
                            .onDrag(
                                    sourceActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DRAG_ENDED,
                                            /* x= */ 0f,
                                            /* y= */ 0f,
                                            MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));
                });

        // 3. Verify drop was ignored: tab counts remain unchanged and group didn't move.
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
                    assertNotNull(
                            "Dragged tab should still exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                    assertNull(
                            "Dragged tab should not exist in destination window",
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_HoverExitReenterDrop() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));

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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));
                    // Second event triggers onDragLocationInStrip / updateReorderPosition.
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    dropXPx,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP));

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

        // 6. Complete drop and verify tab group reparents successfully at index 1.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        verifyGroupDroppedAtIndex(
                sourceActivity,
                destActivity,
                sourceGroupId,
                draggedTabIds,
                initialSourceCount,
                initialDestCount,
                /* expectedStartIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_IncognitoTabGroup_ToRegularWindow_Rejected() throws Exception {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 0);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of an incognito tab group from source window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab incognitoTab = mock(Tab.class);
                    doReturn(true).when(incognitoTab).isIncognito();
                    doReturn(true).when(incognitoTab).isIncognitoBranded();
                    doReturn(9999).when(incognitoTab).getId();

                    Token groupId = new Token(3L, 3L);
                    TabGroupMetadata metadata =
                            new TabGroupMetadata(
                                    /* selectedTabId= */ 9999,
                                    /* sourceWindowId= */ 0,
                                    groupId,
                                    /* tabIdsToUrls= */ new ArrayList<>(),
                                    /* tabGroupColor= */ 0,
                                    /* tabGroupTitle= */ "Incognito Group",
                                    /* mhtmlTabTitle= */ null,
                                    /* tabGroupCollapsed= */ false,
                                    /* isIncognito= */ true);
                    ChromeDropDataAndroid dropData =
                            new ChromeTabGroupDropDataAndroid.Builder()
                                    .withTabs(Collections.singletonList(incognitoTab))
                                    .withTabGroupMetadata(metadata)
                                    .build();
                    startSimulatedDrag(sourceActivity, dropData);
                });

        // 2. Hover over destination strip at index 0 and verify space is NOT created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 0);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should not enter reorder mode for incompatible"
                                    + " model",
                            destHelper.getInReorderModeForTesting());
                });

        // 3. Dispatch drop and verify drop is rejected.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

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
    public void testDragDrop_TabGroup_CollapsedGroup_ToSecondWindow() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token[] sourceGroupId = new Token[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel sourceModel = sourceActivity.getCurrentTabModel();
                    sourceModel.createSingleTabGroup(sourceModel.getTabAt(0));
                    sourceGroupId[0] = sourceModel.getTabAt(0).getTabGroupId();
                    sourceModel.setTabGroupCollapsed(sourceGroupId[0], /* isCollapsed= */ true);
                });
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of the collapsed tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Drop into destination window at index 1.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, and that the collapsed state is preserved.
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab destTab =
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0));
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                    assertEquals(
                            "Dropped tab should preserve its group ID",
                            sourceGroupId[0],
                            destTab.getTabGroupId());
                    assertTrue(
                            "Dropped group should remain collapsed in destination window",
                            destActivity
                                    .getCurrentTabModel()
                                    .getTabGroupCollapsed(sourceGroupId[0]));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_CollapsedGroup_ExpandsIfTabStripHidden() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token[] sourceGroupId = new Token[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel sourceModel = sourceActivity.getCurrentTabModel();
                    sourceModel.createSingleTabGroup(sourceModel.getTabAt(0));
                    sourceGroupId[0] = sourceModel.getTabAt(0).getTabGroupId();
                    sourceModel.setTabGroupCollapsed(sourceGroupId[0], /* isCollapsed= */ true);
                });
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // Set destination strip visibility to hidden so that shouldApplyCollapsedState expands it.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    destActivity
                            .getLayoutManager()
                            .getStripLayoutHelperManager()
                            .setStripVisibilityState(
                                    StripVisibilityState.HIDDEN_BY_HEIGHT_TRANSITION,
                                    /* clear= */ false);
                });

        // 1. Simulate active drag of the collapsed tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Drop into destination window at index 1.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, and that the group was expanded (not collapsed).
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 1,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab destTab =
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0));
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                    assertEquals(
                            "Dropped tab should preserve its group ID",
                            sourceGroupId[0],
                            destTab.getTabGroupId());
                    assertFalse(
                            "Dropped group should expand when strip is hidden",
                            destActivity
                                    .getCurrentTabModel()
                                    .getTabGroupCollapsed(sourceGroupId[0]));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_TabGroup_DropOnSourceStrip_RestoresState() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        Token sourceGroupId = createSingleTabGroup(sourceActivity, /* tabIndex= */ 0);
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);

        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab group from source window.
        List<Integer> draggedTabIds =
                startSimulatedDragForGroup(sourceActivity, /* tabIndexInGroup= */ 0);

        // 2. Hover and drop back onto the SOURCE strip at index 1.
        float dropXPx = getDropXPxForIndex(sourceActivity, /* index= */ 1);
        dispatchDragHover(sourceActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);
        dispatchDropAndEnd(
                sourceActivity, sourceActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB_GROUP);

        // 3. Verify counts, tab presence, and that the tab group remains in the source window.
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
                    assertNotNull(
                            "Source grouped tab should still exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                    assertNull(
                            "Source grouped tab should not exist in destination window",
                            destActivity.getCurrentTabModel().getTabById(draggedTabIds.get(0)));
                    assertEquals(
                            "Source group should still belong to original group",
                            sourceGroupId,
                            sourceActivity
                                    .getCurrentTabModel()
                                    .getTabById(draggedTabIds.get(0))
                                    .getTabGroupId());
                });
    }

    private Token createSingleTabGroup(ChromeTabbedActivity activity, int tabIndex) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = activity.getCurrentTabModel();
                    model.createSingleTabGroup(model.getTabAt(tabIndex));
                    return model.getTabAt(tabIndex).getTabGroupId();
                });
    }

    private List<Integer> startSimulatedDragForGroup(
            ChromeTabbedActivity sourceActivity, int tabIndexInGroup) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel sourceModel = sourceActivity.getCurrentTabModel();
                    Tab groupedTab = sourceModel.getTabAt(tabIndexInGroup);
                    Token groupId = groupedTab.getTabGroupId();
                    int sourceInstanceId =
                            sourceActivity
                                    .getMultiInstanceMangerForTesting()
                                    .getCurrentInstanceId();
                    List<Tab> groupTabs = sourceModel.getTabsInGroup(groupId);
                    TabGroupMetadata metadata =
                            TabGroupMetadataExtractor.extractTabGroupMetadata(
                                    sourceModel,
                                    groupTabs,
                                    sourceInstanceId,
                                    TabModelUtils.getCurrentTabId(sourceModel));
                    ChromeDropDataAndroid dropData =
                            new ChromeTabGroupDropDataAndroid.Builder()
                                    .withTabs(groupTabs)
                                    .withTabGroupMetadata(metadata)
                                    .build();
                    startSimulatedDrag(sourceActivity, dropData);

                    List<Integer> tabIds = new ArrayList<>();
                    for (Tab tab : groupTabs) {
                        tabIds.add(tab.getId());
                    }
                    return tabIds;
                });
    }

    private float getDropXPxForGroupTitle(ChromeTabbedActivity destActivity, Token groupId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutHelper destHelper =
                            TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
                    StripLayoutGroupTitle title = destHelper.findGroupTitle(groupId);
                    assertNotNull("Group title should exist", title);
                    float density = destActivity.getResources().getDisplayMetrics().density;
                    return (title.getDrawX() + title.getWidth() / 2) * density;
                });
    }

    private void verifyGroupDroppedAtIndex(
            ChromeTabbedActivity sourceActivity,
            ChromeTabbedActivity destActivity,
            Token originalGroupId,
            List<Integer> draggedTabIds,
            int initialSourceCount,
            int initialDestCount,
            int expectedStartIndex) {
        int groupSize = draggedTabIds.size();
        MultiWindowTestHelper.waitForTabs(
                "sourceActivity",
                sourceActivity,
                /* expectedTotalTabCount= */ initialSourceCount - groupSize,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + groupSize,
                /* expectedActiveTabId= */ Tab.INVALID_TAB_ID);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel sourceModel = sourceActivity.getCurrentTabModel();
                    TabModel destModel = destActivity.getCurrentTabModel();
                    for (int tabId : draggedTabIds) {
                        assertNull(
                                "Dropped grouped tab should no longer exist in source window",
                                sourceModel.getTabById(tabId));
                        Tab destTab = destModel.getTabById(tabId);
                        assertNotNull(
                                "Dropped grouped tab should exist in destination window", destTab);
                        assertEquals(
                                "Dropped tab should belong to the same tab group",
                                originalGroupId,
                                destTab.getTabGroupId());
                    }
                    Tab firstTab = destModel.getTabById(draggedTabIds.get(0));
                    assertEquals(
                            "Dropped group first tab should land at expected index",
                            expectedStartIndex,
                            destModel.indexOf(firstTab));
                    List<Tab> destGroupTabs = destModel.getTabsInGroup(originalGroupId);
                    assertEquals(
                            "Destination group size should match original group size",
                            groupSize,
                            destGroupTabs.size());
                });
    }
}
