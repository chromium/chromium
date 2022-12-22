// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarCoordinator;
import org.chromium.chrome.browser.toolbar.top.TopToolbarCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/**
 * Integration tests of toolbar with Instant Start which requires 2-stage initialization for Clank
 * startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_NATIVE_INITIALIZATION, ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
    ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INSTANT_START})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
public class InstantStartToolbarTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    /**
     * Tests that the IncognitoSwitchCoordinator isn't create in inflate() if the native library
     * isn't ready. It will be lazily created after native initialization.
     */
    @Test
    @SmallTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void startSurfaceToolbarInflatedPreAndWithNativeTest() {
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Assert.assertFalse(cta.isTablet());
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        Assert.assertEquals("single", StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1));

        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        Assert.assertFalse(LibraryLoader.getInstance().isInitialized());
        TopToolbarCoordinator topToolbarCoordinator =
                (TopToolbarCoordinator) cta.getToolbarManager().getToolbar();

        onViewWaiting(
                allOf(withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar), isDisplayed()));

        StartSurfaceToolbarCoordinator startSurfaceToolbarCoordinator =
                topToolbarCoordinator.getStartSurfaceToolbarForTesting();
        // Verifies that the TabCountProvider for incognito toggle tab layout hasn't been set when
        // the {@link StartSurfaceToolbarCoordinator#inflate()} is called.
        Assert.assertNull(
                startSurfaceToolbarCoordinator.getIncognitoToggleTabCountProviderForTesting());

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> startSurfaceToolbarCoordinator
                                   .getIncognitoToggleTabCountProviderForTesting()
                        != null);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    @DisabledTest(message = "https://crbug.com/1314462")
    public void renderSingleAsHomepage_NoTab_scrollToolbarToTop() throws IOException {
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);

        // Initializes native.
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 0, 0);

        StartSurfaceTestUtils.scrollToolbar(cta);

        View surface = cta.findViewById(org.chromium.chrome.test.R.id.control_container);
        ChromeRenderTestRule.sanitize(surface);
        mRenderTestRule.render(surface, "singlePane_floatingTopToolbar");

        // Focus the omnibox.
        UrlBar urlBar = cta.findViewById(org.chromium.chrome.R.id.url_bar);
        TestThreadUtils.runOnUiThreadBlocking((Runnable) urlBar::requestFocus);
        // Clear the focus.
        TestThreadUtils.runOnUiThreadBlocking(urlBar::clearFocus);
        // Default search engine logo should still show.
        surface = cta.findViewById(org.chromium.chrome.test.R.id.control_container);
        ChromeRenderTestRule.sanitize(surface);
        mRenderTestRule.render(surface, "singlePane_floatingTopToolbar");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS,
            ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE + "=update_available"})
    public void
    testMenuUpdateBadgeWithUpdateAvailable() throws IOException {
        testMenuUpdateBadge(/*shouldShowUpdateBadgeOnStartAndTabs=*/true);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS,
            ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE + "=unsupported_os_version"})
    public void
    testMenuUpdateBadgeWithUnsupportedOsVersion() throws IOException {
        testMenuUpdateBadge(/*shouldShowUpdateBadgeOnStartAndTabs=*/true);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @CommandLineFlags.
    Add({INSTANT_START_TEST_BASE_PARAMS, ChromeSwitches.FORCE_UPDATE_MENU_UPDATE_TYPE + "=none"})
    public void testMenuUpdateBadgeWithoutUpdate() throws IOException {
        testMenuUpdateBadge(/*shouldShowUpdateBadgeOnStartAndTabs=*/false);
    }

    private void testMenuUpdateBadge(boolean shouldShowUpdateBadgeOnStartAndTabs)
            throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0, 1, 2});
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
        StartSurfaceTestUtils.startAndWaitNativeInitialization(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(cta);

        // Check whether the update badge shows on start surface toolbar.
        if (shouldShowUpdateBadgeOnStartAndTabs) {
            onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.menu_badge),
                                  isDescendantOfA(withId(
                                          org.chromium.chrome.test.R.id.tab_switcher_toolbar))))
                    .check(matches(isDisplayed()));
        } else {
            onView(allOf(withId(org.chromium.chrome.test.R.id.menu_badge),
                           isDescendantOfA(
                                   withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))))
                    .check(matches(withEffectiveVisibility(Visibility.INVISIBLE)));
        }

        // Navigate to any tab to check whether the update badge shows on toolbar layout.
        StartSurfaceTestUtils.launchFirstMVTile(cta, /* currentTabCount = */ 3);
        if (shouldShowUpdateBadgeOnStartAndTabs) {
            onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.menu_badge),
                                  isDescendantOfA(withId(org.chromium.chrome.test.R.id.toolbar))))
                    .check(matches(isDisplayed()));
        } else {
            onView(allOf(withId(org.chromium.chrome.test.R.id.menu_badge),
                           isDescendantOfA(withId(org.chromium.chrome.test.R.id.toolbar))))
                    .check(matches(withEffectiveVisibility(Visibility.INVISIBLE)));
        }

        // Update badge shouldn't show on tab switcher surface toolbar.
        TabUiTestHelper.enterTabSwitcher(cta);
        StartSurfaceTestUtils.waitForTabSwitcherVisible(cta);
        onViewWaiting(allOf(withId(org.chromium.chrome.test.R.id.menu_button),
                              isDescendantOfA(
                                      withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))))
                .check(matches(isDisplayed()));
        if (shouldShowUpdateBadgeOnStartAndTabs) {
            // If the update badge should show on homepage and tabs, it's suppressed in
            // StartSurfaceToolbarMediator#onStartSurfaceStateChanged when tab switcher surface is
            // shown. So its visibility should be Gone instead of Invisible (as initialized).
            onView(allOf(withId(org.chromium.chrome.test.R.id.menu_badge),
                           isDescendantOfA(
                                   withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))))
                    .check(matches(withEffectiveVisibility(Visibility.GONE)));
        } else {
            onView(allOf(withId(org.chromium.chrome.test.R.id.menu_badge),
                           isDescendantOfA(
                                   withId(org.chromium.chrome.test.R.id.tab_switcher_toolbar))))
                    .check(matches(withEffectiveVisibility(Visibility.INVISIBLE)));
        }
    }
}
