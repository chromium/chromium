// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ntp.IncognitoNtpOmniboxAutofocusTracker.OmniboxAutofocusTabHeightChange;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link IncognitoNtpOmniboxAutofocusTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class IncognitoNtpOmniboxAutofocusTrackerUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private View mTabView;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private KeyboardVisibilityDelegate mKeyboardDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTab.getView()).thenReturn(mTabView);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mWindowAndroid.getKeyboardDelegate()).thenReturn(mKeyboardDelegate);
        when(mKeyboardDelegate.isKeyboardShowing(mTabView)).thenReturn(true);
    }

    private void runTestAndAssertHistograms(
            int tabHeightBeforeFocus,
            int tabHeightAfterFocus,
            @OmniboxAutofocusTabHeightChange int expectedTabHeightChange,
            boolean expectRecordAllLayoutHistograms) {
        when(mTabView.getHeight()).thenReturn(tabHeightAfterFocus);

        HistogramWatcher watcher =
                buildHistogramWatcher(expectedTabHeightChange, expectRecordAllLayoutHistograms);

        IncognitoNtpOmniboxAutofocusTracker.collectLayoutMetricsOnKeyboardVisible(
                mTab,
                tabHeightBeforeFocus,
                new IncognitoNtpUtils.IncognitoNtpContentMetrics(800, 700, 50, 50),
                true);

        watcher.assertExpected();
    }

    private HistogramWatcher buildHistogramWatcher(
            @OmniboxAutofocusTabHeightChange int expectedTabHeightChange,
            boolean expectRecordAllLayoutHistograms) {
        HistogramWatcher.Builder builder =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                IncognitoNtpOmniboxAutofocusTracker
                                        .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_TAB_HEIGHT_CHANGE,
                                expectedTabHeightChange);

        String[] otherHistograms = {
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_KEYBOARD_HEIGHT_PERCENTAGE,
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_HEIGHT_PERCENTAGE,
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_HEIGHT_PERCENTAGE,
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_FOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_WITH_TOP_PADDING_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE,
            IncognitoNtpOmniboxAutofocusTracker
                    .HISTOGRAM_OMNIBOX_AUTOFOCUS_ON_AUTOFOCUS_NTP_TEXT_CONTENT_NO_PADDINGS_BEHIND_KEYBOARD_HEIGHT_PERCENTAGE
        };

        for (String histogram : otherHistograms) {
            if (expectRecordAllLayoutHistograms) {
                builder.expectAnyRecord(histogram);
            } else {
                builder.expectNoRecords(histogram);
            }
        }

        return builder.build();
    }

    @Test
    public void testRecordsOnlyTabHeightChangeHistogram_IncreasedFromZero() {
        runTestAndAssertHistograms(
                0, 500, OmniboxAutofocusTabHeightChange.INCREASED_FROM_ZERO, false);
    }

    @Test
    public void testRecordsOnlyTabHeightChangeHistogram_UnchangedAtZero() {
        runTestAndAssertHistograms(0, 0, OmniboxAutofocusTabHeightChange.UNCHANGED_AT_ZERO, false);
    }

    @Test
    public void testRecordsAllLayoutHistograms_DecreasedToZero() {
        runTestAndAssertHistograms(
                1000, 0, OmniboxAutofocusTabHeightChange.DECREASED_TO_ZERO, false);
    }

    @Test
    public void testRecordsOnlyTabHeightChangeHistogram_IncreasedFromNonZero() {
        runTestAndAssertHistograms(
                500, 1000, OmniboxAutofocusTabHeightChange.INCREASED_FROM_NON_ZERO, false);
    }

    @Test
    public void testRecordsOnlyTabHeightChangeHistogram_UnchangedAtNonZero() {
        runTestAndAssertHistograms(
                1000, 1000, OmniboxAutofocusTabHeightChange.UNCHANGED_AT_NON_ZERO, false);
    }

    @Test
    public void testRecordsAllLayoutHistograms_DecreasedToNonZero() {
        runTestAndAssertHistograms(
                1000, 500, OmniboxAutofocusTabHeightChange.DECREASED_TO_NON_ZERO, true);
    }
}
