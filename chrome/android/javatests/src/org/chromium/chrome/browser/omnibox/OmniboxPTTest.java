// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.util.Pair;

import androidx.test.filters.LargeTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ChromeTabbedActivityPublicTransitEntryPoints;
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
public class OmniboxPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sChromeTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule public static JniMocker sJniMocker = new JniMocker();

    @Rule
    public BatchedPublicTransitRule<WebPageStation> mBatchedRule =
            new BatchedPublicTransitRule<>(WebPageStation.class, /* expectResetByTest= */ true);

    private static final FakeOmniboxSuggestions sFakeSuggestions =
            new FakeOmniboxSuggestions(sJniMocker);

    ChromeTabbedActivityPublicTransitEntryPoints mEntryPoints =
            new ChromeTabbedActivityPublicTransitEntryPoints(sChromeTabbedActivityTestRule);

    @LargeTest
    @Test
    public void testOpenTypeDelete_fromWebPage() {
        WebPageStation blankPage = mEntryPoints.startOnBlankPage(mBatchedRule);
        var omniboxAndKeyboard = blankPage.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        TransitAsserts.assertFinalDestination(blankPage);
    }

    @LargeTest
    @Test
    public void testOpenTypeDelete_fromNtp() {
        WebPageStation blankPage = mEntryPoints.startOnBlankPage(mBatchedRule);
        RegularNewTabPageStation ntp = blankPage.openGenericAppMenu().openNewTab();
        var omniboxAndKeyboard = ntp.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        blankPage =
                ntp.openTabSwitcherActionMenu()
                        .selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());
        TransitAsserts.assertFinalDestination(blankPage);
    }

    @LargeTest
    @Test
    public void testOpenTypeDelete_fromIncognitoNtp() {
        WebPageStation blankPage = mEntryPoints.startOnBlankPage(mBatchedRule);
        IncognitoNewTabPageStation incognitoNtp =
                blankPage.openGenericAppMenu().openNewIncognitoTab();
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
        omniboxAndKeyboard.first.pressBackToClose();
    }
}
