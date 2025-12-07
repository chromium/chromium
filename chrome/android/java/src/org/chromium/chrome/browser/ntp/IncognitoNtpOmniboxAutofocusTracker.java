// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.WindowAndroid;

/**
 * A utility class for collecting and recording layout metrics on the Incognito New Tab Page for the
 * Omnibox Autofocus feature.
 */
@NullMarked
public class IncognitoNtpOmniboxAutofocusTracker {
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
