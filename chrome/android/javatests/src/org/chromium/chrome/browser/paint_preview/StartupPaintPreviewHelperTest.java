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

import org.chromium.base.test.params.ParameterizedCommandLineFlags;
import org.chromium.base.test.params.ParameterizedCommandLineFlags.Switches;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the {@link StartupPaintPreviewHelper} class.
 * This test suite cannot be batched because tests rely on the cold start behavior of
 * {@link ChromeActivity}.
 */
@RunWith(StartupPaintPreviewHelperTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@ParameterizedCommandLineFlags({
    @Switches("disable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + ","
            + ChromeFeatureList.INSTANT_START)
    ,
            @Switches("enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID),
            @Switches("enable-features=" + ChromeFeatureList.INSTANT_START),
            @Switches("enable-features=" + ChromeFeatureList.START_SURFACE_ANDROID + ","
                    + ChromeFeatureList.INSTANT_START),
})
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
    @Restriction(StartupPaintPreviewHelperTestRunner.RESTRICTION_TYPE_KEEP_ACTIVITIES)
    public void testCaptureOnBackgrounded() throws ExecutionException {
        mActivityTestRule.startMainActivityWithURL(
                mActivityTestRule.getTestServer().getURL(TEST_URL));
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(()
                                            -> PaintPreviewTabServiceFactory.getServiceInstance()
                                                       .hasNativeServiceForTesting(),
                "Native tab service not loaded");
        CriteriaHelper.pollUiThread(()
                                            -> PaintPreviewTabServiceFactory.getServiceInstance()
                                                       .isNativeCacheInitialized(),
                "Native capture cache not loaded");

        // Verify no capture exists for this tab and no paint preview is showing.
        assertHasCaptureForTab(tab, false);
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        Assert.assertFalse("No preview should be showing.", tabbedPaintPreview.isShowing());
        Assert.assertFalse("No preview should be attached.", tabbedPaintPreview.isAttached());

        // Send Chrome to background to trigger capture.
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation()).pressHome();

        assertHasCaptureForTab(tab, true);
        Assert.assertFalse("No preview should be showing.", tabbedPaintPreview.isShowing());
        Assert.assertFalse("No preview should be attached.", tabbedPaintPreview.isAttached());

        // Closing the tab should delete its captured paint preview.
        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> mActivityTestRule.getActivity()
                                                                 .getTabModelSelector()
                                                                 .getCurrentModel()
                                                                 .closeTab(tab));
        assertHasCaptureForTab(tab, false);
    }

    /**
     * Captures paint preview first, restarts Chrome and tests whether the preview is shown on
     * startup.
     */
    @Test
    @MediumTest
    @Restriction(StartupPaintPreviewHelperTestRunner.RESTRICTION_TYPE_KEEP_ACTIVITIES)
    public void testDisplayOnStartup() throws ExecutionException {
        mActivityTestRule.startMainActivityWithURL(
                mActivityTestRule.getTestServer().getURL(TEST_URL));
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(()
                                            -> PaintPreviewTabServiceFactory.getServiceInstance()
                                                       .hasNativeServiceForTesting(),
                "Native tab service not loaded");
        CriteriaHelper.pollUiThread(()
                                            -> PaintPreviewTabServiceFactory.getServiceInstance()
                                                       .isNativeCacheInitialized(),
                "Native capture cache not loaded");

        // Verify no capture exists for this tab and no paint preview is showing.
        assertHasCaptureForTab(tab, false);
        TabbedPaintPreview tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(tab));
        Assert.assertFalse("No preview should be showing.", tabbedPaintPreview.isShowing());
        Assert.assertFalse("No preview should be attached.", tabbedPaintPreview.isAttached());

        // Send Chrome to background to trigger capture.
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation()).pressHome();
        assertHasCaptureForTab(mActivityTestRule.getActivity().getActivityTab(), true);

        // Restart Chrome. Paint preview should be shown on startup.
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> activity.finish());
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed(), "Activity didn't get destroyed.");

        mActivityTestRule.startMainActivityFromLauncher();
        final Tab previewTab = mActivityTestRule.getActivity().getActivityTab();
        tabbedPaintPreview =
                TestThreadUtils.runOnUiThreadBlocking(() -> TabbedPaintPreview.get(previewTab));

        // Paint Preview might be showed and get removed before we can assert it's showing. Hence,
        // we assert that is was *ever* shown for this tab, instead.
        TabbedPaintPreviewTest.assertWasEverShown(tabbedPaintPreview, true);

        // Closing the tab should delete its captured paint preview.
        TestThreadUtils.runOnUiThreadBlocking(()
                                                      -> mActivityTestRule.getActivity()
                                                                 .getTabModelSelector()
                                                                 .getCurrentModel()
                                                                 .closeTab(previewTab));
        assertHasCaptureForTab(previewTab, false);
    }

    private static void assertHasCaptureForTab(Tab tab, boolean shouldHaveCapture) {
        String shownMessage = shouldHaveCapture ? "No paint preview capture found."
                                                : "Paint preview capture should have not existed.";
        CriteriaHelper.pollUiThread(
                ()
                        -> PaintPreviewTabServiceFactory.getServiceInstance().hasCaptureForTab(
                                   tab.getId())
                        == shouldHaveCapture,
                shownMessage);
    }
}
