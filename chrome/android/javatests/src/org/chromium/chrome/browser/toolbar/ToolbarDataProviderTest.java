// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link ToolbarDataProvider}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
@Batch(PER_CLASS)
public class ToolbarDataProviderTest {
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mActivityTestRule, false);

    @Test
    @MediumTest
    public void testPrimaryOTRProfileUsedForIncognitoTabbedActivity() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);
        ToolbarPhone toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = toolbar.getToolbarDataProvider().getProfile();
            assertTrue(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    public void testRegularProfileUsedForRegularTabbedActivity() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/false);
        ToolbarPhone toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = toolbar.getToolbarDataProvider().getProfile();
            assertFalse(profile.isOffTheRecord());
        });
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1154445")
    public void testNonPrimaryOTRProfileUsedForIncognitoCCT() throws TimeoutException {
        Intent intent = CustomTabsTestUtils.createMinimalIncognitoCustomTabIntent(
                InstrumentationRegistry.getContext(), "about:blank");
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabToolbar customTabToolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = customTabToolbar.getToolbarDataProvider().getProfile();
            assertTrue(profile.isOffTheRecord());
            assertFalse(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    public void testRegularProfileUsedForRegularCCT() {
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabToolbar customTabToolbar =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = customTabToolbar.getToolbarDataProvider().getProfile();
            assertFalse(profile.isOffTheRecord());
        });
    }
}
