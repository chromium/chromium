// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.graphics.Color;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests related to the Tab's theme color. */
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

    /** A WebContentsObserver for watching changes in the theme color. */
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

    /** AssertEquals two colors as strings so the text output shows their hex value. */
    private void assertColorsEqual(int color1, int color2) {
        Assert.assertEquals(Integer.toHexString(color1), Integer.toHexString(color2));
    }

    private int getThemeColor() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                mActivityTestRule
                                .getActivity()
                                .getRootUiCoordinatorForTesting()
                                .getTopUiThemeColorProvider()
                        ::getThemeColor);
    }

    private static int getDefaultThemeColor(Tab tab) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ChromeColors.getDefaultThemeColor(tab.getContext(), tab.isIncognito());
                });
    }

    /** Test that the toolbar has the correct color set. */
    @Test
    @Feature({"Toolbar-Theme-Color"})
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "Flaky: https://crbug.com/1355516")
    public void testThemeColorIsCorrect() throws ExecutionException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        ThemeColorWebContentsObserver colorObserver = new ThemeColorWebContentsObserver();
        CallbackHelper themeColorHelper = colorObserver.getCallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getRootUiCoordinatorForTesting()
                            .getTopUiThemeColorProvider()
                            .addThemeColorObserver(colorObserver);
                });

        // Navigate to a themed page.
        int curCallCount = themeColorHelper.getCallCount();
        mActivityTestRule.loadUrl(testServer.getURL(THEMED_TEST_PAGE));
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, getThemeColor());

        // Setting page theme color to white is forbidden.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                "document.querySelector(meta).setAttribute('content', 'white');");
        themeColorHelper.waitForCallback(curCallCount, 1);
        assertColorsEqual(THEME_COLOR, colorObserver.getColor());
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

    @Test
    @Feature({"Toolbar-Theme-Color"})
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testOverlayColorOnNativePages() throws ExecutionException, TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final TopUiThemeColorProvider colorProvider =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getTopUiThemeColorProvider();

        ThemeColorObserver colorObserver =
                (color, animate) -> {
                    Assert.assertNotSame(
                            "Theme color should never be 0 or TRANSPARENT!",
                            Color.TRANSPARENT,
                            colorProvider.getSceneLayerBackground(tab));
                };

        ThreadUtils.runOnUiThreadBlocking(() -> colorProvider.addThemeColorObserver(colorObserver));

        // Load the ntp.
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadUrl(new LoadUrlParams(UrlConstants.NTP_URL)));

        NewTabPageTestUtils.waitForNtpLoaded(tab);

        // Load a second tab so we can freeze the first.
        mActivityTestRule.loadUrlInNewTab(testServer.getURL(THEMED_TEST_PAGE));

        // Freeze the ntp on the first tab.
        ThreadUtils.runOnUiThreadBlocking(() -> tab.freezeNativePage());

        CriteriaHelper.pollUiThread(() -> tab.getNativePage() instanceof FrozenNativePage);

        // Switch back to the native tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model =
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelectorSupplier()
                                    .get()
                                    .getCurrentModel();
                    model.setIndex(model.indexOf(tab), TabSelectionType.FROM_USER);
                });

        Assert.assertEquals(
                "The tab shouldn't be frozen!",
                mActivityTestRule
                        .getActivity()
                        .getActivityTabProvider()
                        .get()
                        .getNativePage()
                        .getClass(),
                NewTabPage.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotSame(
                            "Theme color should never be 0 or TRANSPARENT!",
                            Color.TRANSPARENT,
                            colorProvider.getSceneLayerBackground(tab));
                });
    }
}
