// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;

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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for the LaunchpadActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class LaunchpadActivityTest {
    private static final String APP_PACKAGE_NAME = "package.name.1";
    private static final String APP_NAME = "App Name 1";
    private static final String APP_SHORT_NAME = "App 1";
    private static final String APP_URL = "https://example.com/1";
    private static final Bitmap TEST_ICON = Bitmap.createBitmap(20, 20, Bitmap.Config.ARGB_8888);

    private static final List<LaunchpadItem> MOCK_APP_LIST = new ArrayList<>(Arrays.asList(
            new LaunchpadItem(APP_PACKAGE_NAME, APP_SHORT_NAME, APP_NAME, APP_URL, TEST_ICON)));
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus().setRevision(1).build();

    private LaunchpadActivity mLaunchpadActivity;
    private LaunchpadCoordinator mLaunchpadCoordinator;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        if (mLaunchpadActivity != null) {
            mLaunchpadActivity.finish();
        }
    }

    private void openLaunchpadActivity() {
        LaunchpadUtils.setOverrideItemListForTesting(MOCK_APP_LIST);
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
        mRenderTestRule.render(mLaunchpadCoordinator.getView(), "LaunchpadActivity");
    }
}
