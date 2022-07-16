// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.appmenu.AppMenuObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/** Test for user flows around {@link ContentSettingsType.REQUEST_DESKTOP_SITE}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ContentFeatureList.REQUEST_DESKTOP_SITE_GLOBAL)
public class RequestDesktopSiteTest {
    private static final String URL_1 = "https://www.chromium.org/";
    private static final String URL_2 = "https://www.example.com/";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    Tracker mMockTracker;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        // Disable IPH to avoid interruptions on the app menu.
        TrackerFactory.setTrackerForTests(mMockTracker);
        Mockito.doReturn(false)
                .when(mMockTracker)
                .shouldTriggerHelpUI(ArgumentMatchers.anyString());
        mActivityTestRule.startMainActivityOnBlankPage();
        assertContentSettingsHistogramRecorded();
    }

    @After
    public void tearDown() throws TimeoutException {
        // Clean up content settings.
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(helper::notifyCalled,
                    new int[] {BrowsingDataType.SITE_SETTINGS}, TimePeriod.ALL_TIME);
        });
        helper.waitForCallback(0);
    }

    @Test
    @SmallTest
    public void testGlobalSiteSettingsAndException() throws TimeoutException {
        Tab tab = mActivityTestRule.loadUrlInNewTab(URL_1);
        assertUsingDesktopUserAgent(tab, false, "Default user agent should be mobile.");

        CallbackHelper menuObserver = new CallbackHelper();
        mActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().addObserver(
                new AppMenuObserver() {
                    @Override
                    public void onMenuVisibilityChanged(boolean isVisible) {
                        menuObserver.notifyCalled();
                    }

                    @Override
                    public void onMenuHighlightChanged(boolean highlighting) {}
                });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.showAppMenu(mActivityTestRule.getAppMenuCoordinator(), null, false);
        });

        menuObserver.waitForCallback(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuTestSupport.callOnItemClick(
                    mActivityTestRule.getAppMenuCoordinator(), R.id.request_desktop_site_id);
        });
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isLoading(), Matchers.is(false)));
        assertUsingDesktopUserAgent(
                tab, true, "User agent should be desktop according to site settings.");
        assertChangeUserActionRecorded(true);

        mActivityTestRule.loadUrl(URL_2);
        assertUsingDesktopUserAgent(
                tab, false, "Site settings exceptions should not affect other URL.");

        // Change site settings and reload.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebsitePreferenceBridge.setContentSettingEnabled(
                    Profile.fromWebContents(tab.getWebContents()),
                    ContentSettingsType.REQUEST_DESKTOP_SITE, true);
            tab.reload();
        });
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isLoading(), Matchers.is(false)));
        assertUsingDesktopUserAgent(
                tab, true, "User agent should be desktop according to global site settings.");
    }

    private void assertUsingDesktopUserAgent(
            Tab tab, boolean useDesktopUserAgent, String errorMessage) {
        Assert.assertNotNull("Tab does not have a WebContent.", tab.getWebContents());
        Assert.assertEquals(errorMessage, useDesktopUserAgent,
                tab.getWebContents().getNavigationController().getUseDesktopUserAgent());
    }

    private void assertContentSettingsHistogramRecorded() {
        Assert.assertEquals("<ContentSettings.DefaultRequestDesktopSiteSetting> is not recorded.",
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContentSettings.DefaultRequestDesktopSiteSetting"));
        Assert.assertEquals("<ContentSettings.Exceptions.request-desktop-site> is not recorded.", 1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "ContentSettings.Exceptions.request-desktop-site"));
    }

    private void assertChangeUserActionRecorded(boolean switchToDesktop) {
        int sample = switchToDesktop ? 1 : 0;
        Assert.assertEquals("<Android.RequestDesktopSite.UserSwitchToDesktop> for sample <" + sample
                        + "> is not recorded,",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.RequestDesktopSite.UserSwitchToDesktop", sample));
    }
}
