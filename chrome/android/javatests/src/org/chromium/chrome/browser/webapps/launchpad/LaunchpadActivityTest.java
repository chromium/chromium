// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Render tests for the LaunchpadActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class LaunchpadActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_WEB_APP_INSTALLS)
                    .build();

    private LaunchpadActivity mLaunchpadActivity;
    private LaunchpadCoordinator mLaunchpadCoordinator;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        LaunchpadUtils.setOverrideItemListForTesting(LaunchpadTestUtils.MOCK_APP_LIST);
    }

    @After
    public void tearDown() {
        if (mLaunchpadActivity != null) {
            mLaunchpadActivity.finish();
        }
    }

    private void openLaunchpadActivity() {
        mLaunchpadActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), LaunchpadActivity.class, () -> {
                    TestThreadUtils.runOnUiThreadBlocking(
                            ()
                                    -> LaunchpadUtils.showLaunchpadActivity(
                                            mActivityTestRule.getActivity()));
                });
        mLaunchpadCoordinator = mLaunchpadActivity.getCoordinatorForTesting();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShowLaunchpadActivity() throws IOException {
        openLaunchpadActivity();
        mRenderTestRule.render(mLaunchpadCoordinator.getView(), "launchpad_activity");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAppManagementMenuWithoutShortcut() throws IOException {
        openLaunchpadActivity();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mLaunchpadActivity.getModalDialogManager(), 1 /* itemIndex */);

        mRenderTestRule.render(dialogView, "launchpad_management_menu");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAppManagementMenuWithPermissions() throws IOException {
        openLaunchpadActivity();
        LaunchpadTestUtils.setPermissionDefaults(LaunchpadTestUtils.APP_URL_2);

        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mLaunchpadActivity.getModalDialogManager(), 1 /* itemIndex */);

        mRenderTestRule.render(dialogView, "launchpad_management_menu_with_permissions");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAppManagementMenuWithShortcut() throws IOException {
        openLaunchpadActivity();
        View dialogView = LaunchpadTestUtils.openAppManagementMenu(mLaunchpadCoordinator,
                mLaunchpadActivity.getModalDialogManager(), 0 /* itemIndex */);

        mRenderTestRule.render(dialogView, "launchpad_management_menu_shortcuts");
    }
}
