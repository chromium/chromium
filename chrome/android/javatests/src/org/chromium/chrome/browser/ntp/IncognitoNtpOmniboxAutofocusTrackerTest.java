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
import org.chromium.base.test.util.Batch;
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
@Batch(Batch.PER_CLASS)
public class IncognitoNtpOmniboxAutofocusTrackerTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
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

        waitForOmniboxFocus();

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testRecordsHistograms_onAutofocus_tabletOrDesktopNonAuto() {
        mActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
        WebPageStation blankPage = mActivityTestRule.startOnIncognitoBlankPage();

        HistogramWatcher watcher = createAutoFocusHistogramWatcher();

        IncognitoNewTabPageStation ntpPage = blankPage.openNewIncognitoTabFast();

        waitForOmniboxFocus();

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
        waitForOmniboxFocus();

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    public void testRecordsHistograms_onManualFocus_tabletOrDesktopNonAuto() {
        mActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
        WebPageStation blankPage = mActivityTestRule.startOnIncognitoBlankPage();

        // Enable Incognito Ntp Omnibox Autofocus feature `with_hardware_keyboard` parameter, but
        // make it fail the autofocus by not attaching the hardware keyboard.
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        HistogramWatcher watcher = createManualFocusHistogramWatcher();

        IncognitoNewTabPageStation ntpPage = blankPage.openNewIncognitoTabFast();

        // Since autofocus was failed, focus omnibox manually.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ntpPage.getActivity()
                            .getToolbarManager()
                            .setUrlBarFocus(true, OmniboxFocusReason.OMNIBOX_TAP);
                });
        waitForOmniboxFocus();

        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP)
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_alwaysOn() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome.ALWAYS_ON);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true/with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notFirstTab_withPrediction_withHardwareKeyboard() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        // Open first tab to pass check tabs count later.
        var firstTabWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_PREDICTION_WITH_HARDWARE_KEYBOARD);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        firstTabWatcher.pollInstrumentationThreadUntilSatisfied();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .NOT_FIRST_TAB_WITH_PREDICTION_WITH_HARDWARE_KEYBOARD);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_prediction/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notFirstTab_withPrediction() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);

        // Open first tab to pass check tabs count later.
        var firstTabWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_PREDICTION);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        firstTabWatcher.pollInstrumentationThreadUntilSatisfied();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .NOT_FIRST_TAB_WITH_PREDICTION);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":not_first_tab/true/with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notFirstTab_withHardwareKeyboard() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        // Open first tab to pass check tabs count later.
        var firstTabWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_HARDWARE_KEYBOARD);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        firstTabWatcher.pollInstrumentationThreadUntilSatisfied();

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .NOT_FIRST_TAB_WITH_HARDWARE_KEYBOARD);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":with_prediction/true/with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_withPrediction_withHardwareKeyboard() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_PREDICTION_WITH_HARDWARE_KEYBOARD);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notFirstTab() {
        // On first tab autofocus fails.
        var firstTabWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome.NOT_TRIGGERED);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        firstTabWatcher.pollInstrumentationThreadUntilSatisfied();

        // Open second tab to trigger autofocus.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome.NOT_FIRST_TAB);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_withPrediction() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_PREDICTION);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP
                    + ":with_prediction/true/with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void
            testRecordsAutofocusOutcome_withPrediction_withHardwareKeyboardEnabledButNotAttached() {
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(true);
        // Hardware keyboard is not attached.
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_PREDICTION);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_withHardwareKeyboard() {
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(true);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome
                                .WITH_HARDWARE_KEYBOARD);
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        waitForOmniboxFocus();
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":not_first_tab/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notTriggered_whenNotFirstTab() {
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome.NOT_TRIGGERED);

        // On first tab autofocus will be failed.
        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_prediction/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notTriggered_whenWithPrediction() {
        // Prediction failed.
        IncognitoNtpOmniboxAutofocusManager.setAutofocusAllowedWithPredictionForTesting(false);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome.NOT_TRIGGERED);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @MediumTest
    @EnableFeatures(
            ChromeFeatureList.OMNIBOX_AUTOFOCUS_ON_INCOGNITO_NTP + ":with_hardware_keyboard/true")
    @Restriction(DeviceFormFactor.PHONE)
    public void testRecordsAutofocusOutcome_notTriggered_whenWithHardwareKeyboard() {
        // Hardware keyboard is not attached.
        IncognitoNtpOmniboxAutofocusManager.setIsHardwareKeyboardAttachedForTesting(false);

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        IncognitoNtpOmniboxAutofocusTracker.HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                        IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusOutcome.NOT_TRIGGERED);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL, true);
        watcher.pollInstrumentationThreadUntilSatisfied();
    }

    private void waitForOmniboxFocus() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Omnibox should be focused.",
                            mActivityTestRule.getActivity().getToolbarManager().isUrlBarFocused(),
                            Matchers.is(true));
                });
    }

    private HistogramWatcher createAutoFocusHistogramWatcher() {
        return HistogramWatcher.newBuilder()
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .build();
    }

    private HistogramWatcher createManualFocusHistogramWatcher() {
        return HistogramWatcher.newBuilder()
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectAnyRecord(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectNoRecords(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .expectNoRecords(
                        IncognitoNtpOmniboxAutofocusTracker
                                .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE)
                .build();
    }
}
