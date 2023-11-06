// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;
import android.view.Window;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/** Tests for {@link TabbedNavigationBarColorController}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.O_MR1)
@RequiresApi(Build.VERSION_CODES.O_MR1)
@SuppressLint("NewApi")
public class TabbedNavigationBarColorControllerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();
    private Window mWindow;
    private int mRegularNavigationColor;
    private int mDarkNavigationColor;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mWindow = mActivityTestRule.getActivity().getWindow();
        Context context = mActivityTestRule.getActivity();
        mRegularNavigationColor = SemanticColorUtils.getBottomSystemNavColor(context);
        mDarkNavigationColor = context.getColor(R.color.default_bg_color_dark);
    }

    @Test
    @SmallTest
    public void testToggleOverview() {
        assertEquals(
                "Navigation bar should be colorSurface before entering overview mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        assertEquals(
                "Navigation bar should be colorSurface in overview mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING, false);

        assertEquals(
                "Navigation bar should be colorSurface after exiting overview mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());
    }

    @Test
    @SmallTest
    public void testToggleIncognito() {
        assertEquals(
                "Navigation bar should be colorSurface on normal tabs.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                true,
                true);

        assertEquals(
                "Navigation bar should be dark_elev_3 on incognito tabs.",
                mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                false,
                true);

        assertEquals(
                "Navigation bar should be colorSurface after switching back to normal tab.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1381509")
    public void testToggleFullscreen() throws TimeoutException {
        assertEquals(
                "Navigation bar should be colorSurface before entering fullscreen mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());

        String url =
                mTestServerRule.getServer().getURL("/content/test/data/media/video-player.html");
        mActivityTestRule.loadUrl(url);
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        FullscreenToggleObserver observer = new FullscreenToggleObserver();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getFullscreenManager().addObserver(observer));

        enterFullscreen(observer, activity.getCurrentWebContents());

        assertEquals(
                "Navigation bar should be dark in fullscreen mode.",
                mDarkNavigationColor,
                mWindow.getNavigationBarColor());

        exitFullscreen(observer, activity.getCurrentWebContents());

        assertEquals(
                "Navigation bar should be colorSurface after exiting fullscreen mode.",
                mRegularNavigationColor,
                mWindow.getNavigationBarColor());
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
        observer.getOnEnterFullscreenHelper()
                .waitForCallback("Failed to enter full screen", onEnterCallCount);
    }

    private void exitFullscreen(FullscreenToggleObserver observer, WebContents webContents)
            throws TimeoutException {
        int onExitCallCount = observer.getOnExitFullscreenHelper().getCallCount();
        DOMUtils.exitFullscreen(webContents);
        observer.getOnExitFullscreenHelper()
                .waitForCallback("Failed to exit full screen", onExitCallCount);
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
