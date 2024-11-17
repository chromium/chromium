// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.Color;
import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.DeviceFormFactor;

/** Contains tests for the brand color feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BrandColorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String BRAND_COLOR_1 = "#482329";
    private static final String BRAND_COLOR_2 = "#505050";

    private ToolbarPhone mToolbar;
    private ToolbarDataProvider mToolbarDataProvider;
    private int mDefaultColor;

    private static String getUrlWithBrandColor(String brandColor) {
        String brandColorMetaTag =
                TextUtils.isEmpty(brandColor)
                        ? ""
                        : "<meta name='theme-color' content='" + brandColor + "'>";
        return UrlUtils.encodeHtmlDataUri(
                "<html>"
                        + "  <head>"
                        + "    "
                        + brandColorMetaTag
                        + "  </head>"
                        + "  <body>"
                        + "    Theme color set to "
                        + brandColor
                        + "  </body>"
                        + "</html>");
    }

    private void checkForBrandColor(final int brandColor) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mToolbarDataProvider.getPrimaryColor(), Matchers.is(brandColor));
                    Criteria.checkThat(
                            mToolbarDataProvider.getPrimaryColor(),
                            Matchers.is(mToolbar.getBackgroundDrawable().getColor()));
                });
        if (!SysUtils.isLowEndDevice()) {
            final int expectedStatusBarColor;
            expectedStatusBarColor = brandColor == mDefaultColor ? mDefaultColor : brandColor;
            CriteriaHelper.pollUiThread(
                    () -> {
                        Criteria.checkThat(
                                mActivityTestRule.getActivity().getWindow().getStatusBarColor(),
                                Matchers.is(expectedStatusBarColor));
                    });
        }
    }

    protected void startMainActivityWithURL(String url) {
        mActivityTestRule.startMainActivityWithURL(url);
        mToolbar = (ToolbarPhone) mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        mToolbarDataProvider = mToolbar.getToolbarDataProvider();
        mDefaultColor =
                ChromeColors.getDefaultThemeColor(
                        mActivityTestRule.getActivity(), /* isIncognito= */ false);
    }

    /** Test for having default primary color working correctly. */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"StatusBar", "Omnibox"})
    public void testNoBrandColor() {
        startMainActivityWithURL(getUrlWithBrandColor(""));
        checkForBrandColor(mDefaultColor);
    }

    /** Test for adding a brand color for a url. */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"StatusBar", "Omnibox"})
    public void testBrandColorNoAlpha() {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /** Test for immediately setting the brand color. */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"StatusBar", "Omnibox"})
    public void testImmediateColorChange() {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getToolbarManager()
                            .onThemeColorChanged(mDefaultColor, false);
                    // Since the color should change instantly, there is no need to use the criteria
                    // helper.
                    Assert.assertEquals(
                            mToolbarDataProvider.getPrimaryColor(),
                            mToolbar.getBackgroundDrawable().getColor());
                });
    }

    /** Test to make sure onLoadStarted doesn't reset the brand color. */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"StatusBar", "Omnibox"})
    public void testBrandColorWithLoadStarted() {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(tab);
                    while (observers.hasNext()) {
                        observers.next().onLoadStarted(tab, true);
                    }
                });
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
    }

    /** Test for checking navigating to new brand color updates correctly. */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"StatusBar", "Omnibox"})
    public void testNavigatingToNewBrandColor() {
        startMainActivityWithURL(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        mActivityTestRule.loadUrl(getUrlWithBrandColor(BRAND_COLOR_2));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_2));
    }

    /**
     * Test for checking navigating to a brand color site from a site with no brand color and then
     * back again.
     */
    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    @Feature({"StatusBar", "Omnibox"})
    public void testNavigatingToBrandColorAndBack() {
        startMainActivityWithURL("about:blank");
        checkForBrandColor(mDefaultColor);
        mActivityTestRule.loadUrl(getUrlWithBrandColor(BRAND_COLOR_1));
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        mActivityTestRule.loadUrl("about:blank");
        checkForBrandColor(mDefaultColor);
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> mActivityTestRule.getActivity().onBackPressed());
        checkForBrandColor(Color.parseColor(BRAND_COLOR_1));
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> mActivityTestRule.getActivity().onBackPressed());
        checkForBrandColor(mDefaultColor);
    }
}
