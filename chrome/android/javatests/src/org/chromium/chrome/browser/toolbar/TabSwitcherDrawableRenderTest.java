// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;

import android.graphics.Color;
import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.test.util.RenderTestRule;

/** Render tests for the {@link TabSwitcherDrawable} with notification feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabSwitcherDrawableRenderTest {
    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(2)
                    .build();

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    private ToggleTabStackButton mToggleTabStackButton;
    private TabSwitcherDrawable mTabSwitcherDrawable;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity activity = sActivityTestRule.getActivity();
                    mToggleTabStackButton = activity.findViewById(R.id.tab_switcher_button);
                    mTabSwitcherDrawable = mToggleTabStackButton.getTabSwitcherDrawableForTesting();
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures(ChromeFeatureList.DATA_SHARING)
    public void testTabSwitcherDrawable_toggleNotificationRegular() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        sActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);

        int tabCount = 2;
        View toolbarView = activity.findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, "tab_page_toolbar_view_regular_off");

        String contentDesc =
                activity.getResources()
                        .getQuantityString(
                                R.plurals.accessibility_toolbar_btn_tabswitcher_toggle_default,
                                tabCount,
                                tabCount);
        assertEquals(contentDesc, mToggleTabStackButton.getContentDescription());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ true);
                });

        String notificationContentDesc =
                activity.getResources()
                        .getQuantityString(
                                R.plurals
                                        .accessibility_toolbar_btn_tabswitcher_toggle_default_with_notification,
                                tabCount,
                                tabCount);
        assertEquals(notificationContentDesc, mToggleTabStackButton.getContentDescription());
        mRenderTestRule.render(toolbarView, "tab_page_toolbar_view_regular_on");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTabSwitcherDrawable_newTabPage() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();

        sActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ false);
        NewTabPageTestUtils.waitForNtpLoaded(activity.getActivityTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ true);
                });
        View toolbarView = activity.findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, "tab_page_toolbar_view_new_tab_page");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testTabSwitcherDrawable_newTabPageIncognito() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();

        sActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ true);
        NewTabPageTestUtils.waitForNtpLoaded(activity.getActivityTab());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ true);
                });
        View toolbarView = activity.findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, "tab_page_toolbar_view_incognito_no_show");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisabledTest(message = "b/359300762")
    public void testTabSwitcherDrawable_themedToolbar() throws Exception {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();

        String pageWithBrandColorUrl =
                sActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/theme_color_test.html");
        sActivityTestRule.loadUrl(pageWithBrandColorUrl);
        ThemeTestUtils.waitForThemeColor(activity, Color.RED);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ true);
                });
        View toolbarView = activity.findViewById(R.id.toolbar);
        mRenderTestRule.render(toolbarView, "tab_page_toolbar_view_themed_toolbar");
    }
}
