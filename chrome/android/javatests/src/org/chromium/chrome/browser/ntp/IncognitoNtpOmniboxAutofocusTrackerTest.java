// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests for {@link IncognitoNtpOmniboxAutofocusTracker}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNtpOmniboxAutofocusTrackerTest {
    private WebPageStation mIncognitoBlankPage;

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        mIncognitoBlankPage = mActivityTestRule.startOnIncognitoBlankPage();
    }

    @After
    public void tearDown() {
        mActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsHistograms_onAutofocus_phone() {
        HistogramWatcher watcher = createAutoFocusHistogramWatcher();

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        // Wait for autofocus.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Omnibox should be autofocused.",
                            mActivityTestRule.getActivity().getToolbarManager().isUrlBarFocused(),
                            Matchers.is(true));
                });

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testRecordsHistograms_onAutofocus_tabletOrDesktopNonAuto() {
        HistogramWatcher watcher = createAutoFocusHistogramWatcher();

        IncognitoNewTabPageStation ntpPage = mIncognitoBlankPage.openNewIncognitoTabFast();

        // Wait for autofocus.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Omnibox should be autofocused.",
                            ntpPage.getActivity().getToolbarManager().isUrlBarFocused(),
                            Matchers.is(true));
                });

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsHistograms_onManualFocus_phone() {
        // Enable Incognito Ntp Omnibox Autofocus feature `with_hardware_keyboard` parameter, but
        // make it fail the autofocus by not attaching the hardware keyboard.
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        HistogramWatcher watcher = createManualFocusHistogramWatcher();

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);

        // Since autofocus was failed, focus omnibox manually.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getToolbarManager()
                            .setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP);
                });

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testRecordsHistograms_onManualFocus_tabletOrDesktopNonAuto() {
        // Enable Incognito Ntp Omnibox Autofocus feature `with_hardware_keyboard` parameter, but
        // make it fail the autofocus by not attaching the hardware keyboard.
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        HistogramWatcher watcher = createManualFocusHistogramWatcher();

        IncognitoNewTabPageStation ntpPage = mIncognitoBlankPage.openNewIncognitoTabFast();

        // Since autofocus was failed, focus omnibox manually.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ntpPage.getActivity()
                            .getToolbarManager()
                            .setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP);
                });

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    private HistogramWatcher createAutoFocusHistogramWatcher() {
        return HistogramWatcher.newBuilder()
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.KeyboardHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage")
                .build();
    }

    private HistogramWatcher createManualFocusHistogramWatcher() {
        return HistogramWatcher.newBuilder()
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.KeyboardHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage")
                .expectAnyRecord(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage")
                .expectNoRecords(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage")
                .expectNoRecords(
                        "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage")
                .build();
    }
}
