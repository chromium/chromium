// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.ui.base.DeviceFormFactor;

/** Instrumentation tests for {@link ToolbarDataProvider}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
// TODO(crbug.com/344665253): Failing when batched, batch this again.
public class ToolbarDataProviderTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    @Test
    @MediumTest
    public void testPrimaryOtrProfileUsedForIncognitoTabbedActivity() {
        mActivityTestRule.startOnBlankPage().openNewIncognitoTabFast();
        ToolbarPhone toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = toolbar.getToolbarDataProvider().getProfile();
                    assertTrue(profile.isPrimaryOtrProfile());
                });
    }

    @Test
    @MediumTest
    public void testRegularProfileUsedForRegularTabbedActivity() {
        mActivityTestRule.startOnBlankPage().openNewTabFast();
        ToolbarPhone toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = toolbar.getToolbarDataProvider().getProfile();
                    assertFalse(profile.isOffTheRecord());
                });
    }
}
