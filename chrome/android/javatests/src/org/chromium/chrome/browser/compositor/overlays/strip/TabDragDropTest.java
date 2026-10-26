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
import org.chromium.chrome.browser.dragdrop.ChromeTabDropDataAndroid;
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

/** Integration tests for single Tab drag and drop across windows on the Tab Strip. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test suite tests multi-window instances.")
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabDragDropTest extends TabStripDragDropTestBase {
    @Test
    @LargeTest
    public void testDragDrop_SingleTab_ToSecondWindow_StartIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover over destination strip at the start (x = 0f) and verify start margin is opened.
        dispatchDragHover(destActivity, /* hoverXPx= */ 0f, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                sourceActivity, destActivity, /* dropXPx= */ 0f, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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

        // 4. Verify counts, tab presence, and that the dropped tab lands at index 0.
        verifyTabDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabId,
                initialSourceCount,
                initialDestCount,
                /* expectedIndex= */ 0);
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_ToSecondWindow_Rtl() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> LocalizationUtils.setRtlForTesting(true));

        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. In RTL, the start of the strip is on the right edge. Hover near the right edge.
        float density = destActivity.getResources().getDisplayMetrics().density;
        float dropXPx = (destHelper.getWidthForTesting() - 10f) * density;
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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

        // 4. Verify counts, tab presence, and that the dropped tab lands at index 0.
        verifyTabDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabId,
                initialSourceCount,
                initialDestCount,
                /* expectedIndex= */ 0);
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_ToSecondWindow_MiddleIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
        verifyTabDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabId,
                initialSourceCount,
                initialDestCount,
                /* expectedIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_ToSecondWindow_EndIndex() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover over destination strip near the end and verify space is created.
        float hoverXPx = getDropXPxForIndex(destActivity, /* index= */ 2);
        dispatchDragHover(destActivity, hoverXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                sourceActivity, destActivity, dropEndXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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

        // 4. Verify counts, tab presence, and that the dropped tab lands at end of strip.
        verifyTabDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabId,
                initialSourceCount,
                initialDestCount,
                /* expectedIndex= */ initialDestCount);
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_LastTabClosesSourceWindow() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 0);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of the only tab from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover over destination window and dispatch drop.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DROP,
                                            dropXPx,
                                            /* y= */ 0f,
                                            MimeTypeUtils.CHROME_MIMETYPE_TAB));
                });

        // 3. Wait for the tab to be reparented into destination window and removed from source.
        MultiWindowTestHelper.waitForTabs(
                "destActivity",
                destActivity,
                /* expectedTotalTabCount= */ initialDestCount + 1,
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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB,
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

        // 5. Verify source window is closed and dest window has the tab.
        CriteriaHelper.pollUiThread(
                sourceActivity::isActivityFinishingOrDestroyed,
                "Source window should close when its last tab is dragged out.");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab destTab = destActivity.getCurrentTabModel().getTabById(draggedTabId);
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_MergeIntoExistingGroup() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        // Group tabs 0 and 1 in destination activity.
        TabStripTestUtils.createTabGroup(
                destActivity, /* isIncognito= */ false, /* firstIndex= */ 0, /* secondIndex= */ 1);
        TabStripUtils.settleDownCompositor(destHelper);
        Token destGroupId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> destActivity.getCurrentTabModel().getTabAt(0).getTabGroupId());

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

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
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);

        // 3. Verify counts, tab presence, and group merge.
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

        CriteriaHelper.pollUiThread(
                () -> {
                    assertNull(
                            "Dropped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabId));
                    Tab destTab = destActivity.getCurrentTabModel().getTabById(draggedTabId);
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                    assertEquals(
                            "Dropped tab should merge into destination group",
                            destGroupId,
                            destTab.getTabGroupId());
                    assertEquals(
                            "Destination group should now have 3 tabs",
                            3,
                            destActivity.getCurrentTabModel().getTabsInGroup(destGroupId).size());
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_CollapsedGroupDoesNotMerge() throws Exception {
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

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover directly over the collapsed group title and drop.
        float dropXPx =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            StripLayoutGroupTitle title = destHelper.findGroupTitle(destGroupId[0]);
                            float density = destActivity.getResources().getDisplayMetrics().density;
                            return (title.getDrawX() + title.getWidth() / 2) * density;
                        });
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);

        // 3. Verify counts, tab presence, and that the tab did NOT merge into collapsed group.
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
                            "Dropped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabId));
                    Tab destTab = destActivity.getCurrentTabModel().getTabById(draggedTabId);
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                    assertNotEquals(
                            "Dropped tab should not merge into collapsed group",
                            destGroupId[0],
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
    public void testDragDrop_SingleTab_DragOutOfSourceGroup() throws Exception {
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

        // 1. Simulate active drag of tab 0 (which is in a group) from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Drop into destination window at index 1.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);

        // 3. Verify counts, tab presence, and that the tab was removed from group upon
        // cross-window drop.
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
                            "Dropped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabId));
                    Tab destTab = destActivity.getCurrentTabModel().getTabById(draggedTabId);
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                    assertNull(
                            "Dropped tab should be ungrouped in destination window",
                            destTab.getTabGroupId());
                    assertEquals(
                            "Dropped tab should land at index 1 in destination window",
                            1,
                            destActivity.getCurrentTabModel().indexOf(destTab));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_CancelDrag_RestoresState() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                            "Source tab should still exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabId));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_DropOnToolbar_Ignored() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB));
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DROP,
                                    dropXPx,
                                    toolbarYPx,
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB));

                    getTabStripDragHandler(sourceActivity)
                            .onDrag(
                                    sourceActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DRAG_ENDED,
                                            /* x= */ 0f,
                                            /* y= */ 0f,
                                            MimeTypeUtils.CHROME_MIMETYPE_TAB));
                });

        // 3. Verify drop was ignored: tab counts remain unchanged and tab didn't move.
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
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabId));
                    assertNull(
                            "Dragged tab should not exist in destination window",
                            destActivity.getCurrentTabModel().getTabById(draggedTabId));
                });
    }

    @Test
    @LargeTest
    public void testDragDrop_SingleTab_HoverExitReenterDrop() throws Exception {
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 2);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 2);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of tab 0 from source window.
        int draggedTabId = startSimulatedDragForTab(sourceActivity, /* tabIndex= */ 0);

        // 2. Hover over destination strip at index 1 and verify space is created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 1);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB));

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
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB));
                    // Second event triggers onDragLocationInStrip / updateReorderPosition.
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    dropXPx,
                                    /* y= */ 0f,
                                    MimeTypeUtils.CHROME_MIMETYPE_TAB));

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

        // 6. Complete drop and verify tab reparents successfully at index 1.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);

        verifyTabDroppedAtIndex(
                sourceActivity,
                destActivity,
                draggedTabId,
                initialSourceCount,
                initialDestCount,
                /* expectedIndex= */ 1);
    }

    @Test
    @LargeTest
    public void testDragDrop_IncognitoTab_ToRegularWindow_Rejected() throws Exception {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        ChromeTabbedActivity sourceActivity = setupSourceWindow(/* additionalTabs= */ 0);
        ChromeTabbedActivity destActivity =
                setupDestinationWindow(sourceActivity, /* additionalTabs= */ 1);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);

        int initialSourceCount = ChromeTabUtils.getNumOpenTabs(sourceActivity);
        int initialDestCount = ChromeTabUtils.getNumOpenTabs(destActivity);

        // 1. Simulate active drag of an incognito tab from source window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab incognitoTab = mock(Tab.class);
                    doReturn(true).when(incognitoTab).isIncognito();
                    doReturn(true).when(incognitoTab).isIncognitoBranded();
                    doReturn(9999).when(incognitoTab).getId();

                    ChromeDropDataAndroid dropData =
                            new ChromeTabDropDataAndroid.Builder().withTab(incognitoTab).build();
                    startSimulatedDrag(sourceActivity, dropData);
                });

        // 2. Hover over destination strip at index 0 and verify space is NOT created.
        float dropXPx = getDropXPxForIndex(destActivity, /* index= */ 0);
        dispatchDragHover(destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Destination strip should not enter reorder mode for incompatible"
                                    + " model",
                            destHelper.getInReorderModeForTesting());
                });

        // 3. Dispatch drop and verify drop is rejected.
        dispatchDropAndEnd(
                sourceActivity, destActivity, dropXPx, MimeTypeUtils.CHROME_MIMETYPE_TAB);

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

    private int startSimulatedDragForTab(ChromeTabbedActivity sourceActivity, int tabIndex) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = sourceActivity.getCurrentTabModel().getTabAt(tabIndex);
                    startSimulatedDrag(
                            sourceActivity,
                            new ChromeTabDropDataAndroid.Builder().withTab(tab).build());
                    return tab.getId();
                });
    }

    private Tab verifyTabDroppedAtIndex(
            ChromeTabbedActivity sourceActivity,
            ChromeTabbedActivity destActivity,
            int draggedTabId,
            int initialSourceCount,
            int initialDestCount,
            int expectedIndex) {
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

        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(
                            "Dropped tab should no longer exist in source window",
                            sourceActivity.getCurrentTabModel().getTabById(draggedTabId));
                    Tab destTab = destActivity.getCurrentTabModel().getTabById(draggedTabId);
                    assertNotNull("Dropped tab should exist in destination window", destTab);
                    assertEquals(
                            "Dropped tab should land at index " + expectedIndex,
                            expectedIndex,
                            destActivity.getCurrentTabModel().indexOf(destTab));
                    return destTab;
                });
    }
}
