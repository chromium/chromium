// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import androidx.test.espresso.Espresso;
import androidx.test.filters.LargeTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.omnibox.FakeOmniboxSuggestions;
import org.chromium.chrome.test.transit.omnibox.OmniboxEnteredTextFacility;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.ui.base.DeviceFormFactor;

/** Public Transit tests for Omnibox. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class OmniboxPTTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final FakeOmniboxSuggestions sFakeSuggestions = new FakeOmniboxSuggestions();
    private WebPageStation mBlankPage;

    @BeforeClass
    public static void setUpClass() {
        sFakeSuggestions.initMocks();
    }

    @AfterClass
    public static void tearDownClass() {
        sFakeSuggestions.destroy();
    }

    @Before
    public void setUp() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(false);
        mBlankPage = mCtaTestRule.startOnBlankPage();
    }

    @LargeTest
    @Test
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // crbug.com/511288411
    public void testOpenTypeDelete_fromWebPage() {
        OmniboxFacility omniboxAndKeyboard = mBlankPage.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        TransitAsserts.assertFinalDestination(mBlankPage);
    }

    @LargeTest
    @Test
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testOpenTypeDelete_fromWebPage_desktop() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        OmniboxFacility omniboxAndKeyboard = mBlankPage.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omniboxAndKeyboard);

        TransitAsserts.assertFinalDestination(mBlankPage);
    }

    @LargeTest
    @Test
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // crbug.com/511288411
    public void testOpenTypeDelete_fromNtp() {
        RegularNewTabPageStation ntp = mBlankPage.openNewTabFast();
        OmniboxFacility omnibox = ntp.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omnibox);

        mBlankPage =
                ntp.openTabSwitcherActionMenu()
                        .selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());
        TransitAsserts.assertFinalDestination(mBlankPage);
    }

    @LargeTest
    @Test
    @Restriction(DeviceFormFactor.DESKTOP)
    public void testOpenTypeDelete_fromNtp_desktop() {
        OmniboxCapabilities.setHasDesktopExperienceForTesting(true);
        RegularNewTabPageStation ntp = mBlankPage.openNewTabFast();
        OmniboxFacility omnibox = ntp.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omnibox);

        mBlankPage =
                ntp.openTabSwitcherActionMenu()
                        .selectCloseTabAndDisplayAnotherTab(WebPageStation.newBuilder());
        TransitAsserts.assertFinalDestination(mBlankPage);
    }

    @LargeTest
    @Test
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void testOpenTypeDelete_fromIncognitoNtp() {
        // Desktop opens an incognito profile as a separate window, which confuses Espresso and
        // leads to test failures.
        IncognitoNewTabPageStation incognitoNtp = mBlankPage.openNewIncognitoTabOrWindowFast();
        OmniboxFacility omnibox = incognitoNtp.openOmnibox(sFakeSuggestions);

        doOpenTypeDelete(omnibox);

        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            incognitoNtp.openTabSwitcherActionMenu().selectCloseTabTo().reachLastStop();
        } else {
            mBlankPage =
                    incognitoNtp
                            .openTabSwitcherActionMenu()
                            .selectCloseTabAndDisplayRegularTab(WebPageStation.newBuilder());
        }
        TransitAsserts.assertFinalDestination(mBlankPage);
    }

    private void doOpenTypeDelete(OmniboxFacility omnibox) {
        OmniboxEnteredTextFacility enteredText = omnibox.typeText("chr");
        enteredText = enteredText.simulateAutocomplete("omium");
        boolean hasDesktopExperience =
                OmniboxCapabilities.hasDesktopExperience(ContextUtils.getApplicationContext());

        // Desktop does not show a delete button.
        if (!hasDesktopExperience) {
            enteredText.clickDelete();
        } else {
            enteredText.clearText();
        }

        Espresso.closeSoftKeyboard();
        omnibox.pressBackTo().exitFacility();
    }
}
