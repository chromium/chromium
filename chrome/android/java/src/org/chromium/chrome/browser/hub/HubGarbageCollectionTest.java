// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.GarbageCollectionTestUtils.canBeGarbageCollected;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.memory.JavaHeapDumpGenerator;
import org.chromium.base.test.transit.TrafficControl;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.lang.ref.WeakReference;

/** Garbage collection tests for the Hub. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(
        reason =
                "GC tests require a clean process to avoid interference from leaks in other tests.")
public class HubGarbageCollectionTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testHubCoordinatorGarbageCollection() throws Exception {
        WebPageStation startPage = mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity activity = mCtaTestRule.getActivity();

        // 1. Enter the Hub.
        RegularTabSwitcherStation tabSwitcher = startPage.openRegularTabSwitcher();

        // 2. Get reference to HubCoordinator and views.
        VarHolder vars =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            HubManagerImpl hubManager =
                                    (HubManagerImpl)
                                            activity.getHubManagerSupplierForTesting().get();
                            assertNotNull(hubManager);
                            HubCoordinator hubCoordinator =
                                    hubManager.getHubCoordinatorForTesting();
                            assertNotNull(hubCoordinator);

                            View toolbar = activity.findViewById(R.id.hub_toolbar);
                            View paneHost = activity.findViewById(R.id.hub_pane_host);
                            View actionButton = activity.findViewById(R.id.toolbar_action_button);

                            return new VarHolder(hubCoordinator, toolbar, paneHost, actionButton);
                        });

        WeakReference<HubCoordinator> coordinatorRef = new WeakReference<>(vars.mHubCoordinator);
        WeakReference<View> toolbarRef = new WeakReference<>(vars.mToolbar);
        WeakReference<View> paneHostRef = new WeakReference<>(vars.mPaneHost);
        WeakReference<View> actionButtonRef = new WeakReference<>(vars.mActionButton);

        vars.mHubCoordinator = null;
        vars.mToolbar = null;
        vars.mPaneHost = null;
        vars.mActionButton = null;
        vars = null;

        // 3. Exit the Hub (select the tab at index 0 to go back to WebPageStation).
        WebPageStation page = tabSwitcher.selectTabAtIndex(0, WebPageStation.newBuilder());
        assertNotNull(page);

        // Hop off public transit to clear active stations from the framework.
        TrafficControl.hopOffPublicTransit();

        // Null out all public transit stations to allow them to be GC'ed.
        startPage = null;
        tabSwitcher = null;
        page = null;

        // 4. Assert GC.
        boolean coordinatorGCed = canBeGarbageCollected(coordinatorRef);
        boolean toolbarGCed = canBeGarbageCollected(toolbarRef);
        boolean paneHostGCed = canBeGarbageCollected(paneHostRef);
        boolean actionButtonGCed = canBeGarbageCollected(actionButtonRef);

        if (!coordinatorGCed || !toolbarGCed || !paneHostGCed || !actionButtonGCed) {
            String filePath = activity.getCacheDir().getPath() + "/hub_leak.hprof";
            if (JavaHeapDumpGenerator.generateHprof(filePath)) {
                Log.i("HubLeakDebug", "HEAP DUMP GENERATED AT: " + filePath);
            } else {
                Log.e("HubLeakDebug", "FAILED TO GENERATE HEAP DUMP");
            }
        }

        assertTrue("HubCoordinator should be garbage collected", coordinatorGCed);
        assertTrue("HubToolbarView should be garbage collected", toolbarGCed);
        assertTrue("HubPaneHostView should be garbage collected", paneHostGCed);
        assertTrue("Action Button should be garbage collected", actionButtonGCed);
    }

    private static class VarHolder {
        public HubCoordinator mHubCoordinator;
        public View mToolbar;
        public View mPaneHost;
        public View mActionButton;

        public VarHolder(
                HubCoordinator coordinator, View toolbar, View paneHost, View actionButton) {
            mHubCoordinator = coordinator;
            mToolbar = toolbar;
            mPaneHost = paneHost;
            mActionButton = actionButton;
        }
    }
}
