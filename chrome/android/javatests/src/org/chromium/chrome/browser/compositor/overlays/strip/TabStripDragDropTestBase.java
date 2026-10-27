// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.ClipDescription;
import android.os.Build;
import android.view.DragEvent;
import android.view.View;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.TabStripDragHandler;
import org.chromium.chrome.browser.dragdrop.ChromeDropDataAndroid;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabDragHandlerBase;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.dragdrop.DragDropGlobalState;

import java.util.ArrayList;
import java.util.List;

/** Base class for Tab Strip drag-and-drop integration tests. */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
@MinAndroidSdkLevel(Build.VERSION_CODES.S)
@DoNotBatch(reason = "This test suite tests multi-window instances.")
public abstract class TabStripDragDropTestBase {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    protected StripLayoutHelper mStripLayoutHelper;
    protected final List<ChromeTabbedActivity> mDestActivities = new ArrayList<>();

    @BeforeClass
    public static void setUpBeforeClass() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
    }

    @AfterClass
    public static void tearDownAfterClass() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startOnBlankPage();
        mStripLayoutHelper =
                TabStripTestUtils.getActiveStripLayoutHelper(mActivityTestRule.getActivity());
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);
    }

    @After
    public void tearDown() {
        for (ChromeTabbedActivity activity : mDestActivities) {
            ApplicationTestUtils.finishActivity(activity);
        }
        mDestActivities.clear();

        if (DragDropGlobalState.hasValue()) {
            DragDropGlobalState.clearForTesting();
        }
        TabDragHandlerBase.setDragTokenForTesting(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity activity = mActivityTestRule.getActivity();
                    if (activity != null && !activity.isActivityFinishingOrDestroyed()) {
                        TabStripDragHandler dragHandler = getTabStripDragHandler(activity);
                        if (dragHandler != null) {
                            dragHandler.setDragSourceViewForTesting(null);
                        }
                        if (mStripLayoutHelper != null
                                && mStripLayoutHelper.getInReorderModeForTesting()) {
                            mStripLayoutHelper.stopReorderMode(/* isDragCancelled= */ false);
                        }
                        activity.getCurrentTabModel()
                                .clearMultiSelection(/* notifyObservers= */ false);
                    }
                });
    }

    protected ChromeTabbedActivity setupSourceWindow(int additionalTabs) {
        ChromeTabbedActivity sourceActivity = mActivityTestRule.getActivity();
        if (additionalTabs > 0) {
            for (int i = 0; i < additionalTabs; i++) {
                ChromeTabUtils.newTabFromMenu(
                        InstrumentationRegistry.getInstrumentation(), sourceActivity);
            }
        }
        TabStripUtils.settleDownCompositor(mStripLayoutHelper);
        return sourceActivity;
    }

    protected ChromeTabbedActivity setupDestinationWindow(
            ChromeTabbedActivity sourceActivity, int additionalTabs) {
        ChromeTabbedActivity destActivity =
                MultiWindowTestHelper.createNewChromeTabbedActivity(sourceActivity);
        mDestActivities.add(destActivity);
        CriteriaHelper.pollUiThread(() -> destActivity.getLayoutManager() != null);
        StripLayoutHelper destHelper = TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
        TabStripUtils.settleDownCompositor(destHelper);
        if (additionalTabs > 0) {
            for (int i = 0; i < additionalTabs; i++) {
                ChromeTabUtils.newTabFromMenu(
                        InstrumentationRegistry.getInstrumentation(), destActivity);
            }
            TabStripUtils.settleDownCompositor(destHelper);
        }
        return destActivity;
    }

    protected void startSimulatedDrag(
            ChromeTabbedActivity sourceActivity, ChromeDropDataAndroid dropData) {
        int sourceInstanceId =
                sourceActivity.getMultiInstanceMangerForTesting().getCurrentInstanceId();
        Token token =
                DragDropGlobalState.store(
                        sourceInstanceId, dropData, /* dragShadowBuilder= */ null);
        TabDragHandlerBase.setDragTokenForTesting(token);
        getTabStripDragHandler(sourceActivity)
                .setDragSourceViewForTesting(sourceActivity.findViewById(R.id.control_container));
        assertTrue("DragDropGlobalState should have value", DragDropGlobalState.hasValue());
    }

    protected TabStripDragHandler getTabStripDragHandler(ChromeTabbedActivity activity) {
        StripLayoutHelperManager manager =
                activity.getLayoutManager() != null
                        ? activity.getLayoutManager().getStripLayoutHelperManager()
                        : null;
        return manager != null ? manager.getTabStripDragHandlerForTesting() : null;
    }

    protected float getDropXPxForIndex(ChromeTabbedActivity destActivity, int index) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutHelper destHelper =
                            TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    float density = destActivity.getResources().getDisplayMetrics().density;
                    return destTabs[index].getTouchTargetLeft() * density;
                });
    }

    protected float getDropXPxForTabCenter(ChromeTabbedActivity destActivity, int index) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutHelper destHelper =
                            TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
                    StripLayoutTab[] destTabs = destHelper.getStripLayoutTabsForTesting();
                    float density = destActivity.getResources().getDisplayMetrics().density;
                    return (destTabs[index].getDrawX() + destTabs[index].getWidth() / 2) * density;
                });
    }

    protected float getDropXPxBeyondEnd(ChromeTabbedActivity destActivity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    StripLayoutHelper destHelper =
                            TabStripTestUtils.getActiveStripLayoutHelper(destActivity);
                    float density = destActivity.getResources().getDisplayMetrics().density;
                    return destHelper.getWidthForTesting() * density + 100f;
                });
    }

    protected void dispatchDragHover(
            ChromeTabbedActivity destActivity, float hoverXPx, String mimeType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabStripDragHandler destDragHandler = getTabStripDragHandler(destActivity);
                    View destContainerView = destActivity.findViewById(R.id.control_container);

                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_STARTED,
                                    /* x= */ 0f,
                                    /* y= */ 0f,
                                    mimeType));
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_ENTERED,
                                    hoverXPx,
                                    /* y= */ 0f,
                                    mimeType));
                    // TabStripDragHandler ignores ACTION_DRAG_ENTERED and consumes the first
                    // ACTION_DRAG_LOCATION to trigger #onDragEnter. A second ACTION_DRAG_LOCATION
                    // is required to trigger #onDragLocationInStrip / #updateReorderPosition.
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    hoverXPx,
                                    /* y= */ 0f,
                                    mimeType));
                    destDragHandler.onDrag(
                            destContainerView,
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_LOCATION,
                                    hoverXPx,
                                    /* y= */ 0f,
                                    mimeType));

                    TabStripTestUtils.getActiveStripLayoutHelper(destActivity).finishAnimations();
                });
    }

    protected void dispatchDropAndEnd(
            ChromeTabbedActivity sourceActivity,
            ChromeTabbedActivity destActivity,
            float dropXPx,
            String mimeType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    createMockDragEvent(
                                            DragEvent.ACTION_DROP, dropXPx, /* y= */ 0f, mimeType));
                });

        // Let the drop and reparenting complete before ending the drag session.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DragEvent dragEndEvent =
                            createMockDragEvent(
                                    DragEvent.ACTION_DRAG_ENDED,
                                    /* x= */ 0f,
                                    /* y= */ 0f,
                                    mimeType,
                                    /* result= */ true);
                    getTabStripDragHandler(sourceActivity)
                            .onDrag(
                                    sourceActivity.findViewById(R.id.control_container),
                                    dragEndEvent);
                    getTabStripDragHandler(destActivity)
                            .onDrag(
                                    destActivity.findViewById(R.id.control_container),
                                    dragEndEvent);
                    TabStripTestUtils.getActiveStripLayoutHelper(destActivity).finishAnimations();
                });
    }

    protected DragEvent createMockDragEvent(int action, float x, float y, String mimeType) {
        return createMockDragEvent(action, x, y, mimeType, /* result= */ false);
    }

    protected DragEvent createMockDragEvent(
            int action, float x, float y, String mimeType, boolean result) {
        DragEvent event = mock(DragEvent.class);
        doReturn(action).when(event).getAction();
        doReturn(x).when(event).getX();
        doReturn(y).when(event).getY();
        doReturn(result).when(event).getResult();

        ClipDescription clipDescription = new ClipDescription("drag_tabs", new String[] {mimeType});
        doReturn(clipDescription).when(event).getClipDescription();
        return event;
    }
}
