// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.res.Resources;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.view.Window;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
@TargetApi(Build.VERSION_CODES.O_MR1)
@SuppressLint("NewApi")
public class TabbedNavigationBarColorControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();
    private Window mWindow;
    private int mLightNavigationColor;
    private int mDarkNavigationColor;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mWindow = mActivityTestRule.getActivity().getWindow();
        final Resources resources = mActivityTestRule.getActivity().getResources();
        mLightNavigationColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_bg_color_light);
        mDarkNavigationColor =
                ApiCompatibilityUtils.getColor(resources, R.color.default_bg_color_dark);
    }

    @Test
    @SmallTest
    public void testToggleOverview() {
        assertEquals("Navigation bar should be white before entering overview mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().showOverview(false));

        assertEquals("Navigation bar should be white in overview mode.", mLightNavigationColor,
                mWindow.getNavigationBarColor());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getLayoutManager().hideOverview(false));

        assertEquals("Navigation bar should be white after exiting overview mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }

    @Test
    @SmallTest
    public void testToggleIncognito() {
        assertEquals("Navigation bar should be white on normal tabs.", mLightNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), true, true);

        assertEquals("Navigation bar should be dark_elev_3 on incognito tabs.",
                mDarkNavigationColor, mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), false, true);

        assertEquals("Navigation bar should be white after switching back to normal tab.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }

    @Test
    @MediumTest
    public void testToggleFullscreen() throws TimeoutException {
        assertEquals("Navigation bar should be white before entering fullscreen mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());

        String url =
                mTestServerRule.getServer().getURL("/content/test/data/media/video-player.html");
        mActivityTestRule.loadUrl(url);
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        FullscreenToggleObserver observer = new FullscreenToggleObserver();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getFullscreenManager().addObserver(observer));

        enterFullscreen(observer, activity.getCurrentWebContents());

        assertEquals("Navigation bar should be dark in fullscreen mode.", mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        exitFullscreen(observer, activity.getCurrentWebContents());

        assertEquals("Navigation bar should be white after exiting fullscreen mode.",
                mLightNavigationColor, mWindow.getNavigationBarColor());
    }

    private void enterFullscreen(FullscreenToggleObserver observer, WebContents webContents)
            throws TimeoutException {
        String video = "video";
        int onEnterCallCount = observer.getOnEnterFullscreenHelper().getCallCount();

        // Start playback to guarantee it's properly loaded.
        Assert.assertTrue("Failed to load video", DOMUtils.isMediaPaused(webContents, video));

        // Trigger requestFullscreen() via a click on a button.
        Assert.assertTrue(
                "Failed to click fullscreen node", DOMUtils.clickNode(webContents, "fullscreen"));
        observer.getOnEnterFullscreenHelper().waitForCallback(
                "Failed to enter full screen", onEnterCallCount);
    }

    private void exitFullscreen(FullscreenToggleObserver observer, WebContents webContents)
            throws TimeoutException {
        int onExitCallCount = observer.getOnExitFullscreenHelper().getCallCount();
        DOMUtils.exitFullscreen(webContents);
        observer.getOnExitFullscreenHelper().waitForCallback(
                "Failed to exit full screen", onExitCallCount);
    }

    private class FullscreenToggleObserver implements FullscreenManager.Observer {
        private final CallbackHelper mOnEnterFullscreenHelper;
        private final CallbackHelper mOnExitFullscreenHelper;

        public FullscreenToggleObserver() {
            mOnEnterFullscreenHelper = new CallbackHelper();
            mOnExitFullscreenHelper = new CallbackHelper();
        }

        public CallbackHelper getOnEnterFullscreenHelper() {
            return mOnEnterFullscreenHelper;
        }

        public CallbackHelper getOnExitFullscreenHelper() {
            return mOnExitFullscreenHelper;
        }

        @Override
        public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
            mOnEnterFullscreenHelper.notifyCalled();
        }

        @Override
        public void onExitFullscreen(Tab tab) {
            mOnExitFullscreenHelper.notifyCalled();
        }
    }
}
