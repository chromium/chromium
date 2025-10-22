// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Tests for {@link IncognitoNtpOmniboxAutofocusTracker}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IncognitoNtpOmniboxAutofocusTrackerTest {
    private static final String HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE =
            "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.KeyboardHeightPercentage";
    private static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingHeightPercentage";
    private static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsHeightPercentage";
    private static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage";
    private static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage";
    private static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage";
    private static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    public void testRecordsHistograms_onAutofocus() {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .build();

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
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    public void testRecordsHistograms_onManualFocus() {
        // Enable Incognito Ntp Omnibox Autofocus feature `with_hardware_keyboard` parameter, but
        // make it fail the autofocus by not attaching the hardware keyboard.
        IncognitoNtpOmniboxAutofocusManager.sIsHardwareKeyboardAttachedForTesting = false;

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectAnyRecord(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectNoRecords(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .expectNoRecords(
                                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                        .build();

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
}
