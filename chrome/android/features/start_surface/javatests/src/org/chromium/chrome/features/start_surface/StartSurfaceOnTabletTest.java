// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_ON_TABLET_TEST_PARAMS;

import android.text.TextUtils;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/**
 * Integration tests of showing a NTP with Start surface UI at startup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(
        {UiRestriction.RESTRICTION_TYPE_TABLET, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@EnableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET,
        ChromeFeatureList.START_SURFACE_RETURN_TIME + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
@DoNotBatch(reason = "This test suite tests startup behaviors.")
public class StartSurfaceOnTabletTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    private static final String TAB_URL = "https://foo.com/";

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    @DisableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    public void testStartSurfaceOnTabletDisabled() throws IOException {
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        verifyTabCountAndActiveTabUrl(
                mActivityTestRule.getActivity(), 1, TAB_URL, null /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testStartSurfaceOnTablet() throws IOException {
        StartSurfaceTestUtils.prepareTabStateMetadataFile(new int[] {0}, new String[] {TAB_URL}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that a NTP is created and set as the current Tab.
        verifyTabCountAndActiveTabUrl(mActivityTestRule.getActivity(), 2, UrlConstants.NTP_URL,
                true /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    @DisabledTest(message = "https://crbug.com/1431467")
    public void testStartSurfaceOnTabletWithNtpExist() throws IOException {
        // The existing NTP isn't the last active Tab.
        String modifiedNtpUrl = UrlConstants.NTP_URL + "/1";
        Assert.assertTrue(UrlUtilities.isNTPUrl(modifiedNtpUrl));
        StartSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 0);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that the existing non active NTP is skipped in Tab restoring, and a new NTP is
        // created and set as the active Tab.
        verifyTabCountAndActiveTabUrl(mActivityTestRule.getActivity(), 2, UrlConstants.NTP_URL,
                true /* expectHomeSurfaceUiShown */);
    }

    @Test
    @MediumTest
    @Feature({"StartSurface"})
    @CommandLineFlags.Add({START_SURFACE_ON_TABLET_TEST_PARAMS})
    public void testStartSurfaceOnTabletWithActiveNtpExist() throws IOException {
        // The existing NTP is set as the last active Tab.
        String modifiedNtpUrl = UrlConstants.NTP_URL + "/1";
        Assert.assertTrue(UrlUtilities.isNTPUrl(modifiedNtpUrl));
        StartSurfaceTestUtils.prepareTabStateMetadataFile(
                new int[] {0, 1}, new String[] {TAB_URL, modifiedNtpUrl}, 1);
        StartSurfaceTestUtils.startMainActivityFromLauncher(mActivityTestRule);
        StartSurfaceTestUtils.waitForTabModel(mActivityTestRule.getActivity());

        // Verifies that no new NTP is created, and the existing NTP is reused and set as the
        // current Tab.
        verifyTabCountAndActiveTabUrl(mActivityTestRule.getActivity(), 2, modifiedNtpUrl,
                false /* expectHomeSurfaceUiShown */);
    }

    private void verifyTabCountAndActiveTabUrl(
            ChromeTabbedActivity cta, int tabCount, String url, Boolean expectHomeSurfaceUiShown) {
        Assert.assertEquals(tabCount, cta.getCurrentTabModel().getCount());
        Tab tab = StartSurfaceTestUtils.getCurrentTabFromUIThread(cta);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(TextUtils.equals(url, tab.getUrl().getSpec())); });
        if (expectHomeSurfaceUiShown != null) {
            Assert.assertEquals(expectHomeSurfaceUiShown,
                    ((NewTabPage) tab.getNativePage()).isSingleTabCardVisibleForTesting());
        }
    }
}
