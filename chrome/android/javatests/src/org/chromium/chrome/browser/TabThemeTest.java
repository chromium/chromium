// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests related to the Tab's theme color.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabThemeTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";
    private static final String THEMED_TEST_PAGE =
            "/chrome/test/data/android/theme_color_test.html";

    // The theme_color_test.html page uses a pure red theme color.
    private static final int THEME_COLOR = 0xffff0000;

    /**
     * A WebContentsObserver for watching changes in the theme color.
     */
    private static class ThemeColorWebContentsObserver extends EmptyTabObserver {
        private CallbackHelper mCallbackHelper;
        private int mColor;

        public ThemeColorWebContentsObserver() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onDidChangeThemeColor(Tab tab, int color) {
            mColor = color;
            mCallbackHelper.notifyCalled();
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        public int getColor() {
            return mColor;
        }
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * AssertEquals two colors as strings so the text output shows their hex value.
     */
    private void assertColorsEqual(int color1, int color2) {
        Assert.assertEquals(Integer.toHexString(color1), Integer.toHexString(color2));
    }

    private static int getThemeColor(Tab tab) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> TabThemeColorHelper.getColor(tab));
    }

    private static int getDefaultThemeColor(Tab tab) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> TabThemeColorHelper.getDefaultColor(tab));
    }

    /**
     * Test that the toolbar has the correct color set.
     */
    @Test
    @Feature({"Toolbar-Theme-Color"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @RetryOnFailure
    public void testThemeColorIsCorrect() throws ExecutionException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        ThemeColorWebContentsObserver colorObserver = new ThemeColorWebContentsObserver();
        CallbackHelper themeColorHelper = colorObserver.getCallbackHelper();
        tab.addObserver(colorObserver);

        // Navigate to a themed page.
        int curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, getThemeColor(tab));

        // Navigate to a native page from a themed page.
        mActivityTestRule.loadUrl("chrome://newtab/");
        // WebContents does not set theme color for native pages, so don't wait for the call.
        assertColorsEqual(getDefaultThemeColor(tab), getThemeColor(tab));

        // Navigate to a themed page from a native page.
        curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, colorObserver.getColor());
        assertColorsEqual(THEME_COLOR, getThemeColor(tab));

        // Navigate to a non-native non-themed page.
        curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(getDefaultThemeColor(tab), colorObserver.getColor());
        assertColorsEqual(getDefaultThemeColor(tab), getThemeColor(tab));

        // Navigate to a themed page from a non-native page.
        curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, colorObserver.getColor());
        assertColorsEqual(THEME_COLOR, getThemeColor(tab));
    }
}
