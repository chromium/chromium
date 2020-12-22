// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.styles.ChromeColors;
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
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";
    private static final String THEMED_TEST_PAGE =
            "/chrome/test/data/android/theme_color_test.html";

    // The theme_color_test.html page uses a pure red theme color.
    private static final int THEME_COLOR = 0xffff0000;

    /**
     * A WebContentsObserver for watching changes in the theme color.
     */
    private static class ThemeColorWebContentsObserver implements ThemeColorObserver {
        private CallbackHelper mCallbackHelper;
        private int mColor;

        public ThemeColorWebContentsObserver() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onThemeColorChanged(int color, boolean shouldAnimate) {
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

    private int getThemeColor() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                mActivityTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getTopUiThemeColorProvider()::getThemeColor);
    }

    private static int getDefaultThemeColor(Tab tab) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            return ChromeColors.getDefaultThemeColor(
                    tab.getContext().getResources(), tab.isIncognito());
        });
    }

    /**
     * Test that the toolbar has the correct color set.
     */
    @Test
    @Feature({"Toolbar-Theme-Color"})
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testThemeColorIsCorrect() throws ExecutionException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        ThemeColorWebContentsObserver colorObserver = new ThemeColorWebContentsObserver();
        CallbackHelper themeColorHelper = colorObserver.getCallbackHelper();
        mActivityTestRule.getActivity()
                .getRootUiCoordinatorForTesting()
                .getTopUiThemeColorProvider()
                .addThemeColorObserver(colorObserver);

        // Navigate to a themed page.
        int curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, getThemeColor());

        // Navigate to a native page from a themed page.
        mActivityTestRule.loadUrl("chrome://newtab/");
        // WebContents does not set theme color for native pages, so don't wait for the call.
        assertColorsEqual(getDefaultThemeColor(tab), getThemeColor());

        // Navigate to a themed page from a native page.
        curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, colorObserver.getColor());
        assertColorsEqual(THEME_COLOR, getThemeColor());

        // Navigate to a non-native non-themed page.
        curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(getDefaultThemeColor(tab), colorObserver.getColor());
        assertColorsEqual(getDefaultThemeColor(tab), getThemeColor());

        // Navigate to a themed page from a non-native page.
        curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, colorObserver.getColor());
        assertColorsEqual(THEME_COLOR, getThemeColor());
    }
}
