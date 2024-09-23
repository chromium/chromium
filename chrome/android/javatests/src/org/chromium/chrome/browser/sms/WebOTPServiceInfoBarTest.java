// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.components.browser_ui.sms.WebOTPServiceInfoBar;
import org.chromium.components.browser_ui.sms.WebOTPServiceUma;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;

/** Tests for the WebOTPServiceInfoBar class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebOTPServiceInfoBarTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeActivity mActivity;
    private static final String INFOBAR_HISTOGRAM = "Blink.Sms.Receive.Infobar";
    private static final String TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM =
            "Blink.Sms.Receive.TimeCancelOnKeyboardDismissal";

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();
    }

    private WebOTPServiceInfoBar createInfoBar() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = mActivity.getActivityTab();
                    WebOTPServiceInfoBar infoBar =
                            WebOTPServiceInfoBar.create(
                                    mActivity.getWindowAndroid(),
                                    /* enumeratedIconId= */ 0,
                                    "title",
                                    "message",
                                    "ok");
                    InfoBarContainer.get(tab).addInfoBarForTesting(infoBar);
                    return infoBar;
                });
    }

    private void assertHistogramRecordedCount(String name, int expectedCount) {
        Assert.assertEquals(expectedCount, RecordHistogram.getHistogramTotalCountForTesting(name));
    }

    private void assertHistogramRecordedCount(String name, int sample, int expectedCount) {
        Assert.assertEquals(
                expectedCount, RecordHistogram.getHistogramValueCountForTesting(name, sample));
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarOk() {
        // Get current counts because Histogram is not reset between runs with test batching.
        int shown_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN);
        int dismissed_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED);
        WebOTPServiceInfoBar infoBar = createInfoBar();

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Click primary button.
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));

        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, shown_count + 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM,
                WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED,
                dismissed_count + 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarClose() {
        // Get current counts because Histogram is not reset between runs with test batching.
        int shown_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN);
        int dismissed_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED);
        WebOTPServiceInfoBar infoBar = createInfoBar();

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Close infobar.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));

        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, shown_count + 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM,
                WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED,
                dismissed_count + 0);
    }

    @DisabledTest(message = "https://crbug.com/1169221")
    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testHideKeyboardWhenInfoBarIsShown() {
        // Get current counts because Histogram is not reset between runs with test batching.
        int shown_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN);
        int dismissed_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED);
        int time_cancel_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, 0);
        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                sActivityTestRule.getKeyboardDelegate();
        EditText editText = new EditText(mActivity);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    FrameLayout.LayoutParams.MATCH_PARENT,
                                    FrameLayout.LayoutParams.MATCH_PARENT);
                    decor.addView(editText, params);
                    editText.requestFocus();
                    keyboardVisibilityDelegate.showKeyboard(editText);
                });

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(
                () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        WebOTPServiceInfoBar infoBar = createInfoBar();

        // Keyboard is hidden after info bar is created and shown.
        CriteriaHelper.pollUiThread(
                () -> !keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, shown_count + 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM,
                WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED,
                dismissed_count + 1);
        assertHistogramRecordedCount(
                TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, time_cancel_count + 0);
        ThreadUtils.runOnUiThreadBlocking(() -> UiUtils.removeViewFromParent(editText));
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testUMARecordedWhenInfobarDismissedAfterHidingKeyboard() {
        // Get current counts because Histogram is not reset between runs with test batching.
        int shown_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN);
        int dismissed_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED);
        int time_cancel_count =
                RecordHistogram.getHistogramValueCountForTesting(
                        TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, 0);
        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                sActivityTestRule.getKeyboardDelegate();
        EditText editText = new EditText(mActivity);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout decor = (FrameLayout) mActivity.getWindow().getDecorView();
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    FrameLayout.LayoutParams.MATCH_PARENT,
                                    FrameLayout.LayoutParams.MATCH_PARENT);
                    decor.addView(editText, params);
                    editText.requestFocus();
                    keyboardVisibilityDelegate.showKeyboard(editText);
                });

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(
                () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        WebOTPServiceInfoBar infoBar = createInfoBar();

        // Keyboard is hidden after info bar is created and shown.
        CriteriaHelper.pollUiThread(
                () -> !keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText));

        // Close info bar.
        InfoBarUtil.clickCloseButton(infoBar);

        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, WebOTPServiceUma.InfobarAction.SHOWN, shown_count + 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM,
                WebOTPServiceUma.InfobarAction.KEYBOARD_DISMISSED,
                dismissed_count + 1);
        assertHistogramRecordedCount(
                TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, time_cancel_count + 1);
        ThreadUtils.runOnUiThreadBlocking(() -> UiUtils.removeViewFromParent(editText));
    }
}
