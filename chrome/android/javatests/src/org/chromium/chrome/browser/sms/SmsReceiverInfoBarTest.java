// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sms;

import android.support.test.filters.MediumTest;
import android.widget.EditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;

/**
 * Tests for the SmsReceiverInfoBar class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SmsReceiverInfoBarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private ChromeActivity mActivity;
    private static final String INFOBAR_HISTOGRAM = "Blink.Sms.Receive.Infobar";
    private static final String TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM =
            "Blink.Sms.Receive.TimeCancelOnKeyboardDismissal";

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
    }

    private SmsReceiverInfoBar createInfoBar() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = mActivity.getActivityTab();
            ActivityWindowAndroid windowAndroid = new ActivityWindowAndroid(mActivity);
            SmsReceiverInfoBar infoBar = SmsReceiverInfoBar.create(
                    windowAndroid, /*enumeratedIconId=*/0, "title", "message", "ok");
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
        SmsReceiverInfoBar infoBar = createInfoBar();

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Click primary button.
        Assert.assertTrue(InfoBarUtil.clickPrimaryButton(infoBar));

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.KEYBOARD_DISMISSED, 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testSmsInfoBarClose() {
        SmsReceiverInfoBar infoBar = createInfoBar();

        Assert.assertFalse(InfoBarUtil.hasSecondaryButton(infoBar));

        // Close infobar.
        Assert.assertTrue(InfoBarUtil.clickCloseButton(infoBar));

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.KEYBOARD_DISMISSED, 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testHideKeyboardWhenInfoBarIsShown() {
        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                mActivityTestRule.getKeyboardDelegate();
        EditText editText = new EditText(mActivity);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.setContentView(editText);
            editText.requestFocus();
            keyboardVisibilityDelegate.showKeyboard(editText);
        });

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(Criteria.equals(
                true, () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText)));

        SmsReceiverInfoBar infoBar = createInfoBar();

        // Keyboard is hidden after info bar is created and shown.
        CriteriaHelper.pollUiThread(Criteria.equals(
                false, () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText)));

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.KEYBOARD_DISMISSED, 1);
        assertHistogramRecordedCount(TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, 0);
    }

    @Test
    @MediumTest
    @Feature({"InfoBars", "UiCatalogue"})
    public void testUMARecordedWhenInfobarDismissedAfterHidingKeyboard() {
        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                mActivityTestRule.getKeyboardDelegate();
        EditText editText = new EditText(mActivity);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.setContentView(editText);
            editText.requestFocus();
            keyboardVisibilityDelegate.showKeyboard(editText);
        });

        // Wait until the keyboard is showing.
        CriteriaHelper.pollUiThread(Criteria.equals(
                true, () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText)));

        SmsReceiverInfoBar infoBar = createInfoBar();

        // Keyboard is hidden after info bar is created and shown.
        CriteriaHelper.pollUiThread(Criteria.equals(
                false, () -> keyboardVisibilityDelegate.isKeyboardShowing(mActivity, editText)));

        // Close info bar.
        InfoBarUtil.clickCloseButton(infoBar);

        assertHistogramRecordedCount(INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.SHOWN, 1);
        assertHistogramRecordedCount(
                INFOBAR_HISTOGRAM, SmsReceiverUma.InfobarAction.KEYBOARD_DISMISSED, 1);
        assertHistogramRecordedCount(TIME_CANCEL_ON_KEYBOARD_DISMISSAL_HISTOGRAM, 1);
    }
}
