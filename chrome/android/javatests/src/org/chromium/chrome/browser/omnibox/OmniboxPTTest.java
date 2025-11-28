// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.os.Build;
import android.util.Pair;

import androidx.test.filters.LargeTest;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ReusedCtaTransitTestRule;
import org.chromium.chrome.test.transit.SoftKeyboardFacility;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxEnteredTextFacility;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Public Transit tests for Omnibox. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@DisableIf.Build(
        sdk_is_greater_than = Build.VERSION_CODES.R,
        sdk_is_less_than = Build.VERSION_CODES.TIRAMISU,
        message = "Flaky in S, crbug.com/372709072")
public class OmniboxPTTest {
    @Rule
    public ReusedCtaTransitTestRule<WebPageStation> mCtaTestRule =
            ChromeTransitTestRules.blankPageStartReusedActivityRule();

    private static final FakeOmniboxSuggestions sFakeSuggestions = new FakeOmniboxSuggestions();

    @BeforeClass
    public static void setUpClass() {
        sFakeSuggestions.initMocks();
    }

    @AfterClass
    public static void tearDownClass() {
        sFakeSuggestions.destroy();
    }

    @LargeTest
    @Test
    public void testOpenTypeDelete_fromWebPage() {
        ChromeFeatureList.sAndroidBottomToolbarV2ForceBottomForFocusedOmnibox.setForTesting(false);
        WebPageStation blankPage = mCtaTestRule.start();
        var omniboxAndKeyboard = blankPage.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        TransitAsserts.assertFinalDestination(blankPage);
    }

    @LargeTest
    @Test
    public void testOpenTypeDelete_fromNtp() {
        ChromeFeatureList.sAndroidBottomToolbarV2ForceBottomForFocusedOmnibox.setForTesting(false);
        WebPageStation blankPage = mCtaTestRule.start();
        RegularNewTabPageStation ntp = blankPage.openNewTabFast();
        var omniboxAndKeyboard = ntp.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        blankPage =
                ntp.openTabSwitcherActionMenu()
                        .selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());
        TransitAsserts.assertFinalDestination(blankPage);
    }

    @LargeTest
    @Test
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.Q, message = "crbug.com/415805917")
    public void testOpenTypeDelete_fromIncognitoNtp() {
        WebPageStation blankPage = mCtaTestRule.start();
        IncognitoNewTabPageStation incognitoNtp = blankPage.openNewIncognitoTabFast();
        var omniboxAndKeyboard = incognitoNtp.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        blankPage =
                incognitoNtp
                        .openTabSwitcherActionMenu()
                        .selectCloseTabAndDisplayRegularTab(WebPageStation.newBuilder());
        TransitAsserts.assertFinalDestination(blankPage);
    }

    private void doOpenTypeDelete(Pair<OmniboxFacility, SoftKeyboardFacility> omniboxAndKeyboard) {
        OmniboxFacility omnibox = omniboxAndKeyboard.first;
        SoftKeyboardFacility keyboard = omniboxAndKeyboard.second;

        OmniboxEnteredTextFacility enteredText = omnibox.typeText("chr");
        enteredText = enteredText.simulateAutocomplete("omium");
        enteredText.clickDelete();

        keyboard.close();
        omnibox.pressBackTo().exitFacility();
    }
}
