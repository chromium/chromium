// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.test.util.DeviceRestriction;

/** Public Transit test for opening a second Chrome window in a secondary profile. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Creates a persistent secondary profile on disk")
@MinAndroidSdkLevel(
        value = VERSION_CODES.S,
        reason = "Multi-instance singleInstancePerTask requires API 31+")
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
@EnableFeatures({ChromeFeatureList.ANDROID_MULTIPROFILE, "AndroidMultiprofileStartupData"})
public class MultiProfileWindowTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/590138463: Blocked on crrev.com/c/8597395")
    public void testOpenSecondWindowInNewProfile() {
        WebPageStation firstWindow = mCtaTestRule.startOnBlankPage();
        RegularNewTabPageStation secondWindow =
                firstWindow.openNewWindowInNewProfileFast("Secondary");

        TransitAsserts.assertInDifferentTasks(firstWindow, secondWindow);
        TransitAsserts.assertFinalDestinations(firstWindow, secondWindow);

        Profile firstProfile = firstWindow.getOriginalProfile();
        Profile secondProfile = secondWindow.getOriginalProfile();
        assertNotEquals(firstProfile, secondProfile);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(firstProfile.isInitialProfile());
                    assertFalse(secondProfile.isInitialProfile());
                    assertEquals(secondProfile, secondWindow.getTab().getProfile());
                });
    }
}
