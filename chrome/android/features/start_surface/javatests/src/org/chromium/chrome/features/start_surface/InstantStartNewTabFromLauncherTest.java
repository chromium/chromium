// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.INSTANT_START_TEST_BASE_PARAMS;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/**
 * Integration tests of new tab from launcher with Instant Start which requires 2-stage
 * initialization for Clank startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@EnableFeatures({
    ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study,",
    ChromeFeatureList.START_SURFACE_ANDROID + "<Study",
    ChromeFeatureList.INSTANT_START
})
@DisableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
@Restriction({
    Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE,
    UiRestriction.RESTRICTION_TYPE_PHONE
})
public class InstantStartNewTabFromLauncherTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @After
    public void tearDown() {
        if (mActivityTestRule.getActivity() != null) {
            ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        }
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testNewTabFromLauncherWithHomepageDisabled() throws IOException {
        Assert.assertTrue(ChromeFeatureList.sInstantStart.isEnabled());
        testNewTabFromLauncherWithHomepageDisabledImpl();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.INSTANT_START)
    @CommandLineFlags.Add({INSTANT_START_TEST_BASE_PARAMS})
    public void testNewTabFromLauncherWithHomepageDisabled_NoInstant() throws IOException {
        Assert.assertFalse(ChromeFeatureList.sInstantStart.isEnabled());
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
        Assert.assertFalse(ChromeFeatureList.sInstantStart.isEnabled());
        testNewIncognitoTabFromLauncherImpl();
    }

    private void testNewIncognitoTabFromLauncherImpl() throws IOException {
        StartSurfaceTestUtils.createTabStatesAndMetadataFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google");

        startNewTabFromLauncherIcon(true);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 1, 1);

        Assert.assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(UrlUtilities.isNtpUrl(cta.getActivityTab().getUrl()));
                });
    }

    private void testNewTabFromLauncherWithHomepageDisabledImpl() throws IOException {
        StartSurfaceTestUtils.createTabStatesAndMetadataFile(new int[] {0});
        StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        TabAttributeCache.setTitleForTesting(0, "Google");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> HomepageManager.getInstance().setPrefHomepageEnabled(false));
        Assert.assertFalse(HomepageManager.isHomepageEnabled());

        // Launches new Tab from the launcher icon and verifies that a NTP is created and showing.
        startNewTabFromLauncherIcon(false);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        StartSurfaceTestUtils.waitForTabModel(cta);
        TabUiTestHelper.verifyTabModelTabCount(cta, 2, 0);

        Assert.assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(UrlUtilities.isNtpUrl(cta.getActivityTab().getUrl()));
                });
    }

    private void startNewTabFromLauncherIcon(boolean incognito) {
        Intent intent =
                IntentHandler.createTrustedOpenNewTabIntent(
                        ContextUtils.getApplicationContext(), incognito);
        intent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, true);
        mActivityTestRule.prepareUrlIntent(intent, null);
        mActivityTestRule.launchActivity(intent);
    }
}
