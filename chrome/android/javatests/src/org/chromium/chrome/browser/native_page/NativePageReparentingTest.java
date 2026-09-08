// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import android.content.res.Configuration;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Integration tests for reparenting {@link org.chromium.chrome.browser.ui.native_page.NativePage}
 * tabs across Activity recreation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests Activity recreation and reparenting lifecycle")
public class NativePageReparentingTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Test
    @MediumTest
    @Feature({"Reparenting", "NewTabPage"})
    public void testNtpReparentingPreservesTabAndRecreatesNativePage() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity initialActivity = mActivityTestRule.getActivity();

        Tab ntpTab =
                mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ false);
        int tabId = ntpTab.getId();

        NewTabPageTestUtils.waitForNtpLoaded(ntpTab);
        NewTabPage initialNtp = (NewTabPage) ntpTab.getNativePage();
        Assert.assertNotNull(initialNtp);

        triggerReparentingAndRecreateActivity(initialActivity);

        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        Assert.assertNotSame("Activity must be recreated", initialActivity, newActivity);
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> newActivity.getTabModelSelector().getModel(false));
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = normalTabModel.getTabById(tabId);
                    Criteria.checkThat(
                            "Reparented NTP tab must exist", tab, Matchers.notNullValue());
                });

        Tab reparentedTab =
                ThreadUtils.runOnUiThreadBlocking(() -> normalTabModel.getTabById(tabId));
        Assert.assertEquals("Tab ID must be preserved", tabId, reparentedTab.getId());
        Assert.assertSame("Tab instance must be reused in-memory", ntpTab, reparentedTab);
        Assert.assertEquals(
                "WindowAndroid must update to new Activity",
                newActivity.getWindowAndroid(),
                reparentedTab.getWindowAndroid());

        // Verify Java UI recreation with new Context.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "NativePage must be loaded",
                            reparentedTab.getNativePage(),
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "NativePage must be NewTabPage",
                            reparentedTab.getNativePage(),
                            Matchers.instanceOf(NewTabPage.class));
                    Criteria.checkThat(
                            "NewTabPage must be freshly instantiated",
                            reparentedTab.getNativePage(),
                            Matchers.not(Matchers.sameInstance(initialNtp)));
                });
    }

    @Test
    @MediumTest
    @Feature({"Reparenting", "BackgroundTabs"})
    public void testBackgroundNtpFrozenOnReparenting() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeTabbedActivity initialActivity = mActivityTestRule.getActivity();

        // Open background NTP tab.
        Tab bgTab = mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, /* incognito= */ false);
        int bgTabId = bgTab.getId();

        // Switch back to foreground blank tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    initialActivity.getCurrentTabModel().setIndex(0, TabSelectionType.FROM_USER);
                });
        Assert.assertTrue(bgTab.isHidden());

        triggerReparentingAndRecreateActivity(initialActivity);

        ChromeTabbedActivity newActivity = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(newActivity.getTabModelSelector()::isTabStateInitialized);

        TabModel normalTabModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> newActivity.getTabModelSelector().getModel(false));
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab tab = normalTabModel.getTabById(bgTabId);
                    Criteria.checkThat(
                            "Reparented background tab must exist", tab, Matchers.notNullValue());
                });
        Tab reparentedBgTab =
                ThreadUtils.runOnUiThreadBlocking(() -> normalTabModel.getTabById(bgTabId));
        Assert.assertSame("Tab instance must be reused in-memory", bgTab, reparentedBgTab);

        // Assert background tab is frozen rather than eagerly inflated.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Background tab native page must exist",
                            reparentedBgTab.getNativePage(),
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "Background native page should be frozen",
                            reparentedBgTab.getNativePage().isFrozen(),
                            Matchers.is(true));
                });

        // Select background tab and verify on-demand unfreezing.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    newActivity
                            .getCurrentTabModel()
                            .setIndex(
                                    newActivity.getCurrentTabModel().indexOf(reparentedBgTab),
                                    TabSelectionType.FROM_USER);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            reparentedBgTab.getNativePage(), Matchers.instanceOf(NewTabPage.class));
                    Criteria.checkThat(
                            reparentedBgTab.getNativePage().isFrozen(), Matchers.is(false));
                });
    }

    private void triggerReparentingAndRecreateActivity(ChromeTabbedActivity cta) {
        boolean isTestOnTablet = cta.isTablet();
        Configuration config = cta.getSavedConfigurationForTesting();
        Configuration newConfig = new Configuration(config);
        config.smallestScreenWidthDp =
                DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + (isTestOnTablet ? -1 : 1);
        newConfig.smallestScreenWidthDp =
                DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP + (isTestOnTablet ? 1 : -1);
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> {
                            Assert.assertTrue(
                                    "Activity should be ready for tablet mode change.",
                                    cta.getTabletMode().changed);
                            cta.performOnConfigurationChanged(newConfig);
                            Assert.assertTrue(
                                    "ChromeActivity#mIsRecreatingForTabletModeChange should be"
                                            + " true.",
                                    cta.recreatingForTabletModeChangeForTesting());
                        });
        mActivityTestRule.setActivity(newActivity);
    }
}
