// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/**
 * Integration tests of new tab from launcher with Instant Start which requires 2-stage
 * initialization for Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.
    Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
    ChromeFeatureList.TAB_SWITCHER_ON_RETURN + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study", ChromeFeatureList.INSTANT_START})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE})
public class InstantStartNewTabFromLauncherTest {
    // clang-format on
    private static final long MAX_TIMEOUT_MS = 30000L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    @DisabledTest(message = "https://crbug.com/1192559")
    public void testNewTabFromLauncher() throws IOException {
        testNewTabFromLauncherImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncherWithInstantStartDisabled() throws IOException {
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherImpl();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testNewTabFromLauncherWithHomepageDisabled_NoFinale() throws IOException {
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testNewTabFromLauncherWithHomepageDisabled_NoFinale_NoInstant() throws IOException {
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncherWithHomepageDisabled_Finale() throws IOException {
        Assert.assertTrue(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS + "/omnibox_focused_on_new_tab/true"})
    public void testNewTabFromLauncherWithHomepageDisabled_Finale_NoInstant() throws IOException {
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testNewIncognitoTabFromLauncher() throws IOException {
        testNewIncognitoTabFromLauncherImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testNewIncognitoTabFromLauncher_NoInstant() throws IOException {
        Assert.assertFalse(CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START));
        testNewIncognitoTabFromLauncherImpl();
    }

    private void testNewTabFromLauncherImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startNewTabFromLauncherIcon(false);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        waitForView(withId(org.chromium.chrome.start_surface.R.id.search_box_text));
        TextView urlBar = cta.findViewById(org.chromium.chrome.start_surface.R.id.url_bar);
        CriteriaHelper.pollUiThread(
                ()
                        -> StartSurfaceTestUtils.isKeyboardShown(mActivityTestRule)
                        && urlBar.isFocused(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        waitForView(withId(org.chromium.chrome.start_surface.R.id.voice_search_button));
        Assert.assertTrue(TextUtils.isEmpty(urlBar.getText()));
        assertEquals(cta.findViewById(org.chromium.chrome.start_surface.R.id.toolbar_buttons)
                             .getVisibility(),
                View.INVISIBLE);
        ToolbarDataProvider toolbarDataProvider =
                cta.getToolbarManager().getLocationBarModelForTesting();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(TextUtils.equals(toolbarDataProvider.getCurrentUrl(), UrlConstants.NTP_URL));
        });
    }

    private void testNewIncognitoTabFromLauncherImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startNewTabFromLauncherIcon(true);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);

        Assert.assertFalse(cta.getLayoutManager().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UrlUtilities.isNTPUrl(cta.getActivityTab().getUrl())); });
    }

    private void testNewTabFromLauncherWithHomepageDisabledImpl() throws IOException {
        StartSurfaceTestUtils.createTabStateFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0);
        TabAttributeCache.setTitleForTesting(0, "Google");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        // Launches new Tab from the launcher icon and verifies that a NTP is created and showing.
        startNewTabFromLauncherIcon(false);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        Assert.assertFalse(cta.getLayoutManager().overviewVisible());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UrlUtilities.isNTPUrl(cta.getActivityTab().getUrl())); });
    }

    private void startNewTabFromLauncherIcon(boolean incognito) {
        Intent intent = IntentHandler.createTrustedOpenNewTabIntent(
                ContextUtils.getApplicationContext(), incognito);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.launchActivity(intent);
    }
}
