// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.ActivityFinisher;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.RenderTestRule.Component;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for search in the tab switcher. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
public class TabSwitcherSearchRenderTest {
    private static final int SERVER_PORT = 13245;
    private static final String URL_PREFIX = "127.0.0.1:" + SERVER_PORT;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_SWITCHER)
                    .build();

    private EmbeddedTestServer mTestServer;

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
    public void testZeroPrefixSuggestions_oneTab() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SearchActivity.class,
                        () -> onView(withId(R.id.search_box_text)).perform(click()));
        mRenderTestRule.render(
                searchActivity.findViewById(android.R.id.content), "hub_search_zps_singletab");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderZeroPrefixSuggestions() throws IOException {
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html",
                        "/chrome/test/data/android/navigate/two.html",
                        "/chrome/test/data/android/navigate/three.html",
                        "/chrome/test/data/android/about.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SearchActivity.class,
                        () -> onView(withId(R.id.search_box_text)).perform(click()));
        ViewUtils.waitForView(
                searchActivity.findViewById(R.id.control_container), withText("Last open tabs"));
        mRenderTestRule.render(
                searchActivity.findViewById(android.R.id.content), "hub_search_zps_maxtab");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderTypedSuggestions() throws IOException {
        mActivityTestRule.loadUrl(mTestServer.getURL("/chrome/test/data/android/test.html"));
        List<String> urlsToOpen =
                Arrays.asList(
                        "/chrome/test/data/android/test.html",
                        "/chrome/test/data/android/navigate/one.html",
                        "/chrome/test/data/android/navigate/two.html",
                        "/chrome/test/data/android/navigate/three.html",
                        "/chrome/test/data/android/about.html");
        TabSwitcherSearchTestUtils.openUrls(mActivityTestRule, urlsToOpen, /* incognito= */ false);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        SearchActivity searchActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SearchActivity.class,
                        () -> onView(withId(R.id.search_box_text)).perform(click()));
        ViewUtils.waitForView(
                searchActivity.findViewById(R.id.control_container), withText("Last open tabs"));

        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(searchActivity);
        omniboxTestUtils.requestFocus();
        omniboxTestUtils.typeText("one.html", /* execute= */ false);
        omniboxTestUtils.waitAnimationsComplete();

        mRenderTestRule.render(
                searchActivity.findViewById(android.R.id.content), "hub_search_typed");
    }
}
