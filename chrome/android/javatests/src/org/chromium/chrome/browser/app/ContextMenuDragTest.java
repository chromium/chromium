// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.dragdrop.DropDataAndroid;

import java.util.concurrent.TimeoutException;

/** Integration tests for drag interactions with context menu. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.FORCE_CONTEXT_MENU_POPUP
})
@EnableFeatures({ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU})
@Batch(Batch.PER_CLASS)
public class ContextMenuDragTest {

    // Test distance
    private static final int TEST_MIN_DIST = 10;
    private static final String TEST_PATH =
            "/chrome/test/data/android/contextmenu/context_menu_test.html";
    private static final String TEST_IMAGE_ID = "testImage";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    static TestDragAndDropDelegate sTestDragAndDropDelegate = new TestDragAndDropDelegate();

    @Rule
    public BlankCTATabInitialStateRule mTestRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private EmbeddedTestServer mTestServer;

    private ContextMenuCoordinator mContextMenu;
    private String mTestUrl;
    private Tab mTab;

    @BeforeClass
    public static void setupBeforeClass() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        // Stop the real call to android View#startDragAndDrop. Test file do not have real touches
        // over the screen so there's no way to end the drag event properly. Doing this in
        // @BeforeClass since ViewAndroidDelegate is created earlier than @Before due to batching.
        sTestDragAndDropDelegate = new TestDragAndDropDelegate();
        ViewAndroidDelegate.setDragAndDropDelegateForTest(sTestDragAndDropDelegate);
    }

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mTestUrl = mTestServer.getURL(TEST_PATH);

        sActivityTestRule.loadUrl(mTestUrl);
        mTab = sActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> mTab.isUserInteractable() && !mTab.isLoading());
        ChromeApplicationTestUtils.assertWaitForPageScaleFactorMatch(
                sActivityTestRule.getActivity(), 0.5f);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mContextMenu != null) mContextMenu.dismiss();
                });
        sTestDragAndDropDelegate.reset();
    }

    @AfterClass
    public static void tearDownAfterClass() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
    }

    @Test
    @SmallTest
    public void testTriggerContextMenu_Image() throws TimeoutException {
        longPressOpenContextMenu(TEST_IMAGE_ID);
        DropDataAndroid data = getDropData();
        Assert.assertTrue("Drop data should have Image.", data.hasImage());
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=" + ContentFeatures.TOUCH_DRAG_AND_CONTEXT_MENU + "<Study",
        "force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:DragAndDropMovementThresholdDipParam/" + TEST_MIN_DIST
    })
    public void testTriggerContextMenuWithDrag() throws TimeoutException {
        longPressOpenContextMenu(TEST_IMAGE_ID);

        final Rect location = DOMUtils.getNodeBounds(mTab.getWebContents(), TEST_IMAGE_ID);
        final int jitterRange = 1;

        // Clank is not forwarding drag start event to blink; instead, browser only remembers the
        // first drag events as the starting point of context menu.
        DragEvent event1 =
                mockDragEvent(
                        location.centerX(), location.centerY(), DragEvent.ACTION_DRAG_LOCATION);
        DragEvent event2 =
                mockDragEvent(
                        location.centerX() + jitterRange,
                        location.centerY() + jitterRange,
                        DragEvent.ACTION_DRAG_LOCATION);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab.getContentView().onDragEvent(event1);
                    mTab.getContentView().onDragEvent(event2);
                });
        assertContextMenuShowing(true);

        final int minDragThresholdPx =
                (int)
                                (sActivityTestRule
                                                .getActivity()
                                                .getResources()
                                                .getDisplayMetrics()
                                                .density
                                        * TEST_MIN_DIST)
                        + 1;
        DragEvent event3 =
                mockDragEvent(
                        location.centerX() + minDragThresholdPx,
                        location.centerY() + minDragThresholdPx,
                        DragEvent.ACTION_DRAG_LOCATION);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTab.getContentView().onDragEvent(event3);
                });
        assertContextMenuShowing(false);
    }

    private void longPressOpenContextMenu(String nodeId) throws TimeoutException {
        mContextMenu = ContextMenuUtils.openContextMenu(mTab, nodeId);
        Assert.assertNotNull("Context menu is null.", mContextMenu);
        assertContextMenuShowing(true);
    }

    private void assertContextMenuShowing(boolean showing) {
        Assert.assertNotNull("Context menu dialog is null.", mContextMenu.getDialogForTest());
        Assert.assertEquals(
                "Context menu dialog is not showing.",
                showing,
                mContextMenu.getDialogForTest().isShowing());
    }

    private DropDataAndroid getDropData() {
        Assert.assertEquals(
                "#startDragAndDrop is not called.",
                1,
                sTestDragAndDropDelegate.startDragAndDropCallCount);
        Assert.assertNotNull("DropDataAndroid is null.", sTestDragAndDropDelegate.lastDropData);
        return sTestDragAndDropDelegate.lastDropData;
    }

    private DragEvent mockDragEvent(int x, int y, int actionType) {
        DragEvent event = mock(DragEvent.class);
        doReturn(actionType).when(event).getAction();
        doReturn((float) x).when(event).getX();
        doReturn((float) y).when(event).getY();
        return event;
    }

    // Test impl for ViewAndroidDelegate.DragAndDropDelegate as compromise that mockito does not
    // work well with @BeforeClass.
    static class TestDragAndDropDelegate implements DragAndDropDelegate {
        public DropDataAndroid lastDropData;
        public int startDragAndDropCallCount;

        @Override
        public boolean startDragAndDrop(
                View containerView,
                Bitmap shadowImage,
                DropDataAndroid dropData,
                Context context,
                int cursorOffsetX,
                int cursorOffsetY,
                int dragObjRectWidth,
                int dragObjRectHeight) {
            return startDragAndDrop(containerView, null, dropData);
        }

        @Override
        public boolean startDragAndDrop(
                View containerView, DragShadowBuilder dragShadowBuilder, DropDataAndroid dropData) {
            lastDropData = dropData;
            startDragAndDropCallCount += 1;

            return true;
        }

        void reset() {
            lastDropData = null;
            startDragAndDropCallCount = 0;
        }
    }
}
