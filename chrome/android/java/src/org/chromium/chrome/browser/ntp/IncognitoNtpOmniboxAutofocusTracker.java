// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A utility class for collecting and recording layout metrics on the Incognito New Tab Page for the
 * Omnibox Autofocus feature.
 */
@NullMarked
public class IncognitoNtpOmniboxAutofocusTracker {
    public static final String HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE =
            "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.KeyboardHeightPercentage";
    public static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingHeightPercentage";
    public static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsHeightPercentage";
    public static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage";
    public static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnFocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage";

    public static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentWithTopPaddingBehindKeyboardHeightPercentage";
    public static final String
            HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE =
                    "NewTabPage.Incognito.OmniboxAutofocus.OnAutofocus.NtpTextContentNoPaddingsBehindKeyboardHeightPercentage";

    public static final String HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME =
            "NewTabPage.Incognito.OmniboxAutofocus.Outcome";

    // LINT.IfChange(OmniboxAutofocusOutcome)
    @IntDef({
        OmniboxAutofocusOutcome.NOT_TRIGGERED,
        OmniboxAutofocusOutcome.ALWAYS_ON,
        OmniboxAutofocusOutcome.NOT_FIRST_TAB,
        OmniboxAutofocusOutcome.WITH_PREDICTION,
        OmniboxAutofocusOutcome.WITH_HARDWARE_KEYBOARD,
        OmniboxAutofocusOutcome.NOT_FIRST_TAB_WITH_PREDICTION,
        OmniboxAutofocusOutcome.NOT_FIRST_TAB_WITH_HARDWARE_KEYBOARD,
        OmniboxAutofocusOutcome.WITH_PREDICTION_WITH_HARDWARE_KEYBOARD,
        OmniboxAutofocusOutcome.NOT_FIRST_TAB_WITH_PREDICTION_WITH_HARDWARE_KEYBOARD
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface OmniboxAutofocusOutcome {
        int NOT_TRIGGERED = 0;
        int ALWAYS_ON = 1;
        int NOT_FIRST_TAB = 2;
        int WITH_PREDICTION = 3;
        int WITH_HARDWARE_KEYBOARD = 4;
        int NOT_FIRST_TAB_WITH_PREDICTION = 5;
        int NOT_FIRST_TAB_WITH_HARDWARE_KEYBOARD = 6;
        int WITH_PREDICTION_WITH_HARDWARE_KEYBOARD = 7;
        int NOT_FIRST_TAB_WITH_PREDICTION_WITH_HARDWARE_KEYBOARD = 8;
        int COUNT = 9;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:OmniboxAutofocusOutcome)

    /**
     * Records the conditions that led to the omnibox autofocus feature being triggered.
     *
     * @param noConditionsConfigured Whether the feature is configured to always autofocus without
     *     any specific conditions.
     * @param isAutofocusAllowedNotFirstTab Whether autofocus is allowed for a tab that is not the
     *     very first Incognito tab opened in the current session.
     * @param isAutofocusAllowedWithPrediction Whether autofocus is allowed based on the predicted
     *     available free space on the Incognito tab.
     * @param isAutofocusAllowedWithHardwareKeyboard Whether autofocus is allowed when a hardware
     *     keyboard is connected.
     */
    public static void recordAutofocusTriggered(
            boolean noConditionsConfigured,
            boolean isAutofocusAllowedNotFirstTab,
            boolean isAutofocusAllowedWithPrediction,
            boolean isAutofocusAllowedWithHardwareKeyboard) {
        @OmniboxAutofocusOutcome int outcome;
        if (noConditionsConfigured) {
            outcome = OmniboxAutofocusOutcome.ALWAYS_ON;
        } else if (isAutofocusAllowedNotFirstTab
                && isAutofocusAllowedWithPrediction
                && isAutofocusAllowedWithHardwareKeyboard) {
            outcome = OmniboxAutofocusOutcome.NOT_FIRST_TAB_WITH_PREDICTION_WITH_HARDWARE_KEYBOARD;
        } else if (isAutofocusAllowedNotFirstTab && isAutofocusAllowedWithPrediction) {
            outcome = OmniboxAutofocusOutcome.NOT_FIRST_TAB_WITH_PREDICTION;
        } else if (isAutofocusAllowedNotFirstTab && isAutofocusAllowedWithHardwareKeyboard) {
            outcome = OmniboxAutofocusOutcome.NOT_FIRST_TAB_WITH_HARDWARE_KEYBOARD;
        } else if (isAutofocusAllowedWithPrediction && isAutofocusAllowedWithHardwareKeyboard) {
            outcome = OmniboxAutofocusOutcome.WITH_PREDICTION_WITH_HARDWARE_KEYBOARD;
        } else if (isAutofocusAllowedNotFirstTab) {
            outcome = OmniboxAutofocusOutcome.NOT_FIRST_TAB;
        } else if (isAutofocusAllowedWithPrediction) {
            outcome = OmniboxAutofocusOutcome.WITH_PREDICTION;
        } else if (isAutofocusAllowedWithHardwareKeyboard) {
            outcome = OmniboxAutofocusOutcome.WITH_HARDWARE_KEYBOARD;
        } else {
            assert false : "Method called with no conditions met.";
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME, outcome, OmniboxAutofocusOutcome.COUNT);
    }

    /** Records that the omnibox autofocus conditions were checked, but none were met. */
    public static void recordAutofocusNotTriggered() {
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_OUTCOME,
                OmniboxAutofocusOutcome.NOT_TRIGGERED,
                OmniboxAutofocusOutcome.COUNT);
    }

    /**
     * Collects metrics comparing the Incognito NTP layout before and after the keyboard becomes
     * visible.
     *
     * @param tab The {@link Tab} to collect metrics for.
     * @param tabHeightBeforeFocus The height of the tab's view before the keyboard was visible.
     * @param ntpContentMetrics Provides metrics of the main text content area on the Incognito NTP.
     * @param omniboxAutofocused Whether metrics collection was triggered by omnibox autofocus.
     */
    public static void collectLayoutMetricsOnKeyboardVisible(
            Tab tab,
            double tabHeightBeforeFocus,
            IncognitoNtpUtils.IncognitoNtpContentMetrics ntpContentMetrics,
            boolean omniboxAutofocused) {
        if (tab.getView() == null || tabHeightBeforeFocus <= 0) {
            return;
        }

        WindowAndroid windowAndroid = tab.getWindowAndroid();

        if (windowAndroid == null) {
            return;
        }

        Runnable recordLayoutMetricsRunnable =
                () -> {
                    recordLayoutMetrics(
                            tab, tabHeightBeforeFocus, ntpContentMetrics, omniboxAutofocused);
                };

        if (windowAndroid.getKeyboardDelegate().isKeyboardShowing(tab.getView())) {
            recordLayoutMetricsRunnable.run();
            return;
        }

        KeyboardVisibilityListener listener =
                new KeyboardVisibilityListener() {
                    @Override
                    public void keyboardVisibilityChanged(boolean isShowing) {
                        if (isShowing) {
                            windowAndroid
                                    .getKeyboardDelegate()
                                    .removeKeyboardVisibilityListener(this);

                            if (tab.getView() != null) {
                                tab.getView().post(recordLayoutMetricsRunnable);
                            }
                        }
                    }
                };
        windowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(listener);
    }

    /**
     * Records layout metrics on the Incognito NTP.
     *
     * @param tab The {@link Tab} to record metrics for.
     * @param tabHeightBeforeFocus The height of the tab's view before the keyboard was visible.
     * @param ntpContentMetrics Provides metrics of the main text content area on the Incognito NTP.
     * @param omniboxAutofocused Whether metrics collection was triggered by omnibox autofocus.
     */
    private static void recordLayoutMetrics(
            Tab tab,
            double tabHeightBeforeFocus,
            IncognitoNtpUtils.IncognitoNtpContentMetrics ntpContentMetrics,
            boolean omniboxAutofocused) {
        if (tab.getView() == null) {
            return;
        }

        final double tabViewHeightAfterFocus = tab.getView().getHeight();
        final double ntpTextContentHeightWithTopPadding =
                ntpContentMetrics.ntpViewHeightPx - ntpContentMetrics.textContentBottomPaddingPx;
        final double ntpTextContentHeightBehindKeyboardPx =
                Math.max((ntpTextContentHeightWithTopPadding - tabViewHeightAfterFocus), 0);

        final double keyboardHeightRatio =
                (tabHeightBeforeFocus - tabViewHeightAfterFocus) / tabHeightBeforeFocus;

        final double ntpTextContentWithTopPaddingHeightRatio =
                ntpTextContentHeightWithTopPadding / tabHeightBeforeFocus;
        final double ntpTextContentNoPaddingsHeightRatio =
                ntpContentMetrics.textContentHeightPx / tabHeightBeforeFocus;

        final double ntpTextContentWithTopPaddingBehindKeyboardHeightRatio =
                ntpTextContentHeightBehindKeyboardPx / ntpTextContentHeightWithTopPadding;
        final double ntpTextContentNoPaddingsBehindKeyboardHeightRatio =
                ntpTextContentHeightBehindKeyboardPx / ntpContentMetrics.textContentHeightPx;

        RecordHistogram.recordPercentageHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE,
                (int) Math.round(keyboardHeightRatio * 100));
        RecordHistogram.recordLinearCountHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE,
                (int) Math.round(ntpTextContentWithTopPaddingHeightRatio * 100),
                0,
                200,
                100);
        RecordHistogram.recordLinearCountHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE,
                (int) Math.round(ntpTextContentNoPaddingsHeightRatio * 100),
                0,
                200,
                100);
        RecordHistogram.recordPercentageHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
                (int) Math.round(ntpTextContentWithTopPaddingBehindKeyboardHeightRatio * 100));
        RecordHistogram.recordPercentageHistogram(
                HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
                (int) Math.round(ntpTextContentNoPaddingsBehindKeyboardHeightRatio * 100));

        if (omniboxAutofocused) {
            RecordHistogram.recordPercentageHistogram(
                    HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
                    (int) Math.round(ntpTextContentWithTopPaddingBehindKeyboardHeightRatio * 100));
            RecordHistogram.recordPercentageHistogram(
                    HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
                    (int) Math.round(ntpTextContentNoPaddingsBehindKeyboardHeightRatio * 100));
        }
    }
}
