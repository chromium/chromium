// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.ui.base.DeviceFormFactor.PHONE;
import static org.chromium.ui.base.DeviceFormFactor.TABLET;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for search in the tab switcher. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
public class TabSwitcherSearchRenderTest {
    private static final int SERVER_PORT = 13245;
    private static final String URL_PREFIX = "127.0.0.1:" + SERVER_PORT;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(6)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_SWITCHER)
                    .build();

    private EmbeddedTestServer mTestServer;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Before
    public void setUp() throws ExecutionException {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTestServer =
                TabSwitcherSearchTestUtils.setServerPortAndGetTestServer(
                        mActivityTestRule, SERVER_PORT);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        ActivityFinisher.finishAll();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(PHONE)
    public void testHubSearchBox_Phone() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        mRenderTestRule.render(
                cta.findViewById(R.id.tab_switcher_view_holder), "hub_searchbox_phone");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(PHONE)
    public void testHubSearchBox_PhoneLandscape() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_LANDSCAPE);
        enterTabSwitcher(cta);

        mRenderTestRule.render(
                cta.findViewById(R.id.tab_switcher_view_holder), "hub_searchbox_phone_landscape");
        ActivityTestUtils.clearActivityOrientation(cta);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(TABLET)
    public void testHubSearchLoupe_Tablet() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        mRenderTestRule.render(
                cta.findViewById(R.id.tab_switcher_view_holder), "hub_searchloupe_tablet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testZeroPrefixSuggestions(boolean nightModeEnabled) throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/test.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.checkSuggestionsShown(true);

        mRenderTestRule.render(searchActivity.findViewById(android.R.id.content), "hub_search_zps");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testZeroPrefixSuggestions_Incognito() throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/test.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ true);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);
        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.checkSuggestionsShown(false);

        mRenderTestRule.render(
                searchActivity.findViewById(android.R.id.content), "hub_search_zps_incognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderTypedSuggestions(boolean nightModeEnabled) throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.checkSuggestionsShown(true);

        mRenderTestRule.render(
                searchActivity.findViewById(android.R.id.content), "hub_search_typed");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderTypedSuggestions_Incognito() throws IOException {
        List<String> urlsToOpen = Arrays.asList("/chrome/test/data/android/navigate/one.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ true);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                TabSwitcherSearchTestUtils.launchSearchActivityFromTabSwitcherAndWaitForLoad(cta);

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.checkSuggestionsShown(true);

        mRenderTestRule.render(
                searchActivity.findViewById(android.R.id.content), "hub_search_typed_incognito");
    }
}
