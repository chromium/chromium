// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.base.ColdStartTracker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.metrics.SimpleStartupForegroundSessionDetector;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the {@link StartupPaintPreviewHelper} class. This test suite cannot be batched because
 * tests rely on the cold start behavior of {@link ChromeActivity}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class StartupPaintPreviewHelperTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_URL = "/chrome/test/data/android/about.html";

    /**
     * Tests that paint preview is captured when Chrome is backgrounded and deleted when a tab is
     * closed.
     */
    @Test
    @MediumTest
    public void testCaptureOnBackgrounded() throws ExecutionException {
        mActivityTestRule.startMainActivityWithURL(
                mActivityTestRule.getTestServer().getURL(TEST_URL));
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(
                () ->
                        PaintPreviewTabServiceFactory.getServiceInstance()
                                .hasNativeServiceForTesting(),
                "Native tab service not loaded");
        CriteriaHelper.pollUiThread(
                () -> PaintPreviewTabServiceFactory.getServiceInstance().isNativeCacheInitialized(),
                "Native capture cache not loaded");

        // Verify no capture exists for this tab and no paint preview is showing.
        assertHasCaptureForTab(tab, false);
        TabbedPaintPreview tabbedPaintPreview =
                ThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        Assert.assertFalse("No preview should be showing.", tabbedPaintPreview.isShowing());
        Assert.assertFalse("No preview should be attached.", tabbedPaintPreview.isAttached());

        // Send Chrome to background to trigger capture.
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation()).pressHome();

        assertHasCaptureForTab(tab, true);
        Assert.assertFalse("No preview should be showing.", tabbedPaintPreview.isShowing());
        Assert.assertFalse("No preview should be attached.", tabbedPaintPreview.isAttached());

        // Closing the tab should delete its captured paint preview.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getCurrentModel()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(false).build()));
        assertHasCaptureForTab(tab, false);
    }

    /**
     * Captures paint preview first, restarts Chrome and tests whether the preview is shown on
     * startup.
     */
    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER})
    @DisabledTest(message = "Pending revival. See crbug.com/333779543.")
    public void testDisplayOnStartup() throws ExecutionException {
        mActivityTestRule.startMainActivityWithURL(
                mActivityTestRule.getTestServer().getURL(TEST_URL));
        final ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> activity.getTabModelSelector().isTabStateInitialized(),
                "Tab state never initialized.");
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(
                () ->
                        PaintPreviewTabServiceFactory.getServiceInstance()
                                .hasNativeServiceForTesting(),
                "Native tab service not loaded");
        CriteriaHelper.pollUiThread(
                () -> PaintPreviewTabServiceFactory.getServiceInstance().isNativeCacheInitialized(),
                "Native capture cache not loaded");

        // Verify no capture exists for this tab and no paint preview is showing.
        assertHasCaptureForTab(tab, false);
        TabbedPaintPreview tabbedPaintPreview =
                ThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        Assert.assertFalse("No preview should be showing.", tabbedPaintPreview.isShowing());
        Assert.assertFalse("No preview should be attached.", tabbedPaintPreview.isAttached());

        // Send Chrome to background to trigger capture.
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation()).pressHome();
        assertHasCaptureForTab(mActivityTestRule.getActivity().getActivityTab(), true);

        // Emulate browser cold start. Paint preview should be shown on startup.
        pretendColdStartBeforeForegrounded();
        ThreadUtils.runOnUiThreadBlocking(activity::finish);
        CriteriaHelper.pollUiThread(activity::isDestroyed, "Activity didn't get destroyed.");

        mActivityTestRule.startMainActivityFromLauncher();
        final ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> newActivity.getTabModelSelector().isTabStateInitialized(),
                "Tab state never initialized.");
        final Tab previewTab = newActivity.getActivityTab();
        tabbedPaintPreview =
                ThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(previewTab));

        // Paint Preview might be showed and get removed before we can assert it's showing. Instead
        // assert that it was shown for this tab at least once.
        TabbedPaintPreviewTest.assertWasEverShown(tabbedPaintPreview, true);

        // Closing the tab should delete its captured paint preview.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mActivityTestRule
                                .getActivity()
                                .getTabModelSelector()
                                .getCurrentModel()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(false).build()));
        assertHasCaptureForTab(previewTab, false);
    }

    private void pretendColdStartBeforeForegrounded() {
        // Paint preview is shown only on cold start, but it is not possible to restart the process
        // within a test. Instead set up the ColdStartTracker to report a cold start.
        ColdStartTracker.setStartedAsColdForTesting();
        // Reset the SimpleStartupForegroundSessionDetector to the initial state as if the
        // foreground session has not started.
        SimpleStartupForegroundSessionDetector.resetForTesting();
    }

    private static void assertHasCaptureForTab(Tab tab, boolean shouldHaveCapture) {
        String shownMessage =
                shouldHaveCapture
                        ? "No paint preview capture found."
                        : "Paint preview capture should have not existed.";
        CriteriaHelper.pollUiThread(
                () ->
                        PaintPreviewTabServiceFactory.getServiceInstance()
                                        .hasCaptureForTab(tab.getId())
                                == shouldHaveCapture,
                shownMessage);
    }
}
