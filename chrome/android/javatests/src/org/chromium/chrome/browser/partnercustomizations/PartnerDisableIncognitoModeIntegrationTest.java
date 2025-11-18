// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.modelutil.MVCListAdapter;

import java.util.List;
import java.util.concurrent.ExecutionException;

/** Integration tests for the partner disabling incognito mode feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(UiAndroidFeatures.USE_NEW_ETC1_ENCODER) // https://crbug.com/401244299
public class PartnerDisableIncognitoModeIntegrationTest {
    private final BasePartnerBrowserCustomizationIntegrationTestRule
            mPartnerBrowserCustomizationRule =
                    new BasePartnerBrowserCustomizationIntegrationTestRule();

    private final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mPartnerBrowserCustomizationRule).around(mActivityTestRule);

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Before
    public void setUp() {
        mHomepageTestRule.useChromeNtpForTest();
    }

    private void setParentalControlsEnabled(boolean enabled) {
        Uri uri =
                CustomizationProviderDelegateUpstreamImpl.buildQueryUri(
                        PartnerBrowserCustomizations.PARTNER_DISABLE_INCOGNITO_MODE_PATH);
        Bundle bundle = new Bundle();
        bundle.putBoolean(
                TestPartnerBrowserCustomizationsProvider.INCOGNITO_MODE_DISABLED_KEY, enabled);
        Context context = ApplicationProvider.getApplicationContext();
        context.getContentResolver().call(uri, "setIncognitoModeDisabled", null, bundle);
    }

    private void assertIncognitoMenuItemEnabled(boolean enabled) {
        MVCListAdapter.ModelList modelList =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                AppMenuTestSupport.getAppMenuPropertiesDelegate(
                                                mActivityTestRule.getAppMenuCoordinator())
                                        .getMenuItems());
        MVCListAdapter.ListItem newIncognitoItem = null;
        for (MVCListAdapter.ListItem item : modelList) {
            int itemId = item.model.get(AppMenuItemProperties.MENU_ITEM_ID);
            if (List.of(R.id.new_incognito_tab_menu_id, R.id.new_incognito_window_menu_id)
                    .contains(itemId)) {
                newIncognitoItem = item;
                break;
            }
        }
        Assert.assertNotNull(newIncognitoItem);
        Assert.assertEquals(enabled, newIncognitoItem.model.get(AppMenuItemProperties.ENABLED));
    }

    private void waitForParentalControlsEnabledState(final boolean parentalControlsEnabled) {
        CriteriaHelper.pollUiThread(
                () -> {
                    // areParentalControlsEnabled is updated on a background thread, so we
                    // also wait on the isIncognitoModeEnabled to ensure the updates on the
                    // UI thread have also triggered.
                    Criteria.checkThat(
                            PartnerBrowserCustomizations.isIncognitoDisabled(),
                            Matchers.is(parentalControlsEnabled));
                    Criteria.checkThat(
                            IncognitoUtils.isIncognitoModeEnabled(
                                    ProfileManager.getLastUsedRegularProfile()),
                            Matchers.not(parentalControlsEnabled));
                });
    }

    private void toggleActivityForegroundState() {
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onPause());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStop());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onStart());
        ThreadUtils.runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().onResume());
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoEnabledIfNoParentalControls() throws InterruptedException {
        setParentalControlsEnabled(false);
        var page = mActivityTestRule.startOnBlankPage();
        waitForParentalControlsEnabledState(false);
        page.openNewIncognitoTabOrWindowFast();
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testIncognitoMenuItemEnabledBasedOnParentalControls()
            throws InterruptedException, ExecutionException {
        setParentalControlsEnabled(true);
        mActivityTestRule.startOnBlankPage();
        waitForParentalControlsEnabledState(true);
        assertIncognitoMenuItemEnabled(false);

        setParentalControlsEnabled(false);
        toggleActivityForegroundState();
        waitForParentalControlsEnabledState(false);
        assertIncognitoMenuItemEnabled(true);
    }

    @Test
    @MediumTest
    @Feature({"DisableIncognitoMode"})
    public void testEnabledParentalControlsClosesIncognitoTabs() throws InterruptedException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        String[] testUrls = {
            testServer.getURL("/chrome/test/data/android/about.html"),
            testServer.getURL("/chrome/test/data/android/ok.txt"),
            testServer.getURL("/chrome/test/data/android/test.html")
        };
        setParentalControlsEnabled(false);
        var page = mActivityTestRule.startOnBlankPage();
        waitForParentalControlsEnabledState(false);
        page.openNewIncognitoTabOrWindowFast()
                .loadWebPageProgrammatically(testUrls[0])
                .openNewIncognitoTabOrWindowFast()
                .loadWebPageProgrammatically(testUrls[1])
                .openNewIncognitoTabOrWindowFast()
                .loadWebPageProgrammatically(testUrls[2]);
        mActivityTestRule.loadUrlInNewTab(testUrls[0], false);
        setParentalControlsEnabled(true);
        toggleActivityForegroundState();
        waitForParentalControlsEnabledState(true);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.tabsCount(/* incognito= */ true), Matchers.is(0));
                });
    }
}
