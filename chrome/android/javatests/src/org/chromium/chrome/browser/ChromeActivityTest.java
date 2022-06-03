// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Instrumentation tests for ChromeActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String FILE_PATH = "/chrome/test/data/android/test.html";

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        if (mTestServer != null) mTestServer.stopAndDestroyServer();
    }

    /**
     * Verifies that the front tab receives the hide() call when the activity is stopped (hidden);
     * and that it receives the show() call when the activity is started again. This is a regression
     * test for http://crbug.com/319804 .
     */
    @Test
    @MediumTest
    public void testTabVisibility() {
        // Create two tabs - tab[0] in the foreground and tab[1] in the background.
        final TabImpl[] tabs = new TabImpl[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Foreground tab.
            ChromeTabCreator tabCreator =
                    mActivityTestRule.getActivity().getCurrentTabCreator();
            tabs[0] = (TabImpl) tabCreator.createNewTab(
                    new LoadUrlParams(mTestServer.getURL(FILE_PATH)), TabLaunchType.FROM_CHROME_UI,
                    null);
            // Background tab.
            tabs[1] = (TabImpl) tabCreator.createNewTab(
                    new LoadUrlParams(mTestServer.getURL(FILE_PATH)),
                    TabLaunchType.FROM_LONGPRESS_BACKGROUND, null);
        });

        // Verify that the front tab is in the 'visible' state.
        Assert.assertFalse(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());

        // Fake sending the activity to background.
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onPause());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStop());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onWindowFocusChanged(false));
        // Verify that both Tabs are hidden.
        Assert.assertTrue(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());

        // Fake bringing the activity back to foreground.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().onWindowFocusChanged(true));
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStart());
        TestThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onResume());
        // Verify that the front tab is in the 'visible' state.
        Assert.assertFalse(tabs[0].isHidden());
        Assert.assertTrue(tabs[1].isHidden());
    }

    @Test
    @SmallTest
    public void testTabAnimationsCorrectlyEnabled() {
        boolean animationsEnabled = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getLayoutManager().animationsEnabled());
        Assert.assertEquals(animationsEnabled, DeviceClassManager.enableAnimations());
    }
}
