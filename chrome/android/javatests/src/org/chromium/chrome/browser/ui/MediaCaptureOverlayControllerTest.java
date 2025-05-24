// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.view.View;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests for MediaCaptureOverlayController. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MediaCaptureOverlayControllerTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;

    private MediaCaptureOverlayController mController;
    private View mOverlayView;

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mController = MediaCaptureOverlayController.from(mActivity.getWindowAndroid());
                });
        Assert.assertNotNull(mController);
        mOverlayView = mActivity.findViewById(R.id.capture_overlay);
    }

    public Tab openNewTab() {
        // Launch a new tab in the foreground.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), mActivity);
        return mActivity.getActivityTab();
    }

    public void waitForOverlayVisibility(boolean visible) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return visible
                            ? (mOverlayView.getVisibility() == View.VISIBLE)
                            : (mOverlayView.getVisibility() != View.VISIBLE);
                },
                "Overlay did not reach desired visibility in the alloted time.");
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSimpleCapture() {
        Tab tab = mActivity.getActivityTab();

        // Before capture starts the overlay should not be visible.
        waitForOverlayVisibility(false);

        // Once capture starts the overlay should be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.startCapture(tab));
        waitForOverlayVisibility(true);

        // Once capture stops the overlay should no longer be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.stopCapture(tab));
        waitForOverlayVisibility(false);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testShowHideWithOverview() throws TimeoutException {
        Tab tab = mActivity.getActivityTab();

        // Start capturing the tab and assert that the overlay is visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.startCapture(tab));
        waitForOverlayVisibility(true);

        // Summon the overview, and assert that the overlay is no longer visible.
        TabUiTestHelper.enterTabSwitcher(mActivity);
        waitForOverlayVisibility(false);

        // Now hide the overview and assert that it becomes visible again.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivity.getLayoutManager().showLayout(LayoutType.BROWSING, false));
        waitForOverlayVisibility(true);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testCloseTabStopsOverlay() {
        Tab tab = mActivity.getActivityTab();

        // Start capturing the tab and assert that the overlay is visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.startCapture(tab));
        waitForOverlayVisibility(true);

        // Now close the tab and assert that the overlay disappears.
        ChromeTabUtils.closeCurrentTab(InstrumentationRegistry.getInstrumentation(), mActivity);
        waitForOverlayVisibility(false);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSwitchToNonCapturedTab() throws TimeoutException {
        Tab firstTab = mActivity.getActivityTab();

        // Start capturing the tab and assert that the overlay is visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.startCapture(firstTab));
        waitForOverlayVisibility(true);

        // Now open a new tab and assert that the overlay disappears.
        Tab secondTab = openNewTab();
        waitForOverlayVisibility(false);
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testStopOtherCapturedTab() throws TimeoutException {
        Tab firstTab = mActivity.getActivityTab();

        // Start capturing the tab and assert that the overlay is visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.startCapture(firstTab));
        waitForOverlayVisibility(true);

        // Now open a new tab and begin capturing it and assert that the overlay becomes visible.
        Tab secondTab = openNewTab();
        ThreadUtils.runOnUiThreadBlocking(() -> mController.startCapture(secondTab));
        waitForOverlayVisibility(true);

        // Stop capturing the non-visible tab, and then assert that the overlay is still visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mController.stopCapture(firstTab));
        waitForOverlayVisibility(true);
    }
}
